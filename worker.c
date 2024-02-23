#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

#include "shared.h"

void printUsage(const char * programName);
void processArgs(int argc, char * argv[], int * addSeconds, int * addNanoseconds);

int main(int argc, char * argv[]) {
  int addSeconds, addNanoseconds;
  processArgs(argc, argv, & addSeconds, & addNanoseconds);

  key_t key = getSharedMemoryKey();
  int shmId = shmget(key, sizeof(SimulatedClock), 0666);
  if (shmId < 0) {
    perror("shmget");
    exit(EXIT_FAILURE);
  }

  SimulatedClock * simClock = (SimulatedClock * ) shmat(shmId, NULL, 0);
  if (simClock == (void * ) - 1) {
    perror("shmat");
    exit(EXIT_FAILURE);
  }

  long long termTimeS = simClock -> seconds + addSeconds;
  long long termTimeN = simClock -> nanoseconds + addNanoseconds;
  while (termTimeN >= 1000000000) {
    termTimeN -= 1000000000;
    termTimeS += 1;
  }

  printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %lld TermTimeNano: %lld --Just Starting\n",
    getpid(), getppid(), simClock -> seconds, simClock -> nanoseconds, termTimeS, termTimeN);

  long long lastReportedSecond = simClock -> seconds - 1;
  while (simClock -> seconds < termTimeS || (simClock -> seconds == termTimeS && simClock -> nanoseconds < termTimeN)) {
    if (simClock -> seconds > lastReportedSecond) {
      printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d --%lld seconds have passed since starting\n",
        getpid(), getppid(), simClock -> seconds, simClock -> nanoseconds, simClock -> seconds - (termTimeS - addSeconds));
      lastReportedSecond = simClock -> seconds;
    }
  }

  printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d --Terminating\n",
    getpid(), getppid(), simClock -> seconds, simClock -> nanoseconds);

  if (shmdt(simClock) == -1) {
    perror("shmdt");
    exit(EXIT_FAILURE);
  }

  return 0;
}

void printUsage(const char * programName) {
  fprintf(stderr, "Usage: %s <seconds> <nanoseconds>\n", programName);
  fprintf(stderr, "Both <seconds> and <nanoseconds> must be non-negative integers.\n");
}

void processArgs(int argc, char * argv[], int * addSeconds, int * addNanoseconds) {
  if (argc != 3) {
    printUsage(argv[0]);
    exit(EXIT_FAILURE);
  }

  long long seconds = atoll(argv[1]);
  long long nanoseconds = atoll(argv[2]);

  if (seconds < 0 || nanoseconds < 0 || nanoseconds >= 1000000000) {
    fprintf(stderr, "Error: Invalid time values.\n");
    printUsage(argv[0]);
    exit(EXIT_FAILURE);
  }

  *addSeconds = (int) seconds;
  *addNanoseconds = (int) nanoseconds;
}
