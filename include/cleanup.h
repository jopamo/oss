#ifndef CLEANUP_H
#define CLEANUP_H

#include "cleanup.h"
#include "globals.h"
#include "process.h"
#include "shared.h"
#include "user_process.h"

#include <signal.h>
#include <sys/types.h>

extern volatile sig_atomic_t cleanupInitiated;

int messageQueue_cleanup(void);
pid_t forkAndExecute(const char *executable);
void cleanupAndExit(void);
void cleanupResources(void);
void cleanupSharedMemorySegment(int shmId, const char *segmentName);
void logFile_cleanup(void);
int semUnlinkCreate(void);
void semaphore_cleanup(void);
void sharedMemory_cleanup(void);
void killProcess(int pid);

void sendSignalToChildGroups(int sig);

void initializeSharedResources(void);

#endif
