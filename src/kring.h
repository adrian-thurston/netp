#ifndef __KRING_H
#define __KRING_H

#if defined(__cplusplus)
extern "C" {
#endif

#define KRING 25
#define NPAGES 2048

#define PGOFF_CTRL 0
#define PGOFF_DATA 1

/*
 * Memap identity information. 
 */

/* Ring id (5), region to map (1) */

/* Must match region shift below. */
#define MAX_RINGS_PER_SET 32

#define MAX_WRITERS_PER_RING 32

#define PGOFF_ID_SHIFT 0
#define PGOFF_ID_MASK  0x1f

#define PGOFF_REGION_SHIFT 5
#define PGOFF_REGION_MASK  0x20

#define KR_RING_ID_ALL -1

/* MUST match system page size. */
#define KRING_PAGE_SIZE 4096

#define DSC_READER_SHIFT    2
#define DSC_WRITER_OWNED    0x01
#define DSC_SKIPPED         0x02
#define DSC_READER_OWNED    0xfc
#define DSC_READER_BIT(id)  ( 0x1 << ( DSC_READER_SHIFT + (id) ) )

#define KRING_ERR_SOCK       -1
#define KRING_ERR_MMAP       -2
#define KRING_ERR_BIND       -3
#define KRING_ERR_READER_ID  -4
#define KRING_ERR_RING_N     -5
#define KRING_ERR_ENTER      -6

/* Direction: from client, or from server. */
#define KRING_DIR_CLIENT 1
#define KRING_DIR_SERVER 2

#define KRING_DIR_INSIDE  1
#define KRING_DIR_OUTSIDE 2

#define KRING_NLEN 32
#define NRING_READERS 6

#define KR_OPT_READER_ID 1
#define KR_OPT_RING_N    2

/* Records an error in the user struct. Use before goto to function cleanup. */
#define kring_func_error( _ke, _ee ) \
	do { u->krerr = _ke; u->_errno = _ee; } while (0)

enum KRING_TYPE
{
	KRING_PACKETS = 1,
	KRING_DECRYPTED
};

enum KRING_MODE
{
	KRING_READ = 1,
	KRING_WRITE
};

typedef unsigned short shr_desc_t;
typedef unsigned long shr_off_t;

struct shared_writer
{
	shr_off_t whead;
	shr_off_t wresv;
};

struct shared_reader
{
	shr_off_t rhead;
	unsigned long skips;
	unsigned char entered;
};

struct shared_desc
{
	shr_desc_t desc;
};

struct page_desc
{
	struct page *p;
	void *m;
};

struct kring_page
{
	char d[KRING_PAGE_SIZE];
};

#define KRING_CTRL_SZ ( \
	sizeof(struct shared_writer) + \
	sizeof(struct shared_reader) * NRING_READERS + \
	sizeof(struct shared_desc) * NPAGES \
)

#define KRING_DATA_SZ KRING_PAGE_SIZE * NPAGES

struct kring_control
{
	struct shared_writer *writer;
	struct shared_reader *reader;
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
	int N;
	int reader_id;
	enum KRING_MODE mode;

	struct kring_control *control;
	struct kring_data *data;

	int krerr;
	int _errno;
	char *errstr;
};

struct kring_addr
{
	char name[KRING_NLEN];
	enum KRING_MODE mode;
	int ring_id;
};

int kring_open( struct kring_user *u, enum KRING_TYPE type, const char *ringset, int rid, enum KRING_MODE mode );

int kring_write_decrypted( struct kring_user *u, int type, const char *remoteHost, char *data, int len );
char *kring_error( struct kring_user *u, int err );

inline unsigned long kring_skips( struct kring_user *u )
{
	unsigned long skips = 0;
	if ( u->ring_id != KR_RING_ID_ALL )
		skips = u->control->reader[u->reader_id].skips;
	else {
		int ring;
		for ( ring = 0; ring < u->N; ring++ )
			skips += u->control[ring].reader[u->reader_id].skips;
	}
	return skips;
}

