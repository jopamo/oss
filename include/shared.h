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

typedef struct {
  long senderPid;   // Message type, used for routing (e.g., process ID)
  int commandType;  // Command type (1 for request, 0 for release)
  int resourceType; // Resource type being requested or released
  int count;        // Number of instances being requested or released
} MessageA5;

int getCurrentChildren(void);
void setCurrentChildren(int value);

void cleanupSharedResources(void);

void *attachSharedMemory(const char *path, int proj_id, size_t size,
                         const char *segmentName);
int detachSharedMemory(void **shmPtr, const char *segmentName);
void log_message(int level, int logToFile, const char *format, ...);
key_t getSharedMemoryKey(const char *path, int proj_id);
int sendMessage(int msqId, const void *msg, size_t msgSize);
int receiveMessage(int msqId, void *msg, size_t msgSize, long msgType,
                   int flags);

#endif
