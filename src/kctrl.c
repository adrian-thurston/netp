#include "attribute.h"
#include <linux/kobject.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/cacheflush.h>

#include "krkern.h"

static int kctrl_sock_release( struct socket *sock );
static int kctrl_sock_create( struct net *net, struct socket *sock, int protocol, int kern );
static int kctrl_sock_mmap( struct file *file, struct socket *sock, struct vm_area_struct *vma );
static int kctrl_bind( struct socket *sock, struct sockaddr *sa, int addr_len);
static unsigned int kctrl_poll(struct file *file, struct socket *sock, poll_table * wait);
static int kctrl_setsockopt( struct socket *sock, int level, int optname, char __user * optval, unsigned int optlen);
static int kctrl_getsockopt( struct socket *sock, int level, int optname, char __user *optval, int __user *optlen);
static int kctrl_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg);
static int kctrl_recvmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len, int flags );
static int kctrl_sendmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len );

static struct proto_ops kctrl_ops = {
	.family = KCTRL,
	.owner = THIS_MODULE,

	.release = kctrl_sock_release,
	.bind = kctrl_bind,
	.mmap = kctrl_sock_mmap,
	.poll = kctrl_poll,
	.setsockopt = kctrl_setsockopt,
	.getsockopt = kctrl_getsockopt,
	.ioctl = kctrl_ioctl,
	.recvmsg = kctrl_recvmsg,
	.sendmsg = kctrl_sendmsg,

	/* Not used. */
	.connect = sock_no_connect,
	.socketpair = sock_no_socketpair,
	.accept = sock_no_accept,
	.getname = sock_no_getname,
	.listen = sock_no_listen,
	.shutdown = sock_no_shutdown,
	.sendpage = sock_no_sendpage,
};

struct kctrl_sock
{
	struct sock sk;
	struct kctrl_ringset *ringset;
	int ring_id;
	enum KCTRL_MODE mode;
	int writer_id;
	int reader_id;
};

static void kctrl_copy_name( char *dest, const char *src )
{
	strncpy( dest, src, KCTRL_NLEN );
	dest[KCTRL_NLEN-1] = 0;
}

static inline struct kctrl_sock *kctrl_sk( const struct sock *sk )
{
	return container_of( sk, struct kctrl_sock, sk );
}

static struct proto kctrl_proto = {
	.name = "KCTRL",
	.owner = THIS_MODULE,
	.obj_size = sizeof(struct kctrl_sock),
};

static struct net_proto_family kctrl_family_ops = {
	.family = KCTRL,
	.create = kctrl_sock_create,
	.owner = THIS_MODULE,
};


static int kctrl_sock_release( struct socket *sock )
{
	struct sock *sk = sock->sk;

	if ( !sk )
		return 0;

	printk( "kctrl_sock_release\n" );

	sock->sk = NULL;
	sock_put( sk );
	return 0;
}

static void decon_pgoff( unsigned long pgoff, unsigned long *rid, unsigned long *region )
{
	*rid = ( pgoff & KCTRL_PGOFF_ID_MASK ) >> KCTRL_PGOFF_ID_SHIFT;
	*region = ( pgoff & KCTRL_PGOFF_REGION_MASK ) >> KCTRL_PGOFF_REGION_SHIFT;
}

static int kctrl_sock_mmap( struct file *file, struct socket *sock, struct vm_area_struct *vma )
{
	struct kctrl_sock *krs = kctrl_sk( sock->sk );
	struct kctrl_ringset *r = krs->ringset;
	unsigned long ring_id, region;

	/* Ensure bound. */
	if ( r == 0 ) {
		printk( "kring mmap: socket not bound to ring\n" );
		return -EINVAL;
	}
	
	decon_pgoff( vma->vm_pgoff, &ring_id, &region );

	if ( ring_id > r->nrings ) {
		printk( "kring mmap: error: rid > r->nrings\n" );
		return -EINVAL;
	}

	switch ( region  ) {
		case KCTRL_PGOFF_CTRL: {
			printk( "mapping control region %p of ring %p-%lu\n", r->ring[ring_id].ctrl, r, ring_id );
			remap_vmalloc_range( vma, r->ring[ring_id].ctrl, 0 );
			break;
		}

		case KCTRL_PGOFF_DATA: {
			int i;
			unsigned long uaddr = vma->vm_start;
			printk( "mapping data region %lu of ring %p-%lu\n", uaddr, r, ring_id );
			for ( i = 0; i < KCTRL_NPAGES; i++ ) {
				vm_insert_page( vma, uaddr, r->ring[ring_id].pd[i].p );
				uaddr += PAGE_SIZE;
			}

			break;
		}
	}

	return 0;
}

