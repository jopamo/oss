#include "cleanup.h"
#include "shared.h"

volatile sig_atomic_t cleanupInitiated = 0;

void setupSignalHandlers(void) {
  struct sigaction sa = {0};

  sa.sa_handler = signalHandler;
  sigemptyset(&sa.sa_mask);

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    log_message(LOG_LEVEL_ERROR, "Error setting SIGINT handler: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (sigaction(SIGALRM, &sa, NULL) == -1) {
    log_message(LOG_LEVEL_ERROR, "Error setting SIGALRM handler: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }

  sa.sa_handler = SIG_IGN;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    log_message(LOG_LEVEL_ERROR, "Error setting SIGCHLD to ignore: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void atexitHandler(void) {

  if (!cleanupInitiated) {
    cleanupResources();
  }
}

void cleanupResources(void) {
  if (cleanupInitiated)
    return;
  cleanupInitiated = 1;
  log_message(LOG_LEVEL_INFO, "Initiating cleanup process...");

  if (clockSem != NULL) {
    if (sem_close(clockSem) == -1) {
      log_message(LOG_LEVEL_ERROR, "Failed to close semaphore: %s",
                  strerror(errno));
    }
    if (gProcessType == PROCESS_TYPE_OSS) {
      if (sem_unlink(clockSemName) == -1) {
        log_message(LOG_LEVEL_ERROR, "Failed to unlink semaphore: %s",
                    strerror(errno));
      }
    }
    clockSem = NULL;
  }

  if (logFile) {
    fclose(logFile);
    logFile = NULL;
    log_message(LOG_LEVEL_DEBUG, "Log file closed successfully.");
  }

  if (gProcessType == PROCESS_TYPE_OSS) {
    killAllWorkers();
  }

  if (detachSharedMemory() == 0) {
    log_message(LOG_LEVEL_DEBUG, "Shared memory detached successfully.");
  }

  if (gProcessType == PROCESS_TYPE_OSS && cleanupSharedMemory() == 0) {
    log_message(LOG_LEVEL_DEBUG, "Shared memory segment marked for deletion.");
  }

  if (gProcessType == PROCESS_TYPE_OSS && cleanupMessageQueue() == 0) {
    log_message(LOG_LEVEL_DEBUG, "Message queue removed successfully.");
  }
}

void signalHandler(int sig) {
  char *signalType =
      sig == SIGALRM ? "Maximum runtime reached" : "Interrupt signal received";
  log_message(LOG_LEVEL_INFO, "%s. Initiating cleanup...", signalType);
  cleanupAndExit();
}

void killAllWorkers(void) {
  log_message(LOG_LEVEL_INFO,
              "Sending termination signals to all worker processes.");
  Message msg = {.mtype = 1, .mtext = 0};
  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {

      msg.mtype = processTable[i].pid;

      if (sendMessage(msqId, &msg) == -1) {
        log_message(LOG_LEVEL_ERROR,
                    "Failed to send termination message to PID: %d.",
                    processTable[i].pid);
      } else {
        log_message(LOG_LEVEL_DEBUG, "Termination message sent to PID: %d.",
                    processTable[i].pid);
      }
      processTable[i].occupied = 0;
    }
  }
}

int cleanupSharedMemory(void) {
  if (shmId != -1) {

    if (simClock != NULL) {
      if (shmdt(simClock) == -1) {
        log_message(LOG_LEVEL_ERROR, "Failed to detach shared memory: %s",
                    strerror(errno));

      } else {
        log_message(LOG_LEVEL_DEBUG, "Shared memory detached successfully.");
      }
      simClock = NULL;
    }

    if (shmctl(shmId, IPC_RMID, NULL) == -1) {
      log_message(LOG_LEVEL_ERROR, "Failed to remove shared memory segment: %s",
                  strerror(errno));
      return -1;
    } else {
      log_message(LOG_LEVEL_DEBUG,
                  "Shared memory segment marked for deletion.");
      shmId = -1;
    }
  }
  return 0;
}

int cleanupMessageQueue(void) {
  if (msqId != -1) {
    if (msgctl(msqId, IPC_RMID, NULL) == -1) {
      log_message(LOG_LEVEL_ERROR, "Failed to remove message queue: %s",
                  strerror(errno));
      return -1;
    }
    msqId = -1;
    log_message(LOG_LEVEL_DEBUG, "Message queue removed successfully.");
  }
  return 0;
}

void timeoutHandler(int signum) {
  (void)signum;
  log_message(LOG_LEVEL_INFO, "Timeout signal received.");
  cleanupAndExit();
}

void setupTimeout(int seconds) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = timeoutHandler;
  sigaction(SIGALRM, &sa, NULL);

  alarm(seconds);
  log_message(LOG_LEVEL_INFO, "Timeout setup for %d seconds.", seconds);
}

void cleanupAndExit(void) {
  log_message(LOG_LEVEL_INFO, "Initiating cleanup.");

  killAllWorkers();
  cleanupResources();

  if (logFile) {
    fclose(logFile);
    logFile = NULL;
  }

  log_message(LOG_LEVEL_INFO, "Cleanup completed. Exiting now.");
  _exit(EXIT_SUCCESS);
}
