#include "attribute.h"

#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/etherdevice.h>
#include <net/route.h>
#include <linux/etherdevice.h>

/* Root object. */
struct filter
{
	struct kobject kobj;
};

/* Passtrhough link. */
struct link
{
	struct kobject kobj;
	char name[32];
	struct net_device *inside, *outside;
	struct net_device *dev;

	struct list_head link_list;
};

struct inline_dev_priv
{
	struct link *link;
};

struct list_head link_list;
static int create_netdev( struct link *l, const char *name );

static inline struct link *get_link( const struct net_device *dev )
{
	return rcu_dereference( dev->rx_handler_data );
}

int filter_handle_frame_finish( struct sk_buff *skb )
{
	struct iphdr *iph = ip_hdr(skb);
	ip_route_input( skb, iph->daddr, iph->saddr, iph->tos, skb->dev );
	skb->pkt_type = PACKET_HOST;
	netif_receive_skb( skb );
	return 0;
}

rx_handler_result_t filter_handle_frame( struct sk_buff **pskb )
{
	struct sk_buff *skb = *pskb;
	struct link *link = get_link( skb->dev );

	if ( link == 0 || link->inside == 0 || link->outside == 0 ) {
		kfree_skb( skb );
		return RX_HANDLER_CONSUMED;
	}

	if ( skb->dev == link->inside ) {
		if ( eth_hdr(skb)->h_proto == htons( ETH_P_ARP ) ) {
			struct sk_buff *up = skb_clone( skb, GFP_ATOMIC );
			printk( "filter.ko: sending up arp\n" );
			up->dev = link->dev;
			up->pkt_type = PACKET_HOST;
			netif_receive_skb( up );
		}
		else if ( eth_hdr(skb)->h_proto == htons( ETH_P_IP ) ) {
			// printk( "filter.ko: ip traffic\n" );
			if ( ip_hdr(skb)->protocol == IPPROTO_TCP ) {
				const int ihlen = ip_hdr(skb)->ihl * 4;
				struct tcphdr *th = (struct tcphdr*) ( ( (char*)ip_hdr(skb)) + ihlen );

				// printk( "filter.ko: ihl: %u\n", (unsigned) ip_hdr(skb)->ihl );
				// printk( "filter.ko: version: %u\n", (unsigned) ip_hdr(skb)->version );
				// printk( "filter.ko: tcp dest: %hu\n", ntohs(th->dest) );

				if ( th->dest == htons( 443 ) ) {
					// printk( "filter.ko: ssl traffic\n" );
					skb->dev = link->dev;
					NF_HOOK( NFPROTO_IPV4, NF_INET_PRE_ROUTING, skb, skb->dev, NULL, filter_handle_frame_finish );
					//kfree_skb( skb );
					return RX_HANDLER_CONSUMED;
				}
			}
		}
		skb->dev = link->outside;
		skb_push(skb, ETH_HLEN);
		dev_queue_xmit( skb );
	}
	else if ( skb->dev == link->outside ) {
		skb->dev = link->inside;
		skb_push(skb, ETH_HLEN);
		dev_queue_xmit( skb );
	}
	else {
		kfree_skb( skb );
	}

	return RX_HANDLER_CONSUMED;
}

static ssize_t link_port_add_store(
		struct link *obj, const char *iface, const char *dir )
{
	struct net_device *dev;
	bool inside = false;

	if ( strcmp( dir, "inside" ) == 0 )
		inside = true;
	else if ( strcmp( dir, "outside" ) == 0 )
		inside = false;
	else
		return -EINVAL;

	dev = dev_get_by_name( &init_net, iface );
	if ( !dev )
		return -EINVAL;
	
	printk( "found iface %s for %s\n", iface, dir );

	/* FIXME: fail if not down. */

	if ( inside )
		obj->inside = dev;
	else
		obj->outside = dev;

	rtnl_lock();
	dev_set_promiscuity( dev, 1 );
	netdev_rx_handler_register( dev, filter_handle_frame, obj );
	rtnl_unlock();

	return 0;
}

static ssize_t link_port_del_store(
		struct link *obj, const char *iface )
{
	struct net_device *dev;

	dev = dev_get_by_name( &init_net, iface );
	if ( !dev )
		return -EINVAL;

	printk( "found iface %s\n", iface );

	/* FIXME: fail if not down. */

	if ( obj->inside == dev )
		obj->inside = 0;
	else if ( obj->outside == dev )
		obj->outside = 0;

	rtnl_lock();
	netdev_rx_handler_unregister( dev );
	dev_set_promiscuity( dev, -1 );
	rtnl_unlock();

	/* Once for the dev get by name above, and again for the release. */
	dev_put( dev );
	dev_put( dev );

	return 0;
}

static ssize_t filter_add_store( struct filter *obj, const char *name )
{
	struct link *link = 0;
	create_link( &link, name, &root_obj->kobj );
	list_add_tail( &link->link_list, &link_list );
	strcpy( link->name, name );
	create_netdev( link, name );
	return 0;
}

static ssize_t filter_del_store( struct filter *obj, const char *name )
{
	/* Find the link by name. */
	struct link *link = 0;
	struct list_head *h;
	list_for_each( h, &link_list ) {
		struct link *hl = container_of( h, struct link, link_list );
		if ( strcmp( hl->name, name ) == 0 ) {
			link = hl;
			break;
		}
	}

	if ( link ) {
		printk( "found link, removing\n" );

		// dev_set_promiscuity( link->inside, -1 );
		// dev_set_promiscuity( link->outside, -1 );
		// dev_put( link->inside );
		// dev_put( link->outside );

		unregister_netdevice_queue( link->dev, NULL );
		list_del( &link->link_list );
		kobject_put( &link->kobj );
	}

	return 0;
}

