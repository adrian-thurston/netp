#ifndef __KCTRL_H
#define __KCTRL_H


/*
 * Two Part:
 *  1. Allocate buffers using desctriptors to claim, skipping over long-held
 *      buffers.
 *  2. Atomic append to linked link list of messages.
 *
 *  Important that we use ever-increasing (non-wrapping) indicies and mask off.
 *  This way we can detect when a buffer is re-used.
 */


#if defined(__cplusplus)
extern "C" {
#endif

#define KCTRL 20
#define KCTRL_NPAGES 2048

#define KCTRL_INDEX(off) ((off) & 0x7ff)

#define KCTRL_PGOFF_CTRL 0
#define KCTRL_PGOFF_DATA 1

/*
 * Memap identity information. 
 */

/* Ring id (5), region to map (1) */

/* Must match region shift below. */
#define KCTRL_MAX_RINGS_PER_SET 32

#define KCTRL_PGOFF_ID_SHIFT 0
#define KCTRL_PGOFF_ID_MASK  0x1f

#define KCTRL_PGOFF_REGION_SHIFT 5
#define KCTRL_PGOFF_REGION_MASK  0x20

#define KCTRL_RING_ID_ALL -1

/* MUST match system page size. */
#define KCTRL_PAGE_SIZE 4096

/*
 * Argument for non-wrapping offsets, allowing lockless multiple writers and
 * readers.
 *
 * ( 2 ^ 64 * 4096 * 8 ) / ( 1024 * 1024 * 1024 * 1024 ) / ( 60 * 60 * 24 * 360 )
 * = 17674 years
 *
 * DATA      = ( IDX * page-size * bits )
 * 1tbit/s   = ( 1024 * 1024 * 1024 * 1024 )
 * year      = ( sec * min * hour * days ) 
 */

#define KCTRL_DSC_READER_SHIFT    2
#define KCTRL_DSC_WRITER_OWNED    0x01
#define KCTRL_DSC_SKIPPED         0x02
#define KCTRL_DSC_READER_OWNED    0xfc
#define KCTRL_DSC_READER_BIT(id)  ( 0x1 << ( KCTRL_DSC_READER_SHIFT + (id) ) )

#define KCTRL_ERR_SOCK       -1
#define KCTRL_ERR_MMAP       -2
#define KCTRL_ERR_BIND       -3
#define KCTRL_ERR_READER_ID  -4
#define KCTRL_ERR_WRITER_ID  -5
#define KCTRL_ERR_RING_N     -6
#define KCTRL_ERR_ENTER      -7

/* Direction: from client, or from server. */
#define KCTRL_DIR_CLIENT 1
#define KCTRL_DIR_SERVER 2

#define KCTRL_DIR_INSIDE  1
#define KCTRL_DIR_OUTSIDE 2

#define KCTRL_NLEN 32
#define KCTRL_READERS 6
#define KCTRL_WRITERS 6

/* Configurable at allocation time. This specifies the maximum. */
#define KCTRL_MAX_WRITERS_PER_RING 32

#define KR_OPT_WRITER_ID 1
#define KR_OPT_READER_ID 2
#define KR_OPT_RING_N    3

/* Records an error in the user struct. Use before goto to function cleanup. */
#define kctrl_func_error( _ke, _ee ) \
	do { u->krerr = _ke; u->_errno = _ee; } while (0)

enum KCTRL_TYPE
{
	KCTRL_PACKETS = 1,
	KCTRL_DECRYPTED,
	KCTRL_PLAIN
};

enum KCTRL_MODE
{
	KCTRL_READ = 1,
	KCTRL_WRITE
};

typedef unsigned short kctrl_desc_t;
typedef unsigned long kctrl_off_t;

struct kctrl_shared_head
{
	kctrl_off_t alloc;
	kctrl_off_t head;
	kctrl_off_t maybe_tail;

	kctrl_off_t whead;
	kctrl_off_t wresv;
	unsigned long long produced;
	int write_mutex;
	unsigned long long spins;
};

struct kctrl_shared_writer
{
	kctrl_off_t whead;
	kctrl_off_t wresv;
	kctrl_off_t wbar;
};

struct kctrl_shared_reader
{
	kctrl_off_t rhead;
	unsigned long skips;
	unsigned char entered;
	unsigned long long consumed;
};

struct kctrl_shared_desc
{
	kctrl_desc_t desc;
	kctrl_off_t next;
};

struct kctrl_page_desc
{
	struct page *p;
	void *m;
};

struct kctrl_page
{
	char d[KCTRL_PAGE_SIZE];
};

#define KCTRL_CTRL_SZ ( \
	sizeof(struct kctrl_shared_head) + \
	sizeof(struct kctrl_shared_writer) * KCTRL_WRITERS + \
	sizeof(struct kctrl_shared_reader) * KCTRL_READERS + \
	sizeof(struct kctrl_shared_desc) * KCTRL_NPAGES \
)
	
