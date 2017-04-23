#ifndef __KRKERN_H
#define __KRKERN_H

#include "kring.h"

#include <linux/skbuff.h>

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
	void *ctrl;
	struct kring_control control;
	struct kring_page_desc *pd;
	
	int num_readers;
	int num_writers;

	struct kring_ring_reader reader[KRING_READERS];
	struct kring_ring_writer writer[KRING_WRITERS];

	wait_queue_head_t reader_waitqueue;
};

struct kring_ringset
{
	char name[KRING_NLEN];
	wait_queue_head_t reader_waitqueue;

	struct kring_ring *ring;
	int nrings;

	struct kring_ringset *next;
};

struct kring_kern
{
	char name[KRING_NLEN];
	struct kring_ringset *ringset;
	int ring_id;
};

int kring_kopen( struct kring_kern *kring, const char *ringset, int ring_id, enum KRING_MODE mode );
int kring_kclose( struct kring_kern *kring );

void kring_kwrite( struct kring_kern *kring, int dir, const struct sk_buff *skb );
int kring_kavail( struct kring_kern *kring );

void kring_knext_plain( struct kring_kern *kring, struct kring_plain *plain );

#endif
