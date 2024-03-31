#include "shared.h"

ProcessType gProcessType;
struct timeval startTime;
struct timeval lastLogTime;
SimulatedClock *simClock = NULL;
PCB processTable[DEFAULT_MAX_PROCESSES];
FILE *logFile = NULL;
char logFileName[256] = DEFAULT_LOG_FILE_NAME;
volatile sig_atomic_t keepRunning = 1;
int msqId = -1;
int shmId = -1;

int maxProcesses = DEFAULT_MAX_PROCESSES;
int maxSimultaneous = DEFAULT_MAX_SIMULTANEOUS;
int childTimeLimit = DEFAULT_CHILD_TIME_LIMIT;
int launchInterval = DEFAULT_LAUNCH_INTERVAL;

int currentChildren = 0;

int currentLogLevel = LOG_LEVEL_DEBUG;

sem_t *clockSem = NULL;
const char *clockSemName = "/simClockSem";

int getCurrentChildren(void) { return currentChildren; }
void setCurrentChildren(int value) { currentChildren = value; }

void log_message(int level, const char *format, ...) {
  if (level < currentLogLevel) {
    return;
  }

  const char *processTypeStr;
  switch (gProcessType) {
  case PROCESS_TYPE_OSS:
    processTypeStr = "[OSS] ";
    break;
  case PROCESS_TYPE_WORKER:
    processTypeStr = "[Worker] ";
    break;
  case PROCESS_TYPE_TIMEKEEPER:
    processTypeStr = "[Timekeeper] ";
    break;
  default:
    processTypeStr = "[Unknown] ";
  }

  va_list args;
  va_start(args, format);
  int needed = vsnprintf(NULL, 0, format, args) + strlen(processTypeStr) + 1;
  va_end(args);

  char *buffer = (char *)malloc(needed);
  if (buffer) {
    strcpy(buffer, processTypeStr);

    va_start(args, format);
    vsnprintf(buffer + strlen(processTypeStr), needed - strlen(processTypeStr),
              format, args);
    va_end(args);

    fprintf(stderr, "%s\n", buffer);
    if (logFile) {
      fprintf(logFile, "%s\n", buffer);
      fflush(logFile);
    }

    free(buffer);
  }
}

key_t getSharedMemoryKey(void) {
  key_t key = ftok(SHM_PATH, SHM_PROJ_ID);
  if (key == -1) {
    perror("ftok");
  }
  return key;
}

int initSharedMemory(size_t size) {
  key_t key = ftok(SHM_PATH, SHM_PROJ_ID);
  if (key == -1) {
    log_message(LOG_LEVEL_ERROR, "ftok failed: %s", strerror(errno));
    return -1;
  }

  shmId = shmget(key, size, 0644 | IPC_CREAT);
  if (shmId < 0) {
    log_message(LOG_LEVEL_ERROR, "shmget failed: %s", strerror(errno));
    return -1;
  }

  log_message(LOG_LEVEL_DEBUG, "Shared memory initialized, shmId=%d", shmId);
  return shmId;
}

SimulatedClock *attachSharedMemory() {
  key_t key = getSharedMemoryKey();
  if (key == -1) {
    log_message(LOG_LEVEL_ERROR, "Invalid shared memory key.");
    return NULL;
  }

  shmId = shmget(key, sizeof(SimulatedClock), 0644);
  if (shmId < 0) {
    log_message(LOG_LEVEL_ERROR, "Failed to find shared memory: %s",
                strerror(errno));
    return NULL;
  }

  simClock = (SimulatedClock *)shmat(shmId, NULL, 0);
  if (simClock == (void *)-1) {
    log_message(LOG_LEVEL_ERROR, "Failed to attach to shared memory: %s",
                strerror(errno));
    return NULL;
  }

  log_message(LOG_LEVEL_INFO, "Attached to shared memory, shmId=%d", shmId);
  return simClock;
}

int detachSharedMemory(void) {
  if (simClock != NULL && shmdt(simClock) == -1) {
    log_message(LOG_LEVEL_ERROR, "Failed to detach shared memory: %s",
                strerror(errno));
    return -1;
  }
  simClock = NULL;
  log_message(LOG_LEVEL_DEBUG, "Successfully detached shared memory.");
  return 0;
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
                "[SEND] Failed to send message. mtype: %ld, Error: %s",
                msg->mtype, strerror(errno));
    return -1;
  }
  log_message(LOG_LEVEL_DEBUG, "[SEND] Message sent successfully. mtype: %ld",
              msg->mtype);
  return 0;
}

int receiveMessage(int msqId, Message *msg, long msgType, int flags) {
  ssize_t result =
      msgrcv(msqId, msg, sizeof(*msg) - sizeof(long), msgType, flags);
  if (result == -1) {
    if (errno != ENOMSG) {
      log_message(
          LOG_LEVEL_ERROR,
          "[RECEIVE] Failed to receive message. Expected mtype: %ld, Error: %s",
          msgType, strerror(errno));
    }
    return -1;
  }
  log_message(LOG_LEVEL_DEBUG,
              "[RECEIVE] Message received successfully. mtype: %ld, mtext: %d",
              msg->mtype, msg->mtext);
  return 0;
}
