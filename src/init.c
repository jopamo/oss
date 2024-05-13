#include "init.h"

int initializeResourceQueues(void) {
  for (int i = 0; i < maxResources; i++) {
    if (initQueue(&resourceQueues[i], maxResources) == -1) {
      log_message(LOG_LEVEL_ERROR, 0,
                  "Failed to initialize resource queue for resource %d", i);
      return -1;
    }
  }
  log_message(LOG_LEVEL_DEBUG, 0, "Resource queues initialized successfully.");
  return 0;
}

int initMessageQueue(void) {
  key_t msg_key =
      ftok(MSG_PATH, MSG_PROJ_ID); // Generate a key for the message queue
  if (msg_key == -1) {             // Check for errors
    log_message(LOG_LEVEL_ERROR, 0, "ftok for msg_key failed: %s",
                strerror(errno));
    return ERROR_INIT_QUEUE;
  }

  msqId =
      msgget(msg_key,
             IPC_CREAT | MSQ_PERMISSIONS); // Create or access the message queue
  if (msqId < 0) {                         // Check for errors
    log_message(LOG_LEVEL_ERROR, 0, "msgget failed: %s", strerror(errno));
    return ERROR_INIT_QUEUE;
  }

  log_message(LOG_LEVEL_DEBUG, 0, "Message queue initialized successfully");
  return msqId;
}

int initializeSemaphore(void) {
  sem_unlink(clockSemName);
  clockSem = sem_open(clockSemName, O_CREAT | O_EXCL, SEM_PERMISSIONS, 1);
  if (clockSem == SEM_FAILED) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to create or open semaphore: %s",
                strerror(errno));
    return -1;
  }
  return SUCCESS;
}

int initializeClockAndTime(void) {
  simClock = (SimulatedClock *)attachSharedMemory(
      SHM_PATH, SHM_PROJ_ID_SIM_CLOCK, sizeof(SimulatedClock),
      "Simulated Clock");
  if (simClock == NULL)
    return -1;

  actualTime = (ActualTime *)attachSharedMemory(
      SHM_PATH, SHM_PROJ_ID_ACT_TIME, sizeof(ActualTime), "Actual Time");
  if (actualTime == NULL)
    return -1;

  return SUCCESS;
}

int initializeResourceTable(void) {
  log_message(LOG_LEVEL_DEBUG, 0, "Attempting to initialize resource table...");

  resourceTable = (ResourceDescriptor *)attachSharedMemory(
      SHM_PATH, SHM_PROJ_ID_RESOURCE_TABLE,
      sizeof(ResourceDescriptor) * maxResources, "Resource Table");

  if (resourceTable == NULL) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Failed to attach to resource table shared memory.");
    return -1;
  }

  log_message(LOG_LEVEL_DEBUG, 0,
              "Attached to resource table shared memory successfully.");

  for (int i = 0; i < maxResources; i++) {
    resourceTable[i].total = maxInstances;
    resourceTable[i].available = maxInstances;
    memset(resourceTable[i].allocated, 0, sizeof(resourceTable[i].allocated));
    log_message(LOG_LEVEL_DEBUG, 0,
                "Resource %d initialized: total=%d, available=%d.", i,
                resourceTable[i].total, resourceTable[i].available);
  }

  log_message(LOG_LEVEL_DEBUG, 0, "Resource table initialized successfully.");
  return SUCCESS;
}

int initializeProcessTable(void) {
  processTable =
      (PCB *)attachSharedMemory(SHM_PATH, SHM_PROJ_ID_PROCESS_TABLE,
                                maxProcesses * sizeof(PCB), "Process Table");
  if (processTable == NULL)
    return ERROR_INIT_SHM;

  return SUCCESS;
}

void initializeSharedResources(void) {
  if (initializeClockAndTime() == -1 ||
      initMessageQueue() == ERROR_INIT_QUEUE || initializeSemaphore() == -1) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Failed to initialize all shared resources");
    exit(EXIT_FAILURE);
  }

  log_message(LOG_LEVEL_DEBUG, 0,
              "All shared resources initialized successfully.");
}
