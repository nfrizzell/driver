#ifndef SCULL_H
#define SCULL_H

#include <linux/ioctl.h>

#undef PDEBUG /* undef it, just in case */
#ifdef SCULL_DEBUG
#    ifdef __KERNEL__
#        define PDEBUG(fmt, args...) printk( KERN_DEBUG "scull: " fmt, ## args)
#    else
#        define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#        endif
#else
#   define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif
#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#define SCULL_NR_DEVICES 1

#define SCULL_MAJOR 0
#define SCULL_MINOR 0

#define SCULL_QUANTUM_SIZE 4000
#define SCULL_QSET_SIZE 1000

#define SCULL_IOCTL_MAGIC_NUM 0xFF

#define SCULL_IOCTL_RESET		_IO(SCULL_IOCTL_MAGIC_NUM, 0)

#define SCULL_IOCTL_SET_PTR_QUANTUM 	_IOW(SCULL_IOCTL_MAGIC_NUM, 1, int)
#define SCULL_IOCTL_SET_PTR_QSET 	_IOW(SCULL_IOCTL_MAGIC_NUM, 2, int)
#define SCULL_IOCTL_SET_QUANTUM 	_IO(SCULL_IOCTL_MAGIC_NUM, 3)
#define SCULL_IOCTL_SET_QSET 		_IO(SCULL_IOCTL_MAGIC_NUM, 4)
#define SCULL_IOCTL_GET_PTR_QUANTUM 	_IOR(SCULL_IOCTL_MAGIC_NUM, 5, int)
#define SCULL_IOCTL_GET_PTR_QSET	_IOR(SCULL_IOCTL_MAGIC_NUM, 6, int)
#define SCULL_IOCTL_GET_QUANTUM		_IO(SCULL_IOCTL_MAGIC_NUM, 7)
#define SCULL_IOCTL_GET_QSET 		_IO(SCULL_IOCTL_MAGIC_NUM, 8)
#define SCULL_IOCTL_SWAP_PTR_QUANTUM	_IOWR(SCULL_IOCTL_MAGIC_NUM, 9, int)
#define SCULL_IOCTL_SWAP_PTR_QSET	_IOWR(SCULL_IOCTL_MAGIC_NUM, 10, int)
#define SCULL_IOCTL_SWAP_QUANTUM	_IO(SCULL_IOCTL_MAGIC_NUM, 11)
#define SCULL_IOCTL_SWAP_QSET		_IO(SCULL_IOCTL_MAGIC_NUM, 12)

#define SCULL_IOCTL_MAXNR 14

struct scull_qset {
	struct scull_qset * next;
	void ** quanta;
};

struct scull_dev {
	int qset_size;
	int quantum_size;
	unsigned long data_size;	/* Total size of data stored in bytes */
	struct scull_qset * data;	/* Linked-list of data */
	struct mutex lock;
	struct cdev cdev;		/* Char device structure */
};

#endif
