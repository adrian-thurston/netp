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
#define MAX_RINGS_PER_SET 32

#define PGOFF_ID_SHIFT 0
#define PGOFF_ID_MASK  0x1f

#define PGOFF_REGION_SHIFT 5
#define PGOFF_REGION_MASK  0x20

#define KR_RING_ID_ALL -1

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

#define DSC_READER_SHIFT    2
#define DSC_WRITER_OWNED    0x01
#define DSC_SKIPPED         0x02
#define DSC_READER_OWNED    0xfc
#define DSC_READER_BIT(id)  ( 0x1 << ( DSC_READER_SHIFT + (id) ) )

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
#define kring_func_error( _ke, _ee ) \
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

typedef unsigned short kring_desc_t;
typedef unsigned long kring_off_t;

struct kring_shared_head
{
	kring_off_t whead;
	kring_off_t wresv;
	unsigned long long produced;
	int write_mutex;
	unsigned long long spins;
};

struct kring_shared_writer
{
	kring_off_t whead;
	kring_off_t wresv;
	kring_off_t wbar;
};

struct kring_shared_reader
{
	kring_off_t rhead;
	unsigned long skips;
	unsigned char entered;
	unsigned long long consumed;
};

struct shared_desc
{
	kring_desc_t desc;
};

struct kring_page_desc
{
	struct page *p;
	void *m;
};

struct kring_page
{
	char d[KCTL_PAGE_SIZE];
};

#define KCTL_CTRL_SZ ( \
	sizeof(struct kring_shared_head) + \
	sizeof(struct kring_shared_writer) * KCTL_WRITERS + \
	sizeof(struct kring_shared_reader) * KCTL_READERS + \
	sizeof(struct shared_desc) * KCTL_NPAGES \
)
	
#define KCTL_CTRL_OFF_HEAD   0
#define KCTL_CTRL_OFF_WRITER KCTL_CTRL_OFF_HEAD + sizeof(struct kring_shared_head)
#define KCTL_CTRL_OFF_READER KCTL_CTRL_OFF_WRITER + sizeof(struct kring_shared_writer) * KCTL_WRITERS
#define KCTL_CTRL_OFF_DESC   KCTL_CTRL_OFF_READER + sizeof(struct kring_shared_reader) * KCTL_READERS

#define KCTL_DATA_SZ KCTL_PAGE_SIZE * KCTL_NPAGES

struct kring_control
{
	struct kring_shared_head *head;
	struct kring_shared_writer *writer;
	struct kring_shared_reader *reader;
	struct shared_desc *descriptor;
};

struct kring_data
{
	struct kring_page *page;
};

struct kring_shared
{
	struct kring_control *control;
	struct kring_data *data;
};

struct kring_user
{
	int socket;
	int ring_id;
	int nrings;
	int writer_id;
	int reader_id;
	enum KCTL_MODE mode;

	struct kring_control *control;

	/* When used in user space we use the data pointer, which points to the
	 * mmapped region. In the kernel we use pd, which points to the array of
	 * pages+memory pointers. Must be interpreted according to socket value. */
	struct kring_data *data;
	struct kring_page_desc *pd;

	int krerr;
	int _errno;
	char *errstr;
};

struct kring_addr
{
	char name[KCTL_NLEN];
	int ring_id;
	enum KCTL_MODE mode;
};

struct kring_packet
{
	char dir;
	int len;
	int caplen;
	unsigned char *bytes;
};

struct kring_decrypted
{
	 long id;
	 unsigned char type;
	 char *host;
	 unsigned char *bytes;
	 int len;
};

struct kring_plain
{
	 int len;
	 unsigned char *bytes;
};

struct kring_packet_header
{
	int len;
	char dir;
};

struct kring_decrypted_header
{
	int len;
	long id;
	char type;
	char host[63];
};

struct kring_plain_header
{
	int len;
};


int kring_open( struct kring_user *u, enum KCTL_TYPE type, const char *ringset, int rid, enum KCTL_MODE mode );
int kring_write_decrypted( struct kring_user *u, long id, int type, const char *remoteHost, char *data, int len );
int kring_write_plain( struct kring_user *u, char *data, int len );
int kring_read_wait( struct kring_user *u );

static inline int kring_packet_max_data(void)
{
	return KCTL_PAGE_SIZE - sizeof(struct kring_packet_header);
}

static inline int kring_decrypted_max_data(void)
{
	return KCTL_PAGE_SIZE - sizeof(struct kring_decrypted_header);
}

static inline int kring_plain_max_data(void)
{
	return KCTL_PAGE_SIZE - sizeof(struct kring_plain_header);
}

static inline unsigned long long kring_spins( struct kring_user *u )
{
	return u->control->head->spins;
}

char *kring_error( struct kring_user *u, int err );

static inline unsigned long kring_skips( struct kring_user *u )
{
	unsigned long skips = 0;
	if ( u->ring_id != KR_RING_ID_ALL )
		skips = u->control->reader[u->reader_id].skips;
	else {
		int ring;
		for ( ring = 0; ring < u->nrings; ring++ )
			skips += u->control[ring].reader[u->reader_id].skips;
	}
	return skips;
}

