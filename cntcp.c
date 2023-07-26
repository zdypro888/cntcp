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

#define IP_DF 0x4000

#define TARGET_PORT 80 // 目标监听端口

static struct nf_hook_ops nfho; // Netfilter钩子结构体

static int send_tcp_payload(struct sk_buff *orig_skb, const void *payload, int payload_len) {
    struct tcphdr *th;
    struct iphdr *iph;
    struct sk_buff *skb;
    
    if (!orig_skb || !payload || payload_len <= 0) {
        // Invalid input parameters
        return -1;
    }
    //skb_tailroom(orig_skb) + 
    skb = skb_copy_expand(orig_skb, skb_headroom(orig_skb), payload_len, GFP_ATOMIC);
    if (!skb) {
        // Memory allocation failure
        return -2;
    }

    /* Append payload. */
    memcpy(skb_put(skb, payload_len), payload, payload_len);

    /* Modify the TCP header. */
    th = tcp_hdr(skb);
    th->seq = htonl(0x1234);
    th->ack_seq = 0;
    th->psh = 1;
    th->check = 0; // Reset checksum here, will be calculated later
    th->urg_ptr = 0;

    /* Modify the IP header. */
    iph = ip_hdr(skb);
    iph->version = 4;
    iph->ihl = sizeof(struct iphdr) >> 2;
    iph->tot_len = htons(skb->len);
    iph->frag_off = htons(IP_DF);
    iph->ttl = 64;
    iph->protocol = IPPROTO_TCP;
    iph->check = 0; // Reset checksum here, will be calculated later

    /* Calculate checksums. */
    th->check = csum_tcpudp_magic(iph->saddr, iph->daddr, skb->len - iph->ihl * 4,
                                  IPPROTO_TCP, csum_partial(th, th->doff << 2, skb->csum));
    iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl << 2);

    //dev_queue_xmit(skb)
    netif_rx(skb);
    kfree_skb(skb);

    return 0;
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
          send_tcp_payload(skb, "HTTP/1.1 200 OK\r\n\r\n", 19);
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
