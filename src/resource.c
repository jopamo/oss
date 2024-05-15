#include "resource.h"
#include "process.h"

pthread_mutex_t resourceTableMutex =
    PTHREAD_MUTEX_INITIALIZER; // Mutex for resource table

ResourceDescriptor *resourceTable = NULL;

int totalRequests = 0;
int immediateGrantedRequests = 0;
int waitingGrantedRequests = 0;
int terminatedByDeadlock = 0;
int successfullyTerminated = 0;
int deadlockDetectionRuns = 0;

bool isProcessRunning(pid_t pid) {
  pthread_mutex_lock(&processTableMutex);
  int index = findProcessIndexByPID(pid);
  if (index == -1) {
    pthread_mutex_unlock(&processTableMutex);
    log_message(LOG_LEVEL_DEBUG, 0, "PID %d is not found in the process table.",
                pid);
    return false;
  }

  bool running = processTable[index].state == PROCESS_RUNNING;
  pthread_mutex_unlock(&processTableMutex);
  return running;
}

void log_resource_state(const char *operation, pid_t pid, int resourceType,
                        int count, int availableBefore, int availableAfter) {
  unsigned long currentSec, currentNano;
  better_sem_wait(clockSem);
  currentSec = simClock->seconds;
  currentNano = simClock->nanoseconds;
  better_sem_post(clockSem);

  log_message(LOG_LEVEL_INFO, 1,
              "Master %s Process P%d %s R%d %d units at time %lu:%09lu: "
              "Available before: %d, after: %d",
              operation, pid,
              strcmp(operation, "granted") == 0 ? "granting"
                                                : "acknowledged releasing",
              resourceType, count, currentSec, currentNano, availableBefore,
              availableAfter);
}

int requestResource(pid_t pid, int resourceType, int count) {
  log_message(LOG_LEVEL_DEBUG, 0,
              "Attempting to request %d units of resource %d for PID %d", count,
              resourceType, pid);

  pthread_mutex_lock(&resourceTableMutex);

  // Find the correct index in the process table for the given PID
  int index = findProcessIndexByPID(pid);
  if (index == -1) {
    pthread_mutex_unlock(&resourceTableMutex);
    log_message(LOG_LEVEL_ERROR, 0,
                "Invalid PID: %d. Cannot request resources.", pid);
    return -1; // Invalid PID
  }

  if (!isProcessRunning(pid)) {
    pthread_mutex_unlock(&resourceTableMutex);
    log_message(LOG_LEVEL_ERROR, 0,
                "Non-running PID: %d. Cannot request resources.", pid);
    return -1; // Process is not running
  }

  totalRequests++;

  if (resourceTable[resourceType].available < count) {
    pthread_mutex_unlock(&resourceTableMutex);
    log_message(
        LOG_LEVEL_DEBUG, 1,
        "Master: no instances of R%d available, P%d added to wait queue",
        resourceType, pid);
    return -1; // Not enough resources available
  }

  int availableBefore = resourceTable[resourceType].available;
  resourceTable[resourceType].available -= count;
  resourceTable[resourceType].allocated[index] += count;
  int availableAfter = resourceTable[resourceType].available;

  immediateGrantedRequests++;
  log_message(LOG_LEVEL_INFO, 1,
              "Master granting P%d request R%d at time %lu:%09lu. Available "
              "before: %d, after: %d",
              pid, resourceType, simClock->seconds, simClock->nanoseconds,
              availableBefore, availableAfter);

  pthread_mutex_unlock(&resourceTableMutex);
  return 0; // Resource allocated successfully
}