static int validate_ring_name( const char *name )
{
	const char *p = name;
	const char *pe = name + KCTRL_NLEN;
	while ( 1 ) {
		/* Reached the end without validation. */
		if ( p == pe )
			return -1;

		/* Got to a valid string end. Finish. */
		if ( *p == 0 )
			return 0;

		if ( ! ( ( 'A' <= *p && *p <= 'Z' ) ||
				( 'a' <= *p && *p <= 'z' ) ||
				( '0' <= *p && *p <= '9' ) ||
				*p == '_' || *p == '-' || 
				*p == '.' ) )
		{
			/* Invalid char */
			return -1;
		}

		/* Okay, next char. */
		p += 1;
	}
}

static int kctrl_allocate_reader_on_ring( struct kctrl_ring *ring )
{
	int reader_id;

	/* Search for a reader id that is free on the ring requested. */
	for ( reader_id = 0; reader_id < KCTRL_READERS; reader_id++ ) {
		if ( !ring->reader[reader_id].allocated )
			break;
	}

	if ( reader_id == KCTRL_READERS ) {
		/* No valid id found */
		return -EINVAL;
	}

	/* All okay. */
	ring->reader[reader_id].allocated = true;
	ring->num_readers += 1;

	return reader_id;
}

static int kctrl_allocate_reader_all_rings( struct kctrl_ringset *ringset )
{
	int i, reader_id;

	/* Search for a single reader id that is free across all the rings requested. */
	for ( reader_id = 0; reader_id < KCTRL_READERS; reader_id++ ) {
		for ( i = 0; i < ringset->nrings; i++ ) {
			if ( ringset->ring[i].reader[reader_id].allocated )
				goto next_id;
		}

		/* Got through all the rings, break with a valid id. */
		goto good;

		next_id: {}
	}

	/* No valid id found (exited id loop). */
	return -EINVAL;

	/* All okay. */
	good: {}
	
	/* Allocate reader ids, increas reader count. */
	for ( i = 0; i < ringset->nrings; i++ ) {
		ringset->ring[i].reader[reader_id].allocated = true;
		ringset->ring[i].num_readers += 1;
	}
	
	return reader_id;
}

static int kctrl_allocate_writer_on_ring( struct kctrl_ring *ring )
{
	int writer_id;
	bool orig;

again:

	/* Search for a writer id that is free on the ring requested. */
	for ( writer_id = 0; writer_id < KCTRL_WRITERS; writer_id++ ) {
		if ( !ring->writer[writer_id].allocated )
			break;
	}

	if ( writer_id == KCTRL_WRITERS ) {
		/* No valid id found */
		return -EINVAL;
	}

	/* All okay. */
	orig = __sync_val_compare_and_swap( &ring->writer[writer_id].allocated, 0, 1 );
	if ( orig != 0 )
		goto again;

	ring->num_writers += 1;

	return writer_id;
}

