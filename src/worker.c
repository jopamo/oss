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
  unsigned long currentSeconds = 0, currentNanoSeconds = 0;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <lifespan seconds> <lifespan nanoseconds>\n",
            argv[0]);
    return 1;
  }

  setupSignalHandlers();
  initializeWorkerResources();

  unsigned long lifespanSeconds = strtoul(argv[1], NULL, 10);
  unsigned long lifespanNanoSeconds = strtoul(argv[2], NULL, 10);
  unsigned long targetSeconds, targetNanoSeconds;
  calculateTerminationTime(lifespanSeconds, lifespanNanoSeconds, &targetSeconds,
                           &targetNanoSeconds);

  logStartAndTerminationTime(lifespanSeconds, lifespanNanoSeconds);

  Message msg;
  unsigned long iterations = 0;
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

    log_message(LOG_LEVEL_INFO, "Current simulated time: %lu.%lu",
                currentSeconds, currentNanoSeconds);

    if (currentSeconds > targetSeconds ||
        (currentSeconds == targetSeconds &&
         currentNanoSeconds >= targetNanoSeconds)) {
      break;
    }

    iterations++;
    log_message(LOG_LEVEL_INFO,
                "WORKER PID:%d PPID:%d Current Time: %lu.%lu Target Time: "
                "%lu.%lu --%lu iteration(s)",
                getpid(), getppid(), currentSeconds, currentNanoSeconds,
                targetSeconds, targetNanoSeconds, iterations);

    prepareAndSendMessage(msqId, getpid(), 1);
  }

  prepareAndSendMessage(msqId, getpid(), 0);

  log_message(LOG_LEVEL_INFO,
              "WORKER PID:%d Terminating after %lu iterations. Last Time: "
              "%lu.%lu Target Time: %lu.%lu",
              getpid(), iterations, currentSeconds, currentNanoSeconds,
              targetSeconds, targetNanoSeconds);

  workerCleanup();
  return 0;
}

void initializeWorkerResources(void) {
  key_t simClockKey = ftok(SHM_PATH, SHM_PROJ_ID);
  if (simClockKey == -1) {
    log_message(LOG_LEVEL_ERROR,
                "[Worker %d] Failed to generate key for shared memory.",
                getpid());
    exit(EXIT_FAILURE);
  }

  shmId = shmget(simClockKey, sizeof(SimulatedClock), 0666 | IPC_CREAT);
  if (shmId == -1) {
    log_message(LOG_LEVEL_ERROR,
                "[Worker %d] Failed to obtain shared memory segment.",
                getpid());
    exit(EXIT_FAILURE);
  }

  simClock = (SimulatedClock *)shmat(shmId, NULL, 0);
  if (simClock == (void *)-1) {
    log_message(LOG_LEVEL_ERROR,
                "[Worker %d] Failed to attach shared memory segment.",
                getpid());
    exit(EXIT_FAILURE);
  }

  clockSem = sem_open(clockSemName, O_RDWR);
  if (clockSem == SEM_FAILED) {
    log_message(LOG_LEVEL_ERROR, "[Worker %d] Failed to open semaphore: %s",
                getpid(), strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (msqId == -1) {
    log_message(LOG_LEVEL_ERROR,
                "[Worker %d] Message queue ID is not initialized.", getpid());
    exit(EXIT_FAILURE);
  }

  log_message(LOG_LEVEL_INFO, "[Worker %d] Resources initialized successfully.",
              getpid());
}

void calculateTerminationTime(unsigned long lifespanSeconds,
                              unsigned long lifespanNanoSeconds,
                              unsigned long *targetSeconds,
                              unsigned long *targetNanoSeconds) {
  if (sem_wait(clockSem) == -1) {
    log_message(
        LOG_LEVEL_ERROR,
        "[Worker %d] Failed to lock semaphore for reading simulated clock.",
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
        "[Worker %d] Failed to unlock semaphore after reading simulated clock.",
        getpid());
    exit(EXIT_FAILURE);
  }
}

void workerCleanup(void) {
  if (shmdt(simClock) != 0) {
    log_message(LOG_LEVEL_ERROR,
                "[Worker %d] Failed to detach from shared memory.", getpid());
  }

  if (sem_close(clockSem) == -1) {
    log_message(LOG_LEVEL_ERROR, "[Worker %d] Failed to close semaphore.",
                getpid());
  }

  log_message(LOG_LEVEL_INFO, "[Worker %d] Cleanup completed successfully.",
              getpid());
}

void logTerminationTime(unsigned long lifespanSeconds,
                        unsigned long lifespanNanoSeconds) {
  unsigned long targetSeconds, targetNanoSeconds;
  calculateTerminationTime(lifespanSeconds, lifespanNanoSeconds, &targetSeconds,
                           &targetNanoSeconds);
  log_message(LOG_LEVEL_INFO,
              "[Worker %d] Initialized. Lifespan: %lu seconds and %lu "
              "nanoseconds. Termination Time: %lu seconds and %lu nanoseconds.",
              getpid(), lifespanSeconds, lifespanNanoSeconds, targetSeconds,
              targetNanoSeconds);
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