int releaseResource(int pid, int resourceType, int count) {
  log_message(LOG_LEVEL_DEBUG, 0,
              "Attempting to release %d units of resource %d for PID %d", count,
              resourceType, pid);

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += 2; // Wait for 2 seconds

  if (pthread_mutex_timedlock(&resourceTableMutex, &ts) != 0) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Failed to acquire resourceTableMutex for releaseResource");
    return -1;
  }
  log_message(LOG_LEVEL_DEBUG, 0,
              "Acquired resourceTableMutex for releaseResource");

  int index = findProcessIndexByPID(pid);
  if (index == -1 || !isProcessRunning(pid)) {
    log_message(LOG_LEVEL_DEBUG, 0,
                "Invalid or non-running PID: %d. Cannot release resources.",
                pid);
    pthread_mutex_unlock(&resourceTableMutex);
    return -1;
  }

  if (resourceTable[resourceType].allocated[index] < count) {
    log_message(LOG_LEVEL_DEBUG, 0, "No resources to release for PID: %d.",
                pid);
    pthread_mutex_unlock(&resourceTableMutex);
    return -1;
  }

  int availableBefore = resourceTable[resourceType].available;
  resourceTable[resourceType].available += count;
  resourceTable[resourceType].allocated[index] -= count;
  int availableAfter = resourceTable[resourceType].available;

  log_message(LOG_LEVEL_INFO, 1,
              "Master has acknowledged Process P%d releasing R%d at time "
              "%lu:%09lu. Available before: %d, after: %d",
              pid, resourceType, simClock->seconds, simClock->nanoseconds,
              availableBefore, availableAfter);

  pthread_mutex_unlock(&resourceTableMutex);
  log_message(LOG_LEVEL_DEBUG, 0,
              "Released resourceTableMutex for releaseResource");

  return 0;
}

void releaseAllResourcesForProcess(int pid) {
  pthread_mutex_lock(&resourceTableMutex);

  int index = findProcessIndexByPID(pid);
  if (index == -1 || !isProcessRunning(pid)) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Cannot release resources. PID %d is invalid or not running.",
                pid);
    pthread_mutex_unlock(&resourceTableMutex);
    return;
  }

  for (int resourceType = 0; resourceType < MAX_RESOURCES; resourceType++) {
    log_message(LOG_LEVEL_INFO, 0,
                "Trying to release resources for resourceType %d...",
                resourceType);
    int allocation = resourceTable[resourceType].allocated[index];
    if (allocation > 0) {
      log_message(LOG_LEVEL_INFO, 0,
                  "PID %d has %d units of resource %d allocated.", pid,
                  allocation, resourceType);
      resourceTable[resourceType].available += allocation;
      resourceTable[resourceType].allocated[index] = 0;
      log_message(LOG_LEVEL_INFO, 0,
                  "Released %d units of resource %d for PID: %d. Available: %d",
                  allocation, resourceType, pid,
                  resourceTable[resourceType].available);
    }
  }

  log_message(LOG_LEVEL_INFO, 0, "All resources released for PID: %d.", pid);

  pthread_mutex_unlock(&resourceTableMutex);
}

bool unsafeSystem(void) {
  if (!resourceTable) {
    log_message(LOG_LEVEL_ERROR, 0, "Resource table is not initialized.");
    return true;
  }
  int finish[MAX_PROCESSES] = {0};

  for (int i = 0; i < MAX_SIMULTANEOUS; i++) {
    for (int j = 0; j < MAX_RESOURCES; j++) {
      if (resourceTable[j].allocated[i] > 0) {
        finish[i] = 0;
        log_message(LOG_LEVEL_DEBUG, 0,
                    "Process P%d cannot finish; holds resources.", i);
        break;
      }
    }
  }

  bool systemIsSafe = true;
  for (int i = 0; i < MAX_SIMULTANEOUS; i++) {
    if (!finish[i]) {
      log_message(LOG_LEVEL_DEBUG, 0,
                  "System is unsafe: Process P%d cannot finish.", i);
      systemIsSafe = false;
      break;
    }
  }

  if (systemIsSafe) {
    log_message(LOG_LEVEL_INFO, 0, "System is safe: All processes can finish.");
  }

  return !systemIsSafe;
}

