#include "shared.h"

ProcessType gProcessType;
struct timeval startTime;
struct timeval lastLogTime;
SimulatedClock *simClock = NULL;
ActualTime *actualTime = NULL;
PCB processTable[DEFAULT_MAX_PROCESSES];
FILE *logFile = NULL;
char logFileName[256] = DEFAULT_LOG_FILE_NAME;

int msqId = -1;
int shmId = -1;
int actualTimeShmId = -1;

volatile sig_atomic_t childTerminated = 0;
volatile sig_atomic_t keepRunning = 1;

int maxProcesses = DEFAULT_MAX_PROCESSES;
int maxSimultaneous = DEFAULT_MAX_SIMULTANEOUS;
int childTimeLimit = DEFAULT_CHILD_TIME_LIMIT;
int launchInterval = DEFAULT_LAUNCH_INTERVAL;

int currentChildren = 0;

int currentLogLevel = LOG_LEVEL_DEBUG;

sem_t *clockSem = NULL;
const char *clockSemName = "/simClockSem";

pthread_mutex_t logMutex = PTHREAD_MUTEX_INITIALIZER;

int getCurrentChildren(void) { return currentChildren; }
void setCurrentChildren(int value) { currentChildren = value; }

int initSharedMemorySegment(key_t key, size_t size, int *shmIdPtr,
                            const char *segmentName) {
  shmId = shmget(key, size, 0600 | IPC_CREAT | IPC_EXCL);
  if (shmId < 0) {
    if (errno == EEXIST) {
      shmId = shmget(key, size, 0600);
      if (shmId < 0) {
        log_message(LOG_LEVEL_ERROR, "%s shmget existing failed: %s",
                    segmentName, strerror(errno));
        return -1;
      }
      log_message(LOG_LEVEL_INFO, "%s shared memory segment attached, shmId=%d",
                  segmentName, shmId);
    } else {
      log_message(LOG_LEVEL_ERROR, "%s shmget failed: %s", segmentName,
                  strerror(errno));
      return -1;
    }
  } else {
    log_message(LOG_LEVEL_INFO, "%s shared memory segment created, shmId=%d",
                segmentName, shmId);
  }
  *shmIdPtr = shmId;
  return shmId;
}

void *attachSharedMemorySegment(int shmId, const char *segmentName) {
  void *shmPtr = shmat(shmId, NULL, 0);
  if (shmPtr == (void *)-1) {
    log_message(LOG_LEVEL_ERROR, "Attaching %s shared memory failed: %s",
                segmentName, strerror(errno));
    return NULL;
  }
  return shmPtr;
}

