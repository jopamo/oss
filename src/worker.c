#include "arghandler.h"
#include "shared.h"

void operateWorkerCycle(const WorkerConfig *config);
int shouldTerminate(const SimulatedClock *simClock, const WorkerConfig *config);
void logStatus(const SimulatedClock *simClock, const WorkerConfig *config,
               int iteration);

volatile sig_atomic_t cleanupInitiated = 0;

int main(int argc, char *argv[]) {
  gProcessType = PROCESS_TYPE_WORKER;
  WorkerConfig config;
  workerArgs(argc, argv, &config);

  shmId = initSharedMemory();
  simClock = attachSharedMemory();
  msqId = initMessageQueue();

  logStatus(simClock, &config, -1);

  operateWorkerCycle(&config);

  detachSharedMemory();
  return EXIT_SUCCESS;
}

void operateWorkerCycle(const WorkerConfig *config) {
  int iteration = 0;
  pid_t myPid = getpid();
  Message msg = {.mtype = myPid, .mtext = 1};

  while (!shouldTerminate(simClock, config)) {

    if (msgrcv(msqId, &msg, sizeof(msg) - sizeof(long), myPid, 0) == -1) {

      perror("[Worker] msgrcv failed");
      break;
    }

    iteration++;
    logStatus(simClock, config, iteration);

    msg.mtype = myPid;
    msg.mtext = 1;
    if (sendMessage(msqId, &msg) == -1) {
      perror("[Worker] sendMessage failed");
    }
  }

  printf("[Worker] PID:%d - Terminating after %d iterations\n", myPid,
         iteration);
}

void logStatus(const SimulatedClock *simClock, const WorkerConfig *config,
               int iteration) {
  pid_t myPid = getpid();

  printf("[Worker] PID:%d PPID:%d SysClockS: %lu SysclockNano: %lu TermTimeS: "
         "%u TermTimeNano: %u --Iteration:%d\n",
         myPid, getppid(), simClock->seconds, simClock->nanoseconds,
         config->lifespanSeconds, config->lifespanNanoSeconds, iteration);
}

int shouldTerminate(const SimulatedClock *simClock,
                    const WorkerConfig *config) {
  unsigned long endSeconds = config->lifespanSeconds + simClock->seconds;
  unsigned long endNano = config->lifespanNanoSeconds + simClock->nanoseconds;
  if (endNano >= 1000000000) {
    endNano -= 1000000000;
    endSeconds++;
  }
  return simClock->seconds > endSeconds ||
         (simClock->seconds == endSeconds && simClock->nanoseconds >= endNano);
}
