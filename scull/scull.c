#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>

#include "scull.h"

static int scull_major = SCULL_MAJOR;
static int scull_minor = SCULL_MINOR;

static int scull_nr_devs = SCULL_NR_DEVICES;

static int scull_node_size = SCULL_NODE_SIZE;
static int scull_num_nodes = SCULL_NUM_NODES;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_node_size, int, S_IRUGO);
module_param(scull_num_nodes, int, S_IRUGO);

static dev_t devno;
static struct scull_dev scdev;

int scull_trim (struct scull_dev * dev)
{
	int node_size = dev->node_size;

	struct scull_node * next, * cur;
	int i;
	for (cur = dev->data; cur; cur = next) {
		if (cur->arr) {
			for (i = 0; i < node_size; i++)
				kfree(cur->arr[i]);
			kfree(cur->arr);
			cur->arr = NULL;
		}
		next = cur->next;
		kfree(cur);
	}

	dev->total_size = 0;
	dev->node_size = scull_node_size;
	dev->num_nodes = scull_num_nodes;
	dev->data = NULL;

	return 0;
}

ssize_t scull_read (struct file *, char __user *, size_t, loff_t *)
{
	return 0;
}

ssize_t scull_write (struct file *, const char __user *, size_t, loff_t *)
{
	return 0;
}

int scull_open (struct inode * inode, struct file * filp)
{
	struct scull_dev * dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	/* Retain the scull_dev instance for other methods */
	filp->private_data = dev;

	/* Truncate if write only */
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
	{
		scull_trim(dev);
	}

	return 0;
}

int scull_release (struct inode * inode, struct file * filp)
{
	/* Nothing to deallocate/shut down */
	return 0;
}

static struct file_operations scull_fops = {
	.owner =	THIS_MODULE,
	.read =		scull_read,
	.write =	scull_write,
	.release = 	scull_release,
};

static void scull_setup_cdev(struct scull_dev * dev, int index)
{
	devno = MKDEV(scull_major, scull_minor + index);

	cdev_init(&(dev->cdev), &scull_fops);
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
