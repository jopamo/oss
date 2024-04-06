#include "process.h"
#include "shared.h"

void initializeWorkerResources(void);
void calculateTerminationTime(unsigned long lifespanSeconds,
                              unsigned long lifespanNanoSeconds,
                              unsigned long *targetSeconds,
                              unsigned long *targetNanoSeconds);
void workerCleanup(void);
void logStartAndTerminationTime(unsigned long lifespanSeconds,
                                unsigned long lifespanNanoSeconds);
void prepareAndSendMessage(int msqId, pid_t pid, int message);

int main(int argc, char *argv[]) {
  setupSharedMemory();
  gProcessType = PROCESS_TYPE_WORKER;

  setupSignalHandlers(getpid());

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <lifespan seconds> <lifespan nanoseconds>\n",
            argv[0]);
    return 1;
  }

  unsigned long lifespanSeconds = strtoul(argv[1], NULL, 10);
  unsigned long lifespanNanoSeconds = strtoul(argv[2], NULL, 10);

  initializeWorkerResources();

  unsigned long targetSeconds, targetNanoSeconds;
  calculateTerminationTime(lifespanSeconds, lifespanNanoSeconds, &targetSeconds,
                           &targetNanoSeconds);

  logStartAndTerminationTime(lifespanSeconds, lifespanNanoSeconds);

  Message msg;
  while (keepRunning) {
    if (receiveMessage(msqId, &msg, getpid(), 0) == -1) {
      if (errno != EINTR) {
        perror("Worker receive message error");
        break;
      }
      continue;
    }

    sem_wait(clockSem);
    unsigned long currentSeconds = simClock->seconds;
    unsigned long currentNanoSeconds = simClock->nanoseconds;
    sem_post(clockSem);

    if (currentSeconds > targetSeconds ||
        (currentSeconds == targetSeconds &&
         currentNanoSeconds >= targetNanoSeconds)) {
      break;
    }

    prepareAndSendMessage(msqId, getpid(), 1);
  }

  prepareAndSendMessage(msqId, getpid(), 0);

  workerCleanup();
  return 0;
}

void initializeWorkerResources(void) {
  simClock = (SimulatedClock *)attachSharedMemory(simulatedTimeShmId,
                                                  "Simulated Clock");
  actualTime = (ActualTime *)attachSharedMemory(actualTimeShmId, "Actual Time");

  log_message(LOG_LEVEL_DEBUG, "[%d] Simulated Clock shmId: %d", getpid(),
              simulatedTimeShmId);
  log_message(LOG_LEVEL_DEBUG, "[%d] Actual Time shmId: %d", getpid(),
              actualTimeShmId);

  if (!simClock || !actualTime) {
    log_message(LOG_LEVEL_ERROR,
                "[%d] Failed to attach to shared memory segments.", getpid());
    exit(EXIT_FAILURE);
  }

  clockSem = sem_open(clockSemName, O_RDWR);
  if (clockSem == SEM_FAILED) {
    log_message(LOG_LEVEL_ERROR, "[%d] Failed to open semaphore: %s", getpid(),
                strerror(errno));
    exit(EXIT_FAILURE);
  }

  log_message(LOG_LEVEL_INFO, "[%d] Resources initialized successfully.",
              getpid());
}

void workerCleanup(void) {

  if (detachSharedMemory((void **)&simClock, "Simulated Clock") != 0) {
    log_message(LOG_LEVEL_ERROR,
                "[%d] Failed to detach from Simulated Clock shared memory.",
                getpid());
  }

  if (detachSharedMemory((void **)&actualTime, "Actual Time") != 0) {
    log_message(LOG_LEVEL_ERROR,
                "[%d] Failed to detach from Actual Time shared memory.",
                getpid());
  }

  if (sem_close(clockSem) == -1) {
    log_message(LOG_LEVEL_ERROR, "[%d] Failed to close semaphore.", getpid());
  }

  log_message(LOG_LEVEL_INFO, "[%d] Cleanup completed successfully.", getpid());
}

void calculateTerminationTime(unsigned long lifespanSeconds,
                              unsigned long lifespanNanoSeconds,
                              unsigned long *targetSeconds,
                              unsigned long *targetNanoSeconds) {
  if (sem_wait(clockSem) == -1) {
    log_message(LOG_LEVEL_ERROR,
                "[%d] Failed to lock semaphore for reading simulated clock.",
                getpid());
    exit(EXIT_FAILURE);
  }

  *targetSeconds = simClock->seconds + lifespanSeconds;
  *targetNanoSeconds = simClock->nanoseconds + lifespanNanoSeconds;

  if (*targetNanoSeconds >= NANOSECONDS_IN_SECOND) {
    *targetSeconds += *targetNanoSeconds / NANOSECONDS_IN_SECOND;
    *targetNanoSeconds %= NANOSECONDS_IN_SECOND;
  }

  if (sem_post(clockSem) == -1) {
    log_message(
        LOG_LEVEL_ERROR,
        "[%d] Failed to unlock semaphore after reading simulated clock.",
        getpid());
    exit(EXIT_FAILURE);
  }
}

void logStartAndTerminationTime(unsigned long lifespanSeconds,
                                unsigned long lifespanNanoSeconds) {

  unsigned long startSeconds, startNanoSeconds, targetSeconds,
      targetNanoSeconds;

  sem_wait(clockSem);
  startSeconds = simClock->seconds;
  startNanoSeconds = simClock->nanoseconds;
  sem_post(clockSem);

  calculateTerminationTime(lifespanSeconds, lifespanNanoSeconds, &targetSeconds,
                           &targetNanoSeconds);

  log_message(LOG_LEVEL_INFO,
              "WORKER PID:%d PPID:%d SysClockS: %lu SysclockNano: %lu "
              "TermTimeS: %lu TermTimeNano: %lu --Just Starting",
              getpid(), getppid(), startSeconds, startNanoSeconds,
              targetSeconds, targetNanoSeconds);
}