static int kctrl_bind( struct socket *sock, struct sockaddr *sa, int addr_len )
{
	int reader_id = -1, writer_id = -1;
	struct kctrl_addr *addr = (struct kctrl_addr*)sa;
	struct kctrl_sock *krs;
	struct kctrl_ringset *ringset;

	if ( addr_len != sizeof(struct kctrl_addr) ) {
		printk("kctrl_bind: addr_len wrong size\n");
		return -EINVAL;
	}

	if ( validate_ring_name( addr->name ) < 0 ) {
		printk( "kctrl_bind: bad ring name\n" );
		return -EINVAL;
	}
	
	if ( addr->mode != KCTRL_READ && addr->mode != KCTRL_WRITE ) {
		printk( "kctrl_bind: bad mode, not read or write\n" );
		return -EINVAL;
	}
	
	ringset = kctrl_find_ring( addr->name );
	if ( ringset == 0 ) {
		printk( "kctrl_bind: bad mode, not read or write\n" );
		return -EINVAL;
	}

	if ( addr->ring_id != KCTRL_RING_ID_ALL &&
			( addr->ring_id < 0 || addr->ring_id >= ringset->nrings ) )
	{
		printk( "kctrl_bind: bad ring id %d\n", addr->ring_id );
		return -EINVAL;
	}

	/* Cannot write to all rings. */
	if ( addr->mode == KCTRL_WRITE && addr->ring_id == KCTRL_RING_ID_ALL ) {
		printk( "kctrl_bind: cannot write to ring id ALL\n" );
		return -EINVAL;
	}

	krs = kctrl_sk( sock->sk );

	if ( addr->mode == KCTRL_WRITE ) {
		/* Find a writer ID. */
		writer_id = kctrl_allocate_writer_on_ring( &ringset->ring[addr->ring_id] );
		if ( writer_id < 0 )
			return writer_id;
	}
	else if ( addr->mode == KCTRL_READ ) {
		/* Find a reader ID. */
		if ( addr->ring_id != KCTRL_RING_ID_ALL ) {
			/* Reader ID for ring specified. */
			reader_id = kctrl_allocate_reader_on_ring( &ringset->ring[addr->ring_id] );
		}
		else {
			/* Find a reader ID that works for all rings. This can fail. */
			reader_id = kctrl_allocate_reader_all_rings( ringset );
		}

		if ( reader_id < 0 )
			return reader_id;

	}

	krs->ringset = ringset;
	krs->ring_id = addr->ring_id;
	krs->mode = addr->mode;
	krs->writer_id = writer_id;
	krs->reader_id = reader_id;

	return 0;
}

unsigned int kctrl_poll(struct file *file, struct socket *sock, poll_table * wait)
{
	printk( "kctrl_poll\n" );
	return 0;
}

static int kctrl_setsockopt(struct socket *sock, int level, int optname, char __user * optval, unsigned int optlen)
{
	printk( "kctrl_setsockopt\n" );
	return 0;
}

static int kctrl_getsockopt( struct socket *sock, int level, int optname, char __user *optval, int __user *optlen )
{
	int len;
	int val, lv = sizeof(val);
	void *data = &val;
	struct kctrl_sock *krs = kctrl_sk( sock->sk );

	if ( level != SOL_PACKET )
		return -ENOPROTOOPT;

	if ( get_user(len, optlen) )
		return -EFAULT;

	if ( len != lv )
		return -EINVAL;

	printk( "kctrl_getsockopt\n" );

	switch ( optname ) {
		case KR_OPT_RING_N:
			val = krs->ringset->nrings;
			break;
		case KR_OPT_READER_ID:
			val = krs->reader_id;
			break;
		case KR_OPT_WRITER_ID:
			val = krs->writer_id;
			break;
	}

	if ( put_user( len, optlen ) )
		return -EFAULT;

	if ( copy_to_user( optval, data, len) )
		return -EFAULT;
	return 0;
}

static int kctrl_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	printk( "kctrl_ioctl\n" );
	return 0;
}

static int kctrl_kern_avail( struct kctrl_ringset *r, struct kctrl_sock *krs )
{
	struct kctrl_control *control = &r->ring[krs->ring_id].control;

	if ( control->descriptor[ control->head->head ].next > control->head->head )
		return 1;
	return 0;
}

