#ifndef CLEANUP_H
#define CLEANUP_H

#include <signal.h>

void atexitHandler();
void signalHandler(int sig);
void setupSignalHandlers(void);
void cleanupResources(void);

#endif
