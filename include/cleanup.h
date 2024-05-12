#ifndef CLEANUP_H
#define CLEANUP_H

#include <signal.h>
#include <sys/types.h>

extern volatile sig_atomic_t cleanupInitiated;

int messageQueue_cleanup(void);
void cleanupAndExit(void);
void cleanupResources(void);
void cleanupSharedMemorySegment(int shmId, const char *segmentName);
void logFile_cleanup(void);
int semUnlinkCreate(void);
void semaphore_cleanup(void);
void sharedMemory_cleanup(void);

#endif
