#ifndef __KCTL_H
#define __KCTL_H

#if defined(__cplusplus)
extern "C" {
#endif

#define KCTL 20
#define KCTL_NPAGES 2048

#define KCTL_INDEX(off) ((off) & 0x7ff)

#define KCTL_PGOFF_CTRL 0
#define KCTL_PGOFF_DATA 1

/*
 * Memap identity information. 
 */

/* Ring id (5), region to map (1) */

/* Must match region shift below. */
#define KCTL_MAX_RINGS_PER_SET 32

#define KCTL_PGOFF_ID_SHIFT 0
#define KCTL_PGOFF_ID_MASK  0x1f

#define KCTL_PGOFF_REGION_SHIFT 5
#define KCTL_PGOFF_REGION_MASK  0x20

#define KCTL_RING_ID_ALL -1

/* MUST match system page size. */
#define KCTL_PAGE_SIZE 4096

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

#define KCTL_DSC_READER_SHIFT    2
#define KCTL_DSC_WRITER_OWNED    0x01
#define KCTL_DSC_SKIPPED         0x02
#define KCTL_DSC_READER_OWNED    0xfc
#define KCTL_DSC_READER_BIT(id)  ( 0x1 << ( KCTL_DSC_READER_SHIFT + (id) ) )

#define KCTL_ERR_SOCK       -1
#define KCTL_ERR_MMAP       -2
#define KCTL_ERR_BIND       -3
#define KCTL_ERR_READER_ID  -4
#define KCTL_ERR_WRITER_ID  -5
#define KCTL_ERR_RING_N     -6
#define KCTL_ERR_ENTER      -7

/* Direction: from client, or from server. */
#define KCTL_DIR_CLIENT 1
#define KCTL_DIR_SERVER 2

#define KCTL_DIR_INSIDE  1
#define KCTL_DIR_OUTSIDE 2

#define KCTL_NLEN 32
#define KCTL_READERS 6
#define KCTL_WRITERS 6

/* Configurable at allocation time. This specifies the maximum. */
#define KCTL_MAX_WRITERS_PER_RING 32

#define KR_OPT_WRITER_ID 1
#define KR_OPT_READER_ID 2
#define KR_OPT_RING_N    3

/* Records an error in the user struct. Use before goto to function cleanup. */
#define kctl_func_error( _ke, _ee ) \
	do { u->krerr = _ke; u->_errno = _ee; } while (0)

enum KCTL_TYPE
{
	KCTL_PACKETS = 1,
	KCTL_DECRYPTED,
	KCTL_PLAIN
};

enum KCTL_MODE
{
	KCTL_READ = 1,
	KCTL_WRITE
};

typedef unsigned short kctl_desc_t;
typedef unsigned long kctl_off_t;

struct kctl_shared_head
{
	kctl_off_t whead;
	kctl_off_t wresv;
	unsigned long long produced;
	int write_mutex;
	unsigned long long spins;
};

struct kctl_shared_writer
{
	kctl_off_t whead;
	kctl_off_t wresv;
	kctl_off_t wbar;
};

struct kctl_shared_reader
{
	kctl_off_t rhead;
	unsigned long skips;
	unsigned char entered;
	unsigned long long consumed;
};

struct kctl_shared_desc
{
	kctl_desc_t desc;
};

struct kctl_page_desc
{
	struct page *p;
	void *m;
};

struct kctl_page
{
	char d[KCTL_PAGE_SIZE];
};

#define KCTL_CTRL_SZ ( \
	sizeof(struct kctl_shared_head) + \
	sizeof(struct kctl_shared_writer) * KCTL_WRITERS + \
	sizeof(struct kctl_shared_reader) * KCTL_READERS + \
	sizeof(struct kctl_shared_desc) * KCTL_NPAGES \
)
	
#define KCTL_CTRL_OFF_HEAD   0
#define KCTL_CTRL_OFF_WRITER KCTL_CTRL_OFF_HEAD + sizeof(struct kctl_shared_head)
#define KCTL_CTRL_OFF_READER KCTL_CTRL_OFF_WRITER + sizeof(struct kctl_shared_writer) * KCTL_WRITERS
#define KCTL_CTRL_OFF_DESC   KCTL_CTRL_OFF_READER + sizeof(struct kctl_shared_reader) * KCTL_READERS

#define KCTL_DATA_SZ KCTL_PAGE_SIZE * KCTL_NPAGES

struct kctl_control
{
	struct kctl_shared_head *head;
	struct kctl_shared_writer *writer;
	struct kctl_shared_reader *reader;
	struct kctl_shared_desc *descriptor;
};

struct kctl_data
{
	struct kctl_page *page;
};

struct kctl_shared
{
	struct kctl_control *control;
	struct kctl_data *data;
};

