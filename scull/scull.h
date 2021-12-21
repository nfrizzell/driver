#ifndef SCULL_H
#define SCULL_H

#include <linux/ioctl.h>

#define SCULL_NR_DEVICES 1

#define SCULL_MAJOR 0
#define SCULL_MINOR 0

#define SCULL_QUANTUM_SIZE 1000;
#define SCULL_QSET_SIZE 4000;

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
