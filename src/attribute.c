#include "attribute.h"

#include <linux/netdevice.h>
#include <linux/rtnetlink.h>

struct filter
{
	struct kobject kobj;
};

struct link
{
	struct kobject kobj;
	struct net_device *inside, *outside;
};

struct link *lo = 0;

rx_handler_result_t filter_handle_frame( struct sk_buff **pskb )
{
	struct sk_buff *skb = *pskb;

	if ( lo->inside == 0 || lo->outside == 0 ) {
		kfree_skb( skb );
		return RX_HANDLER_CONSUMED;
	}

	if ( skb->dev == lo->inside ) {
		skb->dev = lo->outside;
		skb_push(skb, ETH_HLEN);
		dev_queue_xmit( skb );
	}
	else if ( skb->dev == lo->outside ) {
		skb->dev = lo->inside;
		skb_push(skb, ETH_HLEN);
		dev_queue_xmit( skb );
	}
	else {
		kfree_skb( skb );
	}

	return RX_HANDLER_CONSUMED;
}

static ssize_t link_port_add_store(
		struct link *obj, const char *buf, size_t count )
{
	char iface[32], dir[32];
	struct net_device *dev;
	bool inside = false;

	sscanf( buf, "%s %s", iface, dir );

	if ( strcmp( dir, "inside" ) == 0 )
		inside = true;
	else if ( strcmp( dir, "outside" ) == 0 )
		inside = false;
	else
		return -EINVAL;

	dev = dev_get_by_name( &init_net, iface );
	if ( dev )
		printk( "found iface %s for %s\n", iface, dir );
	else
		return -EINVAL;

	if ( inside )
		obj->inside = dev;
	else
		obj->outside = dev;

	rtnl_lock();
	dev_set_promiscuity( dev, 1 );
	netdev_rx_handler_register( dev, filter_handle_frame, 0 );
	rtnl_unlock();

	return count;
}

static ssize_t link_port_del_store(
		struct link *obj, const char *buf, size_t count )
{
	char iface[32];
	struct net_device *dev;

	sscanf( buf, "%s", iface );

	dev = dev_get_by_name( &init_net, iface );
	if ( dev )
		printk( "found iface %s\n", iface );
	else
		return -EINVAL;

	rtnl_lock();
	netdev_rx_handler_unregister( dev );
	dev_set_promiscuity( dev, -1 );
	rtnl_unlock();

	return count;
}

static ssize_t filter_add_store( struct filter *obj,
		const char *buf, size_t count )
{
	char linkName[32];

	sscanf( buf, "%s", linkName );
	create_link( &lo, linkName, &root_obj->kobj );

	return count;
}

static ssize_t filter_del_store( struct filter *obj,
		const char *buf, size_t count )
{
	char link[32];
	sscanf( buf, "%s", link );

	if ( lo ) {
		dev_set_promiscuity( lo->inside, -1 );
		dev_set_promiscuity( lo->outside, -1 );
		dev_put( lo->inside );
		dev_put( lo->outside );
		kobject_put( &lo->kobj );
	}
	return count;
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

	return 0;
}

static void filter_exit(void)
{
	unregister_netdevice_notifier( &filter_device_notifier );
}
