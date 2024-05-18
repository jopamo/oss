#include "shared.h"

Message createWorkerMessage(pid_t senderPid, int commandType,
                            int resourceType) {
  Message msg;
  msg.senderPid = senderPid;
  msg.commandType = commandType;
  msg.resourceType = resourceType;
  msg.senderID = 1; // Worker senderID
  msg.count = 1;
  return msg;
}

Message createPsmgmtMessage(pid_t senderPid, int commandType, int resourceType,
                            int count) {
  Message msg;
  msg.senderPid = senderPid;
  msg.commandType = commandType;
  msg.resourceType = resourceType;
  msg.senderID = 0; // psmgmt senderID
  msg.count = count;
  return msg;
}

int getCurrentChildren(void) { return currentChildren; }

void setCurrentChildren(int value) { currentChildren = value; }

void *attachSharedMemory(const char *path, int proj_id, size_t size,
                         const char *segmentName) {
  key_t key = getSharedMemoryKey(path, proj_id);
  log_message(
      LOG_LEVEL_DEBUG, 0,
      "Attempting to create or connect to shared memory for %s with key %d",
      segmentName, key);

  int shmId = shmget(key, size, SHM_PERMISSIONS | IPC_CREAT);
  if (shmId < 0) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to obtain shmId for %s due to: %s",
                segmentName, strerror(errno));
    return NULL;
  }

  void *shmPtr = safe_shmat(shmId, NULL, 0); // Use safe_shmat instead of shmat
  if (shmPtr == (void *)-1) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Failed to attach to %s shared memory due to: %s", segmentName,
                strerror(errno));
    return NULL;
  }

  log_message(LOG_LEVEL_DEBUG, 0,
              "Successfully attached to shared memory for %s", segmentName);
  return shmPtr;
}

int detachSharedMemory(void **shmPtr, const char *segmentName) {
  if (shmPtr == NULL || *shmPtr == NULL) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Invalid pointer for detaching %s shared memory.", segmentName);
    return -1;
  }

  if (shmdt(*shmPtr) == -1) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Detaching from %s shared memory failed: %s", segmentName,
                strerror(errno));
    return -1;
  }

  *shmPtr = NULL; // Clear the pointer
  log_message(LOG_LEVEL_DEBUG, 0,
              "Successfully detached from %s shared memory.", segmentName);
  return 0;
}

// Converts the process type enum to a readable string
const char *processTypeToString(ProcessType type) {
  switch (type) {
  case PROCESS_TYPE_PSMGMT:
    return "psmgmt";
  case PROCESS_TYPE_WORKER:
    return "worker";
  default:
    return "Unknown";
  }
}

void log_message(int level, int logToFile, const char *format, ...) {
  if (level < currentLogLevel)
    return; // Skip logging if the level is below the current setting

  if (pthread_mutex_lock(&logMutex) != 0) {
    fprintf(stderr, "Error locking log mutex\n");
    return;
  }

  char buffer[LOG_BUFFER_SIZE]; // Buffer to hold the formatted message
  int offset =
      snprintf(buffer, sizeof(buffer), "[%s] ",
               processTypeToString(gProcessType)); // Prefix with process type

  va_list args;
  va_start(args, format);
  vsnprintf(buffer + offset, sizeof(buffer) - offset, format,
            args); // Format the rest of the message
  va_end(args);

  // Always output to stderr
  fprintf(stderr, "%s\n", buffer);

  // Optionally log to file if enabled and file is open
  if (logToFile && logFile) {
    fprintf(logFile, "%s\n", buffer);
    fflush(logFile);
  }

  if (pthread_mutex_unlock(&logMutex) != 0) { // Release mutex
    fprintf(stderr, "Error unlocking log mutex\n");
  }
}

key_t getSharedMemoryKey(const char *path, int proj_id) {
  key_t key = ftok(path, proj_id); // Generate a System V IPC key
  if (key == -1) {                 // Check for errors
    log_message(LOG_LEVEL_ERROR, 0, "ftok failed for proj_id %d: %s", proj_id,
                strerror(errno));
  } else {
    log_message(LOG_LEVEL_DEBUG, 0,
                "ftok success for proj_id %d: Key generated: %d", proj_id, key);
  }
  return key;
}

int sendMessage(int msqId, const void *msg, size_t msgSize) {
  better_sem_wait(clockSem); // Synchronize access to shared resources

  int result = msgsnd(msqId, msg, msgSize - sizeof(long),
                      0); // Subtract sizeof(long) to exclude the mtype
  if (result == -1) {
    log_message(
        LOG_LEVEL_ERROR, 0,
        "[SEND] Error: Failed to send message. msqId: %d, Error: %s (%d)",
        msqId, strerror(errno), errno);
    better_sem_post(clockSem);
    return -1;
  }

  log_message(LOG_LEVEL_DEBUG, 0, "[SEND] Success: Message sent. msqId: %d",
              msqId);
  better_sem_post(clockSem);
  return 0;
}

int receiveMessage(int msqId, void *msg, size_t msgSize, long msgType,
                   int flags) {
  while (true) {
    if (better_sem_wait(clockSem) == 0) {
      ssize_t result =
          msgrcv(msqId, msg, msgSize - sizeof(long), msgType, flags);

      if (result == -1) {
        if (errno == ENOMSG) { // No message of the desired type was found
          log_message(LOG_LEVEL_DEBUG, 0,
                      "[RECEIVE] Info: No message of desired type available. "
                      "Continuing.");
          better_sem_post(clockSem);
          return -1;
        }
        log_message(LOG_LEVEL_ERROR, 0,
                    "[RECEIVE] Error: Failed to receive message. msqId: %d, "
                    "Error: %s (%d)",
                    msqId, strerror(errno), errno);
        better_sem_post(clockSem);
        return -1;
      }
      log_message(LOG_LEVEL_DEBUG, 0,
                  "[RECEIVE] Success: Message received. msqId: %d", msqId);
      better_sem_post(clockSem);
      return 0;
    }
    better_sleep(0, 10000000); // Sleep for 10ms to avoid busy-waiting
  }
}

void cleanupSharedResources(void) {
  log_message(LOG_LEVEL_DEBUG, 0, "Starting %s.", __func__);

  better_sem_wait(clockSem);

  if (simClock) {
    if (detachSharedMemory((void **)&simClock, "Simulated Clock") != 0) {
      log_message(LOG_LEVEL_ERROR, 0,
                  "Failed to detach Simulated Clock shared memory.");
    } else {
      log_message(LOG_LEVEL_DEBUG, 0,
                  "Detached from Simulated Clock shared memory.");
      simClock = NULL;
    }
  }

  if (actualTime) {
    if (detachSharedMemory((void **)&actualTime, "Actual Time") != 0) {
      log_message(LOG_LEVEL_ERROR, 0,
                  "Failed to detach Actual Time shared memory.");
    } else {
      log_message(LOG_LEVEL_DEBUG, 0,
                  "Detached from Actual Time shared memory.");
      actualTime = NULL;
    }
  }

  if (better_sem_post(clockSem) != 0) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to release semaphore in %s: %s",
                __func__, strerror(errno));
  }
}