struct kring_packet
{
	char dir;
	int len;
	int caplen;
	unsigned char *bytes;
};

struct kring_decrypted
{
	 unsigned char type;
	 char *host;
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
	char type;
	char host[63];
};

inline int kring_avail_impl( struct kring_control *control, int reader_id )
{
	return ( control->reader[reader_id].rhead != control->writer->whead );
}

static inline shr_desc_t kring_write_back( struct kring_control *control,
		shr_off_t off, shr_desc_t oldval, shr_desc_t newval )
{
	return __sync_val_compare_and_swap( &control->descriptor[off].desc, oldval, newval );
}

inline int kring_avail( struct kring_user *u )
{
	if ( u->ring_id != KR_RING_ID_ALL )
		return kring_avail_impl( u->control, u->reader_id );
	else {
		int ctrl;
		for ( ctrl = 0; ctrl < u->N; ctrl++ ) {
			if ( kring_avail_impl( &u->control[ctrl], u->reader_id ) )
				return 1;
		}
		return 0;
	}
}

inline shr_off_t kring_next( shr_off_t off )
{
	off += 1;
	if ( off >= NPAGES )
		off = 0;
	return off;
}

inline shr_off_t kring_prev( shr_off_t off )
{
	if ( off == 0 )
		return NPAGES - 1;
	return off - 1;
}

