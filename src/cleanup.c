#include "cleanup.h"
#include "shared.h"

volatile sig_atomic_t cleanupInitiated = 0;

void cleanupResources(void) {
  if (cleanupInitiated) return;
  cleanupInitiated = 1;

  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      kill(processTable[i].pid, SIGTERM);
      waitpid(processTable[i].pid, NULL, 0);
      processTable[i].occupied = 0;
    }
  }

  if (simClock != NULL) {
    if (shmdt(simClock) == -1) perror("shmdt failed");
    if (shmId != -1 && shmctl(shmId, IPC_RMID, NULL) == -1)
      perror("shmctl IPC_RMID failed");
    shmId = -1;
    simClock = NULL;
  }

  if (msqId != -1) {
    if (msgctl(msqId, IPC_RMID, NULL) == -1) perror("msgctl IPC_RMID failed");
    msqId = -1;
  }

  if (logFile != NULL) {
    fclose(logFile);
    logFile = NULL;
  }
}

void signalHandler(int signum) {
  if (!cleanupInitiated) {
    cleanupInitiated = 1;
    cleanupResources();
  }
  printf("Signal %d received, exiting...\n", signum);
  _Exit(EXIT_SUCCESS);
}

void setupSignalHandlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("Cannot set SIGINT handler");
  }

  if (sigaction(SIGTERM, &sa, NULL) == -1) {
    perror("Cannot set SIGTERM handler");
  }

  if (sigaction(SIGALRM, &sa, NULL) == -1) {
    perror("Cannot set SIGALRM handler");
  }
}

void atexitHandler(void) { cleanupResources(); }
