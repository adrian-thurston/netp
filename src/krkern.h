#ifndef __KRKERN_H
#define __KRKERN_H

#include "kring.h"

#include <linux/skbuff.h>

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

	struct kdata_ring_reader reader[KRING_READERS];
	struct kdata_ring_writer writer[KRING_WRITERS];

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

#endif
