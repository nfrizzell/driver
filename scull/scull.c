#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#include "scull.h"

static int scull_major = SCULL_MAJOR;
static int scull_minor = SCULL_MINOR;

static int scull_nr_devs = SCULL_NR_DEVICES;

static int scull_quantum_size = SCULL_QUANTUM_SIZE;
static int scull_qset_size = SCULL_QSET_SIZE;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum_size, int, S_IRUGO);
module_param(scull_qset_size, int, S_IRUGO);

static dev_t devno;
static struct scull_dev scdev;

int scull_trim(struct scull_dev * dev)
{
	int qset_size = dev->qset_size;

	struct scull_qset * next, * cur;
	int i;
	for (cur = dev->data; cur; cur = next) {
		if (cur->quanta) {
			for (i = 0; i < qset_size; i++)
				kfree(cur->quanta[i]);
			kfree(cur->quanta);
			cur->quanta = NULL;
		}
		next = cur->next;
		kfree(cur);
	}

	dev->quantum_size = scull_quantum_size;
	dev->qset_size = scull_qset_size;
	dev->data_size = 0;
	dev->data = NULL;

	return 0;
}

struct scull_qset * scull_follow(struct scull_dev * dev, int node_idx)
{
	return NULL;
}

ssize_t scull_read(struct file * filp, char __user * buf, size_t count, loff_t * f_pos)
{
	struct scull_dev * dev = filp->private_data;
	struct scull_qset * node_ptr;

	int quantum_size = dev->quantum_size;
	int qset_size = dev->qset_size;
	int node_size = quantum_size * qset_size;
	int node_idx, read_size, qset_idx, quantum_idx;

	ssize_t retval = 0;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;
	if (*f_pos >= dev->data_size)
		goto out;
	if (*f_pos + count > dev->data_size)
		count = dev->data_size - *f_pos;
	
	node_idx = (long) *f_pos / node_size;
	read_size = (long) *f_pos % node_size; // Remaining bytes
	qset_idx = read_size / quantum_size;
	quantum_idx = read_size % quantum_size;

	node_ptr = scull_follow(dev, node_idx);

	if (node_ptr == NULL || !node_ptr->quanta || !node_ptr->quanta[quantum_idx])
		goto out;
	
	/* Read amount specified by the smaller of either count or quantum_size */
	if (count > quantum_size - quantum_idx)
		count = quantum_size - quantum_idx;

	if (copy_to_user(buf, node_ptr->quanta[qset_idx] + quantum_idx, count)) {
		retval = -EFAULT;
		goto out;
	}

	*f_pos += count;
	retval = count;

out:
	mutex_unlock(&dev->lock);
	return retval;
}

ssize_t scull_write(struct file * filp, const char __user * buf, size_t count, loff_t * f_pos)
{
	struct scull_dev * dev = filp->private_data;
	struct scull_qset * node_ptr;
	int quantum_size = dev->quantum_size, qset_size = dev->qset_size;
	int node_size = quantum_size * qset_size;
	int node_idx, read_size, qset_idx, quantum_idx;
	ssize_t retval = -ENOMEM;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	node_idx = (long) *f_pos / node_size;
	read_size = (long) *f_pos % node_size; // Remaining bytes
	qset_idx = read_size / quantum_size;
	quantum_idx = read_size % quantum_size;

	node_ptr = scull_follow(dev, node_idx);
	if (node_ptr == NULL)
		goto out;
	if (!node_ptr->quanta) {
		node_ptr->quanta = kmalloc(qset_size * sizeof(char*), GFP_KERNEL);
		if (!node_ptr->quanta)
			goto out;
		memset(node_ptr->quanta, 0, qset_size * sizeof(char*));
	}
	if (!node_ptr->quanta[quantum_idx]) {
		node_ptr->quanta[quantum_idx] = kmalloc(quantum_size, GFP_KERNEL);
		if (!node_ptr->quanta[quantum_idx])
			goto out;
	}

	/* Write amount specified by the smaller of either count or quantum_size */
	if (count > quantum_size - quantum_idx)
		count = quantum_size - quantum_idx;

	if (copy_from_user(node_ptr->quanta[quantum_idx] + qset_idx, buf, count)) {
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	retval = count;

	/* Update size */
	if (dev->data_size < *f_pos)
		dev->data_size = *f_pos;

out:
	mutex_unlock(&dev->lock);
	return 0;
}

int scull_open(struct inode * inode, struct file * filp)
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

int scull_release(struct inode * inode, struct file * filp)
{
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
	int err, devno = MKDEV(scull_major, scull_minor + index);

	cdev_init(&(dev->cdev), &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;

	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}

static int __init scull_init(void)
{
	int result;

	printk(KERN_NOTICE "scull load");

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
