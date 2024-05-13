#include "cleanup.h"

#include <stdlib.h>

volatile sig_atomic_t cleanupInitiated = 0;

int semUnlinkCreate(void) {
  const char *sem_name = clockSemName;
  if (sem_unlink(sem_name) == -1 && errno != ENOENT) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to unlink semaphore: %s",
                strerror(errno));
    return -1;
  }

  clockSem = sem_open(sem_name, O_CREAT | O_EXCL, SEM_PERMISSIONS, 1);
  if (clockSem == SEM_FAILED) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to create semaphore: %s",
                strerror(errno));
    return -1;
  }

  log_message(LOG_LEVEL_DEBUG, 0, "Semaphore created successfully.");
  return 0;
}

void cleanupResources(void) {
  if (cleanupInitiated)
    return; // Prevent double cleanup
  cleanupInitiated = 1;

  log_message(LOG_LEVEL_DEBUG, 0, "Cleaning up resources...");

  if (msqId != -1) {
    msgctl(msqId, IPC_RMID, NULL);
    msqId = -1;
    log_message(LOG_LEVEL_DEBUG, 0, "Message queue removed successfully.");
  }

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

  log_message(LOG_LEVEL_DEBUG, 0, "Cleanup completed.");
}

void cleanupSharedMemorySegment(int shmId, const char *segmentName) {
  if (shmId >= 0) {
    shmctl(shmId, IPC_RMID, NULL); // Mark the segment for deletion
    log_message(LOG_LEVEL_DEBUG, 0,
                "%s shared memory segment marked for deletion.", segmentName);
  }
}

void cleanupAndExit(void) {
  cleanupSharedResources();
  cleanupResources();
  exit(EXIT_SUCCESS);
}
