#include "shared.h"
#include "user_process.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BOUND_B 500000000 // Upper bound for when actions occur in nanoseconds

void logStatus(long currentSeconds, long currentNanoSeconds, long targetSeconds,
               long targetNanoSeconds, int iterations);
bool isTimeToTerminate(unsigned long currentSeconds,
                       unsigned long currentNanoSeconds,
                       unsigned long targetSeconds,
                       unsigned long targetNanoSeconds);
void normalizeTime(unsigned long *seconds, unsigned long *nanoseconds);
void sendResourceRequest(int msqId, pid_t pid, int requestType,
                         int resourceType, int resourceAmount);

int main(int argc, char *argv[]) {
  gProcessType = PROCESS_TYPE_WORKER;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <lifespan seconds> <lifespan nanoseconds>\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }

  initializeSharedResources();
  setupSignalHandlers();

  // Parse lifespan arguments
  unsigned long lifespanSeconds = strtoul(argv[1], NULL, 10);
  unsigned long lifespanNanoSeconds = strtoul(argv[2], NULL, 10);

  // Synchronize with the system clock
  better_sem_wait(clockSem);
  unsigned long startSeconds = simClock->seconds;
  unsigned long startNanoSeconds = simClock->nanoseconds;
  sem_post(clockSem);

  // Calculate termination target time
  unsigned long targetSeconds = startSeconds + lifespanSeconds;
  unsigned long targetNanoSeconds = startNanoSeconds + lifespanNanoSeconds;
  normalizeTime(&targetSeconds, &targetNanoSeconds);

  srand(time(NULL) ^ (getpid() << 16)); // Seed random number generator uniquely

  while (keepRunning) {
    better_sem_wait(clockSem);
    unsigned long currentSeconds = simClock->seconds;
    unsigned long currentNanoSeconds = simClock->nanoseconds;
    sem_post(clockSem);

    // Decide randomly whether to request or release resources or check if it
    // should terminate
    if (rand() % BOUND_B < BOUND_B / 10) { // Adjust the ratio as necessary
      int resourceType =
          rand() % MAX_RESOURCE_TYPES; // Choose a random resource type
      int resourceAmount = 1;          // Request only 1 unit for simplicity
      sendResourceRequest(msqId, getpid(), 1, resourceType,
                          resourceAmount); // Dummy values for resource request
    }

    if (isTimeToTerminate(currentSeconds, currentNanoSeconds, targetSeconds,
                          targetNanoSeconds)) {
      sendResourceRequest(msqId, getpid(), 0, 0, 0); // Send termination request
      break;
    }

    usleep(100000); // Continue checking every 100ms
  }

  cleanupSharedResources();
  return EXIT_SUCCESS;
}

void sendResourceRequest(int msqId, pid_t pid, int requestType,
                         int resourceType, int resourceAmount) {
  Message msg;
  msg.mtype = pid;
  msg.mtext = requestType; // 0 for terminate, 1 for request resource
  msg.resourceType = resourceType;
  msg.amount = resourceAmount;

  int result = sendMessage(msqId, &msg);
  if (result != 0) {
    perror("sendMessage failed");
    exit(EXIT_FAILURE);
  }
}

void normalizeTime(unsigned long *seconds, unsigned long *nanoseconds) {
  while (*nanoseconds >= 1000000000) {
    (*seconds)++;
    *nanoseconds -= 1000000000;
  }
}

bool isTimeToTerminate(unsigned long currentSeconds,
                       unsigned long currentNanoSeconds,
                       unsigned long targetSeconds,
                       unsigned long targetNanoSeconds) {
  return (currentSeconds > targetSeconds ||
          (currentSeconds == targetSeconds &&
           currentNanoSeconds >= targetNanoSeconds));
}
