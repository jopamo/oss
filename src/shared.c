#include "shared.h"
#include "globals.h"
#include "resource.h"
#include "user_process.h"

// Returns the current number of child processes
int getCurrentChildren(void) { return currentChildren; }

// Sets the current number of child processes
void setCurrentChildren(int value) { currentChildren = value; }

// Attaches to shared memory for various system components
void *attachSharedMemory(const char *path, int proj_id, size_t size,
                         const char *segmentName) {
  key_t key = getSharedMemoryKey(path, proj_id); // Generate a unique key
  int shmId = shmget(key, size,
                     0666 | IPC_CREAT); // Get or create shared memory segment
  if (shmId < 0) {                      // Check for error in shmget
    log_message(LOG_LEVEL_ERROR, "Failed to obtain shmId for %s: %s",
                segmentName, strerror(errno));
    return NULL;
  }
  void *shmPtr = shmat(shmId, NULL, 0); // Attach to the shared memory segment
  if (shmPtr == (void *)-1) {           // Check for error in shmat
    log_message(LOG_LEVEL_ERROR, "Failed to attach to %s shared memory: %s",
                segmentName, strerror(errno));
    return NULL;
  }
  return shmPtr;
}

// Detaches the shared memory segment
int detachSharedMemory(void **shmPtr, const char *segmentName) {
  if (shmPtr == NULL || *shmPtr == NULL) { // Validate pointer
    log_message(LOG_LEVEL_ERROR,
                "Invalid pointer for detaching %s shared memory.", segmentName);
    return -1;
  }

  if (shmdt(*shmPtr) == -1) { // Detach from shared memory
    log_message(LOG_LEVEL_ERROR, "Detaching from %s shared memory failed: %s",
                segmentName, strerror(errno));
    return -1;
  }

  *shmPtr = NULL; // Clear the pointer after detachment
  log_message(LOG_LEVEL_INFO, "Successfully detached from %s shared memory.",
              segmentName);
  return 0;
}

// Converts the process type enum to a readable string
const char *processTypeToString(ProcessType type) {
  switch (type) {
  case PROCESS_TYPE_OSS:
    return "OSS";
  case PROCESS_TYPE_WORKER:
    return "Worker";
  case PROCESS_TYPE_TIMEKEEPER:
    return "Timekeeper";
  default:
    return "Unknown";
  }
}

