#define KERN

#include "attribute.h"
#include "krkern.h"

#include <linux/kobject.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/cacheflush.h>

//static int kctrl_sock_create( struct net *net, struct socket *sock, int protocol, int kern );
int kctrl_bind( struct socket *sock, struct sockaddr *sa, int addr_len);

static void kctrl_init_control( struct kring_ring *ring );
static int  kctrl_wait( struct kring_sock *krs );
static void kctrl_notify( struct kring_sock *krs );
static void kctrl_destruct( struct kring_sock *krs );

struct kring_params kctrl_params =
{
	KCTRL_NPAGES,

	KCTRL_CTRL_SZ,

	KCTRL_CTRL_OFF_HEAD,
	KCTRL_CTRL_OFF_WRITER,
	KCTRL_CTRL_OFF_READER,
	KCTRL_CTRL_OFF_DESC,

	KCTRL_DATA_SZ,

	KCTRL_READERS,
	KCTRL_WRITERS,

	&kctrl_init_control,
	&kctrl_wait,
	&kctrl_notify,
	&kctrl_destruct,
};

static void kctrl_copy_name( char *dest, const char *src )
{
	strncpy( dest, src, KCTRL_NLEN );
	dest[KCTRL_NLEN-1] = 0;
}

int kctrl_kavail( struct kctrl_kern *kring )
{
	return kctrl_avail_impl( kring->user.control );
}

static int kctrl_wait( struct kring_sock *krs )
{
	struct kring_ringset *r = krs->ringset;
	wait_queue_head_t *wq;

	// wq = krs->ring_id == KR_RING_ID_ALL ? &r->reader_waitqueue 
	// : &r->ring[krs->ring_id].reader_waitqueue;

	wq = &r->waitqueue;

	return wait_event_interruptible( *wq, KCTRL_CONTROL(r->ring[krs->ring_id])->head->free != KCTRL_NULL );
}

static void kctrl_notify( struct kring_sock *krs )
{
	struct kring_ringset *r = krs->ringset;
	wake_up_interruptible_all( &r->waitqueue );
}

static void kctrl_destruct( struct kring_sock *krs )
{
	int i;

	switch ( krs->mode ) {
		case KRING_WRITE: {
			krs->ringset->ring[krs->ring_id].writer[krs->writer_id].allocated = false;
			krs->ringset->ring[krs->ring_id].num_writers -= 1;
			break;
		}
		case KRING_READ: {
			krs->ringset->ring[krs->ring_id].num_readers -= 1;

			/* One ring or all? */
			if ( krs->ring_id != KCTRL_RING_ID_ALL ) {
				// struct kctrl_control *control = &krs->ringset->ring[krs->ring_id].control;

				krs->ringset->ring[krs->ring_id].reader[krs->reader_id].allocated = false;

				// if ( control->reader[krs->reader_id].entered ) {
				//	kctrl_off_t prev = control->reader[krs->reader_id].rhead;
				//	kctrl_reader_release( krs->reader_id, &control[0], prev );
				// }
			}
			else {
				for ( i = 0; i < krs->ringset->nrings; i++ ) {
					// struct kctrl_control *control = &krs->ringset->ring[i].control;

					krs->ringset->ring[i].reader[krs->reader_id].allocated = false;

					// if ( control->reader[krs->reader_id].entered ) {
					//	kctrl_off_t prev = control->reader[krs->reader_id].rhead;
					//	kctrl_reader_release( krs->reader_id, &control[0], prev );
					// }
				}
			}
		}
	}

}

