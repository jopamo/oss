#include "cleanup.h"
#include "shared.h"

typedef struct {
  unsigned int lifespanSeconds;
  unsigned int lifespanNanoSeconds;
} WorkerConfig;

void parseCommandLineArguments(int argc, char *argv[], WorkerConfig *config);
void operateWorkerCycle(const WorkerConfig *config);
int shouldTerminate(const WorkerConfig *config, const SimulatedClock *simClock);
void logStatus(const WorkerConfig *config, const SimulatedClock *simClock,
               int iteration);

int main(int argc, char *argv[]) {
  gProcessType = PROCESS_TYPE_WORKER;
  WorkerConfig config;
  parseCommandLineArguments(argc, argv, &config);

  shmId = initSharedMemory();
  simClock = attachSharedMemory(shmId);

  if (!simClock) {
    fprintf(stderr, "Worker: Failed to attach to shared memory\n");
    exit(EXIT_FAILURE);
  }

  operateWorkerCycle(&config);

  if (detachSharedMemory(simClock) != 0) {
    log_debug("Worker: Failed to detach from shared memory");
  }

  return EXIT_SUCCESS;
}

void parseCommandLineArguments(int argc, char *argv[], WorkerConfig *config) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s lifespanSeconds lifespanNanoSeconds\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  config->lifespanSeconds = strtoul(argv[1], NULL, 10);
  config->lifespanNanoSeconds = strtoul(argv[2], NULL, 10);
}

void operateWorkerCycle(const WorkerConfig *config) {
  SimulatedClock *simClock = attachSharedMemory();
  int iteration = 0;
  while (!shouldTerminate(config, simClock)) {
    logStatus(config, simClock, iteration);
    Message msg;
    if (receiveMessage(&msg, getpid(), 0) == -1) {
      perror("Worker: Error receiving message");
      break;
    }
    iteration++;
    if (shouldTerminate(config, simClock)) {
      printf("Worker PID: %d - Terminating after %d iterations\n", getpid(),
             iteration);
      msg.mtype = 1;
      msg.mtext = 0;
      sendMessage(&msg);
      break;
    }
    msg.mtype = 1;
    msg.mtext = 1;
    sendMessage(&msg);
  }
}

int shouldTerminate(const WorkerConfig *config,
                    const SimulatedClock *simClock) {
  unsigned long endSeconds = config->lifespanSeconds + simClock->seconds;
  unsigned long endNano = config->lifespanNanoSeconds + simClock->nanoseconds;
  if (endNano >= 1000000000) {
    endNano -= 1000000000;
    endSeconds++;
  }
  return simClock->seconds > endSeconds ||
         (simClock->seconds == endSeconds && simClock->nanoseconds >= endNano);
}

void logStatus(const WorkerConfig *config,
               const SimulatedClock *simClock,
               int iteration) {
  unsigned long endSeconds = simClock->seconds + config->lifespanSeconds;
  unsigned long endNano = simClock->nanoseconds + config->lifespanNanoSeconds;
  if (endNano >= 1000000000) {
    endNano -= 1000000000;
    endSeconds++;
  }
  printf(
      "WORKER PID: %d - simClockS: %lu simClockNano: %lu TermTimeS: %lu "
      "TermTimeNano: %lu --%d iterations since starting\n",
      getpid(), simClock->seconds, simClock->nanoseconds, endSeconds, endNano,
      iteration);
}
