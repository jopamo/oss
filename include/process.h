#ifndef PROCESS_H
#define PROCESS_H

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

extern pid_t timekeeperPid;
extern pid_t tableprinterPid;
extern pid_t parentPid;

void setupSignalHandlers(void);
void waitForChildProcesses(void);
void signalSafeLog(const char *msg);

#endif
