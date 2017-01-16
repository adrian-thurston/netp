#include "attribute.h"
#include <linux/kobject.h>

/* Root object. */
struct kring
{
	struct kobject kobj;
};

static int kring_init(void)
{
	return 0;
}

static void kring_exit(void)
{
}
