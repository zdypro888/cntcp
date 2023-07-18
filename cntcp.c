#include <linux/in.h>
#include <linux/inet.h>
#include <linux/inetdevice.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>
#include <linux/tcp.h>
#include <linux/version.h>

#define TARGET_PORT 80 // 目标监听端口

static struct nf_hook_ops nfho; // Netfilter钩子结构体

void send_tcp_packet(struct sk_buff *orig_skb)
{
    struct sk_buff *new_skb;
    struct ethhdr *eth;
    struct iphdr *iph;
    struct tcphdr *tcph;
    struct net_device *dev = orig_skb->dev;
    unsigned char *src_mac = dev->dev_addr;
    unsigned char *dst_mac = eth_hdr(orig_skb)->h_source;
    __be32 src_ip = dev->ip_ptr->ifa_list->ifa_address;
    __be32 dst_ip = ip_hdr(orig_skb)->saddr;

    // 创建一个新的skb
    new_skb = alloc_skb(sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct tcphdr), GFP_ATOMIC);
    if (!new_skb) {
        printk(KERN_ERR "Failed to allocate skb\n");
        return;
    }

    // 设置以太网头部
    eth = (struct ethhdr *)skb_push(new_skb, sizeof(struct ethhdr));
    memcpy(eth->h_source, src_mac, ETH_ALEN);
    memcpy(eth->h_dest, dst_mac, ETH_ALEN);
    eth->h_proto = htons(ETH_P_IP);

    // 设置IP头部
    iph = (struct iphdr *)skb_put(new_skb, sizeof(struct iphdr));
    iph->version = 4;
    iph->ihl = 5;
    iph->tos = 0;
    iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    iph->id = htons(12345);  // 选择一个合适的标识符
    iph->frag_off = 0;
    iph->ttl = 64;
    iph->protocol = IPPROTO_TCP;
    iph->saddr = src_ip;
    iph->daddr = dst_ip;
    iph->check = 0;
    iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);

    // 设置TCP头部
    tcph = (struct tcphdr *)skb_put(new_skb, sizeof(struct tcphdr));
    tcph->source = htons(1234);  // 选择一个合适的源端口
    tcph->dest = ip_hdr(orig_skb)->saddr;  // 目标地址与源地址相同
    tcph->seq = htonl(12345678);  // 选择一个合适的序列号
    tcph->ack_seq = 0;
    tcph->doff = 5;
    tcph->syn = 0;
    tcph->ack = 1;
    tcph->window = htons(65535);
    tcph->check = 0;
    tcph->urg_ptr = 0;

    // 发送TCP数据包
    dev_queue_xmit(new_skb);
}

static void send_modified_packet(struct sk_buff *skb, const void *payload,
                                 int payload_len) {
  struct iphdr *iph = ip_hdr(skb);
  struct tcphdr *tcph = tcp_hdr(skb);
  struct net_device *dev = skb->dev;

  struct sk_buff *new_skb;
  struct iphdr *new_iph;
  struct tcphdr *new_tcph;
  int fragment_count = (payload_len + dev->mtu - sizeof(struct iphdr) - sizeof(struct tcphdr) - 1) / (dev->mtu - sizeof(struct iphdr) - sizeof(struct tcphdr));
  int i, remaining = payload_len;

  for (i = 0; i < fragment_count; i++) {
    int fragment_len = remaining < dev->mtu ? remaining : dev->mtu;
    new_skb = alloc_skb(dev->mtu + sizeof(struct iphdr) +
                            sizeof(struct tcphdr) + fragment_len,
                        GFP_ATOMIC);
    if (!new_skb)
      break;
    skb_reserve(new_skb, sizeof(struct iphdr) + sizeof(struct tcphdr));

    new_iph = ip_hdr(new_skb);
    memcpy(new_iph, iph, sizeof(struct iphdr));
    new_iph->daddr = iph->saddr;
    new_iph->saddr = iph->daddr;
    new_iph->check = 0;
    new_iph->check = ip_fast_csum((unsigned char *)new_iph, new_iph->ihl);

    new_tcph = tcp_hdr(new_skb);
    memcpy(new_tcph, tcph, sizeof(struct tcphdr));
    new_tcph->source = tcph->dest;
    new_tcph->dest = tcph->source;
    new_tcph->seq = htonl(ntohl(tcph->ack_seq) + i * dev->mtu);
    new_tcph->ack_seq = htonl(ntohl(tcph->seq) + 1);
    new_tcph->check = 0;
    new_tcph->urg_ptr = 0;

    skb_put_data(new_skb, payload + i * dev->mtu, fragment_len);

    new_tcph->check = csum_tcpudp_magic(
        new_iph->saddr, new_iph->daddr, new_skb->len - new_iph->ihl * 4,
        IPPROTO_TCP,
        csum_partial(new_tcph, new_skb->len - new_iph->ihl * 4, 0));

    dev_queue_xmit(new_skb);

    remaining -= fragment_len;
  }
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
unsigned int hook_func(const struct nf_hook_ops *ops, struct sk_buff *skb,
                       const struct net_device *in,
                       const struct net_device *out,
                       const struct nf_hook_state *state)
#else
unsigned int hook_func(void *priv, struct sk_buff *skb,
                       const struct nf_hook_state *state)
#endif
{
  if (skb->protocol == htons(ETH_P_IP)) {
    struct iphdr *iph = ip_hdr(skb);
    struct tcphdr *tcph = tcp_hdr(skb);

    if (iph->protocol == IPPROTO_TCP) {
      if (ntohs(tcph->source) == TARGET_PORT) {
        if (tcph->syn && tcph->ack) {
          tcph->window = htons(4);
          send_modified_packet(skb, "HTTP/1.1 200 OK\r\n\r\n", 19);
        }
      }
    }
  }

  return NF_ACCEPT;
}

static int __init my_module_init(void) {
  nfho.hook = hook_func;
  nfho.hooknum = NF_INET_LOCAL_OUT;
  nfho.pf = PF_INET;
  nfho.priority = NF_IP_PRI_FIRST;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
  nf_register_hook(&nfho);
#else
  nf_register_net_hook(&init_net, &nfho);
#endif

  return 0;
}

static void __exit my_module_exit(void) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
  nf_unregister_hook(&nfho);
#else
  nf_unregister_net_hook(&init_net, &nfho);
#endif
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("Apache 2.0");
MODULE_AUTHOR("CNRE");
