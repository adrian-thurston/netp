#include "attribute.h"
#include <linux/kobject.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/cacheflush.h>

#include "krkern.h"
#include "common.c"

struct kring
{
	struct kobject kobj;
};

static struct ringset *head = 0;

static int kring_sock_release( struct socket *sock );
static int kring_sock_create( struct net *net, struct socket *sock, int protocol, int kern );
static int kring_sock_mmap(struct file *file, struct socket *sock, struct vm_area_struct *vma );
static int kring_bind(struct socket *sock, struct sockaddr *sa, int addr_len);
static unsigned int kring_poll(struct file *file, struct socket *sock, poll_table * wait);
static int kring_setsockopt(struct socket *sock, int level, int optname, char __user * optval, unsigned int optlen);
static int kring_getsockopt(struct socket *sock, int level, int optname, char __user *optval, int __user *optlen);
static int kring_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg);
static int kring_recvmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len, int flags );
static int kring_sendmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len );

static struct proto_ops kring_ops = {
	.family = KRING,
	.owner = THIS_MODULE,

	.release = kring_sock_release,
	.bind = kring_bind,
	.mmap = kring_sock_mmap,
	.poll = kring_poll,
	.setsockopt = kring_setsockopt,
	.getsockopt = kring_getsockopt,
	.ioctl = kring_ioctl,
	.recvmsg = kring_recvmsg,
	.sendmsg = kring_sendmsg,

	/* Not used. */
	.connect = sock_no_connect,
	.socketpair = sock_no_socketpair,
	.accept = sock_no_accept,
	.getname = sock_no_getname,
	.listen = sock_no_listen,
	.shutdown = sock_no_shutdown,
	.sendpage = sock_no_sendpage,
};

struct kring_sock
{
	struct sock sk;
	struct ringset *ringset;
	enum KRING_MODE mode;
	int ring_id;
	int reader_id;
};

static inline struct kring_sock *kring_sk( const struct sock *sk )
{
	return container_of( sk, struct kring_sock, sk );
}

static struct proto kring_proto = {
	.name = "KRING",
	.owner = THIS_MODULE,
	.obj_size = sizeof(struct kring_sock),
};

static struct net_proto_family kring_family_ops = {
	.family = KRING,
	.create = kring_sock_create,
	.owner = THIS_MODULE,
};


static int kring_sock_release( struct socket *sock )
{
	struct sock *sk = sock->sk;

	if ( !sk )
		return 0;

	printk( "kring_sock_release\n" );

	sock->sk = NULL;
	sock_put( sk );
	return 0;
}

static void decon_pgoff( unsigned long pgoff, unsigned long *rid, unsigned long *region )
{
	*rid = ( pgoff & PGOFF_ID_MASK ) >> PGOFF_ID_SHIFT;
	*region = ( pgoff & PGOFF_REGION_MASK ) >> PGOFF_REGION_SHIFT;
}

