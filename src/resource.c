#include "resource.h"
#include "process.h"

pthread_mutex_t resourceTableMutex =
    PTHREAD_MUTEX_INITIALIZER; // Mutex for resource table

ResourceDescriptor *resourceTable = NULL;

bool hasAllocatedResources(int pid) {
  for (int j = 0; j < maxResources; j++) {
    if (resourceTable[j].allocated[pid] > 0) {
      log_message(LOG_LEVEL_DEBUG, 0, "Process P%d has resources allocated",
                  pid);
      return true;
    }
  }
  log_message(LOG_LEVEL_DEBUG, 0, "Process P%d has no resources allocated",
              pid);
  return false;
}

bool isProcessRunning(int pid) {
  pthread_mutex_lock(&processTableMutex); // Lock access to the process table
  bool running = false;
  if (pid >= 0 && pid < maxProcesses) {
    running = processTable[pid].state == PROCESS_RUNNING;
  }
  pthread_mutex_unlock(&processTableMutex); // Unlock after access
  return running;
}

void log_resource_state(const char *operation, int pid, int availableBefore,
                        int availableAfter) {
  unsigned long currentSec, currentNano;
  better_sem_wait(clockSem); // Locking semaphore to safely access simClock
  currentSec = simClock->seconds;
  currentNano = simClock->nanoseconds;
  better_sem_post(clockSem); // Unlocking semaphore after reading time

  log_message(LOG_LEVEL_INFO, 0,
              "Master %s Process P%d %s at time %lu:%09lu: Available "
              "before: %d, after: %d",
              operation, pid, operation[0] == 'R' ? "requesting" : "releasing",
              currentSec, currentNano, availableBefore, availableAfter);
}

int requestResource(int pid) {
  if (pid < 0 || pid >= maxProcesses) {
    return -1; // PID out of range
  }
  pthread_mutex_lock(&resourceTableMutex);
  if (resourceTable->available < 1) {
    pthread_mutex_unlock(&resourceTableMutex);
    log_resource_state("denied", pid, resourceTable->available,
                       resourceTable->available);
    return -1; // Not enough resources available
  }
  int availableBefore = resourceTable->available;
  resourceTable->available -= 1;
  resourceTable->allocated[pid] += 1;
  log_resource_state("granted", pid, availableBefore, resourceTable->available);
  pthread_mutex_unlock(&resourceTableMutex);
  return 0; // Resource allocated successfully
}

int releaseResource(int pid) {
  pthread_mutex_lock(&resourceTableMutex);
  if (pid < 0 || pid >= maxProcesses || !isProcessRunning(pid)) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Invalid or non-running PID: %d. Cannot release resources.",
                pid);
    pthread_mutex_unlock(&resourceTableMutex);
    return -1;
  }
  if (resourceTable->allocated[pid] < 1) {
    log_message(LOG_LEVEL_WARN, 0, "No resources to release for PID: %d.", pid);
    pthread_mutex_unlock(&resourceTableMutex);
    return -1;
  }
  int availableBefore = resourceTable->available;
  resourceTable->available += 1;
  resourceTable->allocated[pid] -= 1;
  log_message(LOG_LEVEL_INFO, 0,
              "Resource released by PID: %d. Available before: %d, after: %d",
              pid, availableBefore, resourceTable->available);
  pthread_mutex_unlock(&resourceTableMutex);
  return 0;
}

void releaseAllResourcesForProcess(int pid) {
  if (pid < 0 || pid >= maxProcesses) {
    return; // PID out of range
  }
  pthread_mutex_lock(&resourceTableMutex);
  if (resourceTable->allocated[pid] > 0) {
    int releasedQuantity = resourceTable->allocated[pid];
    while (resourceTable->allocated[pid] > 0) {
      releaseResource(pid);
    }
    log_message(LOG_LEVEL_INFO, 0,
                "Process P%d terminated, resources released: %d", pid,
                releasedQuantity);
  }
  pthread_mutex_unlock(&resourceTableMutex);
}

void logResourceTable() {
  log_message(LOG_LEVEL_INFO, 0, "Current system resources:");
  for (int i = 0; i < maxProcesses; i++) {
    if (isProcessRunning(i) && hasAllocatedResources(i)) {
      char buffer[1024] = {0};
      snprintf(buffer, sizeof(buffer), "P%d", i);
      for (int j = 0; j < maxResources; j++) {
        snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer),
                 " %d", resourceTable[j].allocated[i]);
      }
      log_message(LOG_LEVEL_INFO, 0, "%s", buffer);
    }
  }
  log_message(LOG_LEVEL_INFO, 0,
              "--------------------------------------------------");
}

bool unsafeSystem(void) {
  if (!resourceTable) {
    log_message(LOG_LEVEL_ERROR, 0, "Resource table is not initialized.");
    return true; // Unsafe if the resource table not available
  }
  int finish[maxSimultaneous]; // Declare variable-sized array

  // Initialize array to zero
  for (int i = 0; i < maxSimultaneous; i++) {
    finish[i] = true;
  }

  // Determine if each process can finish with the current allocation
  for (int i = 0; i < maxSimultaneous; i++) {
    for (int j = 0; j < maxResources; j++) {
      if (resourceTable[j].allocated[i] > 0) {
        finish[i] = false; // Process cannot finish as it holds resources
        log_message(LOG_LEVEL_DEBUG, 0,
                    "Process P%d cannot finish; holds resources.", i);
        break;
      }
    }
  }

  bool systemIsSafe = true;
  // Check if any process cannot finish, indicating a potentially unsafe state
  for (int i = 0; i < maxSimultaneous; i++) {
    if (!finish[i]) {
      log_message(LOG_LEVEL_WARN, 0,
                  "System is unsafe: Process P%d cannot finish.", i);
      systemIsSafe = false;
    }
  }

  if (systemIsSafe) {
    log_message(LOG_LEVEL_INFO, 0, "System is safe: All processes can finish.");
  }

  return !systemIsSafe; // Return true if system is unsafe
}

void resolveDeadlocks(void) {
  if (!resourceTable) {
    log_message(LOG_LEVEL_ERROR, 0, "Resource table is not initialized.");
    return;
  }

  bool deadlockResolved = false;

  for (int i = 0; i < maxResources; i++) {
    for (int j = 0; j < maxSimultaneous; j++) {
      if (resourceTable[i].allocated[j] > 0) {
        log_message(
            LOG_LEVEL_INFO, 0,
            "Resolving deadlock: Releasing all resources held by Process P%d.",
            j);
        resourceTable[i].available += resourceTable[i].allocated[j];
        resourceTable[i].allocated[j] = 0;
        deadlockResolved = true;
      }
    }
  }

  if (deadlockResolved) {
    log_message(LOG_LEVEL_INFO, 0,
                "Deadlock resolved: All resources held by deadlocked processes "
                "have been released.");
  } else {
    log_message(LOG_LEVEL_INFO, 0,
                "No deadlock resolution needed: No resources were held by any "
                "processes.");
  }
}