static inline int kring_avail_impl( struct kring_control *control, int reader_id )
{
	return ( control->reader[reader_id].rhead != control->head->whead );
}

static inline kring_desc_t kring_read_desc( struct kring_control *control, kring_off_t off )
{
	return control->descriptor[KCTL_INDEX(off)].desc;
}

static inline kring_desc_t kring_write_back( struct kring_control *control,
		kring_off_t off, kring_desc_t oldval, kring_desc_t newval )
{
	return __sync_val_compare_and_swap(
			&control->descriptor[KCTL_INDEX(off)].desc, oldval, newval );
}

static inline kring_off_t kring_wresv_write_back( struct kring_control *control,
		kring_off_t oldval, kring_off_t newval )
{
	return __sync_val_compare_and_swap( &control->head->wresv, oldval, newval );
}

static inline kring_off_t kring_whead_write_back( struct kring_control *control,
		kring_off_t oldval, kring_off_t newval )
{
	return __sync_val_compare_and_swap( &control->head->whead, oldval, newval );
}


static inline void *kring_page_data( struct kring_user *u, int ctrl, kring_off_t off )
{
	if ( u->socket < 0 )
		return u->pd[KCTL_INDEX(off)].m;
	else
		return u->data[ctrl].page + KCTL_INDEX(off);
}

static inline int kring_avail( struct kring_user *u )
{
	if ( u->ring_id != KR_RING_ID_ALL )
		return kring_avail_impl( u->control, u->reader_id );
	else {
		int ctrl;
		for ( ctrl = 0; ctrl < u->nrings; ctrl++ ) {
			if ( kring_avail_impl( &u->control[ctrl], u->reader_id ) )
				return 1;
		}
		return 0;
	}
}

static inline kring_off_t kring_next( kring_off_t off )
{
	return off + 1;
}

