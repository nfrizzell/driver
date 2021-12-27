#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
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

#ifdef SCULL_DEBUG
/* 
The proc filesystem interface was changed in a recent kernel version, so much of
this will be different than the book. Thanks to https://github.com/martinezjavier/ldd3 
for providing working code to build off of.
*/
int scull_read_procmem(struct seq_file *s, void *v)
{
        int i;
        int limit = s->size - 80; /* Don't print more than this */

	struct scull_qset *cur = scdev.data;
	if (mutex_lock_interruptible(&(scdev.lock)))
		return -ERESTARTSYS;

	seq_printf(s,"\nscull: qset %i, q %i, sz %li\n", scdev.qset_size, scdev.quantum_size, scdev.data_size);

	for (; cur && s->count <= limit; cur = cur->next) { /* scan the list */
		seq_printf(s, "  item at %p, qset at %p\n", cur, cur->quanta);
		if (cur->quanta && !cur->next) /* dump only the last item */
			for (i = 0; i < scdev.qset_size; i++) {
				if (cur->quanta[i])
					seq_printf(s, "    % 4i: %8p\n",
						     i, cur->quanta[i]);
			}
	}
	mutex_unlock(&(scdev.lock));
        return 0;
}

static void *scull_seq_start(struct seq_file *s, loff_t *pos)
{
	return &scdev;
}

/* Only one device currently */
static void *scull_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return NULL;
}

static void scull_seq_stop(struct seq_file *s, void *v)
{
}

static int scull_seq_show(struct seq_file *s, void *v)
{
	struct scull_qset *cur;
	int i;

	if (mutex_lock_interruptible(&(scdev.lock)))
		return -ERESTARTSYS;

	seq_printf(s, "\nscull: qset %i, q %i, sz %li\n", scdev.qset_size, scdev.quantum_size, scdev.data_size);

	for (cur = scdev.data; cur; cur = cur->next) { /* scan the list */
		seq_printf(s, "  item at %p, qset at %p\n", cur, cur->quanta);
		if (cur->quanta && !cur->next) /* dump only the last item */
			for (i = 0; i < scdev.qset_size; i++) {
				if (cur->quanta[i])
					seq_printf(s, "    % 4i: %8p\n", i, cur->quanta[i]);
			}
	}
	mutex_unlock(&(scdev.lock));
	return 0;
}

static struct seq_operations scull_seq_ops = {
	.start = scull_seq_start,
	.next  = scull_seq_next,
	.stop  = scull_seq_stop,
	.show  = scull_seq_show
};

static int scullmem_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, scull_read_procmem, NULL);
}

static int scullseq_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &scull_seq_ops);
}

static struct proc_ops scullmem_proc_ops = {
	.proc_open = scullmem_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release
};

static struct proc_ops scullseq_proc_ops = {
	.proc_open = scullseq_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release
};

static void scull_create_proc(void)
{
	proc_create_data("scullmem", 0, NULL, &scullmem_proc_ops, NULL);
	proc_create("scullseq", 0, NULL, &scullseq_proc_ops);
}

static void scull_remove_proc(void)
{
	remove_proc_entry("scullmem", NULL);
	remove_proc_entry("scullseq", NULL);
}

#endif /* SCULL_DEBUG */

static int scull_trim(struct scull_dev *dev)
{
	int qset_size = dev->qset_size;

	struct scull_qset *next, *cur;
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

struct scull_qset *scull_follow(struct scull_dev *dev, int count)
{
	struct scull_qset *qset = dev->data;

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

ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct scull_qset *node_ptr;

	int quantum_size = scdev.quantum_size;
	int qset_size = scdev.qset_size;
	int node_size = quantum_size *qset_size;
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

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct scull_qset *node_ptr;

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

int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	/* Retain the scull_dev instance for other methods */
	filp->private_data = dev;

	/* Truncate if write only */
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY)
	{
		scull_trim(dev);
	}

	return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operations scull_fops = {
	.owner =	THIS_MODULE,
	.read =		scull_read,
	.write =	scull_write,
	.release = 	scull_release,
};

static void scull_setup_cdev(struct scull_dev *dev, int index)
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

#ifdef SCULL_DEBUG
	scull_create_proc();
#endif
	
	PDEBUG("scull init success");
	return 0; 
}

static void __exit scull_exit(void)
{
	cdev_del(&(scdev.cdev));
	unregister_chrdev_region(devno, scull_nr_devs);

#ifdef SCULL_DEBUG
	scull_remove_proc();
#endif

	PDEBUG("scull exit success");
}

module_init(scull_init);
module_exit(scull_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("nfrizzell");
MODULE_DESCRIPTION("scull driver from LDD3 reworked for kernel version 5.15");
