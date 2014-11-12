#include "attribute.h"

struct filter
{
	struct kobject kobj;
};

struct link
{
	struct kobject kobj;
};

struct link *lo;

static ssize_t filter_add_store( struct filter *obj,
		const char *buf, size_t count )
{
	char link[32], inside[32], outside[32];
	sscanf( buf, "%s %s %s", link, inside, outside );

	create_link( &lo, &root_obj->kobj );
	return count;
}

static ssize_t filter_del_store( struct filter *obj,
		const char *buf, size_t count )
{
//	sscanf( buf, "%du", &n );
	char link[32];
	sscanf( buf, "%s", link );


	kobject_put( &lo->kobj );
	return count;
}