struct kctl_user
{
	int socket;
	int ring_id;
	int nrings;
	int writer_id;
	int reader_id;
	enum KCTL_MODE mode;

	struct kctl_control *control;

	/* When used in user space we use the data pointer, which points to the
	 * mmapped region. In the kernel we use pd, which points to the array of
	 * pages+memory pointers. Must be interpreted according to socket value. */
	struct kctl_data *data;
	struct kctl_page_desc *pd;

	int krerr;
	int _errno;
	char *errstr;
};

struct kctl_addr
{
	char name[KCTL_NLEN];
	int ring_id;
	enum KCTL_MODE mode;
};

struct kctl_packet
{
	char dir;
	int len;
	int caplen;
	unsigned char *bytes;
};

struct kctl_decrypted
{
	 long id;
	 unsigned char type;
	 char *host;
	 unsigned char *bytes;
	 int len;
};

struct kctl_plain
{
	 int len;
	 unsigned char *bytes;
};

struct kctl_packet_header
{
	int len;
	char dir;
};

struct kctl_decrypted_header
{
	int len;
	long id;
	char type;
	char host[63];
};

struct kctl_plain_header
{
	int len;
};


int kctl_open( struct kctl_user *u, enum KCTL_TYPE type, const char *ringset, int rid, enum KCTL_MODE mode );
int kctl_write_decrypted( struct kctl_user *u, long id, int type, const char *remoteHost, char *data, int len );
int kctl_write_plain( struct kctl_user *u, char *data, int len );
int kctl_read_wait( struct kctl_user *u );

static inline int kctl_packet_max_data(void)
{
	return KCTL_PAGE_SIZE - sizeof(struct kctl_packet_header);
}

static inline int kctl_decrypted_max_data(void)
{
	return KCTL_PAGE_SIZE - sizeof(struct kctl_decrypted_header);
}

static inline int kctl_plain_max_data(void)
{
	return KCTL_PAGE_SIZE - sizeof(struct kctl_plain_header);
}

static inline unsigned long long kctl_spins( struct kctl_user *u )
{
	return u->control->head->spins;
}

char *kctl_error( struct kctl_user *u, int err );

static inline unsigned long kctl_skips( struct kctl_user *u )
{
	unsigned long skips = 0;
	if ( u->ring_id != KCTL_RING_ID_ALL )
		skips = u->control->reader[u->reader_id].skips;
	else {
		int ring;
		for ( ring = 0; ring < u->nrings; ring++ )
			skips += u->control[ring].reader[u->reader_id].skips;
	}
	return skips;
}

static inline int kctl_avail_impl( struct kctl_control *control, int reader_id )
{
	return ( control->reader[reader_id].rhead != control->head->whead );
}

static inline kctl_desc_t kctl_read_desc( struct kctl_control *control, kctl_off_t off )
{
	return control->descriptor[KCTL_INDEX(off)].desc;
}

static inline kctl_desc_t kctl_write_back( struct kctl_control *control,
		kctl_off_t off, kctl_desc_t oldval, kctl_desc_t newval )
{
	return __sync_val_compare_and_swap(
			&control->descriptor[KCTL_INDEX(off)].desc, oldval, newval );
}

static inline kctl_off_t kctl_wresv_write_back( struct kctl_control *control,
		kctl_off_t oldval, kctl_off_t newval )
{
	return __sync_val_compare_and_swap( &control->head->wresv, oldval, newval );
}

static inline kctl_off_t kctl_whead_write_back( struct kctl_control *control,
		kctl_off_t oldval, kctl_off_t newval )
{
	return __sync_val_compare_and_swap( &control->head->whead, oldval, newval );
}


static inline void *kctl_page_data( struct kctl_user *u, int ctrl, kctl_off_t off )
{
	if ( u->socket < 0 )
		return u->pd[KCTL_INDEX(off)].m;
	else
		return u->data[ctrl].page + KCTL_INDEX(off);
}

static inline int kctl_avail( struct kctl_user *u )
{
	if ( u->ring_id != KCTL_RING_ID_ALL )
		return kctl_avail_impl( u->control, u->reader_id );
	else {
		int ctrl;
		for ( ctrl = 0; ctrl < u->nrings; ctrl++ ) {
			if ( kctl_avail_impl( &u->control[ctrl], u->reader_id ) )
				return 1;
		}
		return 0;
	}
}

static inline kctl_off_t kctl_next( kctl_off_t off )
{
	return off + 1;
}

