#ifndef CLEANUP_H
#define CLEANUP_H

#include "shared.h"

extern volatile sig_atomic_t cleanupInitiated;

void initializeProcessTable(void);
void semUnlinkCreate(void);
void atexitHandler(void);
void cleanupResources(void);
void semaphore_cleanup(void);
void logFile_cleanup(void);
void cleanupSharedMemorySegment(int shmId, const char *segmentName);
void sharedMemory_cleanup(void);
int messageQueue_cleanup(void);
void killAllWorkers(void);
void timeoutHandler(int signum);
void setupTimeout(int seconds);

void parentSignalHandler(int sig);
void setupParentSignalHandlers(void);

void cleanupAndExit(void);
pid_t forkAndExecute(const char *executable);

#endif