void resolveDeadlocks(void) {
  if (!resourceTable) {
    log_message(LOG_LEVEL_ERROR, 0, "Resource table is not initialized.");
    return;
  }

  deadlockDetectionRuns++;
  bool deadlockResolved = false;

  // Create arrays for the Banker's Algorithm
  int work[MAX_RESOURCES];
  bool finish[MAX_SIMULTANEOUS];
  int need[MAX_PROCESSES][MAX_RESOURCES];

  // Initialize work vector as a copy of available resources
  for (int i = 0; i < MAX_RESOURCES; i++) {
    work[i] = resourceTable[i].available;
  }

  // Initialize finish vector to false
  for (int i = 0; i < MAX_SIMULTANEOUS; i++) {
    finish[i] = false;
  }

  // Calculate the need matrix
  for (int i = 0; i < MAX_SIMULTANEOUS; i++) {
    for (int j = 0; j < MAX_RESOURCES; j++) {
      need[i][j] = resourceTable[j].total - resourceTable[j].allocated[i];
    }
  }

  // Banker's Algorithm to detect deadlocks
  bool progress = true;
  while (progress) {
    progress = false;
    for (int i = 0; i < MAX_SIMULTANEOUS; i++) {
      if (!finish[i]) {
        bool canFinish = true;
        for (int j = 0; j < MAX_RESOURCES; j++) {
          if (resourceTable[j].allocated[i] > 0 && need[i][j] > work[j]) {
            canFinish = false;
            break;
          }
        }
        if (canFinish) {
          for (int j = 0; j < MAX_RESOURCES; j++) {
            work[j] += resourceTable[j].allocated[i];
          }
          finish[i] = true;
          progress = true;
        }
      }
    }
  }

  // Identify deadlocked processes
  for (int i = 0; i < MAX_SIMULTANEOUS; i++) {
    if (!finish[i] && processTable[i].occupied) {
      log_message(LOG_LEVEL_INFO, 0,
                  "Process P%d is deadlocked. Terminating process.",
                  processTable[i].pid);
      releaseAllResourcesForProcess(processTable[i].pid);
      terminatedByDeadlock++;
      processTable[i].state = PROCESS_TERMINATED;
      deadlockResolved = true;
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

void logResourceTable() {
  log_message(LOG_LEVEL_INFO, 0,
              "---------------- Resource Table ----------------");

  // Log header
  char header[1024] = "    ";
  for (int j = 0; j < MAX_RESOURCES; j++) {
    char resourceHeader[16];
    snprintf(resourceHeader, sizeof(resourceHeader), "R%-4d", j);
    strcat(header, resourceHeader);
  }
  log_message(LOG_LEVEL_INFO, 0, header);

  // Log resource availability
  char availability[1024] = "Avl ";
  for (int j = 0; j < MAX_RESOURCES; j++) {
    char resourceAvailability[16];
    snprintf(resourceAvailability, sizeof(resourceAvailability), "%-4d",
             resourceTable[j].available);
    strcat(availability, resourceAvailability);
  }
  log_message(LOG_LEVEL_INFO, 0, availability);

  // Log each process's resource allocation
  for (int i = 0; i < MAX_SIMULTANEOUS; i++) {
    if (processTable[i].occupied) {
      char buffer[1024];
      snprintf(buffer, sizeof(buffer), "P%-3d ", processTable[i].pid);
      for (int j = 0; j < MAX_RESOURCES; j++) {
        char resourceAllocation[16];
        snprintf(resourceAllocation, sizeof(resourceAllocation), "%-4d",
                 resourceTable[j].allocated[i]);
        strcat(buffer, resourceAllocation);
      }
      log_message(LOG_LEVEL_INFO, 0, buffer);
    }
  }
  log_message(LOG_LEVEL_INFO, 0,
              "------------------------------------------------");
}

void logStatistics(void) {
  log_message(LOG_LEVEL_INFO, 1, "Statistics:");
  log_message(LOG_LEVEL_INFO, 1, "Total requests: %d", totalRequests);
  log_message(LOG_LEVEL_INFO, 1, "Immediate granted requests: %d",
              immediateGrantedRequests);
  log_message(LOG_LEVEL_INFO, 1, "Waiting granted requests: %d",
              waitingGrantedRequests);
  log_message(LOG_LEVEL_INFO, 1,
              "Processes terminated by deadlock detection: %d",
              terminatedByDeadlock);
  log_message(LOG_LEVEL_INFO, 1, "Processes successfully terminated: %d",
              successfullyTerminated);
  log_message(LOG_LEVEL_INFO, 1, "Deadlock detection runs: %d",
              deadlockDetectionRuns);

  if (deadlockDetectionRuns > 0) {
    float averageTerminations =
        (float)terminatedByDeadlock / deadlockDetectionRuns;
    log_message(LOG_LEVEL_INFO, 0,
                "Average terminations per deadlock detection run: %.2f",
                averageTerminations);
  }
}
