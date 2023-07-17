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

static void send_modified_packet(struct sk_buff *skb, const void *payload, int payload_len)
{
    struct iphdr *iph = ip_hdr(skb);
    struct tcphdr *tcph = tcp_hdr(skb);
    struct net_device *dev = skb->dev;

    // 分配新的数据包
    struct sk_buff *new_skb;
    // 计算分片数量
    int fragment_count = (payload_len + dev->mtu - sizeof(struct iphdr) - sizeof(struct tcphdr) - 1) / (dev->mtu - sizeof(struct iphdr) - sizeof(struct tcphdr));
    int i, remaining = payload_len;

    // 发送数据包分片
    // 发送数据包分片
    for (i = 0; i < fragment_count; i++)
    {
        int fragment_len = min(remaining, dev->mtu);

        // 分配新的sk_buff和头部空间
        new_skb = alloc_skb(dev->mtu + sizeof(struct iphdr) + sizeof(struct tcphdr) + fragment_len,
                            GFP_ATOMIC);
        if (!new_skb)
        {
            break;
        }

        // 初始化新的sk_buff
        skb_reserve(new_skb, sizeof(struct iphdr) + sizeof(struct tcphdr));

        // 设置新的IP头部
        struct iphdr *new_iph = ip_hdr(new_skb);
        memcpy(new_iph, iph, sizeof(struct iphdr));
        new_iph->daddr = iph->saddr;
        new_iph->saddr = iph->daddr;
        new_iph->check = 0;
        new_iph->check = ip_fast_csum((unsigned char *)new_iph, new_iph->ihl);

        // 设置新的TCP头部
        struct tcphdr *new_tcph = tcp_hdr(new_skb);
        memcpy(new_tcph, tcph, sizeof(struct tcphdr));
        new_tcph->source = tcph->dest;
        new_tcph->dest = tcph->source;
        new_tcph->seq = htonl(ntohl(tcph->ack_seq) + i * dev->mtu);
        new_tcph->ack_seq = htonl(ntohl(tcph->seq) + 1);
        new_tcph->check = 0;
        new_tcph->urg_ptr = 0;

        // 将数据复制到新的数据包
        skb_put_data(new_skb, payload + i * dev->mtu, fragment_len);

        // 发送新的数据包
        new_tcph->check = tcp_v4_check(sizeof(struct tcphdr), new_iph->saddr, new_iph->daddr,
                                       csum_partial(new_tcph, sizeof(struct tcphdr), 0));

        // 注入新的数据包到网络协议栈
        dev_queue_xmit(new_skb);

        remaining -= fragment_len;
    }
}

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
                // 构造要发送的数据内容
                const char *payload_data = "Hello, server!";
                int payload_len = strlen(payload_data);
                // 发送新的数据包
                send_modified_packet(skb, payload_data, payload_len);
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
