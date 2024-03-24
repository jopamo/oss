#ifndef PROCESS_H
#define PROCESS_H

#define INT_MAX_STR_SIZE (sizeof(char) * ((CHAR_BIT * sizeof(int) - 1) / 3 + 2))

#include <sys/types.h>

void launchWorkerProcess(int index, unsigned int lifespanSec, unsigned int lifespanNSec);
int findFreeProcessTableEntry();
void updateProcessTableOnFork(int index, pid_t pid);
void updateProcessTableOnTerminate(int index);
void possiblyLaunchNewChild();
void manageProcesses();
void logProcessTable();

#endif
