#ifndef __KRKERN_H
#define __KRKERN_H

#include "kring.h"

#include <linux/skbuff.h>

struct ring_reader
{
	bool allocated;
};

struct ring_writer
{
	bool allocated;
};

struct ring
{
	void *ctrl;
	struct kring_control control;
	struct page_desc *pd;
	
	int num_readers;
	bool writer_attached;

	struct ring_reader reader[KRING_READERS];

	wait_queue_head_t reader_waitqueue;
};

struct ringset
{
	char name[KRING_NLEN];
	wait_queue_head_t reader_waitqueue;

	struct ring *ring;
	int nrings;
	int writers_per_ring;

	struct ringset *next;
};

struct kring_kern
{
	char name[KRING_NLEN];
	struct ringset *ringset;
	int ring_id;
};


int kring_wopen( struct kring_kern *kring, const char *ringset, int rid );
int kring_wclose( struct kring_kern *kring );
void kring_write( struct kring_kern *kring, int dir, const struct sk_buff *skb );

#endif

