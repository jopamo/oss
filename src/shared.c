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
int lastChildSent = 0;

int getCurrentChildren(void) { return currentChildren; }
void setCurrentChildren(int value) { currentChildren = value; }

void log_debug(const char *format, ...) {
  struct timeval now;
  gettimeofday(&now, NULL);

  char timeBuffer[64];
  strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S",
           localtime(&now.tv_sec));

  fprintf(stderr, "[%s] [DEBUG] [%s] ", timeBuffer,
          (gProcessType == PROCESS_TYPE_OSS) ? "OSS" : "Worker");

  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fprintf(stderr, "\n");

  if (logFile != NULL) {
    fprintf(logFile, "[%s] [DEBUG] [%s] ", timeBuffer,
            (gProcessType == PROCESS_TYPE_OSS) ? "OSS" : "Worker");
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);
    fprintf(logFile, "\n");
  }
}

void log_error(const char *format, ...) {
  struct timeval now;
  gettimeofday(&now, NULL);

  char timeBuffer[64];
  strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S",
           localtime(&now.tv_sec));

  fprintf(stderr, "[%s] [ERROR] [%s] ", timeBuffer,
          (gProcessType == PROCESS_TYPE_OSS) ? "OSS" : "Worker");

  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fprintf(stderr, "\n");

  if (logFile != NULL) {
    fprintf(logFile, "[%s] [ERROR] [%s] ", timeBuffer,
            (gProcessType == PROCESS_TYPE_OSS) ? "OSS" : "Worker");
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);
    fprintf(logFile, "\n");
  }
}

int initSharedMemory(void) {
  key_t key = getSharedMemoryKey();
  shmId = shmget(key, sizeof(SimulatedClock), 0644 | IPC_CREAT);

  if (shmId < 0) {
    perror("shmget");
    return -1;
  }
  log_debug("Shared memory initialized successfully, shmId=%d", shmId);

  return shmId;
}

key_t getSharedMemoryKey(void) { return ftok(SHM_PATH, SHM_PROJ_ID); }

SimulatedClock *attachSharedMemory(void) {
  if (shmId < 0) {
    log_debug("Attempt to attach to shared memory failed: Invalid shmId=%d",
              shmId);
    return NULL;
  }

  SimulatedClock *simClock = (SimulatedClock *)shmat(shmId, NULL, 0);
  if (simClock == (SimulatedClock *)-1) {
    perror("shmat");

    if (gProcessType == PROCESS_TYPE_OSS) {
      log_debug("[OSS] Failed to attach to shared memory with shmId=%d", shmId);
    } else if (gProcessType == PROCESS_TYPE_WORKER) {
      log_debug("[Worker] Failed to attach to shared memory with shmId=%d",
                shmId);
    } else {
      log_debug("Unknown process type failed to attach to shared memory with "
                "shmId=%d",
                shmId);
    }
    return NULL;
  }
  log_debug("Attached shared memory successfully, shmId=%d", shmId);
  return simClock;
}

int detachSharedMemory(void) {
  if (simClock != NULL) {
    if (shmdt(simClock) == -1) {
      perror("shmdt");
      log_error("Failed to detach shared memory.");
      return -1;
    }
    simClock = NULL;
    log_debug("Successfully detached shared memory.");
  } else {
    log_debug("Shared memory was not attached or already detached.");
  }
  return 0;
}

int initMessageQueue(void) {
  key_t msg_key = ftok(MSG_PATH, MSG_PROJ_ID);

  if (msg_key == -1) {
    perror("ftok for msg_key failed");
    exit(EXIT_FAILURE);
  }

  msqId = msgget(msg_key, IPC_CREAT | 0666);

  if (msqId < 0) {
    perror("msgget");
    return -1;
  }
  log_debug("Message queue initialized successfully");
  return msqId;
}

int sendMessage(int msqId, Message *msg) {

  log_debug("[SEND] Preparing to send. mtype: %ld, mtext: %d", msg->mtype,
            msg->mtext);

  if (msgsnd(msqId, msg, sizeof(*msg) - sizeof(long), 0) == -1) {
    log_error("[SEND] Failed to send message. mtype: %ld, Error: %s",
              msg->mtype, strerror(errno));
    return -1;
  }

  log_debug("[SEND] Message sent successfully. mtype: %ld", msg->mtype);
  return 0;
}

int receiveMessage(int msqId, Message *msg, long msgType, int flags) {

  log_debug("[RECEIVE] Waiting for message. Expected mtype: %ld", msgType);

  ssize_t result =
      msgrcv(msqId, msg, sizeof(*msg) - sizeof(long), msgType, flags);
  if (result == -1) {

    if (errno == ENOMSG) {
      log_debug("[RECEIVE] No message available for mtype: %ld", msgType);
    } else {
      log_error(
          "[RECEIVE] Failed to receive message. Expected mtype: %ld, Error: %s",
          msgType, strerror(errno));
    }
    return -1;
  }

  log_debug("[RECEIVE] Message received successfully. mtype: %ld, mtext: %d",
            msg->mtype, msg->mtext);
  return 0;
}
