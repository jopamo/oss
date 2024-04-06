#include "process.h"

#include "cleanup.h"
#include "shared.h"

volatile sig_atomic_t cleanupInitiated = 0;

void initializeSemaphore(const char *semName) {
  sem_unlink(semName);
  clockSem = sem_open(semName, O_CREAT | O_EXCL, SEM_PERMISSIONS, 1);
  if (clockSem == SEM_FAILED) {
    log_message(LOG_LEVEL_ERROR, "Failed to initialize semaphore: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void atexitHandler(void) { cleanupResources(); }

void cleanupResources(void) {
  log_message(LOG_LEVEL_INFO, "Initiating cleanup...");

  if (__sync_lock_test_and_set(&cleanupInitiated, 1)) {
    return;
  }

  semaphore_cleanup();
  logFile_cleanup();
  killAllWorkers();
  sharedMemory_cleanup();
  messageQueue_cleanup();

  cleanupInitiated = 0;

  log_message(LOG_LEVEL_INFO, "Cleanup completed. Exiting...");
}

void semaphore_cleanup(void) {
  if (clockSem != NULL && clockSem != SEM_FAILED) {
    sem_close(clockSem);
    sem_unlink(clockSemName);
    clockSem = SEM_FAILED;
  }
}

void logFile_cleanup(void) {
  if (logFile) {
    fflush(stdout);
    fclose(logFile);
    logFile = NULL;
  }
}

void cleanupSharedMemorySegment(int shmId, const char *segmentName) {
  if (shmId >= 0) {

    void *shmPtr = shmat(shmId, NULL, SHM_RDONLY);
    if (shmPtr != (void *)-1) {
      shmdt(shmPtr);
    }

    if (shmctl(shmId, IPC_RMID, NULL) == -1) {
      log_message(
          LOG_LEVEL_ERROR,
          "Failed to mark %s shared memory for deletion (shmId: %d): %s",
          segmentName, shmId, strerror(errno));
    } else {
      log_message(LOG_LEVEL_INFO,
                  "%s shared memory segment marked for deletion (shmId: %d).",
                  segmentName, shmId);
    }
  }
}

void sharedMemory_cleanup(void) {
  cleanupSharedMemorySegment(simulatedTimeShmId, "Simulated Clock");
  cleanupSharedMemorySegment(actualTimeShmId, "Actual Time");
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
      if (sendMessage(msqId, &msg) != -1) {
        int status;
        waitpid(processTable[i].pid, &status, 0);
        log_message(LOG_LEVEL_DEBUG,
                    "Termination message sent and worker PID: %d exited.",
                    processTable[i].pid);
      } else {
        log_message(LOG_LEVEL_ERROR,
                    "Failed to send termination message to PID: %d.",
                    processTable[i].pid);
      }
      processTable[i].occupied = 0;
    }
  }
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
  _exit(EXIT_SUCCESS);
}

int initSharedMemorySegment(key_t key, size_t size, const char *segmentName) {

  int shmId = shmget(key, size, IPC_CREAT | 0666);
  if (shmId < 0) {

    if (errno == EEXIST) {

      shmId = shmget(key, size, 0666);
      if (shmId < 0) {
        log_message(LOG_LEVEL_ERROR,
                    "Failed to attach to existing shared memory for %s: %s",
                    segmentName, strerror(errno));
        return -1;
      }
      log_message(
          LOG_LEVEL_INFO,
          "Shared memory for %s already exists, attached to existing shmId: %d",
          segmentName, shmId);
    } else {
      log_message(LOG_LEVEL_ERROR, "Failed to create shared memory for %s: %s",
                  segmentName, strerror(errno));
      return -1;
    }
  } else {
    log_message(LOG_LEVEL_INFO,
                "Created new shared memory for %s with shmId: %d", segmentName,
                shmId);
  }

  return shmId;
}
