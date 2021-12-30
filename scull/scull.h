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

#define SCULL_QUANTUM_SIZE 4000;
#define SCULL_QSET_SIZE 1000;

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
