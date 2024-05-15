#ifndef CLEANUP_H
#define CLEANUP_H

#include <signal.h>

#include "globals.h"
#include "process.h"
#include "queue.h"
#include "resource.h"
#include "shared.h"
#include "user_process.h"

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