static int filter_device_event( struct notifier_block *unused,
		unsigned long event, void *ptr )
{
	printk( "filter_device_event\n" );
	return 0;
}

static int filter_dev_open( struct net_device *dev )
{
	printk( "filter_dev_open\n" );
	netdev_update_features( dev );
	netif_start_queue( dev );
	return 0;
}

static int filter_dev_stop( struct net_device *dev )
{
	printk( "filter_dev_stop\n" );
	netif_stop_queue( dev );
	return 0;
}

static int filter_dev_init(struct net_device *dev)
{
	printk( "filter_dev_init\n" );
	return 0;
}

static void filter_dev_uninit(struct net_device *dev)
{
	printk( "filter_dev_uninit\n" );
}

/* Transmit always called with BH disabled. */
netdev_tx_t filter_dev_xmit( struct sk_buff *skb, struct net_device *dev )
{
	struct inline_dev_priv *priv = netdev_priv( dev );

	printk( "filter_dev_xmit, skb: %p, inside: %p\n", skb, priv->link->inside );

	/* Probably need to find the right mac address now. */
	skb->dev = priv->link->inside;
	dev_queue_xmit( skb );

	return NETDEV_TX_OK;
}

int filter_dev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	printk( "filter_dev_ioctl\n" );
	return -EOPNOTSUPP;
}

static struct rtnl_link_stats64 *filter_get_stats64( struct net_device *dev,
		struct rtnl_link_stats64 *stats )
{
	printk("filter_get_stats64\n");
	stats->tx_bytes   = 0;
	stats->tx_packets = 0;
	stats->rx_bytes   = 0;
	stats->rx_packets = 0;
	return stats;
}

static void filter_dev_set_multicast_list( struct net_device *dev )
{
	printk("filter_dev_set_multicast_list\n");
}

static int filter_change_mtu( struct net_device *dev, int new_mtu )
{
	printk("filter_change_mtu\n");
	return -EOPNOTSUPP;
}

static int filter_change_carrier(struct net_device *dev, bool new_carrier)
{
	printk("filter_change_carrier\n");
	if ( new_carrier )
		netif_carrier_on( dev );
	else
		netif_carrier_off( dev );
	return 0;
}

static struct notifier_block filter_device_notifier = {
	.notifier_call = filter_device_event
};

static const struct net_device_ops inline_netdev_ops = {
	.ndo_init              = filter_dev_init,
	.ndo_uninit            = filter_dev_uninit,
	.ndo_start_xmit        = filter_dev_xmit,
	.ndo_validate_addr     = eth_validate_addr,
	.ndo_set_rx_mode       = filter_dev_set_multicast_list,
	.ndo_set_mac_address   = eth_mac_addr,
	.ndo_get_stats64       = filter_get_stats64,
	.ndo_change_carrier    = filter_change_carrier,

	.ndo_open              = filter_dev_open,
	.ndo_stop              = filter_dev_stop,
	.ndo_change_mtu        = filter_change_mtu,
	.ndo_do_ioctl          = filter_dev_ioctl,
};

static void inline_dev_free(struct net_device *dev)
{
	free_netdev(dev);
}

static const struct ethtool_ops inline_ethtool_ops = {
	.get_drvinfo = 0, /* br_getinfo, */
	.get_link   = 0, /* ethtool_op_get_link, */
};

static struct device_type inline_type = {
	.name   = "inline",
};

#define COMMON_FEATURES ( NETIF_F_SG | NETIF_F_FRAGLIST | NETIF_F_HIGHDMA | NETIF_F_GSO_MASK | NETIF_F_HW_CSUM )

void inline_dev_setup(struct net_device *dev)
{
	eth_hw_addr_random(dev);
	ether_setup(dev);

	dev->netdev_ops = &inline_netdev_ops;
	dev->destructor = inline_dev_free;
	dev->ethtool_ops = &inline_ethtool_ops;
	SET_NETDEV_DEVTYPE( dev, &inline_type );
	dev->tx_queue_len = 0;
	
	/* dev->priv_flags = IFF_?; */

	dev->features = COMMON_FEATURES | NETIF_F_LLTX | NETIF_F_NETNS_LOCAL | NETIF_F_HW_VLAN_CTAG_TX | NETIF_F_HW_VLAN_STAG_TX;
	dev->hw_features = COMMON_FEATURES | NETIF_F_HW_VLAN_CTAG_TX | NETIF_F_HW_VLAN_STAG_TX;
	dev->vlan_features = COMMON_FEATURES;
}

struct rtnl_link_ops inline_link_ops __read_mostly = {
	.kind       = "bridge",
	.priv_size  = sizeof(struct inline_dev_priv),
	.setup      = 0,
	.validate   = 0,
	.newlink    = 0,
	.dellink    = 0,
};

static int create_netdev( struct link *link, const char *name )
{
	int res;
	struct net_device *dev;
	struct inline_dev_priv *priv;

	dev = alloc_netdev( sizeof(struct inline_dev_priv), name, inline_dev_setup );

	if (!dev)
		return -ENOMEM;

	dev_net_set( dev, &init_net );
	dev->rtnl_link_ops = &inline_link_ops;
	
	priv = netdev_priv( dev );
	priv->link = link;

	res = register_netdev( dev );
	if ( res ) {
		free_netdev(dev);
	}
	else {
		link->dev = dev;
	}
	return res;
}

static int filter_init(void)
{
	int retval = register_netdevice_notifier( &filter_device_notifier );
	if ( retval )
		return retval;

	INIT_LIST_HEAD( &link_list );

	return 0;
}

static void filter_exit(void)
{
	unregister_netdevice_notifier( &filter_device_notifier );
}
