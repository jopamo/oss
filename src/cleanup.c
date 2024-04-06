#include "process.h"

#include "cleanup.h"
#include "shared.h"

volatile sig_atomic_t cleanupInitiated = 0;

void atexitHandler(void) { cleanupResources(); }

void cleanupResources(void) {
  if (__sync_lock_test_and_set(&cleanupInitiated, 1)) {
    return;
  }

  semaphore_cleanup();

  logFile_cleanup();

  killAllWorkers();

  sharedMemory_cleanup();

  messageQueue_cleanup();
}

void semaphore_cleanup(void) {
  if (clockSem != NULL) {
    sem_close(clockSem);
    sem_unlink(clockSemName);
    clockSem = NULL;
  }
}

void logFile_cleanup(void) {
  if (logFile) {
    fflush(stdout);
    fclose(logFile);
    logFile = NULL;
  }
}

void sharedMemory_cleanup(void) {
  cleanupSharedMemorySegment((void **)&simClock, shmId);
  cleanupSharedMemorySegment((void **)&actualTime, actualTimeShmId);
}

int cleanupSharedMemorySegment(void **shmPtr, int shmId) {
  if (*shmPtr != NULL && shmdt(*shmPtr) == -1) {
    perror("Failed to detach shared memory");
    *shmPtr = NULL;
    return -1;
  }
  *shmPtr = NULL;

  if (shmctl(shmId, IPC_RMID, NULL) == -1) {
    perror("Failed to mark shared memory for deletion");
    return -1;
  }

  return 0;
}

int messageQueue_cleanup(void) {
  if (msqId != -1) {
    if (msgctl(msqId, IPC_RMID, NULL) == -1) {
      perror("Message queue removal failed");
      return -1;
    }
    msqId = -1;
  }
  return 0;
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
  cleanupResources();
  log_message(LOG_LEVEL_INFO, "Cleanup completed, exiting...");
  _exit(EXIT_SUCCESS);
}
