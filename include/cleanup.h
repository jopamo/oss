#ifndef CLEANUP_H
#define CLEANUP_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

void initializeSemaphore(const char *semName);
int initSharedMemorySegment(key_t key, size_t size, const char *segmentName);
void cleanupResources(void);
void semaphore_cleanup(void);
void logFile_cleanup(void);
void killAllWorkers(void);
void sharedMemory_cleanup(void);
void cleanupSharedMemorySegment(int shmId, const char *segmentName);
int messageQueue_cleanup(void);
void setupTimeout(int seconds);
void cleanupAndExit(void);
void atexitHandler(void);

#endif
