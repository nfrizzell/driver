#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>

static int scull_nr_devs = 1;

static int scull_major = 0;
static int scull_minor = 0;
static dev_t dev;

static int __init scull_init(void)
{
	int result;
	if (scull_major) {
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, scull_nr_devs, "scull");
	} else {
		result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs, "scull");
		scull_major = MAJOR(dev);
	}

	if (result < 0)
	{
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}

	return 0;
}

static void __exit scull_exit(void)
{
}

module_init(scull_init);
module_exit(scull_exit);

MODULE_LICENSE("GPL");
