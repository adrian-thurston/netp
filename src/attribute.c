#include "attribute.h"
#include <linux/kobject.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/cacheflush.h>

#include "kring.h"

struct kring
{
	struct kobject kobj;
};

struct ring
{
	void *ctrl;

	struct kring_shared shared;

	struct page_desc *pd;

	wait_queue_head_t reader_waitqueue;
};

static struct ring r0;
static struct ring r1;

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

static int kring_sock_mmap( struct file *file, struct socket *sock, struct vm_area_struct *vma )
{
	struct ring *r = ( vma->vm_pgoff & 0xffff0000 ) ? &r1 : &r0;
	switch ( vma->vm_pgoff & 0xffff ) {
		case PGOFF_CTRL: {
			printk( "mapping control region\n" );
			remap_vmalloc_range( vma, r->ctrl, 0 );
			break;
		}

		case PGOFF_DATA: {
			int i;
			unsigned long uaddr = vma->vm_start;
			printk( "mapping data region %lu\n", uaddr );
			for ( i = 0; i < NPAGES; i++ ) {
				vm_insert_page( vma, uaddr, r->pd[i].p );
				uaddr += PAGE_SIZE;
			}

			break;
		}
	}

	return 0;
}

static int kring_bind(struct socket *sock, struct sockaddr *sa, int addr_len)
{
	printk( "kring_bind\n" );
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

static int kring_getsockopt(struct socket *sock, int level, int optname, char __user *optval, int __user *optlen)
{
	printk( "kring_getsockopt\n" );
	return 0;
}

static int kring_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	printk( "kring_ioctl\n" );
	return 0;
}

static int kring_recvmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len, int flags )
{
	struct ring *r = flags == 0 ? &r0 : &r1;
	shr_off_t swhead = r->shared.control->whead;
	wait_event_interruptible( r->reader_waitqueue, r->shared.control->whead != swhead );
	return 0;
}

static int kring_sendmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len )
{
	printk( "kring_sendmsg\n" );
	return 0;
}

static void kring_sock_destruct(struct sock *sk)
{
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

void *alloc_shared_memory( int size )
{
	void *mem;
	size = PAGE_ALIGN(size);
	mem = vmalloc_user(size);
	memset( mem, 0, size );
	return mem;
}

void free_shared_memory( void *m )
{
	vfree(m);
}

static void ring_alloc( struct ring *r )
{
	int i;

	r->ctrl = alloc_shared_memory( KRING_CTRL_SZ );

	r->shared.control = r->ctrl;
	r->shared.descriptors = r->ctrl + sizeof(struct shared_ctrl);

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

static int kring_init(void)
{
	int rc;

	ring_alloc( &r0 );
	ring_alloc( &r1 );

	sock_register(&kring_family_ops);

	if ( (rc = proto_register(&kring_proto, 0) ) != 0 )
		return rc;

	return 0;
}

static void kring_exit(void)
{
	sock_unregister( KRING );

	proto_unregister( &kring_proto );

	ring_free( &r1 );
	ring_free( &r0 );
}

void kring_write( int rid, int dir, void *d, int len )
{
	int *plen;
	char *pdir;
	void *pdata;
	shr_off_t whead;

	/* Which ring? */
	struct ring *r = rid == 0 ? &r0 : ( rid == 1 ? &r1 : 0 );

	/* Limit the size. */
	const int headsz = sizeof(int) + 1;
	if ( len > ( KRING_PAGE_SIZE - headsz ) ) {
		printk("KRING: large write: %d\n", len );
		len = PAGE_SIZE - headsz;
	}

	/* Find the place to write to, skipping ahead as necessary. */
	whead = find_write_loc( &r->shared );

	/* Reserve the space. */
	r->shared.control->wresv = whead;

	plen = r->pd[whead].m;
	pdir = (char*)plen + sizeof(int);
	pdata = (char*)plen + headsz;

	*plen = len;
	*pdir = (char) dir;
	memcpy( pdata, d, len );

	/* Clear the writer owned bit from the buffer. */
	if ( writer_release( &r->shared, whead ) < 0 )
		printk( "writer release unexected value\n" );

	/* Write back the write head, thereby releasing the buffer to writer. */
	r->shared.control->whead = whead;

	wake_up_interruptible_all( &r->reader_waitqueue );
}

EXPORT_SYMBOL_GPL(kring_write);
