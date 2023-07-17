#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/inet.h>
#include <linux/in.h>

#define TARGET_PORT 80 // 目标监听端口
// #define TARGET_APP_NAME "nginx" // 目标应用程序名称

static struct nf_hook_ops nfho; // Netfilter钩子结构体

// Netfilter钩子函数，用于捕获数据包
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
unsigned int hook_func(const struct nf_hook_ops *ops, struct sk_buff *skb, const struct net_device *in, const struct net_device *out, const struct nf_hook_state *state)
#else
unsigned int hook_func(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
#endif
{
    struct iphdr *iph = ip_hdr(skb);
    struct tcphdr *tcph = tcp_hdr(skb);

    // 仅处理TCP数据包
    if (iph->protocol == IPPROTO_TCP)
    {
        // 判断目标端口和应用程序名称
        if (ntohs(tcph->source) == TARGET_PORT /*&& strstr(current->comm, TARGET_APP_NAME)*/)
        {
            // 如果检测到SYN+ACK包，则修改TCP窗口大小
            if (tcph->syn && tcph->ack)
            {
                tcph->window = htons(4); // 修改窗口大小为1234
            }
        }
    }

    return NF_ACCEPT;
}

// 模块加载函数
static int __init my_module_init(void)
{
    nfho.hook = hook_func;            // 设置钩子函数
    nfho.hooknum = NF_INET_LOCAL_OUT; // 在数据包离开本地输出路径时触发
    nfho.pf = PF_INET;                // IPv4
    nfho.priority = NF_IP_PRI_FIRST;  // 设置优先级
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
    nf_register_hook(&nfho);
#else
    nf_register_net_hook(&init_net, &nfho);   // 注册钩子
#endif
    return 0;
}

// 模块卸载函数
static void __exit my_module_exit(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
    nf_unregister_hook(&nfho);
#else
    nf_unregister_net_hook(&init_net, &nfho); // 注销钩子
#endif
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("Apache 2.0");
MODULE_AUTHOR("CNRE");
