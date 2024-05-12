#include "globals.h"
#include "resource.h"
#include "shared.h"
#include "user_process.h"

void *safe_shmat(int shmId, const void *shmaddr, int shmflg) {
  void *addr;
  int retries = 5;
  while (retries--) {
    addr = shmat(shmId, shmaddr, shmflg);
    if (addr == (void *)-1 && errno == EINTR && retries) {
      log_message(LOG_LEVEL_WARN, 0, "shmat interrupted, retrying...");
      continue;
    }
    break;
  }
  return addr;
}

// Initialize resource table in shared memory with retry logic
int initializeResourceTable(void) {
  log_message(LOG_LEVEL_INFO, 0, "Initializing resource table...");
  key_t key = ftok(SHM_PATH, SHM_PROJ_ID_RESOURCE_TABLE);
  int shmId =
      shmget(key, sizeof(ResourceDescriptor) * MAX_RESOURCES, IPC_CREAT | 0666);
  if (shmId < 0) {
    log_message(LOG_LEVEL_ERROR, 0, "shmget failed: %s", strerror(errno));
    return -1;
  }

  ResourceDescriptor *resourceTable =
      (ResourceDescriptor *)safe_shmat(shmId, NULL, 0);
  if (resourceTable == (void *)-1) {
    log_message(LOG_LEVEL_ERROR, 0, "shmat failed: %s", strerror(errno));
    return -1;
  }

  for (int i = 0; i < MAX_RESOURCES; i++) {
    resourceTable[i].total = INSTANCES_PER_RESOURCE;
    resourceTable[i].available = INSTANCES_PER_RESOURCE;
    memset(resourceTable[i].allocated, 0, sizeof(resourceTable[i].allocated));
  }
  log_message(LOG_LEVEL_INFO, 0, "Resource table initialized successfully.");
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
      msgget(msg_key, IPC_CREAT | 0666); // Create or access the message queue
  if (msqId < 0) {                       // Check for errors
    log_message(LOG_LEVEL_ERROR, 0, "msgget failed: %s", strerror(errno));
    return ERROR_INIT_QUEUE;
  }

  log_message(LOG_LEVEL_DEBUG, 0, "Message queue initialized successfully");
  return msqId;
}

int initializeSemaphore(void) {
  sem_unlink(clockSemName);
  clockSem = sem_open(clockSemName, O_CREAT | O_EXCL, 0644, 1);
  if (clockSem == SEM_FAILED) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to create or open semaphore: %s",
                strerror(errno));
    return -1;
  }
  return SUCCESS;
}

int initializeSimulatedClock(void) {
  key_t simClockKey = ftok(SHM_PATH, SHM_PROJ_ID_SIM_CLOCK);
  if (simClockKey == -1) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Failed to generate key for Simulated Clock: %s",
                strerror(errno));
    return ERROR_INIT_SHM;
  }

  simulatedTimeShmId =
      shmget(simClockKey, sizeof(SimulatedClock), IPC_CREAT | 0666);
  if (simulatedTimeShmId < 0) {
    log_message(
        LOG_LEVEL_ERROR, 0,
        "Failed to create or open shared memory for Simulated Clock: %s",
        strerror(errno));
    return ERROR_INIT_SHM;
  }

  simClock = (SimulatedClock *)shmat(simulatedTimeShmId, NULL, 0);
  if (simClock == (void *)-1) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Failed to attach to shared memory for Simulated Clock: %s",
                strerror(errno));
    return ERROR_INIT_SHM;
  }
  return SUCCESS;
}

int initializeActualTime(void) {
  key_t actualTimeKey = ftok(SHM_PATH, SHM_PROJ_ID_ACT_TIME);
  if (actualTimeKey == -1) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Failed to generate key for Actual Time: %s", strerror(errno));
    return ERROR_INIT_SHM;
  }

  actualTimeShmId = shmget(actualTimeKey, sizeof(ActualTime), IPC_CREAT | 0666);
  if (actualTimeShmId < 0) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Failed to create or open shared memory for Actual Time: %s",
                strerror(errno));
    return ERROR_INIT_SHM;
  }

  actualTime = (ActualTime *)shmat(actualTimeShmId, NULL, 0);
  if (actualTime == (void *)-1) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Failed to attach to shared memory for Actual Time: %s",
                strerror(errno));
    return ERROR_INIT_SHM;
  }
  return SUCCESS;
}

// Initialization of the process table in shared memory
int initializeProcessTable(void) {
  key_t processTableKey =
      getSharedMemoryKey(SHM_PATH, SHM_PROJ_ID_PROCESS_TABLE);
  processTableShmId = shmget(
      processTableKey, DEFAULT_MAX_PROCESSES * sizeof(PCB), IPC_CREAT | 0666);
  if (processTableShmId < 0) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Failed to create shared memory for processTable: %s",
                strerror(errno));
    return ERROR_INIT_SHM;
  }

  processTable = (PCB *)shmat(processTableShmId, NULL, 0);
  if (processTable == (void *)-1) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Failed to attach to processTable shared memory: %s",
                strerror(errno));
    return ERROR_INIT_SHM;
  }
  return SUCCESS;
}

void initializeSharedResources(void) {
  if (initMessageQueue() == ERROR_INIT_QUEUE) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to initialize message queue");
    exit(EXIT_FAILURE);
  }
  if (initializeSemaphore() == -1) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to initialize semaphore");
    exit(EXIT_FAILURE);
  }
  if (initializeSimulatedClock() == -1) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to initialize simulated clock");
    exit(EXIT_FAILURE);
  }
  if (initializeActualTime() == -1) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to initialize actual time");
    exit(EXIT_FAILURE);
  }
  if (initializeProcessTable() == -1) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to initialize process table");
    exit(EXIT_FAILURE);
  }
  if (initializeResourceTable() == -1) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to initialize resource table");
    exit(EXIT_FAILURE);
  }
  log_message(LOG_LEVEL_INFO, 0,
              "All shared resources initialized successfully.");
}