#define KCTRL_CTRL_OFF_HEAD   0
#define KCTRL_CTRL_OFF_WRITER KCTRL_CTRL_OFF_HEAD + sizeof(struct kctrl_shared_head)
#define KCTRL_CTRL_OFF_READER KCTRL_CTRL_OFF_WRITER + sizeof(struct kctrl_shared_writer) * KCTRL_WRITERS
#define KCTRL_CTRL_OFF_DESC   KCTRL_CTRL_OFF_READER + sizeof(struct kctrl_shared_reader) * KCTRL_READERS

#define KCTRL_DATA_SZ KCTRL_PAGE_SIZE * KCTRL_NPAGES

struct kctrl_control
{
	struct kctrl_shared_head *head;
	struct kctrl_shared_writer *writer;
	struct kctrl_shared_reader *reader;
	struct kctrl_shared_desc *descriptor;
};

struct kctrl_data
{
	struct kctrl_page *page;
};

struct kctrl_shared
{
	struct kctrl_control *control;
	struct kctrl_data *data;
};

struct kctrl_user
{
	int socket;
	int ring_id;
	int nrings;
	int writer_id;
	int reader_id;
	enum KCTRL_MODE mode;

	struct kctrl_control *control;

	/* When used in user space we use the data pointer, which points to the
	 * mmapped region. In the kernel we use pd, which points to the array of
	 * pages+memory pointers. Must be interpreted according to socket value. */
	struct kctrl_data *data;
	struct kctrl_page_desc *pd;

	int krerr;
	int _errno;
	char *errstr;
};

struct kctrl_addr
{
	char name[KCTRL_NLEN];
	int ring_id;
	enum KCTRL_MODE mode;
};

struct kctrl_packet
{
	char dir;
	int len;
	int caplen;
	unsigned char *bytes;
};

struct kctrl_decrypted
{
	 long id;
	 unsigned char type;
	 char *host;
	 unsigned char *bytes;
	 int len;
};

struct kctrl_plain
{
	 int len;
	 unsigned char *bytes;
};

struct kctrl_packet_header
{
	int len;
	char dir;
};

struct kctrl_decrypted_header
{
	int len;
	long id;
	char type;
	char host[63];
};

struct kctrl_plain_header
{
	int len;
};

int kctrl_open( struct kctrl_user *u, enum KCTRL_TYPE type, const char *ringset, int rid, enum KCTRL_MODE mode );
int kctrl_write_decrypted( struct kctrl_user *u, long id, int type, const char *remoteHost, char *data, int len );
int kctrl_write_plain( struct kctrl_user *u, char *data, int len );
int kctrl_read_wait( struct kctrl_user *u );

static inline int kctrl_packet_max_data(void)
{
	return KCTRL_PAGE_SIZE - sizeof(struct kctrl_packet_header);
}

static inline int kctrl_decrypted_max_data(void)
{
	return KCTRL_PAGE_SIZE - sizeof(struct kctrl_decrypted_header);
}

static inline int kctrl_plain_max_data(void)
{
	return KCTRL_PAGE_SIZE - sizeof(struct kctrl_plain_header);
}

static inline unsigned long long kctrl_spins( struct kctrl_user *u )
{
	return u->control->head->spins;
}

char *kctrl_error( struct kctrl_user *u, int err );

static inline unsigned long kctrl_skips( struct kctrl_user *u )
{
	unsigned long skips = 0;
	if ( u->ring_id != KCTRL_RING_ID_ALL )
		skips = u->control->reader[u->reader_id].skips;
	else {
		int ring;
		for ( ring = 0; ring < u->nrings; ring++ )
			skips += u->control[ring].reader[u->reader_id].skips;
	}
	return skips;
}

