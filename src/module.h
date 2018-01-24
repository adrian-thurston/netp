#ifndef _SHUTTLE_MODULE_H
#define _SHUTTLE_MODULE_H

#include <linux/kobject.h>
#include <kring/krkern.h>

/* Root object. */
struct shuttle
{
	struct kobject kobj;
};

#define LINK_IPS 256

/* Passtrhough link. */
struct link
{
	struct kobject kobj;
	char name[32];
	struct net_device *inside, *outside;
	struct net_device *dev;

	__be32 ips[LINK_IPS];
	int nips;

	struct kring_kern kring;
	struct kring_kern cmd;

	struct list_head link_list;

};

int shuttle_init(void);
void shuttle_exit(void);

extern struct shuttle *root_obj;

#endif

