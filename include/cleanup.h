#ifndef CLEANUP_H
#define CLEANUP_H

#include <signal.h>

void atexitHandler(void);
void cleanupResources(void);
int cleanupMessageQueue(void);
int cleanupSharedMemory(void);
void killAllWorkers(void);
void setupSignalHandlers(void);
void cleanupAndExit(void);
void signalHandler(int sig);

#endif
