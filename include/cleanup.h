#ifndef CLEANUP_H
#define CLEANUP_H

#include "shared.h"

extern volatile sig_atomic_t cleanupInitiated;

int messageQueue_cleanup(void);
pid_t forkAndExecute(const char *executable);
void atexitHandler(void);
void childExitHandler(int sig);
void cleanupAndExit(void);
void cleanupResources(void);
void cleanupSharedMemorySegment(int shmId, const char *segmentName);
void initializeProcessTable(void);
void killAllWorkers(void);
void logFile_cleanup(void);
void parentSignalHandler(int sig);
void semUnlinkCreate(void);
void semaphore_cleanup(void);
void setupParentSignalHandlers(void);
void setupTimeout(int seconds);
void sharedMemory_cleanup(void);
void timeoutHandler(int signum);

#endif