inline shr_off_t kring_advance_rhead( struct kring_user *u, int ctrl, shr_off_t rhead )
{
	shr_desc_t desc;
	while ( 1 ) {
		rhead = kring_next( rhead );

		/* reserve next. */
		desc = u->control[ctrl].descriptor[rhead].desc;
		if ( ! ( desc & DSC_WRITER_OWNED ) ) {
			/* Okay we can take it. */
			shr_desc_t newval = desc | DSC_READER_BIT( u->reader_id );
		
			/* Attemp write back. */
			shr_desc_t before = kring_write_back( &u->control[ctrl], rhead, desc, newval );
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
inline void kring_reader_release( struct kring_user *u, int ctrl, shr_off_t prev )
{
	shr_desc_t before, desc, newval;
again:
	/* Take a copy, modify, then try to write back. */
	desc = u->control[ctrl].descriptor[prev].desc;
	
	newval = desc & ~( DSC_READER_BIT( u->reader_id ) );

	/* Was it skipped? */
	if ( desc & DSC_SKIPPED ) {
		/* If we are the last to release it, then reset the skipped bit. */
		if ( ! ( newval & DSC_READER_OWNED ) )
			newval &= ~DSC_SKIPPED;
	}

	before = kring_write_back( &u->control[ctrl], prev, desc, newval );
	if ( before != desc )
		goto again;
}

inline void *kring_data( struct kring_user *u, int ctrl )
{
	return ( u->data[ctrl].page + u->control[ctrl].reader[u->reader_id].rhead );
}

static int kring_select_ctrl( struct kring_user *u )
{
	if ( u->ring_id != KR_RING_ID_ALL )
		return 0;
	else {
		int ctrl;
		for ( ctrl = 0; ctrl < u->N; ctrl++ ) {
			if ( kring_avail_impl( &u->control[ctrl], u->reader_id ) )
				return ctrl;
		}
		return -1;
	}
}

inline void kring_next_packet( struct kring_user *u, struct kring_packet *packet )
{
	struct kring_packet_header *h;
	unsigned char *bytes;

	int ctrl = kring_select_ctrl( u );

	shr_off_t prev = u->control[ctrl].reader[u->reader_id].rhead;
	shr_off_t rhead = prev;

	rhead = kring_advance_rhead( u, ctrl, rhead );

	/* Set the rheadset rhead. */
	u->control[ctrl].reader[u->reader_id].rhead = rhead;

	/* Release the previous only if we have entered with a successful read. */
	if ( u->control[ctrl].reader[u->reader_id].entered )
		kring_reader_release( u, ctrl, prev );

	/* Indicate we have entered. */
	u->control[ctrl].reader[u->reader_id].entered = 1;

	h = (struct kring_packet_header*)kring_data( u, ctrl );
	bytes = (unsigned char*)( h + 1 );

	packet->len = h->len;
	packet->caplen = h->len;
	packet->dir = h->dir;
	packet->bytes = bytes;
}

inline void kring_next_decrypted( struct kring_user *u, struct kring_decrypted *decrypted )
{
	struct kring_decrypted_header *h;
	unsigned char *bytes;

	int ctrl = kring_select_ctrl( u );

	shr_off_t prev = u->control[ctrl].reader[u->reader_id].rhead;
	shr_off_t rhead = prev;

	rhead = kring_advance_rhead( u, ctrl, rhead );

	/* Set the rheadset rhead. */
	u->control[ctrl].reader[u->reader_id].rhead = rhead;
	
	/* Release the previous only if we have entered with a successful read. */
	if ( u->control[ctrl].reader[u->reader_id].entered )
		kring_reader_release( u, ctrl, prev );

	/* Indicate we have entered. */
	u->control[ctrl].reader[u->reader_id].entered = 1;

	h = (struct kring_decrypted_header*)kring_data( u, ctrl );
	bytes = (unsigned char*)( h + 1 );

	decrypted->len = h->len;
	decrypted->type = h->type;
	decrypted->host = h->host;
	decrypted->bytes = bytes;
}


inline unsigned long kring_one_back( unsigned long pos )
{
	return pos == 0 ? NPAGES - 1 : pos - 1;
}

inline unsigned long kring_one_forward( unsigned long pos )
{
	pos += 1;
	return pos == NPAGES ? 0 : pos;
}

inline unsigned long find_write_loc( struct kring_control *control )
{
	int id;
	shr_desc_t desc = 0;
	shr_off_t whead = control->writer->whead;
	while ( 1 ) {
		/* Move to the next slot. */
		whead = kring_next( whead );

retry:
		/* Read the descriptor. */
		desc = control->descriptor[whead].desc;

		/* Check, if not okay, go on to next. */
		if ( desc & DSC_READER_OWNED || desc & DSC_SKIPPED ) {
			shr_desc_t before;

			/* register skips. */
			for ( id = 0; id < NRING_READERS; id++ ) {
				if ( desc & DSC_READER_BIT( id ) ) {
					/* reader id present. */
					control->reader[id].skips += 1;
				}
			}

			/* Mark as skipped. If a reader got in before us, retry. */
			before = kring_write_back( control, whead, desc, desc | DSC_SKIPPED );
			if ( before != desc )
				goto retry;
		}
		else if ( desc & DSC_WRITER_OWNED ) {
			/* Unusual situation. */
		}
		else {
			/* Available. */
			shr_desc_t newval = desc | DSC_WRITER_OWNED;

			/* Okay. Attempt to claim with an atomic write back. */
			shr_desc_t before = kring_write_back( control, whead, desc, newval );
			if ( before != desc )
				goto retry;

			/* Write back okay. No reader claimed. We can use. */
			return whead;
		}

		/* FIXME: if we get back to where we started then bail */
	}
}

inline int writer_release( struct kring_control *control, shr_off_t whead )
{
	/* orig value. */
	shr_desc_t desc = control->descriptor[whead].desc;

	/* Unrelease writer. */
	shr_desc_t newval = desc & ~DSC_WRITER_OWNED;

	/* Write back with check. No other reader or writer should have altered the
	 * descriptor. */
	shr_desc_t before = kring_write_back( control, whead, desc, newval );
	if ( before != desc )
		return -1;

	return 0;
}

inline int kring_prep_enter( struct kring_user *u, int ctrl )
{
	/* Init the read head. */
	shr_off_t rhead = u->control[ctrl].writer->whead;

	/* Okay good. */
	u->control[ctrl].reader[u->reader_id].rhead = rhead; 
	u->control[ctrl].reader[u->reader_id].skips = 0;
	u->control[ctrl].reader[u->reader_id].entered = 0;

	return 0;
}

#if defined(__cplusplus)
}
#endif

#endif
