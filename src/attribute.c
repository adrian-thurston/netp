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


static void kring_ringset_free( struct kring_ringset *r )
{
	int i;
	for ( i = 0; i < r->nrings; i++ )
		kring_ring_free( &r->ring[i] );
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

static void kctrl_ringset_free( struct kring_ringset *r )
{
	int i;
	for ( i = 0; i < r->nrings; i++ )
		kctrl_ring_free( &r->ring[i] );
	kfree( r->ring );
}

void kctrl_free_ringsets( struct kring_ringset *head )
{
	struct kring_ringset *r = head;
	while ( r != 0 ) {
		kctrl_ringset_free( r );
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
	struct kring_ringset *r;
	if ( rings_per_set < 1 || rings_per_set > KCTRL_MAX_RINGS_PER_SET )
		return -EINVAL;

	r = kmalloc( sizeof(struct kring_ringset), GFP_KERNEL );
	kctrl_ringset_alloc( r, name, rings_per_set );

	kctrl_add_ringset( &head_cmd, r );

	return 0;
}

ssize_t kctrl_add_cmd_store( const char *name )
{
	struct kring_ringset *r;

	r = kmalloc( sizeof(struct kring_ringset), GFP_KERNEL );
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

