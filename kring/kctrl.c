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


int kctrl_kavail( struct kring_kern *kring )
{
	return kctrl_avail_impl( kctrl_control(kring->user.control) );
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

void kctrl_knext_plain( struct kring_kern *kring, struct kctrl_plain *plain )
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

EXPORT_SYMBOL_GPL(kctrl_kavail);
EXPORT_SYMBOL_GPL(kctrl_knext_plain);

