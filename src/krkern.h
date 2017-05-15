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

	struct kring_control control_;
};

struct kring_ringset
{
	char name[KRING_NLEN];
	struct kring_ring *ring;
	wait_queue_head_t waitqueue;
	int nrings;

	struct kring_ringset *next;
};

/*
 * Data.
 */

#define KDATA_CONTROL(p) ( (struct kdata_control*) &((p).control_) )

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
void kring_ring_free( struct kring_ring *r );

/*
 * Command.
 */

#define KCTRL_CONTROL(p) ((struct kctrl_control*) &((p).control_))

struct kctrl_kern
{
	char name[KCTRL_NLEN];
	struct kring_ringset *ringset;
	int ring_id;
	int writer_id;
	struct kctrl_user user;
};

int kctrl_kopen( struct kctrl_kern *kring, const char *ringset, enum KCTRL_MODE mode );
int kctrl_kclose( struct kctrl_kern *kring );

void kctrl_kwrite( struct kctrl_kern *kring, int dir, const struct sk_buff *skb );
int kctrl_kavail( struct kctrl_kern *kring );

void kctrl_knext_plain( struct kctrl_kern *kring, struct kctrl_plain *plain );

void kctrl_ringset_alloc( struct kring_ringset *r, const char *name, long nrings );
void kring_ringset_alloc( struct kring_ringset *r, const char *name, long nrings );

void kctrl_add_ringset( struct kring_ringset **phead, struct kring_ringset *set );
void kctrl_free_ringsets( struct kring_ringset *head );

void kctrl_ring_free( struct kring_ring *r );

struct kring_ringset *kctrl_find_ring( const char *name );
struct kring_ringset *kdata_find_ring( const char *name );

/*
 * Common.
 */

/* General. */
void kring_copy_name( char *dest, const char *src );

/* Sockets. */
int kring_ioctl( struct socket *sock, unsigned int cmd, unsigned long arg );
unsigned int kring_poll( struct file *file, struct socket *sock, poll_table *wait );
int kring_setsockopt( struct socket *sock, int level, int optname, char __user * optval, unsigned int optlen );


#endif
