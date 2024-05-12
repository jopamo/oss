#include "shared.h"
#include "user_process.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include <unistd.h>

#define BOUND_B                                                                \
  500000000 // Upper boundary for random decision-making in nanoseconds
#define REQUEST_PROBABILITY                                                    \
  80 // Probability percentage of requesting resources over releasing

// Main process loop for worker
int main(void) {
  gProcessType = PROCESS_TYPE_WORKER;
  initializeSharedResources();
  setupSignalHandlers();

  srand(time(NULL) ^ (getpid() << 16)); // Seed the random number generator

  int msqid = initMessageQueue();
  if (msqid == -1) {
    fprintf(stderr, "Failed to initialize message queue\n");
    exit(EXIT_FAILURE);
  }

  int resources[MAX_RESOURCE_TYPES] = {
      0}; // Track resources held by this process
  time_t lastActionTime = time(NULL), currentTime;

  while (keepRunning) {
    currentTime = time(NULL);

    // Enforcing a minimum runtime before termination check
    if (difftime(currentTime, lastActionTime) >= 0.25) { // Act every 250ms
      lastActionTime = currentTime;
      int decision = rand() % 100;
      int resourceType =
          rand() % MAX_RESOURCE_TYPES; // Assuming MAX_RESOURCE_TYPES is defined
      int action = (decision < REQUEST_PROBABILITY) ? REQUEST_RESOURCE
                                                    : RELEASE_RESOURCE;

      MessageA5 msg = {
          .senderPid = getpid(),
          .commandType = action,
          .resourceType = resourceType,
          .count = 1 // Always request or release one unit at a time
      };

      if (action == REQUEST_RESOURCE &&
          resources[resourceType] < MAX_INSTANCES_PER_RESOURCE) {
        if (sendMessage(msqid, &msg, sizeof(msg)) == -1) {
          fprintf(stderr, "Failed to send request message\n");
          exit(EXIT_FAILURE);
        }
        resources[resourceType]++;
      } else if (action == RELEASE_RESOURCE && resources[resourceType] > 0) {
        if (sendMessage(msqid, &msg, sizeof(msg)) == -1) {
          fprintf(stderr, "Failed to send release message\n");
          exit(EXIT_FAILURE);
        }
        resources[resourceType]--;
      }
    }

    // Check if it should terminate after at least 1 second runtime
    if (difftime(currentTime, lastActionTime) > 1 &&
        (rand() % 100 < 10)) { // 10% chance to try to terminate every 250ms
      keepRunning = 0;
      break;
    }
  }

  // Release all resources held before exiting
  for (int i = 0; i < MAX_RESOURCE_TYPES; i++) {
    if (resources[i] > 0) {
      MessageA5 msg = {.senderPid = getpid(),
                       .commandType = RELEASE_RESOURCE,
                       .resourceType = i,
                       .count = resources[i]};
      sendMessage(msqid, &msg,
                  sizeof(msg)); // No need to check for errors in cleanup phase
    }
  }

  // Clean up before exiting
  cleanupSharedResources();
  return EXIT_SUCCESS;
}
