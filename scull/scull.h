#ifndef SCULL_H
#define SCULL_H

#define SCULL_NR_DEVICES 1

#define SCULL_MAJOR 0
#define SCULL_MINOR 0

#define SCULL_NUM_NODES 1000;
#define SCULL_NODE_SIZE 4000;

struct scull_node {
	struct scull_node * next;
	void ** arr;
};

struct scull_dev {
	int num_nodes;
	int node_size;
	unsigned long total_size;
	struct scull_node * data;	/* Linked-list of data */
	struct cdev cdev;		/* Char device structure */
};

#endif
