#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "shared.h"
#include "util.h"

int currentChildren = 0;
PCB processTable[MAX_WORKERS];
FILE* logFile = NULL;
char logFileName[256] = "oss.log";
SystemClock* sysClock = NULL;
int shmId = -1;
int msqId = -1;

int getCurrentChildren() { return currentChildren; }

void setCurrentChildren(int value) { currentChildren = value; }

int initSharedMemory() {
  int shmId = shmget(SHM_KEY, sizeof(SystemClock), IPC_CREAT | 0666);
  if (shmId < 0) {
    perror("shmget");
    return -1;
  }
  log_debug("Shared memory initialized successfully");
  return shmId;
}
