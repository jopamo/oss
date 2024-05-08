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

int initializeSimulatedClock(void);
int initializeActualTime(void);
int initializeProcessTable(void);
void initializeSharedResources(void);
void cleanupSharedResources(void);

void *attachSharedMemory(const char *path, int proj_id, size_t size,
                         const char *segmentName);
int detachSharedMemory(void **shmPtr, const char *segmentName);
void log_message(int level, const char *format, ...);
key_t getSharedMemoryKey(const char *path, int proj_id);
int initMessageQueue(void);
int sendMessage(int msqId, Message *msg);
int receiveMessage(int msqId, Message *msg, long msgType, int flags);

#endif
