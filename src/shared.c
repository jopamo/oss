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

int simulatedTimeShmId = -1;
int actualTimeShmId = -1;

volatile sig_atomic_t childTerminated = 0;
volatile sig_atomic_t keepRunning = 1;

int maxProcesses = DEFAULT_MAX_PROCESSES;
int maxSimultaneous = DEFAULT_MAX_SIMULTANEOUS;
int childTimeLimit = DEFAULT_CHILD_TIME_LIMIT;
int launchInterval = DEFAULT_LAUNCH_INTERVAL;

int currentChildren = 0;

int currentLogLevel = LOG_LEVEL_DEBUG;

sem_t *clockSem = SEM_FAILED;
const char *clockSemName = "/simClockSem";

pthread_mutex_t logMutex = PTHREAD_MUTEX_INITIALIZER;

int getCurrentChildren(void) { return currentChildren; }
void setCurrentChildren(int value) { currentChildren = value; }

void *attachSharedMemory(int shmId, const char *segmentName) {
  if (shmId < 0 || segmentName == NULL) {
    log_message(LOG_LEVEL_ERROR, "Invalid shared memory ID for %s",
                segmentName);
    return NULL;
  }

  void *shmPtr = shmat(shmId, NULL, 0);
  if (shmPtr == (void *)-1) {
    log_message(LOG_LEVEL_ERROR, "Attaching to %s shared memory failed: %s",
                segmentName, strerror(errno));
    return NULL;
  }

  log_message(LOG_LEVEL_INFO, "Successfully attached to %s shared memory.",
              segmentName);
  return shmPtr;
}

int detachSharedMemory(void **shmPtr, const char *segmentName) {
  if (shmPtr == NULL || *shmPtr == NULL) {
    log_message(LOG_LEVEL_ERROR,
                "Invalid pointer for detaching %s shared memory.", segmentName);
    return -1;
  }

  if (shmdt(*shmPtr) == -1) {
    log_message(LOG_LEVEL_ERROR, "Detaching from %s shared memory failed: %s",
                segmentName, strerror(errno));
    return -1;
  }

  *shmPtr = NULL;
  log_message(LOG_LEVEL_INFO, "Successfully detached from %s shared memory.",
              segmentName);
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
  key_t key = ftok(path, proj_id);
  if (key == -1) {
    log_message(LOG_LEVEL_ERROR, "ftok failed for proj_id %d: %s", proj_id,
                strerror(errno));
  } else {
    log_message(LOG_LEVEL_INFO,
                "ftok success for proj_id %d: Key generated: %d", proj_id, key);
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

  if (simClock) {
    if (detachSharedMemory((void **)&simClock, "Simulated Clock") == 0) {
      log_message(LOG_LEVEL_INFO,
                  "Detached from Simulated Clock shared memory.");
    }
    simClock = NULL;
  }

  if (actualTime) {
    if (detachSharedMemory((void **)&actualTime, "Actual Time") == 0) {
      log_message(LOG_LEVEL_INFO, "Detached from Actual Time shared memory.");
    }
    actualTime = NULL;
  }

  if (clockSem != SEM_FAILED) {
    if (sem_close(clockSem) == 0) {
      log_message(LOG_LEVEL_INFO, "Closed semaphore.");
    }
    if (sem_unlink(clockSemName) == 0) {
      log_message(LOG_LEVEL_INFO, "Unlinked semaphore.");
    }
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

void setupSharedMemory(void) {

  key_t simClockKey = getSharedMemoryKey(SHM_PATH, SHM_PROJ_ID_SIM_CLOCK);
  key_t actualTimeKey = getSharedMemoryKey(SHM_PATH, SHM_PROJ_ID_ACT_TIME);

  if (simClockKey == (key_t)-1 || actualTimeKey == (key_t)-1) {
    log_message(LOG_LEVEL_ERROR, "Failed to generate keys for shared memory.");
    exit(EXIT_FAILURE);
  }

  simulatedTimeShmId = shmget(simClockKey, sizeof(SimulatedClock), 0666);
  if (simulatedTimeShmId == -1) {
    simulatedTimeShmId =
        shmget(simClockKey, sizeof(SimulatedClock), IPC_CREAT | 0666);
    if (simulatedTimeShmId == -1) {
      log_message(LOG_LEVEL_ERROR,
                  "Failed to create Simulated Clock shared memory.");
      exit(EXIT_FAILURE);
    }
  }
  simClock = (SimulatedClock *)shmat(simulatedTimeShmId, NULL, 0);
  if (simClock == (void *)-1) {
    log_message(LOG_LEVEL_ERROR,
                "Failed to attach to Simulated Clock shared memory.");
    exit(EXIT_FAILURE);
  }

  actualTimeShmId = shmget(actualTimeKey, sizeof(ActualTime), 0666);
  if (actualTimeShmId == -1) {
    actualTimeShmId =
        shmget(actualTimeKey, sizeof(ActualTime), IPC_CREAT | 0666);
    if (actualTimeShmId == -1) {
      log_message(LOG_LEVEL_ERROR,
                  "Failed to create Actual Time shared memory.");
      exit(EXIT_FAILURE);
    }
  }
  actualTime = (ActualTime *)shmat(actualTimeShmId, NULL, 0);
  if (actualTime == (void *)-1) {
    log_message(LOG_LEVEL_ERROR,
                "Failed to attach to Actual Time shared memory.");
    exit(EXIT_FAILURE);
  }
}
