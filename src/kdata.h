#ifndef __KDATA_H
#define __KDATA_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "krdep.h"

#define KDATA 25
#define KDATA_NPAGES 2048

#define KDATA_PGOFF_CTRL 0
#define KDATA_PGOFF_DATA 1

/*
 * Memap identity information. 
 */

/* Ring id (5), region to map (1) */

/* Must match region shift below. */
#define KDATA_MAX_RINGS_PER_SET 32

#define KDATA_PGOFF_ID_SHIFT 0
#define KDATA_PGOFF_ID_MASK  0x1f

#define KDATA_PGOFF_REGION_SHIFT 5
#define KDATA_PGOFF_REGION_MASK  0x20

#define KDATA_RING_ID_ALL -1


#define KDATA_DSC_READER_SHIFT    2
#define KDATA_DSC_WRITER_OWNED    0x01
#define KDATA_DSC_SKIPPED         0x02
#define KDATA_DSC_READER_OWNED    0xfc
#define KDATA_DSC_READER_BIT(id)  ( 0x1 << ( KDATA_DSC_READER_SHIFT + (id) ) )

/* Direction: from client, or from server. */
#define KDATA_DIR_CLIENT 1
#define KDATA_DIR_SERVER 2

#define KDATA_DIR_INSIDE  1
#define KDATA_DIR_OUTSIDE 2

#define KRING_NLEN 32
#define KDATA_READERS 6
#define KDATA_WRITERS 6

/* Configurable at allocation time. This specifies the maximum. */
#define KDATA_MAX_WRITERS_PER_RING 32

#define KR_OPT_WRITER_ID 1
#define KR_OPT_READER_ID 2
#define KR_OPT_RING_N    3

/* Records an error in the user struct. Use before goto to function cleanup. */
#define kdata_func_error( _ke, _ee ) \
	do { u->krerr = _ke; u->_errno = _ee; } while (0)

enum KRING_TYPE
{
	KRING_PACKETS = 1,
	KRING_DECRYPTED,
	KRING_PLAIN
};

enum KRING_MODE
{
	KRING_READ = 1,
	KRING_WRITE
};

typedef unsigned short kdata_desc_t;
typedef unsigned long kdata_off_t;

struct kdata_shared_head
{
	kdata_off_t whead;
	kdata_off_t wresv;
	unsigned long long produced;
};

struct kdata_shared_writer
{
	kdata_off_t whead;
	kdata_off_t wresv;
	kdata_off_t wbar;
};

struct kdata_shared_reader
{
	kdata_off_t rhead;
	unsigned long skips;
	unsigned char entered;
	unsigned long long consumed;
};

struct kdata_shared_desc
{
	kdata_desc_t desc;
};

#define KRING_CTRL_SZ ( \
	sizeof(struct kdata_shared_head) + \
	sizeof(struct kdata_shared_writer) * KDATA_WRITERS + \
	sizeof(struct kdata_shared_reader) * KDATA_READERS + \
	sizeof(struct kdata_shared_desc) * KDATA_NPAGES \
)
	
#define KRING_CTRL_OFF_HEAD   0
#define KRING_CTRL_OFF_WRITER KRING_CTRL_OFF_HEAD + sizeof(struct kdata_shared_head)
#define KRING_CTRL_OFF_READER KRING_CTRL_OFF_WRITER + sizeof(struct kdata_shared_writer) * KDATA_WRITERS
#define KRING_CTRL_OFF_DESC   KRING_CTRL_OFF_READER + sizeof(struct kdata_shared_reader) * KDATA_READERS

#define KRING_DATA_SZ KRING_PAGE_SIZE * KDATA_NPAGES

struct kdata_control
{
	struct kdata_shared_head *head;
	struct kdata_shared_writer *writer;
	struct kdata_shared_reader *reader;
	struct kdata_shared_desc *descriptor;
};


struct kdata_shared
{
	struct kdata_control *control;
	struct kdata_data *data;
};

struct kdata_user
{
	int socket;
	int ring_id;
	int nrings;
	int writer_id;
	int reader_id;
	enum KRING_MODE mode;

	struct kdata_control *control;

	/* When used in user space we use the data pointer, which points to the
	 * mmapped region. In the kernel we use pd, which points to the array of
	 * pages+memory pointers. Must be interpreted according to socket value. */
	struct kring_data *data;
	struct kring_page_desc *pd;

	int krerr;
	int _errno;
	char *errstr;
};

struct kdata_addr
{
	char name[KRING_NLEN];
	int ring_id;
	enum KRING_MODE mode;
};

struct kdata_packet
{
	char dir;
	int len;
	int caplen;
	unsigned char *bytes;
};

struct kdata_decrypted
{
	 long id;
	 unsigned char type;
	 char *host;
	 unsigned char *bytes;
	 int len;
};

