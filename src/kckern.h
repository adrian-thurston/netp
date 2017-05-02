#ifndef __KCKERN_H
#define __KCKERN_H

#include "kctrl.h"

#include <linux/skbuff.h>

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

int kctrl_kopen( struct kctrl_kern *kring, const char *ringset, int ring_id, enum KCTRL_MODE mode );
int kctrl_kclose( struct kctrl_kern *kring );

void kctrl_kwrite( struct kctrl_kern *kring, int dir, const struct sk_buff *skb );
int kctrl_kavail( struct kctrl_kern *kring );

void kctrl_knext_plain( struct kctrl_kern *kring, struct kctrl_plain *plain );

void kctrl_ringset_alloc( struct kctrl_ringset *r, const char *name, long nrings );
void kctrl_add_ringset( struct kctrl_ringset **phead, struct kctrl_ringset *set );
void kctrl_free_ringsets( struct kctrl_ringset *head );
struct kctrl_ringset *kctrl_find_ring( const char *name );

#endif
