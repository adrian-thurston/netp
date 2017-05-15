#define KERN

#include "krkern.h"

struct kring
{
	struct kobject kobj;
};


int kctrl_init(void);
int kdata_init(void);
void kdata_exit(void);
void kctrl_exit(void);

static struct kring_ringset *head_data = 0;
static struct kring_ringset *head_cmd = 0;

extern struct kring_params kdata_params;
extern struct kring_params kctrl_params;

int kring_ioctl( struct socket *sock, unsigned int cmd, unsigned long arg )
{
	printk( "kring_ioctl\n" );
	return 0;
}

unsigned int kring_poll( struct file *file, struct socket *sock, poll_table *wait )
{
	printk( "kring_poll\n" );
	return 0;
}

int kring_setsockopt( struct socket *sock, int level, int optname, char __user * optval, unsigned int optlen )
{
	printk( "kring_setsockopt\n" );
	return 0;
}

void kring_copy_name( char *dest, const char *src )
{
	strncpy( dest, src, KRING_NLEN );
	dest[KRING_NLEN-1] = 0;
}

struct kring_ringset *kctrl_find_ring( const char *name )
{
	struct kring_ringset *r = head_cmd;
	while ( r != 0 ) {
		if ( strcmp( r->name, name ) == 0 )
			return r;

		r = r->next;
	}

	return 0;
}

struct kring_ringset *kdata_find_ring( const char *name )
{
	struct kring_ringset *r = head_data;
	while ( r != 0 ) {
		if ( strcmp( r->name, name ) == 0 )
			return r;

		r = r->next;
	}

	return 0;
}

static void *kring_alloc_shared_memory( int size )
{
	void *mem;
	size = PAGE_ALIGN( size );
	mem = vmalloc_user( size );
	memset( mem, 0, size );
	return mem;
}

static void kring_ring_alloc( struct kring_params *params, struct kring_ring *r )
{
	int i;

	r->num_writers = 0;
	r->num_readers = 0;

	r->pd = kmalloc( sizeof(struct kring_page_desc) * params->npages, GFP_KERNEL );
	for ( i = 0; i < params->npages; i++ ) {
		r->pd[i].p = alloc_page( GFP_KERNEL | __GFP_ZERO );
		if ( r->pd[i].p ) {
			r->pd[i].m = page_address(r->pd[i].p);
		}
		else {
			printk( "alloc_page for ring allocation failed\n" );
		}
	}

	init_waitqueue_head( &r->waitqueue );

	r->ctrl = kring_alloc_shared_memory( params->ctrl_sz );

	r->_control_.head = r->ctrl + params->ctrl_off_head;
	r->_control_.writer = r->ctrl + params->ctrl_off_writer;
	r->_control_.reader = r->ctrl + params->ctrl_off_reader;
	r->_control_.descriptor = r->ctrl + params->ctrl_off_desc;

	(*params->init_control)( r );
}

void kring_ringset_alloc( struct kring_params *params, struct kring_ringset *r, const char *name, long nrings )
{
	int i;

	printk( "allocating %ld rings\n", nrings );

	strncpy( r->name, name, KRING_NLEN );
	r->name[KRING_NLEN-1] = 0;

	r->nrings = nrings;

	r->ring = kmalloc( sizeof(struct kring_ring) * nrings, GFP_KERNEL );
	memset( r->ring, 0, sizeof(struct kring_ring) * nrings  );

	for ( i = 0; i < nrings; i++ )
		kring_ring_alloc( params, &r->ring[i] );

	init_waitqueue_head( &r->waitqueue );

	r->params = params;
}

static void kring_ring_free( struct kring_params *params, struct kring_ring *r )
{
	int i;
	for ( i = 0; i < params->npages; i++ )
		__free_page( r->pd[i].p );

	vfree( r->ctrl );
	kfree( r->pd );
}

static void kring_ringset_free( struct kring_ringset *r )
{
	int i;
	for ( i = 0; i < r->nrings; i++ )
		kring_ring_free( r->params, &r->ring[i] );
	kfree( r->ring );
}

static void kring_free_ringsets( struct kring_ringset *head )
{
	struct kring_ringset *r = head;
	while ( r != 0 ) {
		kring_ringset_free( r );
		r = r->next;
	}
}

static void kring_add_ringset( struct kring_ringset **phead, struct kring_ringset *set )
{
	if ( *phead == 0 )
		*phead = set;
	else {
		struct kring_ringset *tail = *phead;
		while ( tail->next != 0 )
			tail = tail->next;
		tail->next = set;
	}
	set->next = 0;
}

ssize_t kring_add_data_store( struct kring *obj, const char *name, long rings_per_set )
{
	struct kring_ringset *r;
	if ( rings_per_set < 1 || rings_per_set > KDATA_MAX_RINGS_PER_SET )
		return -EINVAL;

	r = kmalloc( sizeof(struct kring_ringset), GFP_KERNEL );
	kring_ringset_alloc( &kdata_params, r, name, rings_per_set );

	kring_add_ringset( &head_data, r );

	return 0;
}

ssize_t kring_add_cmd_store( struct kring *obj, const char *name )
{
	struct kring_ringset *r;

	r = kmalloc( sizeof(struct kring_ringset), GFP_KERNEL );
	kring_ringset_alloc( &kctrl_params, r, name, 1 );

	kring_add_ringset( &head_cmd, r );

	return 0;
}

ssize_t kring_del_store( struct kring *obj, const char *name  )
{
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
	kring_free_ringsets( head_cmd );
}