int kctrl_kopen( struct kctrl_kern *kring, const char *rsname, enum KRING_MODE mode )
{
	int reader_id = -1, writer_id = -1;
	int ring_id = 0;

	struct kring_ringset *ringset = kring_find_ring( rsname );
	if ( ringset == 0 )
		return -1;
	
	kctrl_copy_name( kring->name, rsname );
	kring->ringset = ringset;
	kring->ring_id = 0;

	if ( mode == KRING_WRITE ) {
		/* Find a writer ID. */
		writer_id = kring_allocate_writer_on_ring( ringset, &ringset->ring[ring_id] );
		if ( writer_id < 0 )
			return writer_id;
	}
	else if ( mode == KRING_READ ) {
		/* Find a reader ID. */
		if ( ring_id != KCTRL_RING_ID_ALL ) {
			/* Reader ID for ring specified. */
			reader_id = kring_allocate_reader_on_ring( ringset, &ringset->ring[ring_id] );
		}
		else {
			/* Find a reader ID that works for all rings. This can fail. */
			reader_id = kring_allocate_reader_all_rings( ringset );
		}

		if ( reader_id < 0 )
			return reader_id;

		/*res = */kctrl_prep_enter( KCTRL_CONTROL(ringset->ring[ring_id]), 0 );
		//if ( res < 0 ) {
		//	kctrl_func_error( KCTRL_ERR_ENTER, 0 );
		//	return -1;
		//}
	}

	/* Set up the user read/write struct for unified read/write operations between kernel and user space. */
	kring->user.socket = -1;
	kring->user.control = KCTRL_CONTROL(ringset->ring[ring_id]);
	kring->user.data = 0;
	kring->user.pd = ringset->ring[ring_id].pd;
	kring->user.mode = mode;
	kring->user.ring_id = ring_id;
	kring->user.reader_id = reader_id;
	kring->user.writer_id = writer_id;
	kring->user.nrings = ringset->nrings;

	return 0;
}

int kctrl_kclose( struct kctrl_kern *kring )
{
	kring->ringset->ring[kring->ring_id].num_writers -= 1;
	return 0;
}

static void kctrl_write_single( struct kctrl_kern *kring, int dir,
		const struct sk_buff *skb, int offset, int write, int len )
{
	struct kctrl_packet_header *h;
	void *pdata;

	/* Which ringset? */
	struct kring_ringset *r = kring->ringset;

	h = kctrl_write_FIRST( &kring->user );

	h->len = len;
	h->dir = (char) dir;

	pdata = (char*)(h + 1);
	skb_copy_bits( skb, offset, pdata, write );

	kctrl_write_SECOND( &kring->user );

	/* track the number of packets produced. Note we don't account for overflow. */
//	__sync_add_and_fetch( &r->ring[kring->ring_id].control.head->produced, 1 );

	#if 0
	for ( id = 0; id < KCTRL_READERS; id++ ) {
		if ( r->ring[kring->ring_id].reader[id].allocated ) {
			unsigned long long diff =
					r->ring[kring->ring_id].control.writer->produced -
					r->ring[kring->ring_id].control.reader[id].units;
			// printk( "diff: %llu\n", diff );
			if ( diff > ( KCTRL_NPAGES / 2 ) ) {
				printk( "half full: ring_id: %d reader_id: %d\n", kring->ring_id, id );
			}
		}
	}
	#endif

	wake_up_interruptible_all( &r->waitqueue );
}

void kctrl_kwrite( struct kctrl_kern *kring, int dir, const struct sk_buff *skb )
{
	int offset = 0, write, len;

	while ( true ) {
		/* Number of bytes left in the packet (maybe overflows) */
		len = skb->len - offset;
		if ( len <= 0 )
			break;

		/* What can we write this time? */
		write = len;
		if ( write > kctrl_packet_max_data() )
			write = kctrl_packet_max_data();

		kctrl_write_single( kring, dir, skb, offset, write, len );

		offset += write;
	}
}

void kctrl_knext_plain( struct kctrl_kern *kring, struct kctrl_plain *plain )
{
	struct kctrl_plain_header *h;

	h = (struct kctrl_plain_header*) kctrl_next_generic( &kring->user );

	wake_up_interruptible_all( &kring->ringset->ring[kring->ring_id].waitqueue );

	plain->len = h->len;
	plain->bytes = (unsigned char*)(h + 1);
}

static void kctrl_init_control( struct kring_ring *ring )
{
	struct kctrl_control *control = KCTRL_CONTROL( *ring );
	int i;

	/* Use the first page as the stack sential. */
	control->head->head = 0;
	control->head->tail = 0;
	control->head->stack = 0;

	control->descriptor[0].next = KCTRL_NULL;

	/* Link item 1 through to the last into the free list. */
	control->head->free = 1;
	for ( i = 1; i < KCTRL_NPAGES - 1; i++ )
		control->descriptor[i].next = i + 1;

	/* Terminate the free list. */
	control->descriptor[KCTRL_NPAGES-1].next = KCTRL_NULL;
}

EXPORT_SYMBOL_GPL(kctrl_kopen);
EXPORT_SYMBOL_GPL(kctrl_kclose);
EXPORT_SYMBOL_GPL(kctrl_kwrite);
EXPORT_SYMBOL_GPL(kctrl_kavail);
EXPORT_SYMBOL_GPL(kctrl_knext_plain);

