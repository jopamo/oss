#ifndef PROCESS_H
#define PROCESS_H

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

void setupSignalHandlers(pid_t pid);
void genericSignalHandler(int sig);

#endif
