#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
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

int scull_read_procmem(char * buf, char ** start, off_t offset, int count, int * eof, void * data)
{
	int i, j, len = 0;
	int limit = count - 80; /* Don't print more than this */

	struct scull_qset * qset = scdev.data;
	if (mutex_lock_interruptible(&(scdev.lock)))
		return -ERESTARTSYS;

	len += sprintf(buf+len,"\nDevice %i: qset %i, q %i, sz %li\n", i, scdev.qset_size, scdev.quantum_size, scdev.data_size);

	for (; qset && len <= limit; qset = qset->next) { /* scan the list */
		len += sprintf(buf + len, " item at %p, qset at %p\n", qset, qset->quanta);
		if (qset->quanta && !qset->next) /* dump only the last item */
			for (j = 0; j < scdev.qset_size; j++) {
				if (qset->quanta[j])
					len += sprintf(buf + len," % 4i: %8p\n", j, qset->quanta[j]);
			}

		mutex_unlock(&(scdev.lock));
	}

	*eof = 1;
	return len;
}

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

struct scull_qset * scull_follow(struct scull_dev * dev, int count)
{
	struct scull_qset * qset = dev->data;

        /* Allocate first qset explicitly if need be */
	if (!qset) {
		qset = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
		if (qset == NULL)
			return NULL;  /* Never mind */
		memset(qset, 0, sizeof(struct scull_qset));
	}

	/* Then follow the list */
	while (count--) {
		if (!qset->next) {
			qset->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
			if (qset->next == NULL)
				return NULL;  /* Never mind */
			memset(qset->next, 0, sizeof(struct scull_qset));
		}
		qset = qset->next;
		continue;
	}

	return qset;
}

ssize_t scull_read(struct file * filp, char __user * buf, size_t count, loff_t * f_pos)
{
	struct scull_qset * node_ptr;

	int quantum_size = scdev.quantum_size;
	int qset_size = scdev.qset_size;
	int node_size = quantum_size * qset_size;
	int node_idx, read_size, qset_idx, quantum_idx;

	ssize_t retval = 0;

	if (mutex_lock_interruptible(&(scdev.lock)))
		return -ERESTARTSYS;
	if (*f_pos >= scdev.data_size)
		goto out;
	if (*f_pos + count > scdev.data_size)
		count = scdev.data_size - *f_pos;

	node_idx = (long) *f_pos / node_size;
	read_size = (long) *f_pos % node_size; // Remaining bytes
	qset_idx = read_size / quantum_size;
	quantum_idx = read_size % quantum_size;

	node_ptr = scull_follow(&scdev, node_idx);

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
	mutex_unlock(&(scdev.lock));
	return retval;
}

ssize_t scull_write(struct file * filp, const char __user * buf, size_t count, loff_t * f_pos)
{
	struct scull_qset * node_ptr;

	int quantum_size = scdev.quantum_size, qset_size = scdev.qset_size;
	int node_size = quantum_size * qset_size;
	int node_idx, read_size, qset_idx, quantum_idx;
	ssize_t retval = -ENOMEM;

	if (mutex_lock_interruptible(&(scdev.lock)))
		return -ERESTARTSYS;

	node_idx = (long) *f_pos / node_size;
	read_size = (long) *f_pos % node_size; // Remaining bytes
	qset_idx = read_size / quantum_size;
	quantum_idx = read_size % quantum_size;

	node_ptr = scull_follow(&scdev, node_idx);
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
	if (scdev.data_size < *f_pos)
		scdev.data_size = *f_pos;

out:
	mutex_unlock(&(scdev.lock));
	return retval;
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

	dev->quantum_size = scull_quantum_size;
	dev->qset_size = scull_qset_size;

	mutex_init(&(dev->lock));

	cdev_init(&(dev->cdev), &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;

	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		PDEBUG("Error %d adding scull%d", err, index);
}

static int __init scull_init(void)
{
	int result;

	if (scull_major) {
		devno = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(devno, 1, "scull");
	} else {
		result = alloc_chrdev_region(&devno, scull_minor, 1, "scull");
		scull_major = MAJOR(devno);
	}

	if (result < 0) {
		PDEBUG("scull: can't get major %d\n", scull_major);
		return result;
	}

		scull_setup_cdev(&scdev, 0);

	PDEBUG("scull init success");
	return 0; 
}

static void __exit scull_exit(void)
{
	cdev_del(&(scdev.cdev));
	unregister_chrdev_region(devno, scull_nr_devs);
	PDEBUG("scull exit success");
}

module_init(scull_init);
module_exit(scull_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("nfrizzell");
MODULE_DESCRIPTION("scull driver from LDD3 reworked for kernel version 5.15");
