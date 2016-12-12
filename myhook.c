 #ifdef HNDCTF
 #define NFPROTO_FASTNAT NFPROTO_UNSPEC
 /* add by mo : for compatible with fastnat , hooknum*/
#define NF_BR_FASTNAT_XMIT_HOOK_L2	0
#define NF_BR_FASTNAT_XMIT_HOOK_L3	1
//NF_MAX_HOOKS = 8

/* priority*/
enum NF_BR_FASTNAT_hook_priorities {
	FASTNAT_PRI_FIRST = INT_MIN,
	FASTNAT_PRI_OTHER = -100,
	FASTNAT_PRI_BRNF = 0,
	FASTNAT_PRI_SUQ = 100,
	FASTNAT_PRI_LAST = INT_MAX,
};
 int br_nf_fastnat_xmit_finish(struct sk_buff *skb){		
	skb->xmit_flag = true;
	dev_queue_xmit(skb);
	return 0;
 }
int br_nf_fastnat_xmit_l3_finish(struct sk_buff *skb){
	skb_push(skb, ETH_HLEN); // skb->data  back to eth	
	//NF_BR_FASTNAT_XMIT_HOOK_L2 返回 NF_ACCEPT || verdict == NF_STOP 才会执行br_nf_fastnat_xmit_finish
	NF_HOOK_THRESH(NFPROTO_FASTNAT, NF_BR_FASTNAT_XMIT_HOOK_L2, skb,  skb->bridge_rx_dev? :skb->rx_dev, 
				skb->bridge_tx_dev? :skb->dev, br_nf_fastnat_xmit_finish, FASTNAT_PRI_BRNF + 1);
	//注意,回到二层上后,是从FASTNAT_PRI_BRNF+1开始回调处理函数
	return 0;
}
static unsigned int br_nf_fastnat_xmit_l3(unsigned int hook, struct sk_buff *skb,
				      const struct net_device *in,
				      const struct net_device *out,
				      int (*okfn)(struct sk_buff *))
{
	//skb->data  point to  iph	
	skb_pull_inline(skb, ETH_HLEN);
	/*
	   注册到NF_BR_FASTNAT_XMIT_HOOK_L3 的每个处理函数返回ACCEPT才会轮循完
	   NF_STOP 应该就是不遍历后面(即优先级更低)的处理函数, 
	   如果NF_BR_FASTNAT_XMIT_HOOK_L3 没有注册钩子处理函数,返回ACCEPT, 并执行okfn
	   NF_BR_FASTNAT_XMIT_HOOK_L3 返回 NF_ACCEPT || verdict == NF_STOP 才会执行okfn 即 br_nf_fastnat_xmit_l3_finish	   
	*/
	NF_HOOK(NFPROTO_FASTNAT, NF_BR_FASTNAT_XMIT_HOOK_L3, skb, skb->ipc_rx_dev? : skb->rx_dev, skb->dev,
			 br_nf_fastnat_xmit_l3_finish);
                                                    //br_nf_fastnat_xmit_l3_finish 截skb 并由br_nf_fastnat_xmit_finish发送,所以下面返回stolen
	return NF_STOLEN; 
	//这样不会执行dev_queue_xmit() 的br_nf_fastnat_xmit_finish, 即dev_queue_xmit的NF_HOOK 返回NF_STOLEN
        
}
static struct nf_hook_ops br_nf_fastnat_ops[] __read_mostly = {
	{
		.hook = br_nf_fastnat_xmit_l3,
		.owner = THIS_MODULE,
		.pf = NFPROTO_UNSPEC,
		.hooknum = NF_BR_FASTNAT_XMIT_HOOK_L2,
		.priority = FASTNAT_PRI_BRNF,
	},
};
//================ test ============
#define PRINT_DEV_NAME(dev) 	(dev)?((struct net_device *)dev)->name:"null"
#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]
static unsigned int test_qos_fn(unsigned int hook, struct sk_buff *skb,
				      const struct net_device *in,
				      const struct net_device *out,
				      int (*okfn)(struct sk_buff *))
{
	if(in)
		printk(" ========fun=%s, in dev->name =%s =======\n", __FUNCTION__, in->name);
	if(out)
		printk(" ========fun=%s, out dev->name =%s =======\n", __FUNCTION__, out->name);

	printk(" ========fun=%s, bridge_rx_dev =%s ,bridge_tx_dev =%s, ipc_rx_dev=%s, skb->dev=%s =======\n", __FUNCTION__,
		PRINT_DEV_NAME(skb->bridge_rx_dev),PRINT_DEV_NAME(skb->bridge_tx_dev),
		PRINT_DEV_NAME(skb->ipc_rx_dev), PRINT_DEV_NAME(skb->dev));
	
	return NF_ACCEPT;
}
static unsigned int test_auth_fn(unsigned int hook, struct sk_buff *skb,
				      const struct net_device *in,
				      const struct net_device *out,
				      int (*okfn)(struct sk_buff *))
{
	struct iphdr *iph;
	iph = ip_hdr(skb);
	if(iph)
		printk("======fun=%s, saddr=%u.%u.%u.%u , daddr=%u.%u.%u.%u ===\n", __FUNCTION__, NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));
	if(skb->nfct){
		printk(" ========fun=%s,  skb->nfct =%d =======\n", __FUNCTION__, atomic_read(skb->nfct));
	}	
	if(in)
		printk(" ========fun=%s, in dev->name =%s =======\n", __FUNCTION__, in->name);
	if(out)
		printk(" ========fun=%s,  out dev->name =%s =======\n", __FUNCTION__, out->name);

	printk(" ========fun=%s, bridge_rx_dev =%s ,bridge_tx_dev =%s, ipc_rx_dev=%s, skb->dev=%s =======\n", __FUNCTION__,
		PRINT_DEV_NAME(skb->bridge_rx_dev),PRINT_DEV_NAME(skb->bridge_tx_dev),
		PRINT_DEV_NAME(skb->ipc_rx_dev), PRINT_DEV_NAME(skb->dev));	
	return NF_ACCEPT; 
}
static struct nf_hook_ops test_ops[] __read_mostly = {
	{
		.hook = test_qos_fn,
		.owner = THIS_MODULE,
		.pf = NFPROTO_FASTNAT,
		.hooknum = NF_BR_FASTNAT_XMIT_HOOK_L2,
		.priority = FASTNAT_PRI_SUQ,
	},
	{
		.hook = test_auth_fn,
		.owner = THIS_MODULE,
		.pf = NFPROTO_FASTNAT,
		.hooknum = NF_BR_FASTNAT_XMIT_HOOK_L3,
		.priority = FASTNAT_PRI_BRNF,
	},	
};
 #endif
 //-ENOMEM  NET_XMIT_DROP  NET_XMIT_SUCCESS  -ENETDOWN
int BCMFASTPATH_HOST dev_queue_xmit(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct netdev_queue *txq;
	struct Qdisc *q;
	int rc = -ENOMEM;

/* ==========add by mo: compatible with fastnat，in dev_queue_xmit================*/
#ifdef HNDCTF
	//NF_BR_FORWARD  NF_MAX_HOOKS = 8
	//suq  NFPROTO_UNSPEC , 
	if (!skb->xmit_flag) {
		/*
		skb_reset_mac_header(skb);				
		if (skb_pull_inline(skb, ETH_HLEN))	{
			skb->protocol = eth_hdr(skb)->h_proto; // have been done in osl_startxmit 
			skb_reset_network_header(skb);
		}
		*/
		//skb->data point to eth
		return NF_HOOK(NFPROTO_FASTNAT, NF_BR_FASTNAT_XMIT_HOOK_L2, skb, skb->bridge_rx_dev? :skb->rx_dev, 
				skb->bridge_tx_dev? :skb->dev, br_nf_fastnat_xmit_finish);
		
	}
#endif
/* ==========end by mo================*/