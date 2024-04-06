#ifndef CLEANUP_H
#define CLEANUP_H

#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

void cleanupResources(void);
void semaphore_cleanup(void);
void logFile_cleanup(void);
void killAllWorkers(void);
int cleanupSharedMemory(void);
int cleanupMessageQueue(void);
void cleanupAndExit(void);
void atexitHandler(void);

void sharedMemory_cleanup(void);
int messageQueue_cleanup(void);

int cleanupSharedMemorySegment(void **segment, int shmId);
void setupTimeout(int seconds);

#endif
