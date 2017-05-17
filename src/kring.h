#ifndef _KRING_H
#define _KRING_H

#include "kdata.h"
#include "kctrl.h"

#define KRING_ERR_SOCK       -1
#define KRING_ERR_MMAP       -2
#define KRING_ERR_BIND       -3
#define KRING_ERR_READER_ID  -4
#define KRING_ERR_WRITER_ID  -5
#define KRING_ERR_RING_N     -6
#define KRING_ERR_ENTER      -7

struct kring_addr
{
	char name[KCTRL_NLEN];
	int ring_id;
	enum KRING_MODE mode;
};


#endif
