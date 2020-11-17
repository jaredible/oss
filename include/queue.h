/*
 * queue.h 11/17/20
 * Jared Diehl (jmddnb@umsystem.edu)
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>

typedef struct {
	unsigned int front, rear, size;
	unsigned int capacity;
	int *array;
} queue_t;

queue_t *queue_create(unsigned int);
void queue_push(queue_t*, int);
int queue_pop(queue_t*);
int queue_peek(queue_t*);
bool queue_full(queue_t*);
bool queue_empty(queue_t*);
void queue_display(queue_t*);

#endif
