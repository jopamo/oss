#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>

#include "shared.h"

void printUsage(const char *programName);
void processArgs(int argc, char *argv[], int *addSeconds, int *addNanoseconds);

int main(int argc, char *argv[]) {
  int addSeconds, addNanoseconds;

  processArgs(argc, argv, &addSeconds, &addNanoseconds);

  // Obtain key for shared memory access
  key_t key = getSharedMemoryKey();
  // Connect to the shared memory segment for the simulated clock
  int shmId = shmget(key, sizeof(SimulatedClock), 0666);
  if (shmId < 0) {
    perror("shmget");
    exit(EXIT_FAILURE);
  }

  // Attach to the shared memory segment to access the simulated clock
  SimulatedClock *simClock = (SimulatedClock *)shmat(shmId, NULL, 0);
  if (simClock == (void *)-1) {
    perror("shmat");
    exit(EXIT_FAILURE);
  }

  // Calculate the worker's termination time based on the current simulated clock time
  long long termTimeS = simClock->seconds + addSeconds;
  long long termTimeN = simClock->nanoseconds + addNanoseconds;
  // Handle nanosecond overflow
  while (termTimeN >= 1000000000) {
    termTimeN -= 1000000000;
    termTimeS += 1;
  }

  // Print initial status message with the calculated termination time
  printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %lld TermTimeNano: %lld --Just Starting\n",
         getpid(), getppid(), simClock->seconds, simClock->nanoseconds, termTimeS, termTimeN);

  // Last reported second for periodic status updates
  long long lastReportedSecond = simClock->seconds - 1;
  // Busy-wait loop checking for termination condition
  while (simClock->seconds < termTimeS || (simClock->seconds == termTimeS && simClock->nanoseconds < termTimeN)) {
    // Print status update every second
    if (simClock->seconds > lastReportedSecond) {
      printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d --%lld seconds have passed since starting\n",
             getpid(), getppid(), simClock->seconds, simClock->nanoseconds, simClock->seconds - (termTimeS - addSeconds));
      lastReportedSecond = simClock->seconds;
    }
    sched_yield(); // Yield the processor to reduce CPU usage
  }

  // Print final message before termination
  printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d --Terminating\n",
         getpid(), getppid(), simClock->seconds, simClock->nanoseconds);

  // Detach from the shared memory segment
  if (shmdt(simClock) == -1) {
    perror("shmdt");
    exit(EXIT_FAILURE);
  }

  return 0;
}

void printUsage(const char *programName) {
  fprintf(stderr, "Usage: %s <seconds> <nanoseconds>\n", programName);
  fprintf(stderr, "Both <seconds> and <nanoseconds> must be non-negative integers.\n");
}

/**
 * Parses command line arguments for additional seconds and nanoseconds adjustments
 * Validates the input ensuring two arguments are provided for seconds and nanoseconds
 * Exits with a usage message if inputs are invalid or not in the expected range
 *
 * @param argc Number of command line arguments
 * @param argv Array of command line arguments
 * @param addSeconds Pointer to store parsed seconds
 * @param addNanoseconds Pointer to store parsed nanoseconds
 */
void processArgs(int argc, char *argv[], int *addSeconds, int *addNanoseconds) {
  if (argc != 3) {
    printUsage(argv[0]); // Show how to properly use the program
    exit(EXIT_FAILURE);
  }

  // Convert arguments to long long for full range checking
  long long seconds = atoll(argv[1]);
  long long nanoseconds = atoll(argv[2]);

  // Validate seconds and nanoseconds are within proper range
  if (seconds < 0 || nanoseconds < 0 || nanoseconds >= 1000000000) {
    fprintf(stderr, "Error: Invalid time values.\n");
    printUsage(argv[0]); // Show usage if values are out of bounds
    exit(EXIT_FAILURE);
  }

  // Cast to int after validation
  *addSeconds = (int)seconds;
  *addNanoseconds = (int)nanoseconds;
}
