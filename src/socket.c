#include "krkern.h"
#include "kring.h"

int kdata_sock_create( struct net *net, struct socket *sock, int protocol, int kern );

static struct proto_ops kdata_ops = {
	.family = KDATA,
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

static inline struct kring_sock *kring_sk( const struct sock *sk )
{
	return container_of( sk, struct kring_sock, sk );
}

static struct proto kdata_proto = {
	.name = "KDATA",
	.owner = THIS_MODULE,
	.obj_size = sizeof(struct kring_sock),
};

static struct net_proto_family kdata_family_ops = {
	.family = KDATA,
	.create = kdata_sock_create,
	.owner = THIS_MODULE,
};

int kdata_sock_create( struct net *net, struct socket *sock, int protocol, int kern )
{
	struct sock *sk;

//	/* Privs? */
//	if ( !capable( CAP_NET_ADMIN ) )
//		return -EPERM;

	if ( sock->type != SOCK_RAW )
		return -ESOCKTNOSUPPORT;

	if ( protocol != htons(ETH_P_ALL) )
		return -EPROTONOSUPPORT;

	sk = sk_alloc( net, PF_INET, GFP_KERNEL, &kdata_proto );

	if ( sk == NULL )
		return -ENOMEM;

	sock->ops = &kdata_ops;
	sock_init_data( sock, sk );

	sk->sk_family = KDATA;
	sk->sk_destruct = &kring_sock_destruct;

	return 0;
}


int kring_ioctl( struct socket *sock, unsigned int cmd, unsigned long arg )
{
	printk( "kring_ioctl\n" );
	return 0;
}

unsigned int kring_poll( struct file *file, struct socket *sock, poll_table *wait )
{
	printk( "kring_poll\n" );
	return 0;
}

int kring_setsockopt( struct socket *sock, int level, int optname, char __user * optval, unsigned int optlen )
{
	printk( "kring_setsockopt\n" );
	return 0;
}

int kring_getsockopt( struct socket *sock, int level, int optname, char __user *optval, int __user *optlen )
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

static void decon_pgoff( unsigned long pgoff, unsigned long *rid, unsigned long *region )
{
	*rid = ( pgoff & KRING_PGOFF_ID_MASK ) >> KRING_PGOFF_ID_SHIFT;
	*region = ( pgoff & KRING_PGOFF_REGION_MASK ) >> KRING_PGOFF_REGION_SHIFT;
}

int kring_sock_mmap( struct file *file, struct socket *sock, struct vm_area_struct *vma )
{
	struct kring_sock *krs = kring_sk( sock->sk );
	struct kring_ringset *r = krs->ringset;
	unsigned long ring_id, region;

	/* Ensure bound. */
	if ( r == 0 ) {
		printk( "kdata mmap: socket not bound to ring\n" );
		return -EINVAL;
	}
	
	decon_pgoff( vma->vm_pgoff, &ring_id, &region );

	if ( ring_id > r->nrings ) {
		printk( "kdata mmap: error: rid > r->nrings\n" );
		return -EINVAL;
	}

	switch ( region  ) {
		case KRING_PGOFF_CTRL: {
			printk( "mapping control region %p of ring %p-%lu\n", r->ring[ring_id].ctrl, r, ring_id );
			remap_vmalloc_range( vma, r->ring[ring_id].ctrl, 0 );
			break;
		}

		case KRING_PGOFF_DATA: {
			int i;
			unsigned long uaddr = vma->vm_start;
			printk( "mapping data region %lu of ring %p-%lu\n", uaddr, r, ring_id );
			for ( i = 0; i < r->params->npages; i++ ) {
				vm_insert_page( vma, uaddr, r->ring[ring_id].pd[i].p );
				uaddr += PAGE_SIZE;
			}

			break;
		}
	}

	return 0;
}

int kring_sock_release( struct socket *sock )
{
	struct sock *sk = sock->sk;

	if ( !sk )
		return 0;

	sock->sk = NULL;
	sock_put( sk );
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

int kring_allocate_writer_on_ring( struct kring_ringset *ringset, struct kring_ring *ring )
{
	int writer_id;
	bool orig;

again:

	/* Search for a writer id that is free on the ring requested. */
	for ( writer_id = 0; writer_id < ringset->params->writers; writer_id++ ) {
		if ( !ring->writer[writer_id].allocated )
			break;
	}

	if ( writer_id == ringset->params->writers ) {
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

int kring_allocate_reader_on_ring( struct kring_ringset *ringset, struct kring_ring *ring )
{
	int reader_id;
	bool orig;

again:

	/* Search for a reader id that is free on the ring requested. */
	for ( reader_id = 0; reader_id < ringset->params->readers; reader_id++ ) {
		if ( !ring->reader[reader_id].allocated )
			break;
	}

	if ( reader_id == ringset->params->readers ) {
		/* No valid id found */
		return -EINVAL;
	}

	/* All okay. */
	orig = __sync_val_compare_and_swap( &ring->reader[reader_id].allocated, 0, 1 );
	if ( orig != 0 )
		goto again;

	ring->num_readers += 1;

	return reader_id;
}

int kring_allocate_reader_all_rings( struct kring_ringset *ringset )
{
	int i, reader_id;

	/* Search for a single reader id that is free across all the rings requested. */
	for ( reader_id = 0; reader_id < ringset->params->readers; reader_id++ ) {
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

int kring_bind( struct socket *sock, struct sockaddr *sa, int addr_len )
{
	int reader_id = -1, writer_id = -1;
	struct kring_addr *addr = (struct kring_addr*)sa;
	struct kring_sock *krs;
	struct kring_ringset *ringset;

	if ( addr_len != sizeof(struct kring_addr) ) {
		printk( "kring_bind: addr_len wrong size\n" );
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

	ringset = kring_find_ring( addr->name );
	if ( ringset == 0 ) {
		printk( "kring_bind: bad mode, not read or write\n" );
		return -EINVAL;
	}

	if ( addr->ring_id != KRING_RING_ID_ALL &&
			( addr->ring_id < 0 || addr->ring_id >= ringset->nrings ) )
	{
		printk( "kring_bind: bad ring id %d\n", addr->ring_id );
		return -EINVAL;
	}

	/* Cannot write to all rings. */
	if ( addr->mode == KRING_WRITE && addr->ring_id == KRING_RING_ID_ALL ) {
		printk( "kring_bind: cannot write to ring id ALL\n" );
		return -EINVAL;
	}

	krs = kring_sk( sock->sk );

	if ( addr->mode == KRING_WRITE ) {
		/* Find a writer ID. */
		writer_id = kring_allocate_writer_on_ring( ringset, &ringset->ring[addr->ring_id] );
		if ( writer_id < 0 )
			return writer_id;
	}
	else if ( addr->mode == KRING_READ ) {
		/* Find a reader ID. */
		if ( addr->ring_id != KCTRL_RING_ID_ALL ) {
			/* Reader ID for ring specified. */
			reader_id = kring_allocate_reader_on_ring( ringset, &ringset->ring[addr->ring_id] );
		}
		else {
			/* Find a reader ID that works for all rings. This can fail. */
			reader_id = kring_allocate_reader_all_rings( ringset );
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

int kring_recvmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len, int flags )
{
	struct kring_sock *krs = kring_sk( sock->sk );
	struct kring_ringset *r = krs->ringset;
	sigset_t blocked, oldset;
	int ret;

	/* Allow kill, stop and the user sigs. This assumes we are operating under
	 * the genf program framework where we want to atomically unmask the user
	 * signals so we can receive genf inter-thread messages. */
	siginitsetinv( &blocked, sigmask(SIGKILL) | sigmask(SIGSTOP) |
			sigmask(SIGUSR1) | sigmask(SIGUSR2) );
	sigprocmask( SIG_SETMASK, &blocked, &oldset );

	ret = (*r->params->wait)( krs );

	if ( ret == -ERESTARTSYS ) {
		/* Interrupted by a signal. */
		memcpy(&current->saved_sigmask, &oldset, sizeof(oldset));
		set_restore_sigmask();
		return -EINTR;
	}

	sigprocmask(SIG_SETMASK, &oldset, NULL);
	return 0;
}

int kring_sendmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len )
{
	struct kring_sock *krs = kring_sk( sock->sk );
	struct kring_ringset *r = krs->ringset;
	wake_up_interruptible_all( &r->waitqueue );
	(*r->params->notify)( krs );
	return 0;
}

void kring_sock_destruct( struct sock *sk )
{
	struct kring_sock *krs = kring_sk( sk );

	if ( krs == 0 ) {
		printk("kring_sock_destruct: don't have krs\n" );
		return;
	}

	printk( "kring_sock_destruct mode: %d ring_id: %d reader_id: %d\n",
			krs->mode, krs->ring_id, krs->reader_id );

	(*krs->ringset->params->destruct)( krs );
}

int kring_sk_init(void)
{
	int rc;

	sock_register(&kdata_family_ops);

	if ( (rc = proto_register(&kdata_proto, 0) ) != 0 )
		return rc;

	return 0;
}

void kring_sk_exit(void)
{
	sock_unregister( KDATA );

	proto_unregister( &kdata_proto );
}
