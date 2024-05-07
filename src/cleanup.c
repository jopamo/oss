#include "cleanup.h"
#include "globals.h"
#include "process.h"
#include "shared.h"
#include "user_process.h"

volatile sig_atomic_t cleanupInitiated = 0;

// Attempts to create a semaphore with retries and controlled delay
void semUnlinkCreate(void) {
  const int max_retries = 5;           // Maximum number of retries
  int attempt = 0;                     // Current attempt counter
  const char *sem_name = clockSemName; // Name of the semaphore

  while (attempt < max_retries) {
    sem_unlink(sem_name); // Try to unlink the semaphore
    clockSem = sem_open(sem_name, O_CREAT | O_EXCL, SEM_PERMISSIONS,
                        1); // Try to create the semaphore
    if (clockSem != SEM_FAILED) {
      log_message(LOG_LEVEL_INFO, "Semaphore created successfully.");
      return;
    }

    log_message(LOG_LEVEL_ERROR,
                "Attempt %d: Failed to create semaphore '%s': %s", attempt + 1,
                sem_name, strerror(errno));

    if (attempt + 1 < max_retries) {
      int sleep_time = (1 << attempt); // Exponential backoff
      sleep(sleep_time);
    }
    attempt++;
  }

  log_message(LOG_LEVEL_ERROR,
              "Failed to create semaphore after %d attempts. Exiting.",
              max_retries);
  exit(EXIT_FAILURE);
}

// Cleans up all system resources on termination
void cleanupResources(void) {
  log_message(LOG_LEVEL_INFO, "Initiating cleanup...");

  keepRunning = 0; // Stop the system

  if (__sync_lock_test_and_set(&cleanupInitiated, 1)) {
    return; // Prevent double cleanup
  }

  sleep(1); // Short delay before starting cleanup

  // Clean up message queue
  int msqIdLocal = msqId;
  if (msqIdLocal != -1) {
    msqId = -1;
    if (msgctl(msqIdLocal, IPC_RMID, NULL) == -1) {
      log_message(LOG_LEVEL_ERROR, "Failed to remove message queue: %s",
                  strerror(errno));
    } else {
      log_message(LOG_LEVEL_INFO, "Message queue removed successfully.");
    }
  }

  // Send termination signal to all child processes
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      kill(processTable[i].pid, SIGTERM);
      log_message(LOG_LEVEL_INFO, "Termination signal sent to child PID: %d",
                  processTable[i].pid);
    }
  }
  sendSignalToChildGroups(SIGTERM);

  // Further cleanup tasks
  sleep(1);
  semaphore_cleanup(); // Cleanup semaphores
  sleep(1);
  logFile_cleanup(); // Close and clean up log files
  sleep(1);
  sharedMemory_cleanup(); // Cleanup shared memory segments

  cleanupInitiated = 0; // Reset cleanup initiated flag

  log_message(LOG_LEVEL_INFO, "Cleanup completed. Exiting...");
}

// Cleanup semaphore resources
void semaphore_cleanup(void) {
  if (clockSem != NULL && clockSem != SEM_FAILED) {
    sem_close(clockSem);      // Close the semaphore
    sem_unlink(clockSemName); // Unlink the semaphore
    clockSem = SEM_FAILED;    // Reset the semaphore handle
  }
}

// Cleanup log file resources
void logFile_cleanup(void) {
  if (logFile) {
    fflush(stdout);  // Flush stdout
    fclose(logFile); // Close the log file
    logFile = NULL;  // Reset the log file handle
  }
}

// Cleanup a specific shared memory segment
void cleanupSharedMemorySegment(int shmId, const char *segmentName) {
  if (shmId >= 0) {
    void *shmPtr = shmat(shmId, NULL, SHM_RDONLY); // Attach to shared memory
    if (shmPtr != (void *)-1) {
      shmdt(shmPtr); // Detach shared memory
    }
    if (shmctl(shmId, IPC_RMID, NULL) ==
        -1) { // Mark the segment to be destroyed
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

// Cleanup all shared memory segments
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

// Perform a final cleanup and exit the process
void cleanupAndExit(void) {
  cleanupResources();
  _exit(EXIT_SUCCESS);
}

// Forks a process and executes a given program
pid_t forkAndExecute(const char *executable) {
  pid_t pid = fork(); // Fork the current process

  if (pid == 0) {             // Child process
    if (setpgid(0, 0) != 0) { // Set process group ID for job control
      log_message(LOG_LEVEL_ERROR,
                  "Failed to set PGID in child process '%s': %s", executable,
                  strerror(errno));
      _exit(EXIT_FAILURE);
    }

    execl(executable, executable, (char *)NULL); // Execute the program

    // Only reached if execution fails
    log_message(LOG_LEVEL_ERROR, "Failed to start process '%s': %s", executable,
                strerror(errno));
    _exit(EXIT_FAILURE);
  } else if (pid > 0) {           // Parent process
    if (setpgid(pid, pid) != 0) { // Set the child's PGID from the parent
      log_message(LOG_LEVEL_ERROR,
                  "Failed to set PGID in parent for process '%s': %s",
                  executable, strerror(errno));
    }
  } else { // Fork failed
    log_message(LOG_LEVEL_ERROR, "Failed to fork for process '%s': %s",
                executable, strerror(errno));
  }

  return pid; // Return the PID of the forked process
}

void killProcess(int pid) {
  if (pid < 0 || pid >= MAX_PROCESSES) {
    fprintf(stderr, "Invalid process ID: %d\n", pid);
    return;
  }

  PCB *process = &processTable[pid];
  if (process->occupied) {
    process->occupied = 0;
    log_message(LOG_LEVEL_INFO, "Process %d terminated to resolve deadlock.",
                pid);
  }
}