struct kdata_plain
{
	 int len;
	 unsigned char *bytes;
};

struct kdata_packet_header
{
	int len;
	char dir;
};

struct kdata_decrypted_header
{
	int len;
	long id;
	char type;
	char host[63];
};

struct kdata_plain_header
{
	int len;
};

int kdata_open( struct kdata_user *u, enum KRING_TYPE type, const char *ringset, int rid, enum KRING_MODE mode );
int kdata_write_decrypted( struct kdata_user *u, long id, int type, const char *remoteHost, char *data, int len );
int kdata_write_plain( struct kdata_user *u, char *data, int len );
int kdata_read_wait( struct kdata_user *u );

static inline int kdata_packet_max_data(void)
{
	return KRING_PAGE_SIZE - sizeof(struct kdata_packet_header);
}

static inline int kdata_decrypted_max_data(void)
{
	return KRING_PAGE_SIZE - sizeof(struct kdata_decrypted_header);
}

static inline int kdata_plain_max_data(void)
{
	return KRING_PAGE_SIZE - sizeof(struct kdata_plain_header);
}

char *kdata_error( struct kdata_user *u, int err );

static inline unsigned long kdata_skips( struct kdata_user *u )
{
	unsigned long skips = 0;
	if ( u->ring_id != KDATA_RING_ID_ALL )
		skips = u->control->reader[u->reader_id].skips;
	else {
		int ring;
		for ( ring = 0; ring < u->nrings; ring++ )
			skips += u->control[ring].reader[u->reader_id].skips;
	}
	return skips;
}

static inline int kdata_avail_impl( struct kdata_control *control, int reader_id )
{
	return ( control->reader[reader_id].rhead != control->head->whead );
}

static inline kdata_desc_t kdata_read_desc( struct kdata_control *control, kdata_off_t off )
{
	return control->descriptor[off].desc;
}

static inline kdata_desc_t kdata_write_back( struct kdata_control *control,
		kdata_off_t off, kdata_desc_t oldval, kdata_desc_t newval )
{
	return __sync_val_compare_and_swap(
			&control->descriptor[off].desc, oldval, newval );
}

static inline kdata_off_t kdata_wresv_write_back( struct kdata_control *control,
		kdata_off_t oldval, kdata_off_t newval )
{
	return __sync_val_compare_and_swap( &control->head->wresv, oldval, newval );
}

static inline kdata_off_t kdata_whead_write_back( struct kdata_control *control,
		kdata_off_t oldval, kdata_off_t newval )
{
	return __sync_val_compare_and_swap( &control->head->whead, oldval, newval );
}

static inline void *kdata_page_data( struct kdata_user *u, int ctrl, kdata_off_t off )
{
	if ( u->socket < 0 )
		return u->pd[off].m;
	else
		return &u->data[ctrl].page[off];
}

static inline int kdata_avail( struct kdata_user *u )
{
	if ( u->ring_id != KDATA_RING_ID_ALL )
		return kdata_avail_impl( u->control, u->reader_id );
	else {
		int ctrl;
		for ( ctrl = 0; ctrl < u->nrings; ctrl++ ) {
			if ( kdata_avail_impl( &u->control[ctrl], u->reader_id ) )
				return 1;
		}
		return 0;
	}
}

static inline kdata_off_t kdata_next( kdata_off_t off )
{
	off += 1;
	if ( off >= KDATA_NPAGES )
		off = 0;
	return off;
}

static inline kdata_off_t kdata_prev( kdata_off_t off )
{
	if ( off == 0 )
		return KDATA_NPAGES - 1;
	return off - 1;
}

static inline kdata_off_t kdata_advance_rhead( struct kdata_control *control, int reader_id, kdata_off_t rhead )
{
	kdata_desc_t desc;
	while ( 1 ) {
		rhead = kdata_next( rhead );

		/* reserve next. */
		desc = kdata_read_desc( control, rhead );
		if ( ! ( desc & KDATA_DSC_WRITER_OWNED ) ) {
			/* Okay we can take it. */
			kdata_desc_t newval = desc | KDATA_DSC_READER_BIT( reader_id );
		
			/* Attemp write back. */
			kdata_desc_t before = kdata_write_back( control, rhead, desc, newval );
			if ( before == desc ) {
				/* Write back okay. We can use. */
				break;
			}
		}

		/* Todo: limit the number of skips. If we get to whead then we skipped
		 * over invalid buffers until we got to the write head. There is
		 * nothing to read. Not a normal situation because whead should not
		 * advance unless a successful write was made. */
	}

	return rhead;
}

