#include "globals.h"
#include "init.h"
#include "shared.h"
#include "timeutils.h"
#include "user_process.h"

#define B 250000000L // Upper bound in nanoseconds for random timing
#define TERMINATION_CHECK_INTERVAL 1000000000L // Check termination every second
#define REQUEST_PROBABILITY 99 // Probability of requesting vs releasing

int main(void) {
  gProcessType = PROCESS_TYPE_WORKER;
  initializeSharedResources();
  setupSignalHandlers();

  unsigned long lastActionTimeSec = simClock->seconds;
  unsigned long lastActionTimeNano = simClock->nanoseconds;
  unsigned long lastCheckSec = simClock->seconds; // Initialize last check time

  log_message(LOG_LEVEL_DEBUG, 0, "Worker process started with PID %d",
              getpid());

  while (keepRunning) {
    better_sem_wait(clockSem); // Lock semaphore to safely read time
    unsigned long currentSec = simClock->seconds;
    unsigned long currentNano = simClock->nanoseconds;
    better_sem_post(clockSem); // Unlock semaphore after reading time

    unsigned long elapsedNano = (currentSec - lastActionTimeSec) * 1000000000L +
                                (currentNano - lastActionTimeNano);

    if (elapsedNano >=
        (unsigned long)(rand() % B)) { // Use B as the bound for actions
      lastActionTimeSec = currentSec;
      lastActionTimeNano = currentNano;

      int decision =
          rand() % 100; // Decide whether to request or release a resource
      int action = (decision < REQUEST_PROBABILITY) ? REQUEST_RESOURCE
                                                    : RELEASE_RESOURCE;

      MessageA5 msg = {
          .senderPid = getpid(),
          .commandType = action,
          .count = 1 // Always request or release one unit
      };

      if (sendMessage(msqId, &msg, sizeof(msg)) == 0) {
        log_message(LOG_LEVEL_INFO, 0, "Worker %d: Successfully %sed resource",
                    getpid(),
                    action == REQUEST_RESOURCE ? "request" : "release");
      } else {
        log_message(LOG_LEVEL_ERROR, 0,
                    "Worker %d: Failed to send message for %s resource",
                    getpid(),
                    action == REQUEST_RESOURCE ? "requesting" : "releasing");
      }
    }

    if ((currentSec - lastCheckSec) >=
        1) { // Check if it's time to possibly terminate
      lastCheckSec = currentSec;
      if ((rand() % 100) < 10) { // 10% chance to decide to terminate
        log_message(LOG_LEVEL_INFO, 0, "Worker %d: Deciding to terminate",
                    getpid());
        keepRunning = 0;
      }
    }

    usleep(10000); // Sleep for 10ms to reduce CPU usage
  }

  cleanupSharedResources();
  log_message(LOG_LEVEL_INFO, 0, "Worker %d: Exiting and cleaning up resources",
              getpid());
  return EXIT_SUCCESS;
}
