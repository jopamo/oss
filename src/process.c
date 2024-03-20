#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

#include "shared.h"
#include "process.h"

extern int shmId;
extern int msqId;
extern PCB processTable[MAX_WORKERS];
extern int currentChildren;
extern int maxSimultaneous;

extern SystemClock* sysClock;

void launchWorkerProcess(int index) {
  if (index < 0 || index >= MAX_PROCESSES) {
    fprintf(stderr, "launchWorkerProcess: Index out of bounds.\n");
    return;
  }

  pid_t pid = fork();

  if (pid < 0) {
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    char shmIdStr[10];
    char msqIdStr[10];

    snprintf(shmIdStr, sizeof(shmIdStr), "%d", shmId);
    snprintf(msqIdStr, sizeof(msqIdStr), "%d", msqId);

    execl("./worker", "worker", shmIdStr, msqIdStr, (char*)NULL);

    perror("execl failed to run worker");
    exit(EXIT_FAILURE);
  } else {
    processTable[index].occupied = 1;
    processTable[index].pid = pid;

    printf("Launched worker process with PID %d at index %d\n", pid, index);
  }
}

int findFreeProcessTableEntry() {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (!processTable[i].occupied) {
      return i;
    }
  }

  return -1;
}

void updateProcessTableOnFork(int index, pid_t pid) {
  if (index >= 0 && index < MAX_PROCESSES) {
    processTable[index].occupied = 1;
    processTable[index].pid = pid;

    processTable[index].startSeconds = sysClock->seconds;
    processTable[index].startNano = sysClock->nanoseconds;

    printf("Process table updated at index %d with PID %d\n", index, pid);
  } else {
    fprintf(stderr, "Invalid index %d for process table.\n", index);
  }
}

void updateProcessTableOnTerminate(int index) {
  if (index >= 0 && index < MAX_PROCESSES) {
    processTable[index].occupied = 0;
    processTable[index].pid = -1;
    processTable[index].startSeconds = 0;
    processTable[index].startNano = 0;

    printf("Process table entry at index %d marked as free.\n", index);
  } else {
    fprintf(stderr, "Invalid index %d for process table.\n", index);
  }
}

void possiblyLaunchNewChild() {
  if (currentChildren < maxSimultaneous) {
    int index = findFreeProcessTableEntry();
    if (index != -1) {
      launchWorkerProcess(index);
      currentChildren++;
    } else {
      printf("No free process table entries available.\n");
    }
  } else {
    printf("Maximum number of child processes already running.\n");
  }
}
