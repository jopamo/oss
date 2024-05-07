#include "resource.h"
#include "globals.h"
#include "shared.h"
#include "user_process.h"

void initializeResourceTable(void) {
  key_t resourceTableKey =
      getSharedMemoryKey(SHM_PATH, SHM_PROJ_ID_RESOURCE_TABLE);
  resourceTableShmId =
      shmget(resourceTableKey, sizeof(ResourceDescriptor) * MAX_RESOURCES,
             IPC_CREAT | 0666);
  if (resourceTableShmId < 0) {
    perror("shmget failed");
    exit(EXIT_FAILURE);
  }

  resourceTable = (ResourceDescriptor *)shmat(resourceTableShmId, NULL, 0);
  if (resourceTable == (void *)-1) {
    perror("shmat failed");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < MAX_RESOURCES; i++) {
    resourceTable[i].total[i] =
        INSTANCES_PER_RESOURCE; // Set total for each resource type
    resourceTable[i].available[i] =
        INSTANCES_PER_RESOURCE; // Set available instances

    // Initialize all allocations to zero
    for (int j = 0; j < MAX_USER_PROCESSES; j++) {
      resourceTable[i].allocated[j][i] = 0;
    }
  }
}

void initializeResourceDescriptors(ResourceDescriptor *rd) {
  if (rd == NULL) {
    log_message(LOG_LEVEL_ERROR,
                "Null pointer provided to initializeResourceDescriptors.");
    return;
  }
  for (int i = 0; i < MAX_RESOURCES; i++) {
    rd->total[i] =
        INSTANCES_PER_RESOURCE; // Here, add debugging to confirm the value
    rd->available[i] = INSTANCES_PER_RESOURCE;
    printf("Initializing resource %d: total and available set to %d\n", i,
           INSTANCES_PER_RESOURCE); // Debug output

    for (int j = 0; j < MAX_USER_PROCESSES; j++) {
      rd->allocated[j][i] = 0;
    }
  }
}

int checkSafety(int pid, int resourceType, int request) {
  int tempAvailable[MAX_RESOURCES];
  memcpy(tempAvailable, resourceTable->available, sizeof(tempAvailable));
  int tempAllocation[MAX_USER_PROCESSES][MAX_RESOURCES];
  memcpy(tempAllocation, resourceTable->allocated, sizeof(tempAllocation));

  if (resourceTable->available[resourceType] < request) {
    return 0; // System cannot be safe if resources are insufficient
  }

  tempAvailable[resourceType] -= request;
  tempAllocation[pid][resourceType] += request;

  int finish[MAX_USER_PROCESSES] = {0};
  int iterations = 0;

  do {
    int progress = 0;
    for (int i = 0; i < MAX_USER_PROCESSES; i++) {
      if (!finish[i]) {
        int canFinish = 1;
        for (int j = 0; j < MAX_RESOURCES; j++) {
          if (maxDemand[i][j] - tempAllocation[i][j] > tempAvailable[j]) {
            canFinish = 0;
            break;
          }
        }
        if (canFinish) {
          finish[i] = 1;
          progress = 1;
          for (int k = 0; k < MAX_RESOURCES; k++) {
            tempAvailable[k] += tempAllocation[i][k];
          }
        }
      }
    }
    if (!progress)
      break; // Exit if no progress in this iteration
  } while (++iterations <
           MAX_USER_PROCESSES); // Limit iterations to prevent infinite loops

  for (int i = 0; i < MAX_USER_PROCESSES; i++) {
    if (!finish[i])
      return 0; // If any process cannot finish, system is not safe
  }
  return 1; // If all processes can finish, system is safe
}

void log_resource_state(const char *operation, int pid, int resourceType,
                        int quantity, int availableBefore, int availableAfter) {
  log_message(LOG_LEVEL_DEBUG,
              "%s operation by process %d on resource %d for %d units. "
              "Available before: %d, after: %d",
              operation, pid, resourceType, quantity, availableBefore,
              availableAfter);
}

int requestResource(int resourceType, int quantity, int pid) {
  pthread_mutex_lock(&resourceTableMutex);
  int availableBefore = resourceTable->available[resourceType];
  int safeToAllocate = checkSafety(pid, resourceType, quantity);

  printf("Process %d requesting %d units of resource %d, available before: %d, "
         "safe to allocate: %s\n",
         pid, quantity, resourceType, availableBefore,
         safeToAllocate ? "Yes" : "No");

  if (resourceTable->available[resourceType] >= quantity &&
      safeToAllocate == 1) {
    resourceTable->available[resourceType] -= quantity;
    resourceTable->allocated[pid][resourceType] += quantity;
    printf("Allocation successful: New available: %d, New allocated for PID "
           "%d: %d\n",
           resourceTable->available[resourceType], pid,
           resourceTable->allocated[pid][resourceType]);
    pthread_mutex_unlock(&resourceTableMutex);
    return 0;
  }

  printf("Allocation failed: Not enough resources or not safe to allocate.\n");
  pthread_mutex_unlock(&resourceTableMutex);
  return -1;
}

int releaseResource(int resourceType, int quantity, int pid) {
  pthread_mutex_lock(&resourceTableMutex);
  if (resourceTable->allocated[pid][resourceType] >= quantity) {
    resourceTable->available[resourceType] += quantity;
    resourceTable->allocated[pid][resourceType] -= quantity;
    pthread_mutex_unlock(&resourceTableMutex);
    return 0;
  }
  pthread_mutex_unlock(&resourceTableMutex);
  return -1;
}

void initQueue(Queue *q, int capacity) {
  q->queue = (pid_t *)malloc(capacity * sizeof(pid_t));
  q->front = 0;
  q->rear = 0;
  q->capacity = capacity;
}

void initializeQueues(void) {
  initQueue(&mlfq.highPriority, MAX_PROCESSES);
  initQueue(&mlfq.midPriority, MAX_PROCESSES);
  initQueue(&mlfq.lowPriority, MAX_PROCESSES);
}

int timeToCheckDeadlock(void) {
  return simClock && (simClock->seconds % 100 == 0);
}

int checkForDeadlocks(void) {
  int deadlockDetected = 0;
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (processTable[i].occupied && processTable[i].blocked) {
      for (int j = 0; j < MAX_RESOURCES; j++) {
        if (resourceTable->allocated[i][j] > 0 &&
            resourceTable->available[j] == 0) {
          deadlockDetected = 1; // Deadlock potential found
          break;
        }
      }
    }
    if (deadlockDetected)
      break;
  }
  return deadlockDetected; // Return whether a deadlock was detected
}

void resolveDeadlocks(void) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (processTable[i].occupied && processTable[i].blocked) {
      for (int j = 0; j < MAX_RESOURCES; j++) {
        if (resourceTable->allocated[i][j] > 0) {
          // Release resources one type at a time
          releaseResource(j, resourceTable->allocated[i][j],
                          processTable[i].pid);
        }
      }
    }
  }
}

void releaseResources(int pid, int resourceType, int quantity) {
  // Release a specific quantity of a resource from a specific process
  if (resourceTable->allocated[pid][resourceType] >= quantity) {
    resourceTable->available[resourceType] += quantity;
    resourceTable->allocated[pid][resourceType] -= quantity;
  }
}

void handleResourceRequest(int pid, int resourceType, int quantity) {
  if (requestResource(resourceType, quantity, pid) != 0) {
    printf("Resource request by PID %d for %d units of type %d failed.\n", pid,
           quantity, resourceType);
  } else {
    printf("Resource request by PID %d for %d units of type %d succeeded.\n",
           pid, quantity, resourceType);
  }
}
