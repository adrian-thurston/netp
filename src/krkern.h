#ifndef __KRKERN_H
#define __KRKERN_H

#include "kdata.h"
#include "kctrl.h"

#include <linux/skbuff.h>
#include <linux/kobject.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <asm/cacheflush.h>

/*
 * Common
 */

struct kring_ring_reader
{
	bool allocated;
};

struct kring_ring_writer
{
	bool allocated;
};

struct kring_ring
{
	int num_readers;
	int num_writers;

	struct kring_ring_reader reader[KDATA_READERS];
	struct kring_ring_writer writer[KDATA_WRITERS];

	void *ctrl;
	struct kring_page_desc *pd;

	wait_queue_head_t waitqueue;

	struct kring_control _control_;
};

struct kring_sock
{
	struct sock sk;
	struct kring_ringset *ringset;
	int ring_id;
	enum KRING_MODE mode;
	int writer_id;
	int reader_id;
};

struct kring_params
{
	const int npages;

	const int ctrl_sz;

	const int ctrl_off_head;
	const int ctrl_off_writer;
	const int ctrl_off_reader;
	const int ctrl_off_desc;

	const int data_sz;

	const int readers;
	const int writers;

	void (*init_control)( struct kring_ring *ring );
	int (*wait)( struct kring_sock *krs );
	void (*notify)( struct kring_sock *krs );
	void (*destruct)( struct kring_sock *krs );
};

struct kring_ringset
{
	char name[KRING_NLEN];
	struct kring_ring *ring;
	wait_queue_head_t waitqueue;
	int nrings;
	struct kring_params *params;

	struct kring_ringset *next;
};

/*
 * Data.
 */

#define KDATA_CONTROL(p) ( (struct kdata_control*) &((p)._control_) )

struct kdata_kern
{
	char name[KRING_NLEN];
	struct kring_ringset *ringset;
	int ring_id;
	int writer_id;
	struct kdata_user user;
};

int kdata_kopen( struct kdata_kern *kdata, const char *ringset, int ring_id, enum KRING_MODE mode );
int kdata_kclose( struct kdata_kern *kdata );
void kdata_kwrite( struct kdata_kern *kdata, int dir, const struct sk_buff *skb );
int kdata_kavail( struct kdata_kern *kdata );
void kdata_knext_plain( struct kdata_kern *kdata, struct kdata_plain *plain );

/*
 * Command.
 */

#define KCTRL_CONTROL(p) ((struct kctrl_control*) &((p)._control_))

struct kctrl_kern
{
	char name[KCTRL_NLEN];
	struct kring_ringset *ringset;
	int ring_id;
	int writer_id;
	struct kctrl_user user;
};

int kctrl_kopen( struct kctrl_kern *kring, const char *ringset, enum KRING_MODE mode );
int kctrl_kclose( struct kctrl_kern *kring );

void kctrl_kwrite( struct kctrl_kern *kring, int dir, const struct sk_buff *skb );
int kctrl_kavail( struct kctrl_kern *kring );

void kctrl_knext_plain( struct kctrl_kern *kring, struct kctrl_plain *plain );


struct kring_ringset *kring_find_ring( const char *name );

/*
 * Common.
 */

/* General. */
void kring_copy_name( char *dest, const char *src );
int kring_allocate_writer_on_ring( struct kring_ringset *ringset, struct kring_ring *ring );
int kring_allocate_reader_on_ring( struct kring_ringset *ringset, struct kring_ring *ring );
int kring_allocate_reader_all_rings( struct kring_ringset *ringset );


/* Sockets. */
int kring_ioctl( struct socket *sock, unsigned int cmd, unsigned long arg );
unsigned int kring_poll( struct file *file, struct socket *sock, poll_table *wait );
int kring_setsockopt( struct socket *sock, int level, int optname, char __user * optval, unsigned int optlen );
int kring_getsockopt( struct socket *sock, int level, int optname, char __user *optval, int __user *optlen );
int kring_sock_mmap( struct file *file, struct socket *sock, struct vm_area_struct *vma );
int kring_sock_release( struct socket *sock );
int kring_bind( struct socket *sock, struct sockaddr *sa, int addr_len );
int kring_recvmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len, int flags );
int kring_sendmsg( struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len );
void kring_sock_destruct( struct sock *sk );

int kring_sk_init(void);
void kring_sk_exit(void);

#endif
