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
#include "user_process.h"

typedef struct {
  long senderPid;   // Process ID
  int commandType;  // Type of command (request or release)
  int resourceType; // Type of resource
  int senderID;     // Main is 0, worker is 1
  int count;        // Number of resources
} Message;

typedef struct {
  int lifespanSeconds;
  int lifespanNanoSeconds;
} WorkerConfig;

Message createWorkerMessage(pid_t senderPid, int commandType, int resourceType);
Message createPsmgmtMessage(pid_t senderPid, int commandType, int resourceType,
                            int count);

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
