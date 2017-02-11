#ifndef __KRING_H
#define __KRING_H

#if defined(__cplusplus)
extern "C" {
#endif

#define KRING 25
#define NPAGES 2048
#define PGOFF_CTRL 1
#define PGOFF_DATA 2
#define KRING_PAGE_SIZE 4096

#define DSC_WRITER_OWNED    0x1
#define DSC_READER_OWNED    0x2
#define DSC_EITHER_OWNED    0x3

#define DSC_READER_SHIFT    2

#define KRING_ERR_SOCK -1
#define KRING_ERR_MMAP -2

/* Direction: from client, or from server. */
#define KRING_DIR_CLIENT 1
#define KRING_DIR_SERVER 2

#define KRING_DIR_INSIDE  1
#define KRING_DIR_OUTSIDE 2

enum KRING_TYPE
{
	KRING_PACKETS = 1,
	KRING_DECRYPTED
};

typedef unsigned short shr_desc_t;
typedef unsigned long shr_off_t;

struct shared_ctrl
{
	shr_off_t whead;
	shr_off_t wresv;
	shr_off_t rhead;
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

#define KRING_CTRL_SZ sizeof(struct shared_ctrl) + sizeof(struct shared_desc) * NPAGES

#define KRING_DATA_SZ KRING_PAGE_SIZE * NPAGES


struct kring_shared
{
	struct shared_desc *descriptors;
	struct shared_ctrl *control;
};

struct kring_user
{
	int socket;

	struct kring_shared shared;
	struct kring_page *g;
	int _errno;
	char *errstr;
};

int kring_open( struct kring_user *u, const char *ring, enum KRING_TYPE type );

int kring_write_decrypted( struct kring_user *u, int type, const char *remoteHost, char *data, int len );
char *kring_error( struct kring_user *u, int err );

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

inline int kring_avail_impl( struct kring_shared *shared )
{
	return ( shared->control->rhead != shared->control->whead );
}

inline int kring_avail( struct kring_user *u )
{
	return kring_avail_impl( &u->shared );
}

inline shr_off_t kring_next( shr_off_t off )
{
	/* Next. */
	off += 1;
	if ( off  >= NPAGES )
		off = 0;
	return off;
}

inline void kring_next_packet( struct kring_user *u, struct kring_packet *packet )
{
	struct kring_packet_header *h;
	unsigned char *bytes;

	shr_off_t rhead = u->shared.control->rhead;
	shr_off_t prev = u->shared.control->rhead;
	shr_desc_t desc;
	while ( 1 ) {
		rhead = kring_next( rhead );

		/* reserve next. */
		desc = u->shared.descriptors[rhead].desc;
		if ( ! ( desc & DSC_WRITER_OWNED ) ) {
			/* Okay we can take it. */
			shr_desc_t newval = desc | DSC_READER_OWNED;
		
			/* Attemp write back. */
			shr_desc_t before = __sync_val_compare_and_swap( &u->shared.descriptors[rhead].desc, desc, newval );
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

	/* Set the rheadset rhead. */
	u->shared.control->rhead = rhead;
	
	/* Unreserve prev. */
	u->shared.descriptors[prev].desc &= ~DSC_READER_OWNED;

	h = (struct kring_packet_header*)( u->g + u->shared.control->rhead );
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

	shr_off_t rhead = u->shared.control->rhead;
	shr_off_t prev = u->shared.control->rhead;
	shr_desc_t desc;
	while ( 1 ) {
		rhead = kring_next( rhead );

		/* reserve next. */
		desc = u->shared.descriptors[rhead].desc;
		if ( ! ( desc & DSC_WRITER_OWNED ) ) {
			/* Okay we can take it. */
			shr_desc_t newval = desc | DSC_READER_OWNED;
		
			/* Attemp write back. */
			shr_desc_t before = __sync_val_compare_and_swap( &u->shared.descriptors[rhead].desc, desc, newval );
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

	/* Set the rheadset rhead. */
	u->shared.control->rhead = rhead;
	
	/* Unreserve prev. */
	u->shared.descriptors[prev].desc &= ~DSC_READER_OWNED;

	h = (struct kring_decrypted_header*)( u->g + u->shared.control->rhead );
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

inline unsigned long find_write_loc( struct kring_shared *shared )
{
	int skips = 0;
	shr_desc_t desc = 0;
	shr_off_t whead = shared->control->whead;
	while ( 1 ) {
		/* Move to the next slot. */
		whead = kring_next( whead );

		/* Read the descriptor. */
		desc = shared->descriptors[whead].desc;

		/* Check, if not okay, go on to next. */
		if ( ! ( desc & DSC_EITHER_OWNED ) ) {
			shr_desc_t newval = desc | DSC_WRITER_OWNED;

			/* Okay. Attempt to claim with an atomic write back. */
			shr_desc_t before = __sync_val_compare_and_swap( &shared->descriptors[whead].desc, desc, newval );
			if ( before == desc ) {
				/* Write back okay. We can use. */
				return whead;
			}
		}

		/* if skips == size, bail out. */
		skips += 1;
	}
}

inline int writer_release( struct kring_shared *shared, shr_off_t whead )
{
	/* orig value. */
	shr_desc_t desc = shared->descriptors[whead].desc;

	/* Unrelease writer. */
	shr_desc_t newval = desc & ~DSC_WRITER_OWNED;

	/* Write back with check. No other reader or writer should have altered the
	 * descriptor. */
	shr_desc_t before = __sync_val_compare_and_swap( &shared->descriptors[whead].desc, desc, newval );
	if ( before != desc )
		return -1;

	return 0;
}


#if defined(__cplusplus)
}
#endif

#endif
