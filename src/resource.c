#include "resource.h"
#include "globals.h"
#include "shared.h"

pthread_mutex_t resourceTableMutex =
    PTHREAD_MUTEX_INITIALIZER; // Mutex for resource table

ResourceDescriptor *resourceTable = NULL;
Queue resourceWaitQueue[MAX_RESOURCES];

void log_resource_state(const char *operation, int pid, int resourceType,
                        int quantity, int availableBefore, int availableAfter) {
  log_message(LOG_LEVEL_DEBUG, 0,
              "%s operation by process %d on resource %d for %d units. "
              "Available before: %d, after: %d",
              operation, pid, resourceType, quantity, availableBefore,
              availableAfter);
}

int requestResource(int resourceType, int quantity, int pid) {
  pthread_mutex_lock(&resourceTableMutex);
  log_message(LOG_LEVEL_DEBUG, 0,
              "Process %d is requesting %d units of Resource %d", pid, quantity,
              resourceType);

  if (resourceTable[resourceType].available < quantity) {
    log_message(LOG_LEVEL_WARN, 0,
                "Request denied: Not enough units available. Requested: %d, "
                "Available: %d",
                quantity, resourceTable[resourceType].available);
    pthread_mutex_unlock(&resourceTableMutex);
    return -1; // Not enough resources available
  }

  // Simulate allocation
  resourceTable[resourceType].available -= quantity;
  resourceTable[resourceType].allocated[pid] += quantity;

  log_message(LOG_LEVEL_INFO, 0,
              "Resource allocated: Process %d received %d units of Resource %d",
              pid, quantity, resourceType);
  pthread_mutex_unlock(&resourceTableMutex);
  return 0; // Resource allocated successfully
}

int releaseResource(int resourceType, int quantity, int pid) {
  pthread_mutex_lock(&resourceTableMutex);
  if (resourceTable[resourceType].allocated[pid] < quantity) {
    pthread_mutex_unlock(&resourceTableMutex);
    return -1; // Trying to release more than allocated
  }

  resourceTable[resourceType].available += quantity;
  resourceTable[resourceType].allocated[pid] -= quantity;

  log_message(LOG_LEVEL_INFO, 0,
              "Resource released: Process %d returned %d units of Resource %d",
              pid, quantity, resourceType);
  pthread_mutex_unlock(&resourceTableMutex);
  return 0; // Resource released successfully
}

bool unsafeSystem(void) {
  int finish[MAX_USER_PROCESSES] = {0};

  // Initialize finish based on whether each process has any allocations
  for (int i = 0; i < MAX_USER_PROCESSES; i++) {
    finish[i] = true; // Assume process is finished
    for (int j = 0; j < MAX_RESOURCES; j++) {
      if (resourceTable[j].allocated[i] > 0) {
        finish[i] = false; // Process is not finished
        break;
      }
    }
  }

  // Check if all processes could finish
  for (int i = 0; i < MAX_USER_PROCESSES; i++) {
    if (!finish[i])
      return true; // If any process cannot finish, system is not safe
  }
  return false; // System is safe
}

void resolveDeadlocks(void) {
  for (int i = 0; i < MAX_RESOURCES; i++) {
    for (int j = 0; j < MAX_USER_PROCESSES; j++) {
      if (resourceTable[i].allocated[j] > 0) {
        resourceTable[i].available += resourceTable[i].allocated[j];
        resourceTable[i].allocated[j] = 0;
      }
    }
  }
}

int getAvailable(int resourceType) {
  return resourceTable[resourceType].available;
}

int getAvailableAfter(int resourceType) {
  return resourceTable[resourceType].availableAfter[resourceType];
}

void releaseAllResourcesForProcess(int pid) {
  for (int i = 0; i < MAX_RESOURCES; i++) {
    releaseResources(pid, i, resourceTable[i].allocated[pid]);
    resourceTable[i].allocated[pid] = 0;
  }
}

void releaseResources(int pid, int resourceType, int quantity) {
  if (resourceTable[resourceType].allocated[pid] >= quantity) {
    resourceTable[resourceType].available += quantity;
    resourceTable[resourceType].allocated[pid] -= quantity;
  }
}
