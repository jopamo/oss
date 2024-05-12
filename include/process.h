#ifndef PROCESS_H
#define PROCESS_H

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cleanup.h"
#include "resource.h"
#include "shared.h"
#include "user_process.h"

#define PROCESS_RUNNING 1
#define PROCESS_WAITING 2
#define PROCESS_TERMINATED 3

extern pid_t timekeeperPid;
extern pid_t tableprinterPid;
extern pid_t parentPid;

void waitForChildProcesses(void);
void parentSignalHandler(int sig);
void setupParentSignalHandlers(void);
void childExitHandler(int sig);
void atexitHandler(void);
void setupTimeout(int seconds);
void timeoutHandler(int signum);
void registerChildProcess(pid_t pid);
void terminateProcess(pid_t pid);
int findFreeProcessTableEntry(void);
int stillChildrenToLaunch(void);
pid_t forkAndExecute(const char *executable);
void sendSignalToChildGroups(int sig);
void handleTermination(pid_t pid);
void freeAllProcessResources(int index);
void updateResourceAndProcessTables(void);
void logResourceTable(void);
void logProcessTable(void);
void decrementCurrentChildren(void);
const char *processStateToString(int state);
void clearProcessEntry(int index);
int killProcess(int pid, int sig);
int findProcessIndexByPID(pid_t pid);

#endif
