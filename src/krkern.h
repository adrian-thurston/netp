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
 * Data.
 */

struct kdata_ring_reader
{
	bool allocated;
};

struct kdata_ring_writer
{
	bool allocated;
};

struct kdata_ring
{
	void *ctrl;
	struct kdata_control control;
	struct kdata_page_desc *pd;
	
	int num_readers;
	int num_writers;

	struct kdata_ring_reader reader[KDATA_READERS];
	struct kdata_ring_writer writer[KDATA_WRITERS];

	wait_queue_head_t reader_waitqueue;
};

struct kdata_ringset
{
	char name[KRING_NLEN];
	wait_queue_head_t reader_waitqueue;

	struct kdata_ring *ring;
	int nrings;

	struct kdata_ringset *next;
};

struct kdata_kern
{
	char name[KRING_NLEN];
	struct kdata_ringset *ringset;
	int ring_id;
	int writer_id;
	struct kdata_user user;
};

int kdata_kopen( struct kdata_kern *kdata, const char *ringset, int ring_id, enum KRING_MODE mode );
int kdata_kclose( struct kdata_kern *kdata );
void kdata_kwrite( struct kdata_kern *kdata, int dir, const struct sk_buff *skb );
int kdata_kavail( struct kdata_kern *kdata );
void kdata_knext_plain( struct kdata_kern *kdata, struct kdata_plain *plain );
void kring_ring_free( struct kdata_ring *r );

/*
 * Command.
 */

struct kctrl_ring_reader
{
	bool allocated;
};

struct kctrl_ring_writer
{
	bool allocated;
};

struct kctrl_ring
{
	void *ctrl;
	struct kctrl_control control;
	struct kctrl_page_desc *pd;
	
	int num_readers;
	int num_writers;

	struct kctrl_ring_reader reader[KCTRL_READERS];
	struct kctrl_ring_writer writer[KCTRL_WRITERS];

	wait_queue_head_t reader_waitqueue;
};

struct kctrl_ringset
{
	char name[KCTRL_NLEN];
	wait_queue_head_t reader_waitqueue;

	struct kctrl_ring *ring;
	int nrings;

	struct kctrl_ringset *next;
};

struct kctrl_kern
{
	char name[KCTRL_NLEN];
	struct kctrl_ringset *ringset;
	int ring_id;
	int writer_id;
	struct kctrl_user user;
};

int kctrl_kopen( struct kctrl_kern *kring, const char *ringset, enum KCTRL_MODE mode );
int kctrl_kclose( struct kctrl_kern *kring );

void kctrl_kwrite( struct kctrl_kern *kring, int dir, const struct sk_buff *skb );
int kctrl_kavail( struct kctrl_kern *kring );

void kctrl_knext_plain( struct kctrl_kern *kring, struct kctrl_plain *plain );

void kctrl_ringset_alloc( struct kctrl_ringset *r, const char *name, long nrings );
void kring_ringset_alloc( struct kdata_ringset *r, const char *name, long nrings );

void kctrl_add_ringset( struct kctrl_ringset **phead, struct kctrl_ringset *set );
void kctrl_free_ringsets( struct kctrl_ringset *head );

void kctrl_ring_free( struct kctrl_ring *r );

struct kctrl_ringset *kctrl_find_ring( const char *name );
struct kdata_ringset *kdata_find_ring( const char *name );

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
