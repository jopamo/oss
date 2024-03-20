#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cleanup.h"
#include "shared.h"

extern SystemClock* sysClock;
extern PCB processTable[MAX_WORKERS];
extern FILE* logFile;
extern int shmId;
extern int msqId;

volatile sig_atomic_t cleanupInitiated = 0;

void cleanupResources(void) {
  if (cleanupInitiated) return;
  cleanupInitiated = 1;

  printf("Cleaning up resources...\n");

  if (sysClock != NULL) {
    if (shmdt(sysClock) == -1) perror("shmdt failed");
    sysClock = NULL;
  }

  if (shmId != -1) {
    if (shmctl(shmId, IPC_RMID, NULL) == -1) perror("shmctl IPC_RMID failed");
    shmId = -1;
  }

  if (msqId != -1) {
    if (msgctl(msqId, IPC_RMID, NULL) == -1) perror("msgctl IPC_RMID failed");
    msqId = -1;
  }

  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      kill(processTable[i].pid, SIGTERM);
      waitpid(processTable[i].pid, NULL, 0);
    }
  }

  if (logFile != NULL) {
    fclose(logFile);
    logFile = NULL;
  }
}

void signalHandler(int signum) {
  cleanupResources();
  printf("Signal %d received, exiting...\n", signum);
  _Exit(EXIT_FAILURE);
}

void setupSignalHandlers(void) {
  struct sigaction sa;
  sa.sa_handler = signalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("Cannot set SIGINT handler");
  }

  if (sigaction(SIGTERM, &sa, NULL) == -1) {
    perror("Cannot set SIGTERM handler");
  }

  sa.sa_handler = SIG_IGN;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("Cannot set SIGCHLD handler");
  }
}

void atexitHandler(void) { cleanupResources(); }
