#ifndef PROCESS_H
#define PROCESS_H

#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cleanup.h"
#include "globals.h"
#include "resource.h"
#include "shared.h"
#include "user_process.h"

#define PROCESS_RUNNING 1
#define PROCESS_WAITING 2
#define PROCESS_TERMINATED 3

void parentSignalHandler(int sig);
void setupParentSignalHandlers(void);
void childExitHandler(int sig);
void atexitHandler(void);
void setupTimeout(int seconds);
void timeoutHandler(int signum);
void registerChildProcess(pid_t pid);
int findFreeProcessTableEntry(void);
int stillChildrenToLaunch(void);
pid_t forkAndExecute(const char *executable);
void handleTermination(pid_t pid);
void freeAllProcessResources(int index);
void updateResourceAndProcessTables(void);
void logProcessTable(void);
void decrementCurrentChildren(void);
const char *processStateToString(int state);
void clearProcessEntry(int index);
int killProcess(int pid, int sig);
int findProcessIndexByPID(int pid);

#endif
