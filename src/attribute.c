#include "attribute.h"

#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/list.h>

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

	struct list_head link_list;
};

struct list_head link_list;

static inline struct link *get_link( const struct net_device *dev )
{
	return rcu_dereference( dev->rx_handler_data );
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
	if ( dev )
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

	return 0;
}

static ssize_t filter_add_store( struct filter *obj,
		const char *name )
{
	struct link *link = 0;
	create_link( &link, name, &root_obj->kobj );
	list_add_tail( &link->link_list, &link_list );
	strcpy( link->name, name );
	return 0;
}

static ssize_t filter_del_store( struct filter *obj,
		const char *name )
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
		dev_set_promiscuity( link->inside, -1 );
		dev_set_promiscuity( link->outside, -1 );
		dev_put( link->inside );
		dev_put( link->outside );
		list_del( &link->link_list );
		kobject_put( &link->kobj );
	}
	return 0;
}

static int filter_device_event( struct notifier_block *unused,
		unsigned long event, void *ptr )
{
	return 0;
}

static struct notifier_block filter_device_notifier = {
	.notifier_call = filter_device_event
};

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