/* Unreserve prev. */
static inline void kdata_reader_release( int reader_id, struct kdata_control *control, kdata_off_t prev )
{
	kdata_desc_t before, desc, newval;
again:
	/* Take a copy, modify, then try to write back. */
	desc = kdata_read_desc( control, prev );
	
	newval = desc & ~( KDATA_DSC_READER_BIT( reader_id ) );

	/* Was it skipped? */
	if ( desc & KDATA_DSC_SKIPPED ) {
		/* If we are the last to release it, then reset the skipped bit. */
		if ( ! ( newval & KDATA_DSC_READER_OWNED ) )
			newval &= ~KDATA_DSC_SKIPPED;
	}

	before = kdata_write_back( control, prev, desc, newval );
	if ( before != desc )
		goto again;
	
	__sync_add_and_fetch( &control->reader->consumed, 1 );
}

static inline int kdata_select_ctrl( struct kdata_user *u )
{
	if ( u->ring_id != KDATA_RING_ID_ALL )
		return 0;
	else {
		int ctrl;
		for ( ctrl = 0; ctrl < u->nrings; ctrl++ ) {
			if ( kdata_avail_impl( &u->control[ctrl], u->reader_id ) )
				return ctrl;
		}
		return -1;
	}
}

static inline void *kdata_next_generic( struct kdata_user *u )
{
	int ctrl = kdata_select_ctrl( u );

	kdata_off_t prev = u->control[ctrl].reader[u->reader_id].rhead;
	kdata_off_t rhead = prev;

	rhead = kdata_advance_rhead( &u->control[ctrl], u->reader_id, rhead );

	/* Set the rhead. */
	u->control[ctrl].reader[u->reader_id].rhead = rhead;

	/* Release the previous only if we have entered with a successful read. */
	if ( u->control[ctrl].reader[u->reader_id].entered )
		kdata_reader_release( u->reader_id, &u->control[ctrl], prev );

	/* Indicate we have entered. */
	u->control[ctrl].reader[u->reader_id].entered = 1;

	return kdata_page_data( u, ctrl, rhead );
}


static inline void kdata_next_packet( struct kdata_user *u, struct kdata_packet *packet )
{
	struct kdata_packet_header *h;

	h = (struct kdata_packet_header*) kdata_next_generic( u );

	packet->len = h->len;
	packet->caplen = 
			( h->len <= kdata_packet_max_data() ) ?
			h->len :
			kdata_packet_max_data();
	packet->dir = h->dir;
	packet->bytes = (unsigned char*)( h + 1 );
}

static inline void kdata_next_decrypted( struct kdata_user *u, struct kdata_decrypted *decrypted )
{
	struct kdata_decrypted_header *h;

	h = (struct kdata_decrypted_header*) kdata_next_generic( u );

	decrypted->len = h->len;
	decrypted->id = h->id;
	decrypted->type = h->type;
	decrypted->host = h->host;
	decrypted->bytes = (unsigned char*)( h + 1 );
}

static inline void kdata_next_plain( struct kdata_user *u, struct kdata_plain *plain )
{
	struct kdata_plain_header *h;

	h = (struct kdata_plain_header*) kdata_next_generic( u );

	plain->len = h->len;
	plain->bytes = (unsigned char*)( h + 1 );
}

static inline unsigned long kdata_find_write_loc( struct kdata_control *control )
{
	int id;
	kdata_desc_t desc = 0;
	kdata_off_t whead = control->head->whead;
	while ( 1 ) {
		/* Move to the next slot. */
		whead = kdata_next( whead );

retry:
		/* Read the descriptor. */
		desc = kdata_read_desc( control, whead );

		/* Check, if not okay, go on to next. */
		if ( desc & KDATA_DSC_READER_OWNED || desc & KDATA_DSC_SKIPPED ) {
			kdata_desc_t before;

			/* register skips. */
			for ( id = 0; id < KDATA_READERS; id++ ) {
				if ( desc & KDATA_DSC_READER_BIT( id ) ) {
					/* reader id present. */
					control->reader[id].skips += 1;
				}
			}

			/* Mark as skipped. If a reader got in before us, retry. */
			before = kdata_write_back( control, whead, desc, desc | KDATA_DSC_SKIPPED );
			if ( before != desc )
				goto retry;

			/* After registering the skip, go on to look for another block. */
		}
		else if ( desc & KDATA_DSC_WRITER_OWNED ) {
			/* A different writer has the block. Go forward to find another
			 * block. */
		}
		else {
			/* Available. */
			kdata_desc_t newval = desc | KDATA_DSC_WRITER_OWNED;

			/* Okay. Attempt to claim with an atomic write back. */
			kdata_desc_t before = kdata_write_back( control, whead, desc, newval );
			if ( before != desc )
				goto retry;

			/* Write back okay. No reader claimed. We can use. */
			return whead;
		}

		/* FIXME: if we get back to where we started then bail */
	}
}

