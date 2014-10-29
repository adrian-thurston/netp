#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("colm networks proprietary");
MODULE_AUTHOR("colm networks");
MODULE_DESCRIPTION("module test");

static int __init ring_init(void)
{
	printk( KERN_INFO "ring init\n" );
	return 0;
}

static void __exit ring_exit(void)
{
	printk( KERN_INFO "ring exit\n" );
}

module_init(ring_init);
module_exit(ring_exit);
