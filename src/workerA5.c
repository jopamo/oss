#include "globals.h"
#include "init.h"
#include "shared.h"
#include "timeutils.h"
#include "user_process.h"

#define B 250000000L // Upper bound in nanoseconds for random timing
#define TERMINATION_CHECK_INTERVAL 250000000L // Check termination every 250ms
#define REQUEST_PROBABILITY 90 // 90% probability of requesting vs releasing

// Local array to track the resources held by the worker
int heldResources[MAX_RESOURCES] = {0};

void sendResourceRequest(int action, int resourceType) {
  MessageA5 msg = {
      .senderPid = getpid(),
      .commandType = action,
      .resourceType = resourceType,
      .count = 1 // Always request or release one unit
  };

  if (sendMessage(msqId, &msg, sizeof(msg)) == 0) {
    log_message(LOG_LEVEL_DEBUG, 0,
                "Worker %d: Sent message to %s resource R%d", getpid(),
                action == REQUEST_RESOURCE ? "request" : "release",
                resourceType);
  } else {
    log_message(LOG_LEVEL_ERROR, 0,
                "Worker %d: Failed to send message to %s resource R%d",
                getpid(), action == REQUEST_RESOURCE ? "request" : "release",
                resourceType);
  }
}

void waitForResourceResponse(int action, int resourceType) {
  MessageA5 response;

  while (true) {
    if (receiveMessage(msqId, &response, sizeof(response), IPC_NOWAIT) == 0) {
      log_message(LOG_LEVEL_DEBUG, 0,
                  "Worker %d: Received response for resource %s", getpid(),
                  action == REQUEST_RESOURCE ? "request" : "release");

      // Update local resource tracking based on the action
      if (action == REQUEST_RESOURCE) {
        heldResources[resourceType] += response.count;
      } else if (action == RELEASE_RESOURCE) {
        heldResources[resourceType] -= response.count;
      }
      break;
    }
    usleep(10000); // Sleep for 10ms to avoid busy-waiting
  }
}

void sendTerminationMessage(void) {
  MessageA5 msg = {.senderPid = getpid(),
                   .commandType = TERMINATE_PROCESS,
                   .resourceType = -1,
                   .count = 0};

  if (sendMessage(msqId, &msg, sizeof(msg)) == 0) {
    log_message(LOG_LEVEL_DEBUG, 0, "Worker %d: Sent termination message",
                getpid());
  } else {
    log_message(LOG_LEVEL_ERROR, 0,
                "Worker %d: Failed to send termination message", getpid());
  }
}

int main(void) {
  gProcessType = PROCESS_TYPE_WORKER;
  initializeSharedResources();
  setupSignalHandlers();

  unsigned long startSec, startNano;
  better_sem_wait(clockSem);
  startSec = simClock->seconds;
  startNano = simClock->nanoseconds;
  better_sem_post(clockSem);

  log_message(LOG_LEVEL_DEBUG, 0, "Worker process started with PID %d",
              getpid());

  while (keepRunning) {
    better_sem_wait(clockSem);
    unsigned long currentSec = simClock->seconds;
    unsigned long currentNano = simClock->nanoseconds;
    better_sem_post(clockSem);

    unsigned long elapsedNano =
        (currentSec - startSec) * 1000000000L + (currentNano - startNano);

    if (elapsedNano >=
        (unsigned long)(rand() % B)) { // Use B as the bound for actions
      startSec = currentSec;
      startNano = currentNano;

      int decision =
          rand() % 100; // Decide whether to request or release a resource
      int action = (decision < REQUEST_PROBABILITY) ? REQUEST_RESOURCE
                                                    : RELEASE_RESOURCE;
      int resourceType =
          rand() % MAX_RESOURCES; // Randomly choose a resource type

      if (action == REQUEST_RESOURCE &&
          heldResources[resourceType] < MAX_INSTANCES) {
        sendResourceRequest(action, resourceType);
        waitForResourceResponse(action, resourceType);
      } else if (action == RELEASE_RESOURCE &&
                 heldResources[resourceType] > 0) {
        sendResourceRequest(action, resourceType);
        waitForResourceResponse(action, resourceType);
      }
    }

    // Check if it's time to possibly terminate
    if ((currentSec - startSec) >= 1) {
      if ((rand() % 100) < 10) { // 10% chance to decide to terminate
        log_message(LOG_LEVEL_INFO, 0, "Worker %d: Deciding to terminate",
                    getpid());
        keepRunning = 0;

        // Release all resources before terminating
        for (int resourceType = 0; resourceType < MAX_RESOURCES;
             resourceType++) {
          while (heldResources[resourceType] > 0) {
            sendResourceRequest(RELEASE_RESOURCE, resourceType);
            waitForResourceResponse(RELEASE_RESOURCE, resourceType);
          }
        }

        sendTerminationMessage();
      }
    }

    usleep(10000); // Sleep for 10ms to reduce CPU usage
  }

  cleanupSharedResources();
  log_message(LOG_LEVEL_DEBUG, 0,
              "Worker %d: Exiting and cleaning up resources", getpid());
  return EXIT_SUCCESS;
}
