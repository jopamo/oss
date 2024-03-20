#ifndef CLEANUP_H
#define CLEANUP_H

#include <signal.h>


void cleanup();
void cleanupAndExit(int signum);
void atexitHandler();
void signalHandler(int sig);

#endif 
