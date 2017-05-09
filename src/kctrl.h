#ifndef __KCTRL_H
#define __KCTRL_H


/*
 * Two Part:
 *  1. Allocate buffers using desctriptors to claim, skipping over long-held
 *      buffers.
 *
 *  2. Atomic append to linked link list of messages.
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
	kctrl_off_t head;
	kctrl_off_t tail;
	kctrl_off_t free;
};

struct kctrl_shared_writer
{
	kctrl_off_t whead;
};

struct kctrl_shared_reader
{
	char unused;
};

struct kctrl_shared_desc
{
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

char *kctrl_error( struct kctrl_user *u, int err );

static inline void *kctrl_page_data( struct kctrl_user *u, int ctrl, kctrl_off_t off )
{
	if ( u->socket < 0 )
		return u->pd[KCTRL_INDEX(off)].m;
	else
		return u->data[ctrl].page + KCTRL_INDEX(off);
}

static inline int kctrl_avail( struct kctrl_user *u )
{
	struct kctrl_control *control = u->control;

	if ( control->descriptor[ KCTRL_INDEX(control->head->head) ].next != 0 )
		return 1;

	return 0;

}

/* Return the block to the free list. */
static inline void kctrl_push_to_free_list( struct kctrl_user *u, kctrl_off_t head )
{
	kctrl_off_t before, free;

again:
	free = u->control->head->free;

	u->control->descriptor[KCTRL_INDEX(head)].next = free;

	before = __sync_val_compare_and_swap(
			&u->control->head->free, free, head );

	if ( before != free )
		goto again;
}

static inline void *kctrl_next_generic( struct kctrl_user *u )
{
	int ctrl = 0;
	kctrl_off_t head;

	head = u->control->head->head;

	u->control->head->head = u->control->descriptor[ KCTRL_INDEX(u->control->head->head) ].next;

	kctrl_push_to_free_list( u, head );

	return kctrl_page_data( u, ctrl, u->control->head->head );
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

static inline void *kctrl_write_FIRST( struct kctrl_user *u )
{
	volatile kctrl_off_t before, next, free;

again:
	free = u->control->head->free;

	if ( free == 0 )
		goto again;

	next = u->control->descriptor[free].next;
	
	before = __sync_val_compare_and_swap(
			&u->control->head->free, free, next );
	
	if ( before != free ) 
		goto again;

	u->control->writer[u->writer_id].whead = free;

	u->control->descriptor[KCTRL_INDEX(free)].next = 0;

	return kctrl_page_data( u, 0, free );
}

static inline void kctrl_write_SECOND( struct kctrl_user *u )
{
	volatile kctrl_off_t tail, before;

again:
	/* Move forward to the true tail. */
	tail = u->control->head->tail;

	before = __sync_val_compare_and_swap(
			&u->control->head->tail, tail,
			u->control->writer[u->writer_id].whead );
	
	if ( before != tail )
		goto again;
	
	u->control->descriptor[KCTRL_INDEX(tail)].next = u->control->writer[u->writer_id].whead;
}

static inline int kctrl_prep_enter( struct kctrl_control *control, int reader_id )
{
	return 0;
}

#if defined(__cplusplus)
}
#endif

#endif
