#ifndef _KRING_MODULE_H
#define _KRING_MODULE_H

int kring_init( void );
int kring_exit( void );

struct kring
{
	struct kobject kobj;
};

#endif
