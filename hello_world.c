#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/moduleparam.h>

static char * who = "world";
static int repeat = 1;

module_param(who, charp, S_IRUGO);
module_param(repeat, int, S_IRUGO);

static int __init hello_init(void)
{
	int i;
	for (i = 0; i < repeat; i++)
		printk(KERN_ALERT "hello %s\n", who);

	return 0;
}

static void __exit hello_exit(void)
{
	printk(KERN_ALERT "'hello_world' module unloaded\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
