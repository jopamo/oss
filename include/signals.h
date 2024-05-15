#ifndef SIGNAL_H
#define SIGNAL_H

#include "globals.h"
#include "process.h"
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

#endif