static inline int kctrl_avail_impl( struct kctrl_control *control, int reader_id )
{
	return ( control->reader[reader_id].rhead != control->head->whead );
}

static inline kctrl_desc_t kctrl_read_desc( struct kctrl_control *control, kctrl_off_t off )
{
	return control->descriptor[KCTRL_INDEX(off)].desc;
}

static inline kctrl_desc_t kctrl_write_back( struct kctrl_control *control,
		kctrl_off_t off, kctrl_desc_t oldval, kctrl_desc_t newval )
{
	return __sync_val_compare_and_swap(
			&control->descriptor[KCTRL_INDEX(off)].desc, oldval, newval );
}

static inline kctrl_off_t kctrl_wresv_write_back( struct kctrl_control *control,
		kctrl_off_t oldval, kctrl_off_t newval )
{
	return __sync_val_compare_and_swap( &control->head->wresv, oldval, newval );
}

static inline kctrl_off_t kctrl_whead_write_back( struct kctrl_control *control,
		kctrl_off_t oldval, kctrl_off_t newval )
{
	return __sync_val_compare_and_swap( &control->head->whead, oldval, newval );
}


static inline void *kctrl_page_data( struct kctrl_user *u, int ctrl, kctrl_off_t off )
{
	if ( u->socket < 0 )
		return u->pd[KCTRL_INDEX(off)].m;
	else
		return u->data[ctrl].page + KCTRL_INDEX(off);
}

static inline int kctrl_avail( struct kctrl_user *u )
{
	if ( u->ring_id != KCTRL_RING_ID_ALL )
		return kctrl_avail_impl( u->control, u->reader_id );
	else {
		int ctrl;
		for ( ctrl = 0; ctrl < u->nrings; ctrl++ ) {
			if ( kctrl_avail_impl( &u->control[ctrl], u->reader_id ) )
				return 1;
		}
		return 0;
	}
}

static inline kctrl_off_t kctrl_next( kctrl_off_t off )
{
	return off + 1;
}

static inline unsigned long kctrl_one_back( unsigned long pos )
{
	return pos == 0 ? KCTRL_NPAGES - 1 : pos - 1;
}


