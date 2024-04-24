#ifndef PROCESS_H
#define PROCESS_H

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

extern pid_t timekeeperPid;
extern pid_t tableprinterPid;
extern pid_t parentPid;

void atexitHandler(void);
void childExitHandler(int sig);
void parentSignalHandler(int sig);
void sendSignalToChildGroups(int sig);
void setupParentSignalHandlers(void);
void waitForChildProcesses(void);
void ossSignalHandler(int sig);
void setupTimeout(int seconds);
void timeoutHandler(int signum);

#endif
