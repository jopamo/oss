#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>

void launchWorkerProcess(int index);
int findFreeProcessTableEntry();
void updateProcessTableOnFork(int index, pid_t pid);
void updateProcessTableOnTerminate(int index);
void possiblyLaunchNewChild();

#endif
