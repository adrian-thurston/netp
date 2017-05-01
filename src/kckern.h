#ifndef __KCKERN_H
#define __KCKERN_H

#include "kctl.h"

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

	struct kring_ring_reader reader[KCTL_READERS];
	struct kring_ring_writer writer[KCTL_WRITERS];

	wait_queue_head_t reader_waitqueue;
};

struct kring_ringset
{
	char name[KCTL_NLEN];
	wait_queue_head_t reader_waitqueue;

	struct kring_ring *ring;
	int nrings;

	struct kring_ringset *next;
};

struct kring_kern
{
	char name[KCTL_NLEN];
	struct kring_ringset *ringset;
	int ring_id;
	int writer_id;
	struct kring_user user;
};

int kctl_kopen( struct kring_kern *kring, const char *ringset, int ring_id, enum KCTL_MODE mode );
int kctl_kclose( struct kring_kern *kring );

void kctl_kwrite( struct kring_kern *kring, int dir, const struct sk_buff *skb );
int kctl_kavail( struct kring_kern *kring );

void kctl_knext_plain( struct kring_kern *kring, struct kring_plain *plain );

#endif
