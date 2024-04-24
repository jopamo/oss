#ifndef CLEANUP_H
#define CLEANUP_H

extern volatile sig_atomic_t cleanupInitiated;

int messageQueue_cleanup(void);
pid_t forkAndExecute(const char *executable);
void cleanupAndExit(void);
void cleanupResources(void);
void cleanupSharedMemorySegment(int shmId, const char *segmentName);
void initializeProcessTable(void);
void logFile_cleanup(void);
void semUnlinkCreate(void);
void semaphore_cleanup(void);
void sharedMemory_cleanup(void);

#endif