// Logs messages to stderr or a file with a variable argument list based on the
// logging level
void log_message(int level, const char *format, ...) {
  if (level < currentLogLevel)
    return; // Skip logging if the level is below the current setting

  if (pthread_mutex_lock(&logMutex) !=
      0) { // Acquire mutex for thread-safe logging
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

  fprintf(stderr, "%s\n", buffer); // Output to stderr
  if (logFile) {                   // If a log file is open, also log to it
    fprintf(logFile, "%s\n", buffer);
    fflush(logFile);
  }

  if (pthread_mutex_unlock(&logMutex) != 0) { // Release mutex
    fprintf(stderr, "Error unlocking log mutex\n");
  }
}

// Generates a key filesystem path and project identifier
key_t getSharedMemoryKey(const char *path, int proj_id) {
  key_t key = ftok(path, proj_id); // Generate a System V IPC key
  if (key == -1) {                 // Check for errors
    log_message(LOG_LEVEL_ERROR, "ftok failed for proj_id %d: %s", proj_id,
                strerror(errno));
  } else {
    log_message(LOG_LEVEL_INFO,
                "ftok success for proj_id %d: Key generated: %d", proj_id, key);
  }
  return key;
}

// Initializes a System V message queue for interprocess communication.
int initMessageQueue(void) {
  key_t msg_key =
      ftok(MSG_PATH, MSG_PROJ_ID); // Generate a key for the message queue
  if (msg_key == -1) {             // Check for errors
    log_message(LOG_LEVEL_ERROR, "ftok for msg_key failed: %s",
                strerror(errno));
    return -1;
  }

  msqId =
      msgget(msg_key, IPC_CREAT | 0666); // Create or access the message queue
  if (msqId < 0) {                       // Check for errors
    log_message(LOG_LEVEL_ERROR, "msgget failed: %s", strerror(errno));
    return -1;
  }

  log_message(LOG_LEVEL_DEBUG, "Message queue initialized successfully");
  return msqId;
}

// Sends a message to a message queue.
int sendMessage(int msqId, Message *msg) {
  better_sem_wait(clockSem); // Synchronize access to shared resources

  log_message(
      LOG_LEVEL_ANNOY,
      "[SEND] Attempting to send message. msqId: %d, Type: %ld, Content: %d",
      msqId, msg->mtype, msg->mtext);

  size_t messageSize =
      sizeof(*msg) - sizeof(long); // Calculate the size of the message
  int result;
  do { // Attempt to send the message
    result = msgsnd(msqId, msg, messageSize, 0);
    if (result == -1 && errno == EINTR) {
      log_message(LOG_LEVEL_INFO,
                  "[SEND] Send interrupted by signal, retrying.");
    } else if (result == -1) {
      log_message(LOG_LEVEL_ERROR,
                  "[SEND] Error: Failed to send message. msqId: %d, Type: %ld, "
                  "Content: %d, Error: %s (%d)",
                  msqId, msg->mtype, msg->mtext, strerror(errno), errno);
      return -1;
    }
  } while (keepRunning == 1 && result == -1 && errno == EINTR);

  log_message(LOG_LEVEL_ANNOY,
              "[SEND] Success: Message sent. msqId: %d, Type: %ld, Content: %d",
              msqId, msg->mtype, msg->mtext);
  sem_post(clockSem);

  return 0;
}

// Receives a message from a message queue.
int receiveMessage(int msqId, Message *msg, long msgType, int flags) {
  better_sem_wait(clockSem); // Synchronize access to shared resources

  log_message(LOG_LEVEL_ANNOY,
              "[RECEIVE] Attempting to receive message. msqId: %d, Expected "
              "Type: %ld, Flags: %d",
              msqId, msgType, flags);

  ssize_t result;
  do { // Attempt to receive the message
    result = msgrcv(msqId, msg, sizeof(*msg) - sizeof(long), msgType, flags);
    if (result == -1) {
      if (errno == EINTR) {
        log_message(
            LOG_LEVEL_INFO,
            "[RECEIVE] Interrupted by signal, checking if should terminate.");
        if (!keepRunning) {
          log_message(LOG_LEVEL_INFO,
                      "[RECEIVE] Terminating due to signal interruption.");
          return -1;
        }
      } else {
        log_message(LOG_LEVEL_ERROR,
                    "[RECEIVE] Error: Failed to receive message. msqId: %d, "
                    "Expected Type: %ld, Error: %s (%d)",
                    msqId, msgType, strerror(errno), errno);
        return -1;
      }
    }
  } while (keepRunning == 1 && result == -1 && errno == EINTR);

  if (result >= 0) {
    log_message(LOG_LEVEL_INFO,
                "[RECEIVE] Success: Message received. msqId: %d, Type: %ld, "
                "Content: %d",
                msqId, msg->mtype, msg->mtext);
    return 0;
  }

  sem_post(clockSem);
  return -1;
}

// Cleans up shared resources by detaching from shared memory segments
void cleanupSharedResources(void) {
  better_sem_wait(clockSem); // Wait on semaphore to synchronize access

  if (simClock) {
    if (detachSharedMemory((void **)&simClock, "Simulated Clock") ==
        0) { // Detach from simulated clock shared memory
      log_message(LOG_LEVEL_INFO,
                  "Detached from Simulated Clock shared memory.");
    }
    simClock = NULL; // Clear the pointer after detaching
  }

  if (actualTime) {
    if (detachSharedMemory((void **)&actualTime, "Actual Time") ==
        0) { // Detach from actual time shared memory
      log_message(LOG_LEVEL_INFO, "Detached from Actual Time shared memory.");
    }
    actualTime = NULL; // Clear the pointer after detaching
  }
  sem_post(clockSem); // Release semaphore after operation
}

// Initializes the semaphore used for synchronizing access to shared resources
void initializeSemaphore(void) {
  clockSem = sem_open(clockSemName, O_CREAT, 0644,
                      1); // Open or create a semaphore with initial value 1

  if (clockSem == SEM_FAILED) { // Check if semaphore creation failed
    log_message(LOG_LEVEL_ERROR, "Failed to create or open semaphore: %s",
                strerror(errno));
    exit(EXIT_FAILURE); // Exit if semaphore creation fails
  }
}

// Initializes the simulated clock shared memory segment
void initializeSimulatedClock(void) {
  // Generate a unique key for the simulated clock's shared memory
  key_t simClockKey = ftok(SHM_PATH, SHM_PROJ_ID_SIM_CLOCK);
  if (simClockKey == -1) {
    log_message(LOG_LEVEL_ERROR,
                "Failed to generate key for Simulated Clock: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Create or access a shared memory segment for the simulated clock
  simulatedTimeShmId =
      shmget(simClockKey, sizeof(SimulatedClock), IPC_CREAT | 0666);
  if (simulatedTimeShmId < 0) {
    log_message(
        LOG_LEVEL_ERROR,
        "Failed to create or open shared memory for Simulated Clock: %s",
        strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Attach to the shared memory segment
  simClock = (SimulatedClock *)shmat(simulatedTimeShmId, NULL, 0);
  if (simClock == (void *)-1) {
    log_message(LOG_LEVEL_ERROR,
                "Failed to attach to shared memory for Simulated Clock: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }
}

// Initializes the actual time shared memory segment
void initializeActualTime(void) {
  // Generate a unique key for the actual time's shared memory
  key_t actualTimeKey = ftok(SHM_PATH, SHM_PROJ_ID_ACT_TIME);
  if (actualTimeKey == -1) {
    log_message(LOG_LEVEL_ERROR, "Failed to generate key for Actual Time: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Create or access a shared memory segment for the actual time
  actualTimeShmId = shmget(actualTimeKey, sizeof(ActualTime), IPC_CREAT | 0666);
  if (actualTimeShmId < 0) {
    log_message(LOG_LEVEL_ERROR,
                "Failed to create or open shared memory for Actual Time: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Attach to the shared memory segment
  actualTime = (ActualTime *)shmat(actualTimeShmId, NULL, 0);
  if (actualTime == (void *)-1) {
    log_message(LOG_LEVEL_ERROR,
                "Failed to attach to shared memory for Actual Time: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }
}

// Initializes the process table shared memory segment
void initializeProcessTable(void) {
  // Generate a unique key for the process table's shared memory
  key_t processTableKey =
      getSharedMemoryKey(SHM_PATH, SHM_PROJ_ID_PROCESS_TABLE);

  // Create or access a shared memory segment for the process table
  processTableShmId = shmget(
      processTableKey, DEFAULT_MAX_PROCESSES * sizeof(PCB), IPC_CREAT | 0666);
  if (processTableShmId < 0) {
    log_message(LOG_LEVEL_ERROR,
                "Failed to create shared memory for processTable: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Attach to the shared memory segment
  processTable = (PCB *)shmat(processTableShmId, NULL, 0);
  if (processTable == (void *)-1) {
    log_message(LOG_LEVEL_ERROR,
                "Failed to attach to processTable shared memory: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void initializeSharedResources(void) {
  msqId = initMessageQueue();

  initializeSemaphore();

  initializeSimulatedClock();

  initializeActualTime();

  initializeProcessTable();

  initializeResourceTable();

  log_message(LOG_LEVEL_INFO,
              "All shared resources have been successfully initialized.");
}