static inline kctl_off_t kctl_advance_rhead( struct kctl_control *control, int reader_id, kctl_off_t rhead )
{
	kctl_desc_t desc;
	while ( 1 ) {
		rhead = kctl_next( rhead );

		/* reserve next. */
		desc = kctl_read_desc( control, rhead );
		if ( ! ( desc & KCTL_DSC_WRITER_OWNED ) ) {
			/* Okay we can take it. */
			kctl_desc_t newval = desc | KCTL_DSC_READER_BIT( reader_id );
		
			/* Attemp write back. */
			kctl_desc_t before = kctl_write_back( control, rhead, desc, newval );
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
static inline void kctl_reader_release( int reader_id, struct kctl_control *control, kctl_off_t prev )
{
	kctl_desc_t before, desc, newval;
again:
	/* Take a copy, modify, then try to write back. */
	desc = kctl_read_desc( control, prev );
	
	newval = desc & ~( KCTL_DSC_READER_BIT( reader_id ) );

	/* Was it skipped? */
	if ( desc & KCTL_DSC_SKIPPED ) {
		/* If we are the last to release it, then reset the skipped bit. */
		if ( ! ( newval & KCTL_DSC_READER_OWNED ) )
			newval &= ~KCTL_DSC_SKIPPED;
	}

	before = kctl_write_back( control, prev, desc, newval );
	if ( before != desc )
		goto again;
	
	__sync_add_and_fetch( &control->reader->consumed, 1 );
}

static inline int kctl_select_ctrl( struct kctl_user *u )
{
	if ( u->ring_id != KCTL_RING_ID_ALL )
		return 0;
	else {
		int ctrl;
		for ( ctrl = 0; ctrl < u->nrings; ctrl++ ) {
			if ( kctl_avail_impl( &u->control[ctrl], u->reader_id ) )
				return ctrl;
		}
		return -1;
	}
}

static inline void *kctl_next_generic( struct kctl_user *u )
{
	int ctrl = kctl_select_ctrl( u );

	kctl_off_t prev = u->control[ctrl].reader[u->reader_id].rhead;
	kctl_off_t rhead = prev;

	rhead = kctl_advance_rhead( &u->control[ctrl], u->reader_id, rhead );

	/* Set the rhead. */
	u->control[ctrl].reader[u->reader_id].rhead = rhead;

	/* Release the previous only if we have entered with a successful read. */
	if ( u->control[ctrl].reader[u->reader_id].entered )
		kctl_reader_release( u->reader_id, &u->control[ctrl], prev );

	/* Indicate we have entered. */
	u->control[ctrl].reader[u->reader_id].entered = 1;

	return kctl_page_data( u, ctrl, rhead );
}


static inline void kctl_next_packet( struct kctl_user *u, struct kctl_packet *packet )
{
	struct kctl_packet_header *h;

	h = (struct kctl_packet_header*) kctl_next_generic( u );

	packet->len = h->len;
	packet->caplen = 
			( h->len <= kctl_packet_max_data() ) ?
			h->len :
			kctl_packet_max_data();
	packet->dir = h->dir;
	packet->bytes = (unsigned char*)( h + 1 );
}

static inline void kctl_next_decrypted( struct kctl_user *u, struct kctl_decrypted *decrypted )
{
	struct kctl_decrypted_header *h;

	h = (struct kctl_decrypted_header*) kctl_next_generic( u );

	decrypted->len = h->len;
	decrypted->id = h->id;
	decrypted->type = h->type;
	decrypted->host = h->host;
	decrypted->bytes = (unsigned char*)( h + 1 );
}

static inline void kctl_next_plain( struct kctl_user *u, struct kctl_plain *plain )
{
	struct kctl_plain_header *h;

	h = (struct kctl_plain_header*) kctl_next_generic( u );

	plain->len = h->len;
	plain->bytes = (unsigned char*)( h + 1 );
}

static inline unsigned long kctl_one_back( unsigned long pos )
{
	return pos == 0 ? KCTL_NPAGES - 1 : pos - 1;
}

static inline unsigned long kctl_find_write_loc( struct kctl_control *control )
{
	int id;
	kctl_desc_t desc = 0;
	kctl_off_t whead = control->head->whead;
	while ( 1 ) {
		/* Move to the next slot. */
		whead = kctl_next( whead );

retry:
		/* Read the descriptor. */
		desc = kctl_read_desc( control, whead );

		/* Check, if not okay, go on to next. */
		if ( desc & KCTL_DSC_READER_OWNED || desc & KCTL_DSC_SKIPPED ) {
			kctl_desc_t before;

			/* register skips. */
			for ( id = 0; id < KCTL_READERS; id++ ) {
				if ( desc & KCTL_DSC_READER_BIT( id ) ) {
					/* reader id present. */
					control->reader[id].skips += 1;
				}
			}

			/* Mark as skipped. If a reader got in before us, retry. */
			before = kctl_write_back( control, whead, desc, desc | KCTL_DSC_SKIPPED );
			if ( before != desc )
				goto retry;

			/* After registering the skip, go on to look for another block. */
		}
		else if ( desc & KCTL_DSC_WRITER_OWNED ) {
			/* A different writer has the block. Go forward to find another
			 * block. */
		}
		else {
			/* Available. */
			kctl_desc_t newval = desc | KCTL_DSC_WRITER_OWNED;

			/* Okay. Attempt to claim with an atomic write back. */
			kctl_desc_t before = kctl_write_back( control, whead, desc, newval );
			if ( before != desc )
				goto retry;

			/* Write back okay. No reader claimed. We can use. */
			return whead;
		}

		/* FIXME: if we get back to where we started then bail */
	}
}

static inline void *kctl_write_FIRST( struct kctl_user *u )
{
	kctl_off_t whead;

	/* Find the place to write to, skipping ahead as necessary. */
	whead = kctl_find_write_loc( u->control );

	/* Reserve the space. */
	u->control->head->wresv = whead;

	return kctl_page_data( u, 0, whead );
}

static inline int kctl_writer_release( struct kctl_control *control, kctl_off_t whead )
{
	/* orig value. */
	kctl_desc_t desc = kctl_read_desc( control, whead );

	/* Unrelease writer. */
	kctl_desc_t newval = desc & ~KCTL_DSC_WRITER_OWNED;

	/* Write back with check. No other reader or writer should have altered the
	 * descriptor. */
	kctl_desc_t before = kctl_write_back( control, whead, desc, newval );
	if ( before != desc )
		return -1;

	return 0;
}

static inline void kctl_write_SECOND( struct kctl_user *u )
{
	/* Clear the writer owned bit from the buffer. */
	kctl_writer_release( u->control, u->control->head->wresv );

	/* Write back the write head, thereby releasing the buffer to writer. */
	u->control->head->whead = u->control->head->wresv;
}

static inline void kctl_update_wresv( struct kctl_user *u )
{
	int w;
	kctl_off_t wresv, before, highest;

again:
	wresv = u->control->head->wresv;

	/* we are setting wresv to the highest amongst the writers. If a writer is
	 * inactive then it's resv will be left behind and not affect this
	 * compuation. */
	highest = 0;
	for ( w = 0; w < KCTL_WRITERS; w++ ) {
		if ( u->control->writer[w].wresv > highest )
			highest = u->control->writer[w].wresv;
	}

	before = kctl_wresv_write_back( u->control, wresv, highest );
	if ( before != wresv )
		goto again;
}

static inline void kctl_update_whead( struct kctl_user *u )
{
	int w;
	kctl_off_t orig, before, whead = 0, wbar = ~((kctl_off_t)0);

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
	for ( w = 0; w < KCTL_WRITERS; w++ ) {
		if ( u->control->writer[w].whead > whead )
			whead = u->control->writer[w].whead;
	}

	/* Second pass. Find the lowest barrier. */
	for ( w = 0; w < KCTL_WRITERS; w++ ) {
		if ( u->control->writer[w].wbar != 0 ) {
			if ( u->control->writer[w].wbar < wbar )
				wbar = u->control->writer[w].wbar;
		}
	}

	if ( wbar < whead )
		whead = wbar;
	
	/* Write back. */

	before = kctl_whead_write_back( u->control, orig, whead );
	if ( before != orig )
		goto retry;
}

static inline void *kctl_write_FIRST_2( struct kctl_user *u )
{
	/* Start at wresv. There is nothing free before this value. We cannot start
	 * at whead because other writers may have written and release (not showing
	 * writer owned bits, but we cannot take.*/
	kctl_off_t whead = u->control->head->wresv;

	/* Set the release barrier to the place where we start looking. We cannot
	 * release past this point. */
	u->control->writer[u->writer_id].wbar = whead;

	/* Find the place to write to, skipping ahead as necessary. */
	whead = kctl_find_write_loc( u->control );

	/* Private reserve. */
	u->control->writer[u->writer_id].wresv = whead;

	/* Update the common wreserve. */
	kctl_update_wresv( u );

	return kctl_page_data( u, 0, whead );
}

static inline void kctl_write_SECOND_2( struct kctl_user *u )
{
	/* Clear the writer owned bit from the buffer. */
	kctl_writer_release( u->control, u->control->head->wresv );

	/* Write back to the writer's private write head, which releases the buffer
	 * for this writer. */
	u->control->writer[u->writer_id].whead = u->control->writer[u->writer_id].wresv;

	/* Remove our release barrier. */
	u->control->writer[u->writer_id].wbar = 0;

	/* Maybe release to the readers. */
	kctl_update_whead( u );
}


static inline int kctl_prep_enter( struct kctl_control *control, int reader_id )
{
	/* Init the read head. */
	kctl_off_t rhead = control->head->whead;

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

