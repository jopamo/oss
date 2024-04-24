#ifndef USER_PROCESS_H
#define USER_PROCESS_H

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

void signalSafeLog(const char *msg);
void setupSignalHandlers(void);
void signalHandler(int sig);

#endif
