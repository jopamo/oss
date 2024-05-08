#ifndef RESOURCE_H
#define RESOURCE_H

#include "globals.h"
#include "shared.h"
#include "user_process.h"

void log_resource_state(const char *operation, int pid, int resourceType,
                        int quantity, int availableBefore, int availableAfter);
int requestResource(int resourceType, int quantity, int pid);
int releaseResource(int resourceType, int quantity, int pid);

int initializeResourceTable(void);
int initializeResourceDescriptors(ResourceDescriptor *rd);

int checkSafety(int pid, int resourceType, int request);
int timeToCheckDeadlock(void);
int checkForDeadlocks(void);
void resolveDeadlocks(void);

void releaseResources(int pid, int resourceType, int quantity);
void handleResourceRequest(int pid, int resourceType, int quantity);
int getAvailable(int resourceType);
int getAvailableAfter(int resourceType);
int initializeResourceTable(void);

int initQueue(Queue *q, int capacity);
void freeQueue(Queue *q);
int initializeQueues(MLFQ *mlfq);
void freeQueues(MLFQ *mlfq);

#endif // RESOURCE_H