/* Waiting writers go to sleep with the recvmsg system call. */
static int kctrl_recvmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len, int flags )
{
	struct kctrl_sock *krs = kctrl_sk( sock->sk );
	struct kctrl_ringset *r = krs->ringset;
	sigset_t blocked, oldset;
	int ret;
	wait_queue_head_t *wq;

	/* Allow kill, stop and the user sigs. This assumes we are operating under
	 * the genf program framework where we want to atomically unmask the user
	 * signals so we can receive genf inter-thread messages. */
	siginitsetinv( &blocked, sigmask(SIGKILL) | sigmask(SIGSTOP) |
			sigmask(SIGUSR1) | sigmask(SIGUSR2) );
	sigprocmask( SIG_SETMASK, &blocked, &oldset );

	// wq = krs->ring_id == KR_RING_ID_ALL ? &r->reader_waitqueue 
	// : &r->ring[krs->ring_id].reader_waitqueue;

	wq = &r->reader_waitqueue;

	ret = wait_event_interruptible( *wq, kctrl_kern_avail( r, krs ) );

	if ( ret == -ERESTARTSYS ) {
		/* Interrupted by a signal. */
		memcpy(&current->saved_sigmask, &oldset, sizeof(oldset));
		set_restore_sigmask();
		return -EINTR;
	}

	sigprocmask(SIG_SETMASK, &oldset, NULL);
	return 0;
}

/* The sendmsg system call is used for waking up waiting readers. */
static int kctrl_sendmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len )
{
	struct kctrl_sock *krs = kctrl_sk( sock->sk );
	struct kctrl_ringset *r = krs->ringset;
	wake_up_interruptible_all( &r->reader_waitqueue );
	return 0;
}

static void kctrl_sock_destruct( struct sock *sk )
{
	struct kctrl_sock *krs = kctrl_sk( sk );
	int i;

	if ( krs == 0 ) {
		printk("kctrl_sock_destruct: don't have krs\n" );
		return;
	}

	printk( "kctrl_sock_destruct mode: %d ring_id: %d reader_id: %d\n",
			krs->mode, krs->ring_id, krs->reader_id );

	switch ( krs->mode ) {
		case KCTRL_WRITE: {
			krs->ringset->ring[krs->ring_id].num_writers -= 1;
			break;
		}
		case KCTRL_READ: {

			/* One ring or all? */
			if ( krs->ring_id != KCTRL_RING_ID_ALL ) {
				struct kctrl_control *control = &krs->ringset->ring[krs->ring_id].control;

				krs->ringset->ring[krs->ring_id].reader[krs->reader_id].allocated = false;

				if ( control->reader[krs->reader_id].entered ) {
					kctrl_off_t prev = control->reader[krs->reader_id].rhead;
					kctrl_reader_release( krs->reader_id, &control[0], prev );
				}
			}
			else {
				for ( i = 0; i < krs->ringset->nrings; i++ ) {
					struct kctrl_control *control = &krs->ringset->ring[i].control;

					krs->ringset->ring[i].reader[krs->reader_id].allocated = false;

					if ( control->reader[krs->reader_id].entered ) {
						kctrl_off_t prev = control->reader[krs->reader_id].rhead;
						kctrl_reader_release( krs->reader_id, &control[0], prev );
					}
				}
			}
		}
	}
}

int kctrl_sock_create( struct net *net, struct socket *sock, int protocol, int kern )
{
	struct sock *sk;

//	/* Privs? */
//	if ( !capable( CAP_NET_ADMIN ) )
//		return -EPERM;

	if ( sock->type != SOCK_RAW )
		return -ESOCKTNOSUPPORT;

	if ( protocol != htons(ETH_P_ALL) )
		return -EPROTONOSUPPORT;

	sk = sk_alloc( net, PF_INET, GFP_KERNEL, &kctrl_proto );

	if ( sk == NULL )
		return -ENOMEM;

	sock->ops = &kctrl_ops;
	sock_init_data( sock, sk );

	sk->sk_family = KCTRL;
	sk->sk_destruct = kctrl_sock_destruct;

	return 0;
}

