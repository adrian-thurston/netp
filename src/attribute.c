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

static ssize_t filter_add_store( struct filter *obj,
		const char *buf, size_t count )
{
	char link[32], inside[32], outside[32];
	struct net_device *inside_dev;
	struct net_device *outside_dev;

	sscanf( buf, "%s %s %s", link, inside, outside );


	create_link( &lo, &root_obj->kobj );

	inside_dev = dev_get_by_name( &init_net, inside );
	if ( inside_dev ) {
		printk( "found inside\n" );
	}

	outside_dev = dev_get_by_name( &init_net, outside );
	if ( outside_dev ) {
		printk( "found outside\n" );
	}

	lo->inside = inside_dev;
	lo->outside = outside_dev;

	rtnl_lock();

	netdev_rx_handler_register( inside_dev, filter_handle_frame, 0 );
	netdev_rx_handler_register( outside_dev, filter_handle_frame, 0 );

	rtnl_unlock();

	return count;
}

static ssize_t filter_del_store( struct filter *obj,
		const char *buf, size_t count )
{
	char link[32];
	sscanf( buf, "%s", link );

	if ( lo ) {
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
