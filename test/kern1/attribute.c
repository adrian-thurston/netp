#include "attribute.h"

struct ring
{
	struct kobject kobj;
};

static int n;

static ssize_t ring_foo_show( struct ring *obj, char *buf )
{
	return sprintf( buf, "%d\n", n );
}

static ssize_t ring_foo_store( struct ring *obj,
		const char *buf, size_t count )
{
	sscanf( buf, "%du", &n );
	return count;
}

static ssize_t ring_bar_show( struct ring *obj, char *buf )
{
	return sprintf( buf, "%d\n", n );
}

static ssize_t ring_bar_store( struct ring *obj,
		const char *buf, size_t count )
{
	sscanf( buf, "%du", &n );
	return count;
}

static int ring_init(void)
{
	return 0;
}

static void ring_exit(void)
{
}
