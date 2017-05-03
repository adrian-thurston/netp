#include "krkern.h"

struct kring
{
	struct kobject kobj;
};


int kctrl_init(void);
int kdata_init(void);
void kdata_exit(void);
void kctrl_exit(void);

static struct kdata_ringset *head_data = 0;
static struct kctrl_ringset *head_cmd = 0;

struct kctrl_ringset *kctrl_find_ring( const char *name )
{
	struct kctrl_ringset *r = head_cmd;
	while ( r != 0 ) {
		if ( strcmp( r->name, name ) == 0 )
			return r;

		r = r->next;
	}

	return 0;
}

struct kdata_ringset *kdata_find_ring( const char *name )
{
	struct kdata_ringset *r = head_data;
	while ( r != 0 ) {
		if ( strcmp( r->name, name ) == 0 )
			return r;

		r = r->next;
	}

	return 0;
}


static void kring_ringset_free( struct kdata_ringset *r )
{
	int i;
	for ( i = 0; i < r->nrings; i++ )
		kring_ring_free( &r->ring[i] );
	kfree( r->ring );
}

static void kring_free_ringsets( struct kdata_ringset *head )
{
	struct kdata_ringset *r = head;
	while ( r != 0 ) {
		kring_ringset_free( r );
		r = r->next;
	}
}

static void kctrl_ringset_free( struct kctrl_ringset *r )
{
	int i;
	for ( i = 0; i < r->nrings; i++ )
		kctrl_ring_free( &r->ring[i] );
	kfree( r->ring );
}

void kctrl_free_ringsets( struct kctrl_ringset *head )
{
	struct kctrl_ringset *r = head;
	while ( r != 0 ) {
		kctrl_ringset_free( r );
		r = r->next;
	}
}

static void kring_add_ringset( struct kdata_ringset **phead, struct kdata_ringset *set )
{
	if ( *phead == 0 )
		*phead = set;
	else {
		struct kdata_ringset *tail = *phead;
		while ( tail->next != 0 )
			tail = tail->next;
		tail->next = set;
	}
	set->next = 0;
}

ssize_t kring_add_data_store( struct kring *obj, const char *name, long rings_per_set )
{
	struct kdata_ringset *r;
	if ( rings_per_set < 1 || rings_per_set > KDATA_MAX_RINGS_PER_SET )
		return -EINVAL;

	r = kmalloc( sizeof(struct kdata_ringset), GFP_KERNEL );
	kring_ringset_alloc( r, name, rings_per_set );

	kring_add_ringset( &head_data, r );

	return 0;
}

ssize_t kctrl_add_cmd_store( const char *name );

ssize_t kring_add_cmd_store( struct kring *obj, const char *name )
{
	return kctrl_add_cmd_store( name );
}

ssize_t kring_del_store( struct kring *obj, const char *name  )
{
	return 0;
}


ssize_t kctrl_add_data_store( const char *name, long rings_per_set )
{
	struct kctrl_ringset *r;
	if ( rings_per_set < 1 || rings_per_set > KCTRL_MAX_RINGS_PER_SET )
		return -EINVAL;

	r = kmalloc( sizeof(struct kctrl_ringset), GFP_KERNEL );
	kctrl_ringset_alloc( r, name, rings_per_set );

	kctrl_add_ringset( &head_cmd, r );

	return 0;
}

ssize_t kctrl_add_cmd_store( const char *name )
{
	struct kctrl_ringset *r;

	r = kmalloc( sizeof(struct kctrl_ringset), GFP_KERNEL );
	kctrl_ringset_alloc( r, name, 1 );

	kctrl_add_ringset( &head_cmd, r );

	return 0;
}

ssize_t kctrl_del_store( const char *name  )
{
	return 0;
}

int kring_init(void)
{
	kctrl_init();
	kdata_init();
	return 0;
}

void kring_exit(void)
{
	kctrl_exit();
	kdata_exit();

	kring_free_ringsets( head_data );
	kctrl_free_ringsets( head_cmd );
}

