#include "cleanup.h"
#include "shared.h"

void initializeWorkerResources(void);
void calculateTerminationTime(unsigned long lifespanSeconds,
                              unsigned long lifespanNanoSeconds,
                              unsigned long *targetSeconds,
                              unsigned long *targetNanoSeconds);
void workerCleanup(void);

void workerSigHandler(int sig) {
  (void)sig;
  keepRunning = 0;
  log_message(LOG_LEVEL_INFO,
              "[Worker] Received termination signal. Cleaning up...");
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <lifespan seconds> <lifespan nanoseconds>\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }

  setupSignalHandlers();

  gProcessType = PROCESS_TYPE_WORKER;
  initializeWorkerResources();

  unsigned long lifespanSeconds = strtoul(argv[1], NULL, 10);
  unsigned long lifespanNanoSeconds = strtoul(argv[2], NULL, 10);

  unsigned long targetSeconds, targetNanoSeconds;
  calculateTerminationTime(lifespanSeconds, lifespanNanoSeconds, &targetSeconds,
                           &targetNanoSeconds);

  unsigned long iterations = 0;
  while (keepRunning) {
    Message msg;

    if (receiveMessage(msqId, &msg, getpid(), 0) == -1) {
      perror("[Worker] Error receiving message");
      continue;
    }

    sem_wait(clockSem);
    unsigned long currentSeconds = simClock->seconds;
    unsigned long currentNanoSeconds = simClock->nanoseconds;
    sem_post(clockSem);

    log_message(LOG_LEVEL_INFO,
                "[Worker] PID:%d Iteration: %lu. Current Time: %lu.%lu, "
                "Termination Time: %lu.%lu",
                getpid(), ++iterations, currentSeconds, currentNanoSeconds,
                targetSeconds, targetNanoSeconds);

    if (currentSeconds > targetSeconds ||
        (currentSeconds == targetSeconds &&
         currentNanoSeconds >= targetNanoSeconds)) {
      keepRunning = 0;
    }

    msg.mtext = keepRunning ? 1 : 0;
    if (sendMessage(msqId, &msg) == -1) {
      perror("[Worker] Error sending message");
    }
  }

  log_message(LOG_LEVEL_INFO,
              "[Worker] PID:%d Terminating. Total Iterations: %lu.", getpid(),
              iterations);
  workerCleanup();
  return 0;
}

void initializeWorkerResources() {

  simClock = attachSharedMemory();
  if (simClock == NULL) {
    perror("Failed to attach to shared memory");
    exit(EXIT_FAILURE);
  }

  clockSem = sem_open(clockSemName, O_RDWR);
  if (clockSem == SEM_FAILED) {
    perror("Failed to open semaphore");
    exit(EXIT_FAILURE);
  }

  msqId = initMessageQueue();
  if (msqId == -1) {
    perror("Failed to initialize message queue");
    exit(EXIT_FAILURE);
  }
}

void calculateTerminationTime(unsigned long lifespanSeconds,
                              unsigned long lifespanNanoSeconds,
                              unsigned long *targetSeconds,
                              unsigned long *targetNanoSeconds) {
  sem_wait(clockSem);
  *targetSeconds = simClock->seconds + lifespanSeconds;
  *targetNanoSeconds = simClock->nanoseconds + lifespanNanoSeconds;

  while (*targetNanoSeconds >= 1000000000) {
    *targetSeconds += 1;
    *targetNanoSeconds -= 1000000000;
  }
  sem_post(clockSem);
}

void workerCleanup() {

  if (shmdt(simClock) == -1) {
    perror("Failed to detach from shared memory");
  }

  if (sem_close(clockSem) == -1) {
    perror("Failed to close semaphore");
  }

  exit(EXIT_SUCCESS);
}
