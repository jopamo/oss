#ifndef QUEUE_H
#define QUEUE_H

#include "globals.h"
#include "resource.h"
#include "shared.h"

typedef struct {
  MessageA5 *queue;
  int front;
  int rear;
  int capacity;
} Queue;

extern Queue resourceQueues[MAX_RESOURCES];

int initQueue(Queue *q, int capacity);
void freeQueue(Queue *q);
void enqueue(Queue *q, MessageA5 item);
int dequeue(Queue *q, MessageA5 *item);

#endif