int kctrl_kopen( struct kctrl_kern *kring, const char *rsname, int ring_id, enum KCTRL_MODE mode )
{
	int reader_id = -1, writer_id = -1;

	struct kctrl_ringset *ringset = kctrl_find_ring( rsname );
	if ( ringset == 0 )
		return -1;
	
	if ( ring_id < 0 || ring_id >= ringset->nrings )
		return -1;

	kctrl_copy_name( kring->name, rsname );
	kring->ringset = ringset;
	kring->ring_id = ring_id;

	if ( mode == KCTRL_WRITE ) {
		/* Find a writer ID. */
		writer_id = kctrl_allocate_writer_on_ring( &ringset->ring[ring_id] );
		if ( writer_id < 0 )
			return writer_id;
	}
	else if ( mode == KCTRL_READ ) {
		/* Find a reader ID. */
		if ( ring_id != KCTRL_RING_ID_ALL ) {
			/* Reader ID for ring specified. */
			reader_id = kctrl_allocate_reader_on_ring( &ringset->ring[ring_id] );
		}
		else {
			/* Find a reader ID that works for all rings. This can fail. */
			reader_id = kctrl_allocate_reader_all_rings( ringset );
		}

		if ( reader_id < 0 )
			return reader_id;

		/*res = */kctrl_prep_enter( &ringset->ring[ring_id].control, 0 );
		//if ( res < 0 ) {
		//	kctrl_func_error( KCTRL_ERR_ENTER, 0 );
		//	return -1;
		//}
	}

	/* Set up the user read/write struct for unified read/write operations between kernel and user space. */
	kring->user.socket = -1;
	kring->user.control = &ringset->ring[ring_id].control;
	kring->user.data = 0;
	kring->user.pd = ringset->ring[ring_id].pd;
	kring->user.mode = mode;
	kring->user.ring_id = ring_id;
	kring->user.reader_id = reader_id;
	kring->user.writer_id = writer_id;
	kring->user.nrings = ringset->nrings;

	return 0;
}

int kctrl_kclose( struct kctrl_kern *kring )
{
	kring->ringset->ring[kring->ring_id].num_writers -= 1;
	return 0;
}

static void kctrl_write_single( struct kctrl_kern *kring, int dir,
		const struct sk_buff *skb, int offset, int write, int len )
{
	struct kctrl_packet_header *h;
	void *pdata;

	/* Which ringset? */
	struct kctrl_ringset *r = kring->ringset;

	h = kctrl_write_FIRST( &kring->user );

	h->len = len;
	h->dir = (char) dir;

	pdata = (char*)(h + 1);
	skb_copy_bits( skb, offset, pdata, write );

	kctrl_write_SECOND( &kring->user );

	/* track the number of packets produced. Note we don't account for overflow. */
	__sync_add_and_fetch( &r->ring[kring->ring_id].control.head->produced, 1 );

	#if 0
	for ( id = 0; id < KCTRL_READERS; id++ ) {
		if ( r->ring[kring->ring_id].reader[id].allocated ) {
			unsigned long long diff =
					r->ring[kring->ring_id].control.writer->produced -
					r->ring[kring->ring_id].control.reader[id].units;
			// printk( "diff: %llu\n", diff );
			if ( diff > ( KCTRL_NPAGES / 2 ) ) {
				printk( "half full: ring_id: %d reader_id: %d\n", kring->ring_id, id );
			}
		}
	}
	#endif

	wake_up_interruptible_all( &r->reader_waitqueue );
}

void kctrl_kwrite( struct kctrl_kern *kring, int dir, const struct sk_buff *skb )
{
	int offset = 0, write, len;

	while ( true ) {
		/* Number of bytes left in the packet (maybe overflows) */
		len = skb->len - offset;
		if ( len <= 0 )
			break;

		/* What can we write this time? */
		write = len;
		if ( write > kctrl_packet_max_data() )
			write = kctrl_packet_max_data();

		kctrl_write_single( kring, dir, skb, offset, write, len );

		offset += write;
	}
}

int kctrl_kavail( struct kctrl_kern *kring )
{
	struct kctrl_control *control = kring->user.control;

	if ( control->descriptor[ control->head->head ].next > control->head->head )
		return 1;

	return 0;
}

void kctrl_knext_plain( struct kctrl_kern *kring, struct kctrl_plain *plain )
{
	struct kctrl_plain_header *h;

	h = (struct kctrl_plain_header*) kctrl_next_generic( &kring->user );

	plain->len = h->len;
	plain->bytes = (unsigned char*)(h + 1);
}

