#ifndef SHARED_H
#define SHARED_H

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "globals.h"

int getCurrentChildren(void);
void setCurrentChildren(int value);

void initializeSemaphore(void);
void initializeSimulatedClock(void);
void initializeActualTime(void);
void initializeProcessTable(void);
void initializeResourceTable(void);
void initializeSharedResources(void);
void cleanupSharedResources(void);
void initializeResourceDescriptors(ResourceDescriptor *rd);

int checkSafety(int pid, int resourceType, int request);
void log_resource_state(const char *operation, int pid, int resourceType,
                        int quantity, int availableBefore, int availableAfter);
int requestResource(int resourceType, int quantity, int pid);
int releaseResource(int resourceType, int quantity, int pid);

void initQueue(Queue *q, int capacity);
void initializeQueues(void);

void *attachSharedMemory(const char *path, int proj_id, size_t size,
                         const char *segmentName);
int detachSharedMemory(void **shmPtr, const char *segmentName);
void log_message(int level, const char *format, ...);
key_t getSharedMemoryKey(const char *path, int proj_id);
int initMessageQueue(void);
int sendMessage(int msqId, Message *msg);
int receiveMessage(int msqId, Message *msg, long msgType, int flags);

#endif