static inline void *kdata_write_FIRST( struct kdata_user *u )
{
	kdata_off_t whead;

	/* Find the place to write to, skipping ahead as necessary. */
	whead = kdata_find_write_loc( u->control );

	/* Reserve the space. */
	u->control->head->wresv = whead;

	return kdata_page_data( u, 0, whead );
}

static inline int kdata_writer_release( struct kdata_control *control, kdata_off_t whead )
{
	/* orig value. */
	kdata_desc_t desc = kdata_read_desc( control, whead );

	/* Unrelease writer. */
	kdata_desc_t newval = desc & ~KDATA_DSC_WRITER_OWNED;

	/* Write back with check. No other reader or writer should have altered the
	 * descriptor. */
	kdata_desc_t before = kdata_write_back( control, whead, desc, newval );
	if ( before != desc )
		return -1;

	return 0;
}

static inline void kdata_write_SECOND( struct kdata_user *u )
{
	/* Clear the writer owned bit from the buffer. */
	kdata_writer_release( u->control, u->control->head->wresv );

	/* Write back the write head, thereby releasing the buffer to writer. */
	u->control->head->whead = u->control->head->wresv;
}

static inline void kdata_update_wresv( struct kdata_user *u )
{
	int w;
	kdata_off_t wresv, before, highest;

again:
	wresv = u->control->head->wresv;

	/* we are setting wresv to the highest amongst the writers. If a writer is
	 * inactive then it's resv will be left behind and not affect this
	 * compuation. */
	highest = 0;
	for ( w = 0; w < KDATA_WRITERS; w++ ) {
		if ( u->control->writer[w].wresv > highest )
			highest = u->control->writer[w].wresv;
	}

	before = kdata_wresv_write_back( u->control, wresv, highest );
	if ( before != wresv )
		goto again;
}

static inline void kdata_update_whead( struct kdata_user *u )
{
	int w;
	kdata_off_t orig, before, whead = 0, wbar = ~((kdata_off_t)0);

retry:
	orig = u->control->head->whead;

	/* Situations a writer can be in: 
	 *
	 * wbar = 0 (not trying to write)
	 *   can release to local head, if > shared head.
	 * wbar != 0 trying to write, canot release past this value
	 */
	
	/* Order of these two matters. Cannot look for the limit first. */

	/* First pass, find the highest write head. */
	for ( w = 0; w < KDATA_WRITERS; w++ ) {
		if ( u->control->writer[w].whead > whead )
			whead = u->control->writer[w].whead;
	}

	/* Second pass. Find the lowest barrier. */
	for ( w = 0; w < KDATA_WRITERS; w++ ) {
		if ( u->control->writer[w].wbar != 0 ) {
			if ( u->control->writer[w].wbar < wbar )
				wbar = u->control->writer[w].wbar;
		}
	}

	if ( wbar < whead )
		whead = wbar;
	
	/* Write back. */

	before = kdata_whead_write_back( u->control, orig, whead );
	if ( before != orig )
		goto retry;
}

static inline void *kdata_write_FIRST_2( struct kdata_user *u )
{
	/* Start at wresv. There is nothing free before this value. We cannot start
	 * at whead because other writers may have written and release (not showing
	 * writer owned bits, but we cannot take.*/
	kdata_off_t whead = u->control->head->wresv;

	/* Set the release barrier to the place where we start looking. We cannot
	 * release past this point. */
	u->control->writer[u->writer_id].wbar = whead;

	/* Find the place to write to, skipping ahead as necessary. */
	whead = kdata_find_write_loc( u->control );

	/* Private reserve. */
	u->control->writer[u->writer_id].wresv = whead;

	/* Update the common wreserve. */
	kdata_update_wresv( u );

	return kdata_page_data( u, 0, whead );
}

static inline void kdata_write_SECOND_2( struct kdata_user *u )
{
	/* Clear the writer owned bit from the buffer. */
	kdata_writer_release( u->control, u->control->head->wresv );

	/* Write back to the writer's private write head, which releases the buffer
	 * for this writer. */
	u->control->writer[u->writer_id].whead = u->control->writer[u->writer_id].wresv;

	/* Remove our release barrier. */
	u->control->writer[u->writer_id].wbar = 0;

	/* Maybe release to the readers. */
	kdata_update_whead( u );
}


static inline int kdata_prep_enter( struct kdata_control *control, int reader_id )
{
	/* Init the read head. */
	kdata_off_t rhead = control->head->whead;

	/* Okay good. */
	control->reader[reader_id].rhead = rhead; 
	control->reader[reader_id].skips = 0;
	control->reader[reader_id].entered = 0;
	control->reader[reader_id].consumed = control->head->produced;

	return 0;
}

#if defined(__cplusplus)
}
#endif

#endif
