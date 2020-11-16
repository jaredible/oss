/*
 * queue.c 11/17/20
 * Jared Diehl (jmddnb@umsystem.edu)
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

Queue *queue_create(unsigned int capacity) {
	Queue *queue = (Queue*) malloc(sizeof(Queue));
	queue->capacity = capacity;
	queue->front = queue->size = 0;
	queue->rear = capacity - 1;
	queue->array = (int*) malloc(queue->capacity * sizeof(int));
	return queue;
}

void queue_push(Queue *queue, int item) {
	if (queue_full(queue)) return;
	queue->rear = (queue->rear + 1) % queue->capacity;
	queue->array[queue->rear] = item;
	queue->size = queue->size + 1;
}

int queue_pop(Queue *queue) {
	if (queue_empty(queue)) return -1;
	int item = queue->array[queue->front];
	memset(&queue->array[queue->front], 0, sizeof(int));
	queue->front = (queue->front + 1) % queue->capacity;
	queue->size = queue->size - 1;
	return item;
}

int queue_peek(Queue *queue) {
	if (queue_empty(queue)) return -1;
	int item = queue->array[queue->front];
	return item;
}

bool queue_full(Queue *queue) {
	return queue->size == queue->capacity;
}

bool queue_empty(Queue *queue) {
	return queue->size == 0;
}

void queue_display(Queue *queue) {
	int i;
	printf("Queue(%d): ", queue->size);
	for (i = 0; i < queue->size; i++) {
		int j = (queue->front + i) % queue->capacity;
		printf("%d ", queue->array[j]);
	}
	printf("\n");
}