static inline kctrl_off_t kctrl_advance_rhead( struct kctrl_control *control, int reader_id, kctrl_off_t rhead )
{
	kctrl_desc_t desc;
	while ( 1 ) {
		rhead = kctrl_next( rhead );

		/* reserve next. */
		desc = kctrl_read_desc( control, rhead );
		if ( ! ( desc & KCTRL_DSC_WRITER_OWNED ) ) {
			/* Okay we can take it. */
			kctrl_desc_t newval = desc | KCTRL_DSC_READER_BIT( reader_id );
		
			/* Attemp write back. */
			kctrl_desc_t before = kctrl_write_back( control, rhead, desc, newval );
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
static inline void kctrl_reader_release( int reader_id, struct kctrl_control *control, kctrl_off_t prev )
{
	kctrl_desc_t before, desc, newval;
again:
	/* Take a copy, modify, then try to write back. */
	desc = kctrl_read_desc( control, prev );
	
	newval = desc & ~( KCTRL_DSC_READER_BIT( reader_id ) );

	/* Was it skipped? */
	if ( desc & KCTRL_DSC_SKIPPED ) {
		/* If we are the last to release it, then reset the skipped bit. */
		if ( ! ( newval & KCTRL_DSC_READER_OWNED ) )
			newval &= ~KCTRL_DSC_SKIPPED;
	}

	before = kctrl_write_back( control, prev, desc, newval );
	if ( before != desc )
		goto again;
	
	__sync_add_and_fetch( &control->reader->consumed, 1 );
}

static inline int kctrl_select_ctrl( struct kctrl_user *u )
{
	if ( u->ring_id != KCTRL_RING_ID_ALL )
		return 0;
	else {
		int ctrl;
		for ( ctrl = 0; ctrl < u->nrings; ctrl++ ) {
			if ( kctrl_avail_impl( &u->control[ctrl], u->reader_id ) )
				return ctrl;
		}
		return -1;
	}
}

static inline void *kctrl_next_generic( struct kctrl_user *u )
{
	int ctrl = kctrl_select_ctrl( u );

	kctrl_off_t rhead = u->control->descriptor[ u->control->head->head ].next;
	
	u->control->head->head = rhead;

	return kctrl_page_data( u, ctrl, rhead );
}


static inline void kctrl_next_packet( struct kctrl_user *u, struct kctrl_packet *packet )
{
	struct kctrl_packet_header *h;

	h = (struct kctrl_packet_header*) kctrl_next_generic( u );

	packet->len = h->len;
	packet->caplen = 
			( h->len <= kctrl_packet_max_data() ) ?
			h->len :
			kctrl_packet_max_data();
	packet->dir = h->dir;
	packet->bytes = (unsigned char*)( h + 1 );
}

static inline void kctrl_next_decrypted( struct kctrl_user *u, struct kctrl_decrypted *decrypted )
{
	struct kctrl_decrypted_header *h;

	h = (struct kctrl_decrypted_header*) kctrl_next_generic( u );

	decrypted->len = h->len;
	decrypted->id = h->id;
	decrypted->type = h->type;
	decrypted->host = h->host;
	decrypted->bytes = (unsigned char*)( h + 1 );
}

static inline void kctrl_next_plain( struct kctrl_user *u, struct kctrl_plain *plain )
{
	struct kctrl_plain_header *h;

	h = (struct kctrl_plain_header*) kctrl_next_generic( u );

	plain->len = h->len;
	plain->bytes = (unsigned char*)( h + 1 );
}

static inline unsigned long kctrl_find_write_loc( struct kctrl_control *control )
{
	int id;
	kctrl_desc_t desc = 0;
	kctrl_off_t whead = control->head->alloc;
	while ( 1 ) {
		/* Move to the next slot. */
		whead = kctrl_next( whead );

retry:
		/* Read the descriptor. */
		desc = kctrl_read_desc( control, whead );

		/* Check, if not okay, go on to next. */
		if ( desc & KCTRL_DSC_READER_OWNED || desc & KCTRL_DSC_SKIPPED ) {
			kctrl_desc_t before;

			/* register skips. */
			for ( id = 0; id < KCTRL_READERS; id++ ) {
				if ( desc & KCTRL_DSC_READER_BIT( id ) ) {
					/* reader id present. */
					control->reader[id].skips += 1;
				}
			}

			/* Mark as skipped. If a reader got in before us, retry. */
			before = kctrl_write_back( control, whead, desc, desc | KCTRL_DSC_SKIPPED );
			if ( before != desc )
				goto retry;

			/* After registering the skip, go on to look for another block. */
		}
		else if ( desc & KCTRL_DSC_WRITER_OWNED ) {
			/* A different writer has the block. Go forward to find another
			 * block. */
		}
		else {
			/* Available. */
			kctrl_desc_t newval = desc | KCTRL_DSC_WRITER_OWNED;

			/* Okay. Attempt to claim with an atomic write back. */
			kctrl_desc_t before = kctrl_write_back( control, whead, desc, newval );
			if ( before != desc )
				goto retry;

			/* Write back okay. No reader claimed. We can use. */
			return whead;
		}

		/* FIXME: if we get back to where we started then bail */
	}
}

static inline void *kctrl_write_FIRST( struct kctrl_user *u )
{
	kctrl_off_t whead;

	/* Find the place to write to, skipping ahead as necessary. */
	whead = kctrl_find_write_loc( u->control );

	/* Reserve the space. */
	u->control->head->alloc = whead;

	u->control->writer[u->writer_id].whead = whead;

	return kctrl_page_data( u, 0, whead );
}

static inline int kctrl_writer_release( struct kctrl_control *control, kctrl_off_t whead )
{
	/* orig value. */
	kctrl_desc_t desc = kctrl_read_desc( control, whead );

	/* Unrelease writer. */
	kctrl_desc_t newval = desc & ~KCTRL_DSC_WRITER_OWNED;

	/* Write back with check. No other reader or writer should have altered the
	 * descriptor. */
	kctrl_desc_t before = kctrl_write_back( control, whead, desc, newval );
	if ( before != desc )
		return -1;

	return 0;
}

static inline void kctrl_write_SECOND( struct kctrl_user *u )
{
	kctrl_off_t tail;

	/* Clear the writer owned bit from the buffer. */
	kctrl_writer_release( u->control, u->control->head->wresv );

	/* Move forward to the true tail. */
	tail = u->control->head->maybe_tail;

	while ( 1 ) {
		kctrl_off_t next = u->control->descriptor[KCTRL_INDEX(tail)].next;

		/* Using every-increasing indicies, so when we go forward the index
		 * must go up. If the block gets old that's okay. We go forward until
		 * we are back in the active blocks. If it goes so stale that it is
		 * reused. Well it's index will go up by num pages, but next will stay
		 * the same, thereby becoming a null. Further along in time the next
		 * will get rewritten to something newer, jumping ahead closer to the
		 * real tail.*/
		if ( next <= tail )
			break;

		tail = next;
	}

	/* Set next pointer to new item (release). */
	u->control->descriptor[KCTRL_INDEX(tail)].next = u->control->writer[u->writer_id].whead;

	/* Speed up tail finding. */
	u->control->head->maybe_tail = u->control->writer[u->writer_id].whead;
}

static inline void kctrl_update_wresv( struct kctrl_user *u )
{
	int w;
	kctrl_off_t wresv, before, highest;

again:
	wresv = u->control->head->wresv;

	/* we are setting wresv to the highest amongst the writers. If a writer is
	 * inactive then it's resv will be left behind and not affect this
	 * compuation. */
	highest = 0;
	for ( w = 0; w < KCTRL_WRITERS; w++ ) {
		if ( u->control->writer[w].wresv > highest )
			highest = u->control->writer[w].wresv;
	}

	before = kctrl_wresv_write_back( u->control, wresv, highest );
	if ( before != wresv )
		goto again;
}

static inline void kctrl_update_whead( struct kctrl_user *u )
{
	int w;
	kctrl_off_t orig, before, whead = 0, wbar = ~((kctrl_off_t)0);

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
	for ( w = 0; w < KCTRL_WRITERS; w++ ) {
		if ( u->control->writer[w].whead > whead )
			whead = u->control->writer[w].whead;
	}

	/* Second pass. Find the lowest barrier. */
	for ( w = 0; w < KCTRL_WRITERS; w++ ) {
		if ( u->control->writer[w].wbar != 0 ) {
			if ( u->control->writer[w].wbar < wbar )
				wbar = u->control->writer[w].wbar;
		}
	}

	if ( wbar < whead )
		whead = wbar;
	
	/* Write back. */

	before = kctrl_whead_write_back( u->control, orig, whead );
	if ( before != orig )
		goto retry;
}

static inline void *kctrl_write_FIRST_2( struct kctrl_user *u )
{
	/* Start at wresv. There is nothing free before this value. We cannot start
	 * at whead because other writers may have written and release (not showing
	 * writer owned bits, but we cannot take.*/
	kctrl_off_t whead = u->control->head->wresv;

	/* Set the release barrier to the place where we start looking. We cannot
	 * release past this point. */
	u->control->writer[u->writer_id].wbar = whead;

	/* Find the place to write to, skipping ahead as necessary. */
	whead = kctrl_find_write_loc( u->control );

	/* Private reserve. */
	u->control->writer[u->writer_id].wresv = whead;

	/* Update the common wreserve. */
	kctrl_update_wresv( u );

	return kctrl_page_data( u, 0, whead );
}

static inline void kctrl_write_SECOND_2( struct kctrl_user *u )
{
	/* Clear the writer owned bit from the buffer. */
	kctrl_writer_release( u->control, u->control->head->wresv );

	/* Write back to the writer's private write head, which releases the buffer
	 * for this writer. */
	u->control->writer[u->writer_id].whead = u->control->writer[u->writer_id].wresv;

	/* Remove our release barrier. */
	u->control->writer[u->writer_id].wbar = 0;

	/* Maybe release to the readers. */
	kctrl_update_whead( u );
}


static inline int kctrl_prep_enter( struct kctrl_control *control, int reader_id )
{
	/* Init the read head. */
	kctrl_off_t rhead = control->head->whead;

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