void initializeSharedMemorySegments(void) {
  key_t simClockKey = ftok(SHM_PATH, SHM_PROJ_ID);
  if (simClockKey == -1) {
    log_message(LOG_LEVEL_ERROR, "ftok for Simulated Clock failed: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }
  if (initSharedMemorySegment(simClockKey, sizeof(SimulatedClock), &shmId,
                              "Simulated Clock") < 0) {
    exit(EXIT_FAILURE);
  }
  simClock =
      (SimulatedClock *)attachSharedMemorySegment(shmId, "Simulated Clock");
  if (simClock == NULL) {
    exit(EXIT_FAILURE);
  }

  key_t actualTimeKey = ftok(SHM_PATH, SHM_PROJ_ID + 1);
  if (actualTimeKey == -1) {
    log_message(LOG_LEVEL_ERROR, "ftok for Actual Time failed: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }
  if (initSharedMemorySegment(actualTimeKey, sizeof(ActualTime),
                              &actualTimeShmId, "Actual Time") < 0) {
    exit(EXIT_FAILURE);
  }
  actualTime =
      (ActualTime *)attachSharedMemorySegment(actualTimeShmId, "Actual Time");
  if (actualTime == NULL) {
    exit(EXIT_FAILURE);
  }

  log_message(LOG_LEVEL_INFO, "Shared memory for Simulated Clock and Actual "
                              "Time initialized and attached.");
}

int detachSharedMemorySegment(int shmId, void **shmPtr,
                              const char *segmentName) {
  if (shmId < 0 || *shmPtr == NULL) {
    log_message(LOG_LEVEL_WARN,
                "Attempted to detach %s shared memory with invalid parameters.",
                segmentName);
    return -1;
  }

  if (shmdt(*shmPtr) == -1) {
    log_message(LOG_LEVEL_ERROR, "Failed to detach from %s shared memory: %s",
                segmentName, strerror(errno));
    return -1;
  }

  *shmPtr = NULL;
  return 0;
}

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

void log_message(int level, const char *format, ...) {
  if (level < currentLogLevel)
    return;

  if (pthread_mutex_lock(&logMutex) != 0) {
    fprintf(stderr, "Error locking log mutex\n");
    return;
  }

  char buffer[LOG_BUFFER_SIZE];
  int offset = snprintf(buffer, sizeof(buffer), "[%s] ",
                        processTypeToString(gProcessType));

  va_list args;
  va_start(args, format);
  vsnprintf(buffer + offset, sizeof(buffer) - offset, format, args);
  va_end(args);

  buffer[sizeof(buffer) - 1] = '\0';

  fprintf(stderr, "%s\n", buffer);
  if (logFile) {
    fprintf(logFile, "%s\n", buffer);
    fflush(logFile);
  }

  if (pthread_mutex_unlock(&logMutex) != 0) {
    fprintf(stderr, "Error unlocking log mutex\n");
  }
}

key_t getSharedMemoryKey(const char *path, int proj_id) {
  static key_t key = -1;
  if (key == -1) {
    key = ftok(path, proj_id);
    if (key == -1) {
      log_message(LOG_LEVEL_ERROR, "ftok failed: %s", strerror(errno));
    }
  }
  return key;
}

int initMessageQueue(void) {
  key_t msg_key = ftok(MSG_PATH, MSG_PROJ_ID);
  if (msg_key == -1) {
    log_message(LOG_LEVEL_ERROR, "ftok for msg_key failed: %s",
                strerror(errno));
    return -1;
  }

  msqId = msgget(msg_key, IPC_CREAT | 0666);
  if (msqId < 0) {
    log_message(LOG_LEVEL_ERROR, "msgget failed: %s", strerror(errno));
    return -1;
  }

  log_message(LOG_LEVEL_DEBUG, "Message queue initialized successfully");
  return msqId;
}

int sendMessage(int msqId, Message *msg) {
  if (msgsnd(msqId, msg, sizeof(*msg) - sizeof(long), 0) == -1) {
    log_message(LOG_LEVEL_ERROR,
                "[SEND] Error: Failed to send message. Type: %ld, Error: %s",
                msg->mtype, strerror(errno));
    return -1;
  } else {
    log_message(LOG_LEVEL_INFO,
                "[SEND] Success: Message sent. Type: %ld, Content: %d",
                msg->mtype, msg->mtext);
    return 0;
  }
}

int receiveMessage(int msqId, Message *msg, long msgType, int flags) {
  while (keepRunning) {
    ssize_t result =
        msgrcv(msqId, msg, sizeof(*msg) - sizeof(long), msgType, flags);
    if (result == -1) {
      if (errno == EINTR) {

        if (!keepRunning) {
          log_message(LOG_LEVEL_INFO,
                      "[RECEIVE] Interrupted by signal, terminating.");
          break;
        }

        continue;
      } else {
        log_message(LOG_LEVEL_ERROR,
                    "[RECEIVE] Error: Failed to receive message. Expected "
                    "Type: %ld, Error: %s",
                    msgType, strerror(errno));
        return -1;
      }
    } else {
      log_message(LOG_LEVEL_INFO,
                  "[RECEIVE] Success: Message received. Type: %ld, Content: %d",
                  msg->mtype, msg->mtext);
      return 0;
    }
  }
  return -1;
}

void cleanupSharedResources(void) {
  if (clockSem != SEM_FAILED) {
    sem_wait(clockSem);

    detachSharedMemorySegment(shmId, (void **)&simClock, "Simulated Clock");

    sem_close(clockSem);
    sem_unlink(clockSemName);
    clockSem = SEM_FAILED;
  }
}

void prepareAndSendMessage(int msqId, pid_t pid, int message) {
  Message msg;
  msg.mtype = pid;
  msg.mtext = message;

  if (msgsnd(msqId, &msg, sizeof(msg.mtext), 0) == -1) {
    perror("msgsnd failed");
  }
}
