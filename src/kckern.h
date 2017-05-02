#ifndef __KCKERN_H
#define __KCKERN_H

#include "kctl.h"

#include <linux/skbuff.h>

struct kctl_ring_reader
{
	bool allocated;
};

struct kctl_ring_writer
{
	bool allocated;
};

struct kctl_ring
{
	void *ctrl;
	struct kctl_control control;
	struct kctl_page_desc *pd;
	
	int num_readers;
	int num_writers;

	struct kctl_ring_reader reader[KCTL_READERS];
	struct kctl_ring_writer writer[KCTL_WRITERS];

	wait_queue_head_t reader_waitqueue;
};

struct kctl_ringset
{
	char name[KCTL_NLEN];
	wait_queue_head_t reader_waitqueue;

	struct kctl_ring *ring;
	int nrings;

	struct kctl_ringset *next;
};

struct kctl_kern
{
	char name[KCTL_NLEN];
	struct kctl_ringset *ringset;
	int ring_id;
	int writer_id;
	struct kctl_user user;
};

int kctl_kopen( struct kctl_kern *kring, const char *ringset, int ring_id, enum KCTL_MODE mode );
int kctl_kclose( struct kctl_kern *kring );

void kctl_kwrite( struct kctl_kern *kring, int dir, const struct sk_buff *skb );
int kctl_kavail( struct kctl_kern *kring );

void kctl_knext_plain( struct kctl_kern *kring, struct kctl_plain *plain );

#endif
