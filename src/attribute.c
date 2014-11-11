#include "attribute.h"

struct filter
{
	struct kobject kobj;
};

struct device
{
	struct kobject kobj;
};

static int n;

static ssize_t filter_foo_show( struct filter *obj, char *buf )
{
	return sprintf( buf, "%d\n", n );
}

static ssize_t filter_foo_store( struct filter *obj,
		const char *buf, size_t count )
{
	sscanf( buf, "%du", &n );
	return count;
}

static ssize_t filter_bar_show( struct filter *obj, char *buf )
{
	return sprintf( buf, "%d\n", n );
}

static ssize_t filter_bar_store( struct filter *obj,
		const char *buf, size_t count )
{
	sscanf( buf, "%du", &n );
	return count;
}

static ssize_t device_tracks_show( struct device *obj, char *buf )
{
	return sprintf( buf, "%d\n", n );
}

static ssize_t device_tracks_store( struct device *obj,
		const char *buf, size_t count )
{
	sscanf( buf, "%du", &n );
	return count;
}
