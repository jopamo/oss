#include "cleanup.h"
#include "process.h"
#include "shared.h"
#include "user_process.h"

volatile sig_atomic_t cleanupInitiated = 0;

void semUnlinkCreate(void) {
  sem_unlink(clockSemName);
  clockSem = sem_open(clockSemName, O_CREAT | O_EXCL, SEM_PERMISSIONS, 1);
  if (clockSem == SEM_FAILED) {
    log_message(LOG_LEVEL_ERROR, "Failed to create semaphore: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void cleanupResources(void) {
  log_message(LOG_LEVEL_INFO, "Initiating cleanup...");

  if (__sync_lock_test_and_set(&cleanupInitiated, 1)) {
    return;
  }

  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      kill(processTable[i].pid, SIGTERM);
      log_message(LOG_LEVEL_INFO, "Termination signal sent to child PID: %d",
                  processTable[i].pid);
    }
  }

  sendSignalToChildGroups(SIGTERM);

  semaphore_cleanup();
  logFile_cleanup();
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
  simulatedTimeShmId = -1;

  cleanupSharedMemorySegment(actualTimeShmId, "Actual Time");
  actualTimeShmId = -1;

  cleanupSharedMemorySegment(processTableShmId, "Process Table");
  processTableShmId = -1;

  cleanupSharedMemorySegment(resourceTableShmId, "Resource Table");
  resourceTableShmId = -1;
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

void cleanupAndExit(void) {
  cleanupResources();
  _exit(EXIT_SUCCESS);
}

pid_t forkAndExecute(const char *executable) {
  pid_t pid = fork();
  if (pid == 0) {

    setpgid(0, 0);
    execl(executable, executable, (char *)NULL);
    log_message(LOG_LEVEL_ERROR, "Failed to start process '%s': %s", executable,
                strerror(errno));
    _exit(EXIT_FAILURE);
  } else if (pid > 0) {

    setpgid(pid, pid);
  }
  return pid;
}
