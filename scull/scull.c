#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include "scull.h"

static int scull_major = 0;
static int scull_minor = 0;
static dev_t devno;

static int scull_nr_devs = 1;

static struct scull_dev scdev;

ssize_t scull_read (struct file *, char __user *, size_t, loff_t *)
{
	return 0;
}

ssize_t scull_write (struct file *, const char __user *, size_t, loff_t *)
{
	return 0;
}

int scull_open (struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operations scull_fops = {
	.owner =	THIS_MODULE,
	.read =		scull_read,
	.write =	scull_write,
};

static void scull_setup_cdev(struct scull_dev * dev, int index)
{
	devno = MKDEV(scull_major, scull_minor + index);

	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;

	int err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}

static int __init scull_init(void)
{
	printk(KERN_NOTICE "scull load");

	int result;

	if (scull_major) {
		devno = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(devno, scull_nr_devs, "scull");
	} else {
		result = alloc_chrdev_region(&devno, scull_minor, scull_nr_devs, "scull");
		scull_major = MAJOR(devno);
	}

	if (result < 0) {
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}

	scull_setup_cdev(&scdev, 0);

	return 0;
}

static void __exit scull_exit(void)
{
	printk(KERN_NOTICE "scull unload");

	cdev_del(&(scdev.cdev));
	unregister_chrdev_region(devno, scull_nr_devs);
}

module_init(scull_init);
module_exit(scull_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("nfrizzell");
MODULE_DESCRIPTION("scull driver from LDD3 reworked for kernel version 5.15");
