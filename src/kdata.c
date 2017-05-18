#define KERN
#include "attribute.h"
#include <linux/kobject.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/cacheflush.h>

#include "krkern.h"

static void kdata_init_control( struct kring_ring *ring );
static int  kdata_wait( struct kring_sock *krs );
static void kdata_notify( struct kring_sock *krs );
static void kdata_destruct( struct kring_sock *krs );

struct kring_params kdata_params =
{
	KDATA_NPAGES,

	KDATA_CTRL_SZ,

	KDATA_CTRL_OFF_HEAD,
	KDATA_CTRL_OFF_WRITER,
	KDATA_CTRL_OFF_READER,
	KDATA_CTRL_OFF_DESC,

	KDATA_DATA_SZ,

	KDATA_READERS,
	KDATA_WRITERS,

	&kdata_init_control,
	&kdata_wait,
	&kdata_notify,
	&kdata_destruct,
};

static int kdata_kern_avail( struct kring_ringset *r, struct kring_sock *krs )
{
	if ( krs->ring_id != KDATA_RING_ID_ALL )
		return kdata_avail_impl( KDATA_CONTROL(r->ring[krs->ring_id]), krs->reader_id );
	else {
		int ring;
		for ( ring = 0; ring < r->nrings; ring++ ) {
			if ( kdata_avail_impl( KDATA_CONTROL(r->ring[ring]), krs->reader_id ) )
				return 1;
		}
		return 0;
	}
}

static int kdata_wait( struct kring_sock *krs )
{
	struct kring_ringset *r = krs->ringset;
	wait_queue_head_t *wq;

	// wq = krs->ring_id == KRING_RING_ID_ALL ? &r->reader_waitqueue 
	// : &r->ring[krs->ring_id].reader_waitqueue;

	wq = &r->waitqueue;

	return wait_event_interruptible( *wq, kdata_kern_avail( r, krs ) );
}

static void kdata_notify( struct kring_sock *krs )
{
	struct kring_ringset *r = krs->ringset;
	wake_up_interruptible_all( &r->waitqueue );
}

static void kdata_destruct( struct kring_sock *krs )
{
	int i;

	switch ( krs->mode ) {
		case KRING_WRITE: {
			krs->ringset->ring[krs->ring_id].num_writers -= 1;
			break;
		}
		case KRING_READ: {

			/* One ring or all? */
			if ( krs->ring_id != KDATA_RING_ID_ALL ) {
				struct kdata_control *control = KDATA_CONTROL(krs->ringset->ring[krs->ring_id]);

				krs->ringset->ring[krs->ring_id].reader[krs->reader_id].allocated = false;

				if ( control->reader[krs->reader_id].entered ) {
					kdata_off_t prev = control->reader[krs->reader_id].rhead;
					kdata_reader_release( krs->reader_id, &control[0], prev );
				}
			}
			else {
				for ( i = 0; i < krs->ringset->nrings; i++ ) {
					struct kdata_control *control = KDATA_CONTROL(krs->ringset->ring[i]);

					krs->ringset->ring[i].reader[krs->reader_id].allocated = false;

					if ( control->reader[krs->reader_id].entered ) {
						kdata_off_t prev = control->reader[krs->reader_id].rhead;
						kdata_reader_release( krs->reader_id, &control[0], prev );
					}
				}
			}
		}
	}

}

