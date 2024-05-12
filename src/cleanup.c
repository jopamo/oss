#include "cleanup.h"
#include "globals.h"
#include "process.h"
#include "shared.h"

#include <stdlib.h>

volatile sig_atomic_t cleanupInitiated = 0;

int semUnlinkCreate(void) {
  const char *sem_name = clockSemName; // Semaphore name
  sem_unlink(sem_name);                // Try to unlink existing semaphore

  clockSem = sem_open(sem_name, O_CREAT | O_EXCL, SEM_PERMISSIONS, 1);
  if (clockSem == SEM_FAILED) {
    perror("Failed to create semaphore");
    return -1;
  }

  log_message(LOG_LEVEL_INFO, 0, "Semaphore created successfully.");
  return 0;
}

void cleanupResources(void) {
  if (cleanupInitiated)
    return; // Prevent double cleanup
  cleanupInitiated = 1;

  log_message(LOG_LEVEL_INFO, 0, "Cleaning up resources...");

  if (msqId != -1) {
    msgctl(msqId, IPC_RMID, NULL);
    msqId = -1;
    log_message(LOG_LEVEL_INFO, 0, "Message queue removed successfully.");
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

  log_message(LOG_LEVEL_INFO, 0, "Cleanup completed.");
}

void cleanupSharedMemorySegment(int shmId, const char *segmentName) {
  if (shmId >= 0) {
    shmctl(shmId, IPC_RMID, NULL); // Mark the segment for deletion
    log_message(LOG_LEVEL_INFO, 0,
                "%s shared memory segment marked for deletion.", segmentName);
  }
}

void cleanupAndExit(void) {
  cleanupResources();
  exit(EXIT_SUCCESS);
}
