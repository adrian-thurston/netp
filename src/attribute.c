#include <linux/netdevice.h>
#include "attribute.h"

struct filter
{
	struct kobject kobj;
};

struct link
{
	struct kobject kobj;
};

struct link *lo = 0;

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
		dev_put( inside_dev );
	}

	outside_dev = dev_get_by_name( &init_net, outside );
	if ( outside_dev ) {
		printk( "found outside\n" );
		dev_put( outside_dev );
	}

	return count;
}

static ssize_t filter_del_store( struct filter *obj,
		const char *buf, size_t count )
{
	char link[32];
	sscanf( buf, "%s", link );

	if ( lo ) 
		kobject_put( &lo->kobj );
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
