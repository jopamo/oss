#include "resource.h"

void log_resource_state(const char *operation, int pid, int resourceType,
                        int quantity, int availableBefore, int availableAfter) {
  log_message(LOG_LEVEL_DEBUG,
              "%s operation by process %d on resource %d for %d units. "
              "Available before: %d, after: %d",
              operation, pid, resourceType, quantity, availableBefore,
              availableAfter);
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

int initializeResourceTable(void) {
  log_message(LOG_LEVEL_INFO, "Initializing resource table...");
  key_t resourceTableKey =
      getSharedMemoryKey(SHM_PATH, SHM_PROJ_ID_RESOURCE_TABLE);
  resourceTableShmId =
      shmget(resourceTableKey, sizeof(ResourceDescriptor) * MAX_RESOURCES,
             IPC_CREAT | 0666);
  if (resourceTableShmId < 0) {
    log_message(LOG_LEVEL_ERROR, "shmget failed: %s", strerror(errno));
    return -1;
  }

  resourceTable = (ResourceDescriptor *)shmat(resourceTableShmId, NULL, 0);
  if (resourceTable == (void *)-1) {
    log_message(LOG_LEVEL_ERROR, "shmat failed: %s", strerror(errno));
    return -1;
  }

  for (int i = 0; i < MAX_RESOURCES; i++) {
    for (int j = 0; j < MAX_RESOURCES; j++) {
      resourceTable[i].total[j] = INSTANCES_PER_RESOURCE;
      resourceTable[i].available[j] = INSTANCES_PER_RESOURCE;
      resourceTable[i].availableAfter[j] = INSTANCES_PER_RESOURCE;
    }
    memset(resourceTable[i].allocated, 0, sizeof(resourceTable[i].allocated));
  }
  log_message(LOG_LEVEL_INFO, "Resource table initialized.");
  return 0;
}

int initializeResourceDescriptors(ResourceDescriptor *rd) {
  if (!rd) {
    log_message(LOG_LEVEL_ERROR,
                "Null pointer provided to initializeResourceDescriptors.");
    return -1;
  }
  for (int i = 0; i < MAX_RESOURCES; i++) {
    for (int j = 0; j < MAX_RESOURCES; j++) {
      rd[i].total[j] = INSTANCES_PER_RESOURCE;
      rd[i].available[j] = INSTANCES_PER_RESOURCE;
      rd[i].availableAfter[j] = INSTANCES_PER_RESOURCE;
    }
    memset(rd[i].allocated, 0,
           sizeof(rd[i].allocated[0][0]) * MAX_USER_PROCESSES * MAX_RESOURCES);
  }
  return 0;
}

int requestResource(int resourceType, int quantity, int pid) {
  pthread_mutex_lock(&resourceTableMutex);

  if (resourceTable->available[resourceType] < quantity) {
    pthread_mutex_unlock(&resourceTableMutex);
    log_message(LOG_LEVEL_ERROR,
                "Request failed: not enough resources for PID %d requesting %d "
                "units of resource %d.",
                pid, quantity, resourceType);
    return -1; // Not enough resources
  }

  resourceTable->available[resourceType] -= quantity;
  resourceTable->allocated[pid][resourceType] += quantity;
  log_message(LOG_LEVEL_INFO,
              "Resource requested: PID %d requested %d units of resource %d. "
              "New available: %d.",
              pid, quantity, resourceType,
              resourceTable->available[resourceType]);

  pthread_mutex_unlock(&resourceTableMutex);
  return 0; // Success
}

int releaseResource(int resourceType, int quantity, int pid) {
  pthread_mutex_lock(&resourceTableMutex);
  if (resourceTable->allocated[pid][resourceType] < quantity) {
    pthread_mutex_unlock(&resourceTableMutex);
    log_message(LOG_LEVEL_ERROR,
                "Release failed: PID %d tried to release %d units of resource "
                "%d, but only %d are allocated.",
                pid, quantity, resourceType,
                resourceTable->allocated[pid][resourceType]);
    return -1;
  }

  resourceTable->available[resourceType] += quantity;
  resourceTable->allocated[pid][resourceType] -= quantity;
  log_message(LOG_LEVEL_INFO,
              "Resource released: PID %d released %d units of resource %d. New "
              "available: %d",
              pid, quantity, resourceType,
              resourceTable->available[resourceType]);

  pthread_mutex_unlock(&resourceTableMutex);
  return 0;
}

int initQueue(Queue *q, int capacity) {
  freeQueue(q);

  q->queue = (pid_t *)calloc(capacity, sizeof(pid_t));
  if (q->queue == NULL) {
    // Handle allocation failure; set capacity to 0 to indicate the queue is not
    // usable
    q->front = 0;
    q->rear = 0;
    q->capacity = 0;
    return -1;
  } else {
    // Initialization successful
    q->front = 0;
    q->rear = 0;
    q->capacity = capacity;
    return 0;
  }
}

void freeQueue(Queue *q) {
  if (q->queue != NULL) {
    free(q->queue);
    q->queue = NULL; // Set to NULL to avoid double free
  }
  q->front = 0;
  q->rear = 0;
  q->capacity = 0;
}

int initializeQueues(MLFQ *mlfq) {
  if (initQueue(&mlfq->highPriority, MAX_PROCESSES) == -1 ||
      initQueue(&mlfq->midPriority, MAX_PROCESSES) == -1 ||
      initQueue(&mlfq->lowPriority, MAX_PROCESSES) == -1) {
    freeQueues(mlfq); // Cleanup partially initialized queues
    return -1;
  }
  return 0;
}

void freeQueues(MLFQ *mlfq) {
  freeQueue(&mlfq->highPriority);
  freeQueue(&mlfq->midPriority);
  freeQueue(&mlfq->lowPriority);
}

int timeToCheckDeadlock(void) {
  return simClock && (simClock->seconds % 100 == 0);
}

int checkForDeadlocks(void) {
  int deadlockDetected = 0;
  int waitMatrix[MAX_USER_PROCESSES][MAX_RESOURCES];
  int allocationMatrix[MAX_USER_PROCESSES][MAX_RESOURCES];
  int remainingResources[MAX_RESOURCES];

  // Create a matrix of waiting requests and allocations
  for (int i = 0; i < MAX_USER_PROCESSES; i++) {
    for (int j = 0; j < MAX_RESOURCES; j++) {
      allocationMatrix[i][j] = resourceTable->allocated[i][j];
      waitMatrix[i][j] =
          (resourceTable->total[j] - resourceTable->allocated[i][j]) >
          resourceTable->available[j];
    }
  }

  // Calculate remaining resources
  for (int j = 0; j < MAX_RESOURCES; j++) {
    remainingResources[j] = resourceTable->available[j];
    for (int i = 0; i < MAX_USER_PROCESSES; i++) {
      remainingResources[j] -= allocationMatrix[i][j];
    }
  }

  // Check for processes that are waiting
  int finished[MAX_USER_PROCESSES] = {0};
  int changed;
  do {
    changed = 0;
    for (int i = 0; i < MAX_USER_PROCESSES; i++) {
      if (!finished[i]) {
        int canFinish = 1;
        for (int j = 0; j < MAX_RESOURCES; j++) {
          if (waitMatrix[i][j] && remainingResources[j] <= 0) {
            canFinish = 0;
            break;
          }
        }
        if (canFinish) {
          for (int j = 0; j < MAX_RESOURCES; j++) {
            remainingResources[j] += allocationMatrix[i][j];
          }
          finished[i] = 1;
          changed = 1;
        }
      }
    }
  } while (changed);

  // If any process is not finished, a deadlock exists
  for (int i = 0; i < MAX_USER_PROCESSES; i++) {
    if (!finished[i]) {
      deadlockDetected = 1;
      break;
    }
  }

  return deadlockDetected;
}

void resolveDeadlocks(void) {
  log_message(LOG_LEVEL_INFO, "Attempting to resolve deadlocks.");

  for (int i = 0; i < MAX_RESOURCES; i++) {
    int released =
        0; // Counter to keep track of total released resources for logging
    for (int j = 0; j < MAX_USER_PROCESSES; j++) {
      if (resourceTable->allocated[j][i] > 0) {
        released += resourceTable->allocated[j][i];
        resourceTable->available[i] += resourceTable->allocated[j][i];
        resourceTable->allocated[j][i] = 0; // Reset allocation after release
      }
    }
    log_message(LOG_LEVEL_DEBUG, "Resource %d: Released %d units", i, released);
  }
  log_message(LOG_LEVEL_INFO, "Deadlocks resolved by releasing all resources.");
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

int getAvailable(int resourceType) {
  return resourceTable[resourceType].available[resourceType];
}

int getAvailableAfter(int resourceType) {
  return resourceTable[resourceType]
      .availableAfter[resourceType]; // Access the specific count
}
