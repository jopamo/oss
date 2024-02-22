#include "shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NANOSECOND 10000000

void printUsage(const char *programName) {
  printf("Usage: %s seconds nanoseconds\n", programName);
  printf("Example: %s 5 500000\n", programName);
  printf("Attaches to shared memory, reads the simulated system clock, and determines a termination time. Periodically checks the system clock and terminates once the specified time has elapsed.\n");
}

void processArgs(int argc, char *argv[], int *addSeconds, int *addNanoseconds) {
  if (argc != 3) {
    fprintf(stderr, "Error: Incorrect number of arguments.\n");
    printUsage(argv[0]);
    exit(EXIT_FAILURE);
  }

  *addSeconds = atoi(argv[1]);
  *addNanoseconds = atoi(argv[2]);

  if ( *addSeconds < 0 || *addNanoseconds < 0 || *addNanoseconds >= NANOSECOND) {
    fprintf(stderr, "Error: Invalid arguments. Seconds must be >= 0 and nanoseconds in [0, 999999999].\n");
    exit(EXIT_FAILURE);
  }
}

void verifySharedMemory(int addSeconds, int addNanoseconds) {
  key_t key = getSharedMemoryKey();
  int shmId = shmget(key, sizeof(SimulatedClock), 0666);
  if (shmId < 0) {
    perror("shmget failed");
    exit(EXIT_FAILURE);
  }

  SimulatedClock *simClock = (SimulatedClock *) shmat(shmId, NULL, 0);
  if (simClock == (void *) - 1) {
    perror("shmat failed");
    exit(EXIT_FAILURE);
  }

  printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d --Just Starting\n",
    getpid(), getppid(), simClock -> seconds, simClock -> nanoseconds);

  long long startSeconds = simClock -> seconds;
  long long startNanoseconds = simClock -> nanoseconds;
  long long totalNanoseconds = (long long) simClock -> nanoseconds + addNanoseconds;
  int termTimeS = simClock -> seconds + addSeconds + totalNanoseconds / NANOSECOND;
  int termTimeN = totalNanoseconds % NANOSECOND;

  int lastElapsedSecond = -1;

  while (simClock -> seconds < termTimeS || (simClock -> seconds == termTimeS && simClock -> nanoseconds < termTimeN)) {
    long long currentTotalNanoseconds = (simClock -> seconds - startSeconds) *NANOSECOND + (simClock -> nanoseconds - startNanoseconds);
    int elapsedSeconds = currentTotalNanoseconds / NANOSECOND;

    if (elapsedSeconds > lastElapsedSecond) {
      printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d --%d seconds have passed since starting\n",
        getpid(), getppid(), simClock -> seconds, simClock -> nanoseconds, elapsedSeconds);
      lastElapsedSecond = elapsedSeconds;
    }
  }

  printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d --Terminating\n",
    getpid(), getppid(), simClock -> seconds, simClock -> nanoseconds);

  if (shmdt(simClock) == -1) {
    perror("shmdt failed");
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char *argv[]) {
  int addSeconds, addNanoseconds;

  processArgs(argc, argv, & addSeconds, & addNanoseconds);
  verifySharedMemory(addSeconds, addNanoseconds);
  return 0;
}