static void *kctrl_alloc_shared_memory( int size )
{
	void *mem;
	size = PAGE_ALIGN(size);
	mem = vmalloc_user(size);
	memset( mem, 0, size );
	return mem;
}

static void kctrl_free_shared_memory( void *m )
{
	vfree(m);
}

static void kctrl_ring_alloc( struct kctrl_ring *r )
{
	int i;

	r->ctrl = kctrl_alloc_shared_memory( KCTRL_CTRL_SZ );

	r->control.head = r->ctrl + KCTRL_CTRL_OFF_HEAD;
	r->control.writer = r->ctrl + KCTRL_CTRL_OFF_WRITER;
	r->control.reader = r->ctrl + KCTRL_CTRL_OFF_READER;
	r->control.descriptor = r->ctrl + KCTRL_CTRL_OFF_DESC;

	r->num_writers = 0;
	r->num_readers = 0;

	r->pd = kmalloc( sizeof(struct kctrl_page_desc) * KCTRL_NPAGES, GFP_KERNEL );
	for ( i = 0; i < KCTRL_NPAGES; i++ ) {
		r->pd[i].p = alloc_page( GFP_KERNEL | __GFP_ZERO );
		if ( r->pd[i].p ) {
			r->pd[i].m = page_address(r->pd[i].p);
		}
		else {
			printk( "alloc_page for ring allocation failed\n" );
		}
	}

	r->control.head->whead = r->control.head->wresv = kctrl_one_back( 0 );

	r->control.head->write_mutex = 0;

	/* Use the first page as the "last written," which is our sentinal. */
	r->control.head->alloc = 0;
	r->control.descriptor[0].desc = KCTRL_DSC_WRITER_OWNED;
	r->control.descriptor[0].next = 0;
	r->control.head->head = 0;
	r->control.head->maybe_tail = 0;

	for ( i = 0; i < KCTRL_READERS; i++ )
		r->reader[i].allocated = false;

	init_waitqueue_head( &r->reader_waitqueue );
}

void kctrl_ringset_alloc( struct kctrl_ringset *r, const char *name, long nrings )
{
	int i;

	printk( "allocating %ld rings\n", nrings );

	strncpy( r->name, name, KCTRL_NLEN );
	r->name[KCTRL_NLEN-1] = 0;

	r->nrings = nrings;

	r->ring = kmalloc( sizeof(struct kctrl_ring) * nrings, GFP_KERNEL );
	memset( r->ring, 0, sizeof(struct kctrl_ring) * nrings  );

	for ( i = 0; i < nrings; i++ )
		kctrl_ring_alloc( &r->ring[i] );

	init_waitqueue_head( &r->reader_waitqueue );
}

void kctrl_ring_free( struct kctrl_ring *r )
{
	int i;
	for ( i = 0; i < KCTRL_NPAGES; i++ )
		__free_page( r->pd[i].p );

	kctrl_free_shared_memory( r->ctrl );
	kfree( r->pd );
}


void kctrl_add_ringset( struct kctrl_ringset **phead, struct kctrl_ringset *set )
{
	if ( *phead == 0 )
		*phead = set;
	else {
		struct kctrl_ringset *tail = *phead;
		while ( tail->next != 0 )
			tail = tail->next;
		tail->next = set;
	}
	set->next = 0;
}

int kctrl_init(void)
{
	int rc;

	sock_register(&kctrl_family_ops);

	if ( (rc = proto_register(&kctrl_proto, 0) ) != 0 )
		return rc;

	return 0;
}


void kctrl_exit(void)
{
	sock_unregister( KCTRL );

	proto_unregister( &kctrl_proto );
}


EXPORT_SYMBOL_GPL(kctrl_kopen);
EXPORT_SYMBOL_GPL(kctrl_kclose);
EXPORT_SYMBOL_GPL(kctrl_kwrite);
EXPORT_SYMBOL_GPL(kctrl_kavail);
EXPORT_SYMBOL_GPL(kctrl_knext_plain);

