#ifndef RESOURCE_H
#define RESOURCE_H

#include "globals.h"
#include "shared.h"
#include "user_process.h"

int initQueue(Queue *q, int capacity);
void freeQueue(Queue *q);
int initializeQueues(MLFQ *mlfq);
void freeQueues(MLFQ *mlfq);

#endif
