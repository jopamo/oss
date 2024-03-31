#include "shared.h"

void initializeWorker(const WorkerConfig *config);
void workerMainLoop(const WorkerConfig *config);
int checkForTermination(const SimulatedClock *clock,
                        const WorkerConfig *config);

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr,
            "[Worker] Usage: %s <lifespan seconds> <lifespan nanoseconds>\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }

  WorkerConfig config = {.lifespanSeconds = strtoul(argv[1], NULL, 10),
                         .lifespanNanoSeconds = strtoul(argv[2], NULL, 10)};

  gProcessType = PROCESS_TYPE_WORKER;
  initializeWorker(&config);
  workerMainLoop(&config);

  exit(EXIT_SUCCESS);
}

void initializeWorker(const WorkerConfig *config) {
  shmId = initSharedMemory(sizeof(SimulatedClock));
  simClock = attachSharedMemory(shmId, sizeof(SimulatedClock));
  msqId = initMessageQueue();

  if (shmId < 0 || simClock == NULL || msqId < 0) {
    log_message(LOG_LEVEL_ERROR, "[Worker] Initialization failed. Exiting.");
    exit(EXIT_FAILURE);
  }

  log_message(LOG_LEVEL_INFO,
              "[Worker] PID: %d, PPID: %d, Initialized with termination time: "
              "%lu.%09lu seconds",
              getpid(), getppid(), config->lifespanSeconds,
              config->lifespanNanoSeconds);
}

void workerMainLoop(const WorkerConfig *config) {
  int iterations = 0;
  Message msg = {.mtype = getpid(), .mtext = 1};

  do {
    sendMessage(msqId, &msg);
    receiveMessage(msqId, &msg, getpid(), 0);

    iterations++;
    log_message(LOG_LEVEL_DEBUG,
                "[Worker] PID: %d, Current Time: %lu.%09lu, Iteration: %d",
                getpid(), simClock->seconds, simClock->nanoseconds, iterations);

  } while (!checkForTermination(simClock, config));

  msg.mtext = 0;
  sendMessage(msqId, &msg);
  log_message(LOG_LEVEL_INFO,
              "[Worker] PID: %d - Terminating after %d iterations.", getpid(),
              iterations);
}

int checkForTermination(const SimulatedClock *clock,
                        const WorkerConfig *config) {
  unsigned long termSecond = clock->seconds + config->lifespanSeconds;
  unsigned long termNano = clock->nanoseconds + config->lifespanNanoSeconds;

  if (termNano >= NANOSECONDS_IN_SECOND) {
    termSecond++;
    termNano -= NANOSECONDS_IN_SECOND;
  }

  return (clock->seconds > termSecond) ||
         (clock->seconds == termSecond && clock->nanoseconds >= termNano);
}
