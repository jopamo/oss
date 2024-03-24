#include "shared.h"

ProcessType gProcessType;
struct timeval startTime;
struct timeval lastLogTime;
SimulatedClock* simClock = NULL;
PCB processTable[DEFAULT_MAX_PROCESSES];
FILE* logFile = NULL;
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

int getCurrentChildren() { return currentChildren; }
void setCurrentChildren(int value) { currentChildren = value; }

void initializeSimulationEnvironment() {
  maxProcesses = sysconf(_SC_NPROCESSORS_ONLN);
  printf("Number of CPUs available: %d\n", maxProcesses);

  gettimeofday(&startTime, NULL);
  gettimeofday(&lastLogTime, NULL);

  simClock->seconds = 0;
  simClock->nanoseconds = 0;

  if (msqId == -1) {
    fprintf(stderr, "Failed to initialize message queue.\n");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    processTable[i].occupied = 0;
    processTable[i].pid = -1;
    processTable[i].startSeconds = 0;
    processTable[i].startNano = 0;
  }

  logFile = fopen(logFileName, "w");
  if (logFile == NULL) {
    fprintf(stderr, "Failed to open log file %s\n", logFileName);
    exit(EXIT_FAILURE);
  }

  printf("Shared memory initialized (ID: %d).\n", shmId);
  printf("Message queue initialized (ID: %d).\n", msqId);
  printf("Process table initialized.\n");
  printf("Log file %s opened.\n", logFileName);
}

void log_debug(const char* format, ...) {
  va_list args;
  const char* prefix =
      (gProcessType == PROCESS_TYPE_OSS) ? "[OSS]" : "[Worker]";

  fprintf(stderr, "%s ", prefix);

  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  fprintf(stderr, "\n");
}

int initSharedMemory() {
  key_t key = getSharedMemoryKey();
  shmId = shmget(key, sizeof(SimulatedClock), 0644 | IPC_CREAT);

  if (shmId < 0) {
    perror("shmget");
    return -1;
  }
  log_debug("Shared memory initialized successfully, shmId=%d", shmId);

  return shmId;
}

key_t getSharedMemoryKey() { return ftok(SHM_PATH, SHM_PROJ_ID); }

SimulatedClock* attachSharedMemory() {
  if (shmId < 0) {
    log_debug("Attempt to attach to shared memory failed: Invalid shmId=%d",
              shmId);
    return NULL;
  }

  SimulatedClock* shmPtr = (SimulatedClock*)shmat(shmId, NULL, 0);
  if (shmPtr == (SimulatedClock*)-1) {
    perror("shmat");

    if (gProcessType == PROCESS_TYPE_OSS) {
      log_debug("[OSS] Failed to attach to shared memory with shmId=%d", shmId);
    } else if (gProcessType == PROCESS_TYPE_WORKER) {
      log_debug("[Worker] Failed to attach to shared memory with shmId=%d",
                shmId);
    } else {
      log_debug(
          "Unknown process type failed to attach to shared memory with "
          "shmId=%d",
          shmId);
    }
    return NULL;
  }
  log_debug("Attached shared memory successfully, shmId=%d", shmId);
  return shmPtr;
}

int detachSharedMemory(SimulatedClock* shmPtr) {
  log_debug("Attempting to detach shared memory.");
  if (shmdt(shmPtr) == -1) {
    perror("shmdt");
    log_debug("Failed to detach shared memory.");
    return -1;
  }
  log_debug("Successfully detached shared memory.");
  return 0;
}

int initMessageQueue() {
  msqId = msgget(MSG_KEY, IPC_CREAT | 0666);
  if (msqId < 0) {
    perror("msgget");
    return -1;
  }
  log_debug("Message queue initialized successfully");
  return msqId;
}

int sendMessage(Message* msg) {
  if (msgsnd(msqId, (void*)msg, sizeof(Message) - sizeof(long), IPC_NOWAIT) <
      0) {
    log_debug("Failed to send message: Type=%ld, Text=%d, Error=%s", msg->mtype,
              msg->mtext, strerror(errno));
    return -1;
  }

  log_debug("Message sent successfully to queue ID %d.", msqId);
  return 0;
}

int receiveMessage(Message* msg, long msgType, int flags) {
  if (msgrcv(msqId, (void*)msg, sizeof(Message) - sizeof(long), msgType,
             flags) < 0) {
    if (errno != ENOMSG) {
      log_debug("Failed to receive message: Type=%ld, Queue ID=%d, Error=%s",
                msgType, msqId, strerror(errno));
    }
    return -1;
  }

  log_debug("Message received successfully from queue ID %d.", msqId);
  return 0;
}

void receiveMessageFromChild() {
  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      Message msg;
      if (receiveMessage(&msg, processTable[i].pid, IPC_NOWAIT) >= 0) {
        log_debug("Received message from child %d: %d", processTable[i].pid,
                  msg.mtext);

        if (msg.mtext == 0) {
          log_debug("Child process %d is requesting to terminate.",
                    processTable[i].pid);

          processTable[i].occupied = 0;
          waitpid(processTable[i].pid, NULL, 0);
        }
      }
    }
  }
}

ElapsedTime elapsedTimeSince(const struct timeval* lastTime) {
  struct timeval now, diff;
  gettimeofday(&now, NULL);
  timersub(&now, lastTime, &diff);

  ElapsedTime elapsedTime;
  elapsedTime.seconds = diff.tv_sec;
  elapsedTime.nanoseconds = diff.tv_usec * 1000;
  return elapsedTime;
}
