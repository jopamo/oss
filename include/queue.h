#ifndef QUEUE_H
#define QUEUE_H

#include "globals.h"
#include "shared.h"

typedef struct {
  Message *queue;
  int front;
  int rear;
  int capacity;
  int size;
} Queue;

typedef struct {
  Queue q0;
  Queue q1;
  Queue q2;
  Queue blockedQueue;
} MLFQ;

extern MLFQ mlfq;

void freeQueue(Queue *q);
int initQueue(Queue *q, int capacity);
int enqueue(Queue *q, Message item);
int dequeue(Queue *q, Message *item);
int isQueueEmpty(Queue *q);
int isQueueFull(Queue *q);
int removeFromQueue(Queue *q, pid_t pid);

int initMLFQ(MLFQ *mlfq, int capacity);
void freeMLFQ(MLFQ *mlfq);
int addProcessToMLFQ(MLFQ *mlfq, Message item, int queueLevel);
int getNextProcessFromMLFQ(MLFQ *mlfq, Message *item);
int promoteProcessInMLFQ(MLFQ *mlfq, Message item);
int demoteProcessInMLFQ(MLFQ *mlfq, Message item);

#endif