static inline kring_off_t kring_advance_rhead( struct kring_control *control, int reader_id, kring_off_t rhead )
{
	kring_desc_t desc;
	while ( 1 ) {
		rhead = kring_next( rhead );

		/* reserve next. */
		desc = kring_read_desc( control, rhead );
		if ( ! ( desc & DSC_WRITER_OWNED ) ) {
			/* Okay we can take it. */
			kring_desc_t newval = desc | DSC_READER_BIT( reader_id );
		
			/* Attemp write back. */
			kring_desc_t before = kring_write_back( control, rhead, desc, newval );
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
static inline void kring_reader_release( int reader_id, struct kring_control *control, kring_off_t prev )
{
	kring_desc_t before, desc, newval;
again:
	/* Take a copy, modify, then try to write back. */
	desc = kring_read_desc( control, prev );
	
	newval = desc & ~( DSC_READER_BIT( reader_id ) );

	/* Was it skipped? */
	if ( desc & DSC_SKIPPED ) {
		/* If we are the last to release it, then reset the skipped bit. */
		if ( ! ( newval & DSC_READER_OWNED ) )
			newval &= ~DSC_SKIPPED;
	}

	before = kring_write_back( control, prev, desc, newval );
	if ( before != desc )
		goto again;
	
	__sync_add_and_fetch( &control->reader->consumed, 1 );
}

static inline int kring_select_ctrl( struct kring_user *u )
{
	if ( u->ring_id != KR_RING_ID_ALL )
		return 0;
	else {
		int ctrl;
		for ( ctrl = 0; ctrl < u->nrings; ctrl++ ) {
			if ( kring_avail_impl( &u->control[ctrl], u->reader_id ) )
				return ctrl;
		}
		return -1;
	}
}

static inline void *kring_next_generic( struct kring_user *u )
{
	int ctrl = kring_select_ctrl( u );

	kring_off_t prev = u->control[ctrl].reader[u->reader_id].rhead;
	kring_off_t rhead = prev;

	rhead = kring_advance_rhead( &u->control[ctrl], u->reader_id, rhead );

	/* Set the rhead. */
	u->control[ctrl].reader[u->reader_id].rhead = rhead;

	/* Release the previous only if we have entered with a successful read. */
	if ( u->control[ctrl].reader[u->reader_id].entered )
		kring_reader_release( u->reader_id, &u->control[ctrl], prev );

	/* Indicate we have entered. */
	u->control[ctrl].reader[u->reader_id].entered = 1;

	return kring_page_data( u, ctrl, rhead );
}


static inline void kring_next_packet( struct kring_user *u, struct kring_packet *packet )
{
	struct kring_packet_header *h;

	h = (struct kring_packet_header*) kring_next_generic( u );

	packet->len = h->len;
	packet->caplen = 
			( h->len <= kring_packet_max_data() ) ?
			h->len :
			kring_packet_max_data();
	packet->dir = h->dir;
	packet->bytes = (unsigned char*)( h + 1 );
}

static inline void kring_next_decrypted( struct kring_user *u, struct kring_decrypted *decrypted )
{
	struct kring_decrypted_header *h;

	h = (struct kring_decrypted_header*) kring_next_generic( u );

	decrypted->len = h->len;
	decrypted->id = h->id;
	decrypted->type = h->type;
	decrypted->host = h->host;
	decrypted->bytes = (unsigned char*)( h + 1 );
}

static inline void kring_next_plain( struct kring_user *u, struct kring_plain *plain )
{
	struct kring_plain_header *h;

	h = (struct kring_plain_header*) kring_next_generic( u );

	plain->len = h->len;
	plain->bytes = (unsigned char*)( h + 1 );
}

static inline unsigned long kring_one_back( unsigned long pos )
{
	return pos == 0 ? KCTL_NPAGES - 1 : pos - 1;
}

static inline unsigned long find_write_loc( struct kring_control *control )
{
	int id;
	kring_desc_t desc = 0;
	kring_off_t whead = control->head->whead;
	while ( 1 ) {
		/* Move to the next slot. */
		whead = kring_next( whead );

retry:
		/* Read the descriptor. */
		desc = kring_read_desc( control, whead );

		/* Check, if not okay, go on to next. */
		if ( desc & DSC_READER_OWNED || desc & DSC_SKIPPED ) {
			kring_desc_t before;

			/* register skips. */
			for ( id = 0; id < KCTL_READERS; id++ ) {
				if ( desc & DSC_READER_BIT( id ) ) {
					/* reader id present. */
					control->reader[id].skips += 1;
				}
			}

			/* Mark as skipped. If a reader got in before us, retry. */
			before = kring_write_back( control, whead, desc, desc | DSC_SKIPPED );
			if ( before != desc )
				goto retry;

			/* After registering the skip, go on to look for another block. */
		}
		else if ( desc & DSC_WRITER_OWNED ) {
			/* A different writer has the block. Go forward to find another
			 * block. */
		}
		else {
			/* Available. */
			kring_desc_t newval = desc | DSC_WRITER_OWNED;

			/* Okay. Attempt to claim with an atomic write back. */
			kring_desc_t before = kring_write_back( control, whead, desc, newval );
			if ( before != desc )
				goto retry;

			/* Write back okay. No reader claimed. We can use. */
			return whead;
		}

		/* FIXME: if we get back to where we started then bail */
	}
}

static inline void *kring_write_FIRST( struct kring_user *u )
{
	kring_off_t whead;

	/* Find the place to write to, skipping ahead as necessary. */
	whead = find_write_loc( u->control );

	/* Reserve the space. */
	u->control->head->wresv = whead;

	return kring_page_data( u, 0, whead );
}

static inline int writer_release( struct kring_control *control, kring_off_t whead )
{
	/* orig value. */
	kring_desc_t desc = kring_read_desc( control, whead );

	/* Unrelease writer. */
	kring_desc_t newval = desc & ~DSC_WRITER_OWNED;

	/* Write back with check. No other reader or writer should have altered the
	 * descriptor. */
	kring_desc_t before = kring_write_back( control, whead, desc, newval );
	if ( before != desc )
		return -1;

	return 0;
}

static inline void kring_write_SECOND( struct kring_user *u )
{
	/* Clear the writer owned bit from the buffer. */
	writer_release( u->control, u->control->head->wresv );

	/* Write back the write head, thereby releasing the buffer to writer. */
	u->control->head->whead = u->control->head->wresv;
}

static inline void kring_update_wresv( struct kring_user *u )
{
	int w;
	kring_off_t wresv, before, highest;

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

	before = kring_wresv_write_back( u->control, wresv, highest );
	if ( before != wresv )
		goto again;
}

static inline void kring_update_whead( struct kring_user *u )
{
	int w;
	kring_off_t orig, before, whead = 0, wbar = ~((kring_off_t)0);

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

	before = kring_whead_write_back( u->control, orig, whead );
	if ( before != orig )
		goto retry;
}

static inline void *kring_write_FIRST_2( struct kring_user *u )
{
	/* Start at wresv. There is nothing free before this value. We cannot start
	 * at whead because other writers may have written and release (not showing
	 * writer owned bits, but we cannot take.*/
	kring_off_t whead = u->control->head->wresv;

	/* Set the release barrier to the place where we start looking. We cannot
	 * release past this point. */
	u->control->writer[u->writer_id].wbar = whead;

	/* Find the place to write to, skipping ahead as necessary. */
	whead = find_write_loc( u->control );

	/* Private reserve. */
	u->control->writer[u->writer_id].wresv = whead;

	/* Update the common wreserve. */
	kring_update_wresv( u );

	return kring_page_data( u, 0, whead );
}

static inline void kring_write_SECOND_2( struct kring_user *u )
{
	/* Clear the writer owned bit from the buffer. */
	writer_release( u->control, u->control->head->wresv );

	/* Write back to the writer's private write head, which releases the buffer
	 * for this writer. */
	u->control->writer[u->writer_id].whead = u->control->writer[u->writer_id].wresv;

	/* Remove our release barrier. */
	u->control->writer[u->writer_id].wbar = 0;

	/* Maybe release to the readers. */
	kring_update_whead( u );
}


static inline int kring_prep_enter( struct kring_control *control, int reader_id )
{
	/* Init the read head. */
	kring_off_t rhead = control->head->whead;

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

