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
	int id;
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
	unsigned long rid, region;

	/* Ensure bound. */
	if ( r == 0 )
		return -EINVAL;
	
	decon_pgoff( vma->vm_pgoff, &rid, &region );

	if ( rid > r->N )
		return -EINVAL;

	switch ( region  ) {
		case PGOFF_CTRL: {
			printk( "mapping control region %p of ring %p\n", r->ring->ctrl, r );
			remap_vmalloc_range( vma, r->ring[rid].ctrl, 0 );
			break;
		}

		case PGOFF_DATA: {
			int i;
			unsigned long uaddr = vma->vm_start;
			printk( "mapping data region %lu of ring %p\n", uaddr, r );
			for ( i = 0; i < NPAGES; i++ ) {
				vm_insert_page( vma, uaddr, r->ring[rid].pd[i].p );
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
	int id = -1;
	struct kring_addr *addr = (struct kring_addr*)sa;
	struct kring_sock *krs;
	struct ringset *ringset;

	if ( addr_len != sizeof(struct kring_addr) )
		return -EINVAL;

	if ( validate_ring_name( addr->name ) < 0 )
		return -EINVAL;
	
	if ( addr->mode != KRING_READ && addr->mode != KRING_WRITE )
		return -EINVAL;
	
	ringset = find_ring( addr->name );
	if ( ringset == 0 )
		return -EINVAL;

	if ( addr->mode == KRING_WRITE ) {
		if ( ringset->ring[0].has_writer )
			return -EINVAL;
	}
	else {
		for ( id = 0 ; id < NRING_READERS; id++ ) {
			if ( !ringset->ring[0].reader[id].allocated )
				break;
		}

		if ( id == NRING_READERS )
			return -EINVAL;
	}

	krs = kring_sk( sock->sk );
	krs->ringset = ringset;
	krs->mode = addr->mode;
	krs->id = id;

	ringset->ring[0].reader[id].allocated = true;

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

	if (level != SOL_PACKET)
		return -ENOPROTOOPT;

	if (get_user(len, optlen))
		return -EFAULT;

	if (len < 0)
		return -EINVAL;

	printk( "kring_getsockopt\n" );

	switch ( optname ) {
		case 1:
			val = krs->id;
			break;
	}

	if ( len > lv )
		len = lv;

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

	/* Allow kill, stop and the user sigs. This assumes we are operating under
	 * the genf program framework where we want to atomically unmask the user
	 * signals so we can receive genf inter-thread messages. */
	siginitsetinv( &blocked, sigmask(SIGKILL) | sigmask(SIGSTOP) |
			sigmask(SIGUSR1) | sigmask(SIGUSR2) );
	sigprocmask(SIG_SETMASK, &blocked, &oldset);

	ret = wait_event_interruptible( r->reader_waitqueue, kring_avail_impl( &r->ring[0].shared, krs->id ) );

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
	krs->ringset->ring[0].reader[krs->id].allocated = false;

	printk( "kring_sock_destruct\n" );
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

int kring_wopen( struct kring_kern *kring, const char *ringset, int rid )
{
	struct ringset *r = find_ring( ringset );
	if ( r == 0 )
		return -1;

	copy_name( kring->name, ringset );
	kring->ringset = r;
	kring->rid = rid;
	r->ring[0].has_writer = true;

	return 0;
}

int kring_wclose( struct kring_kern *kring )
{
	kring->ringset->ring[0].has_writer = false;
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
		printk("KRING: large write: %d\n", len );
		len = PAGE_SIZE - headsz;
	}

	/* Find the place to write to, skipping ahead as necessary. */
	whead = find_write_loc( &r->ring[0].shared );

	/* Reserve the space. */
	r->ring[0].shared.control->wresv = whead;

	h = r->ring[0].pd[whead].m;
	pdata = (char*)(h + 1);

	h->len = len;
	h->dir = (char) dir;
	memcpy( pdata, d, len );

	/* Clear the writer owned bit from the buffer. */
	if ( writer_release( &r->ring[0].shared, whead ) < 0 )
		printk( "writer release unexected value\n" );

	/* Write back the write head, thereby releasing the buffer to writer. */
	r->ring[0].shared.control->whead = whead;

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

	r->shared.control = r->ctrl;
	r->shared.reader = r->ctrl + sizeof(struct shared_ctrl);
	r->shared.descriptor = r->ctrl + sizeof(struct shared_ctrl) + sizeof(struct shared_reader) * NRING_READERS;

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

	r->shared.control->whead = r->shared.control->wresv = kring_one_back( 0 );

	for ( i = 0; i < NRING_READERS; i++ )
		r->reader[i].allocated = false;

	init_waitqueue_head( &r->reader_waitqueue );
}

static void ringset_alloc( struct ringset *r, const char *name, long n )
{
	int i;

	strncpy( r->name, name, KRING_NLEN );
	r->name[KRING_NLEN-1] = 0;

	r->N = n;
	r->ring = kmalloc( sizeof(struct ring) * n, GFP_KERNEL );
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
