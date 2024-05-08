#include "cleanup.h"

volatile sig_atomic_t cleanupInitiated = 0;

int semUnlinkCreate(void) {
  const char *sem_name = clockSemName; // Semaphore name
  sem_unlink(sem_name);                // Try to unlink existing semaphore

  clockSem = sem_open(sem_name, O_CREAT | O_EXCL, SEM_PERMISSIONS, 1);
  if (clockSem == SEM_FAILED) {
    perror("Failed to create semaphore");
    return -1;
  }

  log_message(LOG_LEVEL_INFO, "Semaphore created successfully.");
  return 0;
}

void cleanupResources(void) {
  if (cleanupInitiated)
    return; // Prevent double cleanup
  cleanupInitiated = 1;

  log_message(LOG_LEVEL_INFO, "Cleaning up resources...");

  if (msqId != -1) {
    msgctl(msqId, IPC_RMID, NULL);
    msqId = -1;
    log_message(LOG_LEVEL_INFO, "Message queue removed successfully.");
  }

  sendSignalToChildGroups(SIGTERM); // Terminate child processes

  // Cleanup semaphores
  if (clockSem != SEM_FAILED) {
    sem_close(clockSem);
    sem_unlink(clockSemName);
    clockSem = SEM_FAILED;
  }

  // Close log file
  if (logFile) {
    fclose(logFile);
    logFile = NULL;
  }

  // Cleanup shared memory
  cleanupSharedMemorySegment(simulatedTimeShmId, "Simulated Clock");
  cleanupSharedMemorySegment(actualTimeShmId, "Actual Time");
  cleanupSharedMemorySegment(processTableShmId, "Process Table");
  cleanupSharedMemorySegment(resourceTableShmId, "Resource Table");

  log_message(LOG_LEVEL_INFO, "Cleanup completed.");
}

void cleanupSharedMemorySegment(int shmId, const char *segmentName) {
  if (shmId >= 0) {
    shmctl(shmId, IPC_RMID, NULL); // Mark the segment for deletion
    log_message(LOG_LEVEL_INFO, "%s shared memory segment marked for deletion.",
                segmentName);
  }
}

void cleanupAndExit(void) {
  cleanupResources();
  exit(EXIT_SUCCESS);
}

pid_t forkAndExecute(const char *executable) {
  pid_t pid = fork();
  if (pid == 0) {
    execl(executable, executable, (char *)NULL);
    perror("Failed to execute child process");
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    perror("Failed to fork child process");
    exit(EXIT_FAILURE);
  }
  return pid;
}

void sendSignalToChildGroups(int sig) {
  if (timekeeperPid > 0) {
    kill(-timekeeperPid, sig);
  }
  if (tableprinterPid > 0) {
    kill(-tableprinterPid, sig);
  }
}