int kring_kopen( struct kring_kern *kring, enum KRING_TYPE type, const char *rsname, int ring_id, enum KRING_MODE mode )
{
	int reader_id = -1, writer_id = -1;
	struct kring_ringset *ringset;

	if ( type == KRING_CTRL && ring_id != 0 )
		return -1;

	ringset = kring_find_ring( rsname );
	if ( ringset == 0 )
		return -1;
		
	if ( ring_id < 0 || ring_id >= ringset->nrings )
		return -1;

	kring_copy_name( kring->name, rsname );
	kring->ringset = ringset;
	kring->ring_id = ring_id;

	if ( type == KRING_DATA ) {

		if ( mode == KRING_WRITE ) {
			/* Find a writer ID. */
			writer_id = kring_allocate_writer_on_ring( ringset, &ringset->ring[ring_id] );
			if ( writer_id < 0 )
				return writer_id;
		}
		else if ( mode == KRING_READ ) {
			/* Find a reader ID. */
			if ( ring_id != KDATA_RING_ID_ALL ) {
				/* Reader ID for ring specified. */
				reader_id = kring_allocate_reader_on_ring( ringset, &ringset->ring[ring_id] );
			}
			else {
				/* Find a reader ID that works for all rings. This can fail. */
				reader_id = kring_allocate_reader_all_rings( ringset );
			}

			if ( reader_id < 0 )
				return reader_id;

			/*res = */kdata_prep_enter( KDATA_CONTROL(ringset->ring[ring_id]), 0 );
			//if ( res < 0 ) {
			//	kdata_func_error( KRING_ERR_ENTER, 0 );
			//	return -1;
			//}
		}

	}
	else if ( type == KRING_CTRL ) {

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
	}

	/* Set up the user read/write struct for unified read/write operations between kernel and user space. */
	kring->user.socket = -1;
	kring->user.control = &ringset->ring[ring_id]._control_;
	kring->user.data = 0;
	kring->user.pd = ringset->ring[ring_id].pd;
	kring->user.mode = mode;
	kring->user.ring_id = ring_id;
	kring->user.reader_id = reader_id;
	kring->user.writer_id = writer_id;
	kring->user.nrings = ringset->nrings;

	return 0;
}

int kring_kclose( struct kring_kern *kring )
{
	kring->ringset->ring[kring->ring_id].num_writers -= 1;
	return 0;
}

static void kdata_write_single( struct kring_kern *kring, int dir,
		const struct sk_buff *skb, int offset, int write, int len )
{
	struct kdata_packet_header *h;
	void *pdata;

	/* Which ringset? */
	struct kring_ringset *r = kring->ringset;

	h = kdata_write_FIRST( &kring->user );

	h->len = len;
	h->dir = (char) dir;

	pdata = (char*)(h + 1);
	skb_copy_bits( skb, offset, pdata, write );

	kdata_write_SECOND( &kring->user );

	/* track the number of packets produced. Note we don't account for overflow. */
	__sync_add_and_fetch( &KDATA_CONTROL( r->ring[kring->ring_id] )->head->produced, 1 );

	#if 0
	for ( id = 0; id < KRING_READERS; id++ ) {
		if ( r->ring[kring->ring_id].reader[id].allocated ) {
			unsigned long long diff =
					r->ring[kring->ring_id].control.writer->produced -
					r->ring[kring->ring_id].control.reader[id].units;
			// printk( "diff: %llu\n", diff );
			if ( diff > ( KDATA_NPAGES / 2 ) ) {
				printk( "half full: ring_id: %d reader_id: %d\n", kring->ring_id, id );
			}
		}
	}
	#endif

	wake_up_interruptible_all( &r->waitqueue );
}

void kdata_kwrite( struct kring_kern *kring, int dir, const struct sk_buff *skb )
{
	int offset = 0, write, len;

	while ( true ) {
		/* Number of bytes left in the packet (maybe overflows) */
		len = skb->len - offset;
		if ( len <= 0 )
			break;

		/* What can we write this time? */
		write = len;
		if ( write > kdata_packet_max_data() )
			write = kdata_packet_max_data();

		kdata_write_single( kring, dir, skb, offset, write, len );

		offset += write;
	}
}

static void kdata_init_control( struct kring_ring *r )
{
	KDATA_CONTROL(*r)->head->whead = KDATA_CONTROL(*r)->head->wresv = kdata_prev( 0 );
}

EXPORT_SYMBOL_GPL(kring_kopen);
EXPORT_SYMBOL_GPL(kring_kclose);
EXPORT_SYMBOL_GPL(kdata_kwrite);
