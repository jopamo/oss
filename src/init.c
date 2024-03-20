#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/time.h>

#include "init.h"
#include "ipc.h"
#include "shared.h"
#include "util.h"

struct timeval startTime;

extern int shmId;
extern int msqId;
extern SystemClock* sysClock;
extern PCB processTable[MAX_WORKERS];
extern FILE* logFile;
extern char logFileName[256];

int initMessageQueue() {
  msqId = msgget(MSG_KEY, IPC_CREAT | 0666);
  if (msqId < 0) {
    perror("msgget");
    return -1;
  }
  log_debug("Message queue initialized successfully");
  return msqId;
}

void initializeSimulationEnvironment() {
  gettimeofday(&startTime, NULL);

  shmId = initSharedMemory();
  if (shmId == -1) {
    fprintf(stderr, "Failed to initialize shared memory.\n");
    exit(EXIT_FAILURE);
  }
  sysClock = attachSharedMemory(shmId);
  if (sysClock == (void*)-1) {
    fprintf(stderr, "Failed to attach shared memory.\n");
    exit(EXIT_FAILURE);
  }

  sysClock->seconds = 0;
  sysClock->nanoseconds = 0;

  msqId = initMessageQueue();
  if (msqId == -1) {
    fprintf(stderr, "Failed to initialize message queue.\n");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < MAX_WORKERS; i++) {
    processTable[i].occupied = 0;
    processTable[i].pid = -1;
    processTable[i].startSeconds = 0;
    processTable[i].startNano = 0;
  }

  logFile = fopen(logFileName, "w");
  if (logFile == NULL) {
    fprintf(stderr, "Failed to open log file %s\n", logFileName);
    exit(EXIT_FAILURE);
  }

  printf("Shared memory initialized (ID: %d).\n", shmId);
  printf("Message queue initialized (ID: %d).\n", msqId);
  printf("Process table initialized.\n");
  printf("Log file %s opened.\n", logFileName);
}