static int kring_sock_mmap( struct file *file, struct socket *sock, struct vm_area_struct *vma )
{
	struct kring_sock *krs = kring_sk( sock->sk );
	struct ringset *r = krs->ringset;
	unsigned long ring_id, region;

	/* Ensure bound. */
	if ( r == 0 ) {
		printk( "kring mmap: socket not bound to ring\n" );
		return -EINVAL;
	}
	
	decon_pgoff( vma->vm_pgoff, &ring_id, &region );

	if ( ring_id > r->N ) {
		printk( "kring mmap: error: rid > r->N\n" );
		return -EINVAL;
	}

	switch ( region  ) {
		case PGOFF_CTRL: {
			printk( "mapping control region %p of ring %p-%lu\n", r->ring[ring_id].ctrl, r, ring_id );
			remap_vmalloc_range( vma, r->ring[ring_id].ctrl, 0 );
			break;
		}

		case PGOFF_DATA: {
			int i;
			unsigned long uaddr = vma->vm_start;
			printk( "mapping data region %lu of ring %p-%lu\n", uaddr, r, ring_id );
			for ( i = 0; i < NPAGES; i++ ) {
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
	const char *pe = name + KRING_NLEN;
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

static struct ringset *find_ring( const char *name )
{
	struct ringset *r = head;
	while ( r != 0 ) {
		if ( strcmp( r->name, name ) == 0 )
			return r;

		r = r->next;
	}

	return 0;
}

static int kring_bind( struct socket *sock, struct sockaddr *sa, int addr_len )
{
	int i, id = -1;
	struct kring_addr *addr = (struct kring_addr*)sa;
	struct kring_sock *krs;
	struct ringset *ringset;

	if ( addr_len != sizeof(struct kring_addr) ) {
		printk("kring_bind: addr_len wrong size\n");
		return -EINVAL;
	}

	if ( validate_ring_name( addr->name ) < 0 ) {
		printk( "kring_bind: bad ring name\n" );
		return -EINVAL;
	}
	
	if ( addr->mode != KRING_READ && addr->mode != KRING_WRITE ) {
		printk( "kring_bind: bad mode, not read or write\n" );
		return -EINVAL;
	}
	
	ringset = find_ring( addr->name );
	if ( ringset == 0 ) {
		printk( "kring_bind: bad mode, not read or write\n" );
		return -EINVAL;
	}

	if ( addr->ring_id < -1 || addr->ring_id >= ringset->N ) {
		printk( "kring_bind: bad ring id\n" );
		return -EINVAL;
	}

	krs = kring_sk( sock->sk );

	if ( addr->mode == KRING_WRITE ) {
		/* Cannot write to all rings. */
		if ( addr->ring_id == KR_RING_ID_ALL ) {
			printk( "kring_bind: cannot write to ring id ALL\n" );
			return -EINVAL;
		}

		if ( ringset->ring[addr->ring_id].has_writer ) {
			printk( "kring_bind: ring %d already has writer\n", addr->ring_id );
			return -EINVAL;
		}

		ringset->ring[addr->ring_id].has_writer = true;
	}
	else if ( addr->mode == KRING_READ ) {
		/* Compute rings to iterate over. Default to exactly ring id. */
		int low = addr->ring_id, high = addr->ring_id + 1;

		/* Maybe all rings? */
		if ( addr->ring_id == KR_RING_ID_ALL ) {
			low = 0;
			high = ringset->N;
		}

		/* Search for a single reader id that is free across all the rings requested. */
		for ( id = 0; id < NRING_READERS; id++ ) {
			for ( i = low; i < high; i++ ) {
				if ( ringset->ring[i].reader[id].allocated )
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
		
		/* Allocate reader ids. */
		for ( i = low; i < high; i++ )
			ringset->ring[i].reader[id].allocated = true;
	}

	krs->ringset = ringset;
	krs->mode = addr->mode;
	krs->ring_id = addr->ring_id;
	krs->reader_id = id;

	return 0;
}

unsigned int kring_poll(struct file *file, struct socket *sock, poll_table * wait)
{
	printk( "kring_poll\n" );
	return 0;
}

static int kring_setsockopt(struct socket *sock, int level, int optname, char __user * optval, unsigned int optlen)
{
	printk( "kring_setsockopt\n" );
	return 0;
}

static int kring_getsockopt( struct socket *sock, int level, int optname, char __user *optval, int __user *optlen )
{
	int len;
	int val, lv = sizeof(val);
	void *data = &val;
	struct kring_sock *krs = kring_sk( sock->sk );

	if ( level != SOL_PACKET )
		return -ENOPROTOOPT;

	if ( get_user(len, optlen) )
		return -EFAULT;

	if ( len != lv )
		return -EINVAL;

	printk( "kring_getsockopt\n" );

	switch ( optname ) {
		case KR_OPT_RING_N:
			val = krs->ringset->N;
			break;
		case KR_OPT_READER_ID:
			val = krs->reader_id;
			break;
	}

	if ( put_user( len, optlen ) )
		return -EFAULT;

	if ( copy_to_user( optval, data, len) )
		return -EFAULT;
	return 0;
}

static int kring_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	printk( "kring_ioctl\n" );
	return 0;
}

static int kring_recvmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len, int flags )
{
	struct kring_sock *krs = kring_sk( sock->sk );
	struct ringset *r = krs->ringset;
	sigset_t blocked, oldset;
	int ret;
	wait_queue_head_t *wq;

	/* Allow kill, stop and the user sigs. This assumes we are operating under
	 * the genf program framework where we want to atomically unmask the user
	 * signals so we can receive genf inter-thread messages. */
	siginitsetinv( &blocked, sigmask(SIGKILL) | sigmask(SIGSTOP) |
			sigmask(SIGUSR1) | sigmask(SIGUSR2) );
	sigprocmask(SIG_SETMASK, &blocked, &oldset);

	// wq = krs->ring_id == KR_RING_ID_ALL ? &r->reader_waitqueue : &r->ring[krs->ring_id].reader_waitqueue;
	wq = &r->reader_waitqueue;

	ret = wait_event_interruptible( *wq, kring_avail_impl( &r->ring[krs->ring_id].control, krs->reader_id ) );

	if (ret == -ERESTARTNOHAND) {
		memcpy(&current->saved_sigmask, &oldset, sizeof(oldset));
		set_restore_sigmask();
	}
	else {
		sigprocmask(SIG_SETMASK, &oldset, NULL);
	}

	return ret;
}

static int kring_sendmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len )
{
	struct kring_sock *krs = kring_sk( sock->sk );
	struct ringset *r = krs->ringset;
	wake_up_interruptible_all( &r->reader_waitqueue );
	return 0;
}

static void kring_sock_destruct( struct sock *sk )
{
	struct kring_sock *krs = kring_sk( sk );
	int i, low = krs->ring_id, high = krs->ring_id + 1;

	if ( krs == 0 ) {
		printk("kring_sock_destruct: don't have krs\n" );
		return;
	}

	printk( "kring_sock_destruct mode: %d ring_id: %d reader_id: %d\n",
			krs->mode, krs->ring_id, krs->reader_id );

	switch ( krs->mode ) {
		case KRING_WRITE: {
			krs->ringset->ring[krs->ring_id].has_writer = false;
			break;
		}
		case KRING_READ: {
			/* Maybe all rings? */
			if ( krs->ring_id == KR_RING_ID_ALL ) {
				low = 0;
				high = krs->ringset->N;
			}

			for ( i = low; i < high; i++ )
				krs->ringset->ring[i].reader[krs->reader_id].allocated = false;

		}
	}
}

int kring_sock_create( struct net *net, struct socket *sock, int protocol, int kern )
{
	struct sock *sk;

//	/* Privs? */
//	if ( !capable( CAP_NET_ADMIN ) )
//		return -EPERM;

	if ( sock->type != SOCK_RAW )
		return -ESOCKTNOSUPPORT;

	if ( protocol != htons(ETH_P_ALL) )
		return -EPROTONOSUPPORT;

	sk = sk_alloc( net, PF_INET, GFP_KERNEL, &kring_proto );

	if ( sk == NULL )
		return -ENOMEM;

	sock->ops = &kring_ops;
	sock_init_data( sock, sk );

	sk->sk_family = KRING;
	sk->sk_destruct = kring_sock_destruct;

	return 0;
}

int kring_wopen( struct kring_kern *kring, const char *ringset, int ring_id )
{
	struct ringset *r = find_ring( ringset );
	if ( r == 0 )
		return -1;
	
	if ( ring_id < 0 || ring_id >= r->N )
		return -1;

	copy_name( kring->name, ringset );
	kring->ringset = r;
	kring->ring_id = ring_id;

	r->ring[ring_id].has_writer = true;

	return 0;
}

int kring_wclose( struct kring_kern *kring )
{
	kring->ringset->ring[kring->ring_id].has_writer = false;
	return 0;
}

void kring_write( struct kring_kern *kring, int dir, void *d, int len )
{
	struct kring_packet_header *h;
	void *pdata;
	shr_off_t whead;

	/* Which ringset? */
	struct ringset *r = kring->ringset;

	/* Limit the size. */
	const int headsz = sizeof(struct kring_packet_header);
	if ( len > ( KRING_PAGE_SIZE - headsz ) ) {
		/* printk("KRING: large write: %d\n", len ); */
		len = PAGE_SIZE - headsz;
	}

	/* Find the place to write to, skipping ahead as necessary. */
	whead = find_write_loc( &r->ring[kring->ring_id].control );

	/* Reserve the space. */
	r->ring[kring->ring_id].control.writer->wresv = whead;

	h = r->ring[kring->ring_id].pd[whead].m;
	pdata = (char*)(h + 1);

	h->len = len;
	h->dir = (char) dir;
	memcpy( pdata, d, len );

	/* Clear the writer owned bit from the buffer. */
	if ( writer_release( &r->ring[kring->ring_id].control, whead ) < 0 )
		printk( "writer release unexected value\n" );

	/* Write back the write head, thereby releasing the buffer to writer. */
	r->ring[kring->ring_id].control.writer->whead = whead;

	wake_up_interruptible_all( &r->reader_waitqueue );
}

static void *alloc_shared_memory( int size )
{
	void *mem;
	size = PAGE_ALIGN(size);
	mem = vmalloc_user(size);
	memset( mem, 0, size );
	return mem;
}

static void free_shared_memory( void *m )
{
	vfree(m);
}

static void ring_alloc( struct ring *r )
{
	int i;

	r->ctrl = alloc_shared_memory( KRING_CTRL_SZ );

	r->control.writer = r->ctrl;
	r->control.reader = r->ctrl + sizeof(struct shared_writer);
	r->control.descriptor = r->ctrl + sizeof(struct shared_writer) + sizeof(struct shared_reader) * NRING_READERS;

	r->has_writer = false;
	r->num_readers = 0;

	r->pd = kmalloc( sizeof(struct page_desc) * NPAGES, GFP_KERNEL );
	for ( i = 0; i < NPAGES; i++ ) {
		r->pd[i].p = alloc_page( GFP_KERNEL | __GFP_ZERO );
		if ( r->pd[i].p ) {
			r->pd[i].m = page_address(r->pd[i].p);
		}
		else {
			printk( "alloc_page for ring allocation failed\n" );
		}
	}

	r->control.writer->whead = r->control.writer->wresv = kring_one_back( 0 );

	for ( i = 0; i < NRING_READERS; i++ )
		r->reader[i].allocated = false;

	init_waitqueue_head( &r->reader_waitqueue );
}

static void ringset_alloc( struct ringset *r, const char *name, long n )
{
	int i;

	strncpy( r->name, name, KRING_NLEN );
	r->name[KRING_NLEN-1] = 0;

	printk( "allocating %ld rings\n", n );

	r->N = n;

	r->ring = kmalloc( sizeof(struct ring) * n, GFP_KERNEL );
	memset( r->ring, 0, sizeof(struct ring) * n  );

	for ( i = 0; i < n; i++ )
		ring_alloc( &r->ring[i] );

	init_waitqueue_head( &r->reader_waitqueue );
}

static void ring_free( struct ring *r )
{
	int i;
	for ( i = 0; i < NPAGES; i++ )
		__free_page( r->pd[i].p );

	free_shared_memory( r->ctrl );
	kfree( r->pd );
}

static void ringset_free( struct ringset *r )
{
	int i;
	for ( i = 0; i < r->N; i++ )
		ring_free( &r->ring[i] );
	kfree( r->ring );
}

static ssize_t kring_add_store( struct kring *obj, const char *name, long n )
{
	struct ringset *r;
	if ( n < 1 || n > MAX_RINGS_PER_SET )
		return -EINVAL;

	r = kmalloc( sizeof(struct ringset), GFP_KERNEL );
	ringset_alloc( r, name, n );

	if ( head == 0 )
		head = r;
	else {
		struct ringset *tail = head;
		while ( tail->next != 0 )
			tail = tail->next;
		tail->next = r;
	}
	r->next = 0;

	return 0;
}

static ssize_t kring_del_store( struct kring *obj, const char *name  )
{
	return 0;
}

static int kring_init(void)
{
	int rc;

	sock_register(&kring_family_ops);

	if ( (rc = proto_register(&kring_proto, 0) ) != 0 )
		return rc;

	return 0;
}

static void kring_exit(void)
{
	struct ringset *r;

	sock_unregister( KRING );

	proto_unregister( &kring_proto );

	r = head;
	while ( r != 0 ) {
		ringset_free( r );
		r = r->next;
	}
}


EXPORT_SYMBOL_GPL(kring_wopen);
EXPORT_SYMBOL_GPL(kring_wclose);
EXPORT_SYMBOL_GPL(kring_write);
