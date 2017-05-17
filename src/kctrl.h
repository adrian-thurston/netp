#ifndef __KCTRL_H
#define __KCTRL_H

/*
 * Three Part:
 *  1. Writer allocates buffers using a free list.
 *
 *  2. Writer atomically pushes to stack of messages.
 *
 *  3. Reader reverses stack before reading
 */

#if defined(__cplusplus)
extern "C" {
#endif

#include "krdep.h"

#define KCTRL 20
#define KCTRL_NPAGES 2048

#define KCTRL_INDEX(off) ((off) & 0x7ff)

#define KRING_PGOFF_CTRL 0
#define KRING_PGOFF_DATA 1

#define KCTRL_NULL KCTRL_NPAGES

/*
 * Memap identity information. 
 */

/* Must match region shift below. */
#define KCTRL_MAX_RINGS_PER_SET 32

#define KCTRL_PGOFF_ID_SHIFT 0
#define KCTRL_PGOFF_ID_MASK  0x1f

#define KCTRL_PGOFF_REGION_SHIFT 5
#define KCTRL_PGOFF_REGION_MASK  0x20

#define KCTRL_RING_ID_ALL -1

/* Direction: from client, or from server. */
#define KCTRL_DIR_CLIENT 1
#define KCTRL_DIR_SERVER 2

#define KCTRL_DIR_INSIDE  1
#define KCTRL_DIR_OUTSIDE 2

#define KCTRL_NLEN 32
#define KCTRL_WRITERS 6
#define KCTRL_READERS 1

#define KR_OPT_WRITER_ID 1
#define KR_OPT_READER_ID 2
#define KR_OPT_RING_N    3

/* Records an error in the user struct. Use before goto to function cleanup. */
#define kctrl_func_error( _ke, _ee ) \
	do { u->krerr = _ke; u->_errno = _ee; } while (0)

typedef unsigned short kctrl_desc_t;
typedef unsigned long kctrl_off_t;

struct kctrl_shared_head
{
	kctrl_off_t head;
	kctrl_off_t tail;
	kctrl_off_t free;
	kctrl_off_t stack;
};

struct kctrl_shared_writer
{
	kctrl_off_t wloc;
};

struct kctrl_shared_reader
{
	char unused;
};

struct kctrl_shared_desc
{
	kctrl_off_t next;
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

#define KCTRL_DATA_SZ \
	( KRING_PAGE_SIZE * KCTRL_NPAGES )

struct kctrl_control
{
	struct kctrl_shared_head *head;
	struct kctrl_shared_writer *writer;
	struct kctrl_shared_reader *reader;
	struct kctrl_shared_desc *descriptor;
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

int kctrl_open( struct kring_user *u, enum KRING_TYPE type, const char *ringset, enum KRING_MODE mode );
int kctrl_write_decrypted( struct kring_user *u, long id, int type, const char *remoteHost, char *data, int len );
int kctrl_write_plain( struct kring_user *u, char *data, int len );
int kctrl_read_wait( struct kring_user *u );
void kctrl_next_packet( struct kring_user *u, struct kctrl_packet *packet );
void kctrl_next_decrypted( struct kring_user *u, struct kctrl_decrypted *decrypted );
void kctrl_next_plain( struct kring_user *u, struct kctrl_plain *plain );

static inline int kctrl_packet_max_data(void)
{
	return KRING_PAGE_SIZE - sizeof(struct kctrl_packet_header);
}

static inline int kctrl_decrypted_max_data(void)
{
	return KRING_PAGE_SIZE - sizeof(struct kctrl_decrypted_header);
}

static inline int kctrl_plain_max_data(void)
{
	return KRING_PAGE_SIZE - sizeof(struct kctrl_plain_header);
}

char *kctrl_error( struct kring_user *u, int err );

static inline void *kctrl_page_data( struct kring_user *u, kctrl_off_t off )
{
	if ( u->socket < 0 )
		return u->pd[KCTRL_INDEX(off)].m;
	else
		return u->data->page + KCTRL_INDEX(off);
}

static inline int kctrl_avail_impl( struct kctrl_control *control )
{
	if ( control->descriptor[ KCTRL_INDEX(control->head->head) ].next != KCTRL_NULL ||
			control->descriptor[ KCTRL_INDEX(control->head->stack) ].next != KCTRL_NULL )
		return 1;

	return 0;
}

static inline struct kctrl_control *kctrl_control( struct kring_control *control )
{
	return (struct kctrl_control*) control;
}

static inline int kctrl_avail( struct kring_user *u )
{
	return kctrl_avail_impl( kctrl_control( u->control ) );
}

/* Return the block to the free list. */
static inline void kctrl_push_to_free_list( struct kring_user *u, kctrl_off_t head )
{
	kctrl_off_t before, free;

again:
	free = kctrl_control(u->control)->head->free;

	kctrl_control(u->control)->descriptor[KCTRL_INDEX(head)].next = free;

	before = __sync_val_compare_and_swap(
			&kctrl_control(u->control)->head->free, free, head );

	if ( before != free )
		goto again;
}

static inline void kctrl_reverse_stack( struct kring_user *u )
{
	kctrl_off_t prev, next, stack;

	/* Go forward from stack, reversing. */
	prev = KCTRL_NULL;
	stack = kctrl_control(u->control)->head->stack;
	while ( stack != KCTRL_NULL ) {
		next = kctrl_control(u->control)->descriptor[ KCTRL_INDEX(stack) ].next;

		kctrl_control(u->control)->descriptor[ KCTRL_INDEX(stack) ].next = prev;

		prev = stack;
		stack = next;
	}
}

static inline void *kctrl_next_generic( struct kring_user *u )
{
	kctrl_off_t head;

	kctrl_reverse_stack( u );

	/* Pull one off the head. */
	head = kctrl_control(u->control)->head->head;

	kctrl_control(u->control)->head->head = kctrl_control(u->control)->descriptor[ KCTRL_INDEX(kctrl_control(u->control)->head->head) ].next;

	/* Free the node we pulled off. */
	kctrl_push_to_free_list( u, head );

	/* Return the new head. */
	return kctrl_page_data( u, kctrl_control(u->control)->head->head );
}

static inline kctrl_off_t kctrl_allocate( struct kring_user *u )
{
	volatile kctrl_off_t before, next, free;

again:
	/* Read the free pointer. If nothing avail then spin. */
	free = kctrl_control(u->control)->head->free;
	if ( free == KCTRL_NULL )
		return KCTRL_NULL;

	next = kctrl_control(u->control)->descriptor[free].next;
	
	/* Attempt to rewrite the pointer to next. */
	before = __sync_val_compare_and_swap(
			&kctrl_control(u->control)->head->free, free, next );
	
	/* Try again if some other thread wrote first. */
	if ( before != free ) 
		goto again;

	/* Success. Record where we are writing to. Clear the item's next pointer. */
	kctrl_control(u->control)->writer[u->writer_id].wloc = free;

	kctrl_control(u->control)->descriptor[KCTRL_INDEX(free)].next = KCTRL_NULL;

#ifndef KERN
	// printf("allocated: %lu\n", free );
#endif

	return free;
}

/* Returns NULL if there are no free buffers in the ring. */
static inline void *kctrl_write_FIRST( struct kring_user *u )
{
	kctrl_off_t free = kctrl_allocate( u );

	if ( free == KCTRL_NULL )
		return 0;

	return kctrl_page_data( u, free );
}

static inline void kctrl_push_new( struct kring_user *u )
{
	volatile kctrl_off_t stack, before;

again:
	/* Move forward to the true tail. */
	stack = kctrl_control(u->control)->head->stack;

	kctrl_control(u->control)->descriptor[KCTRL_INDEX(kctrl_control(u->control)->writer[u->writer_id].wloc)].next = stack;

	before = __sync_val_compare_and_swap(
			&kctrl_control(u->control)->head->stack, stack,
			kctrl_control(u->control)->writer[u->writer_id].wloc );
	
	if ( before != stack )
		goto again;
}

static inline void kctrl_write_SECOND( struct kring_user *u )
{
	kctrl_push_new( u );
}

static inline int kctrl_prep_enter( struct kctrl_control *control, int reader_id )
{
	return 0;
}

#if defined(__cplusplus)
}
#endif

#endif
