#include "cleanup.h"
#include "shared.h"

volatile sig_atomic_t cleanupInitiated = 0;

void atexitHandler(void) { cleanupResources(); }

void cleanupResources(void) {
  if (cleanupInitiated)
    return;
  cleanupInitiated = 1;

  if (logFile != NULL) {
    fclose(logFile);
    logFile = NULL;
  }

  killAllWorkers();
  sleep(2);

  if (detachSharedMemory() == 0) {
    log_debug("[OSS] Shared memory detached successfully.");
  }

  cleanupSharedMemory();

  if (shmId != -1) {
    if (shmctl(shmId, IPC_RMID, NULL) == -1) {
      perror("Shared memory control IPC_RMID failed");
    } else {
      log_debug("[OSS] Shared memory segment marked for deletion.");
    }
  }

  cleanupMessageQueue();
}

void signalHandler(int sig) {
  if (sig == SIGALRM || sig == SIGINT) {
    keepRunning = 0;
    if (sig == SIGALRM) {
      printf("[OSS] Maximum runtime reached. Initiating cleanup...\n");
    } else if (sig == SIGINT) {
      printf("[OSS] Interrupt signal received. Initiating cleanup...\n");
    }
    killAllWorkers();
  }
}

void killAllWorkers(void) {
  Message msg = {.mtype = 1, .mtext = 0};
  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      if (sendMessage(msqId, &msg) == -1) {
        fprintf(stderr, "Failed to send termination message to process %d\n",
                processTable[i].pid);
      } else {
        printf("[OSS] Termination message sent to PID %d\n",
               processTable[i].pid);
      }
      processTable[i].occupied = 0;
    }
  }
}

int cleanupSharedMemory(void) {
  if (shmId != -1) {
    if (shmdt(simClock) != -1) {
      perror("Shared memory detach failed");
    }
    simClock = NULL;

    if (shmctl(shmId, IPC_RMID, NULL) == -1) {
      perror("Shared memory control IPC_RMID failed");
      return -1;
    }
    shmId = -1;
  }
  return 0;
}

int cleanupMessageQueue(void) {
  if (msqId != -1) {
    if (msgctl(msqId, IPC_RMID, NULL) == -1) {
      perror("Message queue control IPC_RMID failed");
      return -1;
    }
    msqId = -1;
  }
  return 0;
}

void setupSignalHandlers(void) {
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));

  sa.sa_handler = signalHandler;

  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("[OSS] Error setting SIGINT handler");
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGALRM, &sa, NULL) == -1) {
    perror("[OSS] Error setting SIGALRM handler");
    exit(EXIT_FAILURE);
  }

  sa.sa_handler = SIG_IGN;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    log_error("Error setting up SIGCHLD handler");
    exit(EXIT_FAILURE);
  }
}

void cleanupAndExit(void) {
  killAllWorkers();
  cleanupResources();
  if (logFile != NULL) {
    fclose(logFile);
  }

  printf("[OSS] Cleanup completed.\n");
}
