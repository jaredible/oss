/*
 * queue.h December 2, 2020
 * Jared Diehl (jmddnb@umsystem.edu)
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>

typedef struct QueueNode {
	int index;
	struct QueueNode *next;
} QueueNode;

typedef struct {
	QueueNode *front;
	QueueNode *rear;
	int count;
} Queue;

Queue *queue_create();
QueueNode *queue_node(int);
void queue_push(Queue*, int);
QueueNode *queue_pop(Queue*);
void queue_remove(Queue*, int);
bool queue_empty(Queue*);
int queue_size(Queue*);

#endif
