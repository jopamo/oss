#include <time.h>
#include <unistd.h>

#include "shared.h"
#include "user_process.h"

void runTimekeeper(void) {
  gProcessType = PROCESS_TYPE_TIMEKEEPER;

  initializeSharedResources();

  log_message(LOG_LEVEL_INFO, "Starting timekeeping service.");

  if (!simClock || !actualTime) {
    log_message(LOG_LEVEL_ERROR, "Failed to attach to shared memory segments.");
    exit(EXIT_FAILURE);
  }

  if (!simClock->initialized) {
    simClock->seconds = 0;
    simClock->nanoseconds = 0;
    simClock->initialized = 1;
  }

  double simSpeedFactor = 0.28;
  long simIncrementPerCycle = (long)(simSpeedFactor * NANOSECONDS_IN_SECOND);

  struct timespec startTime, currentTime;
  clock_gettime(CLOCK_MONOTONIC, &startTime);

  while (keepRunning) {
    nanosleep(&(struct timespec){0, 250000000L}, NULL);

    if (better_sem_wait(clockSem) != -1) {
      clock_gettime(CLOCK_MONOTONIC, &currentTime);
      long elapsedSec = currentTime.tv_sec - startTime.tv_sec;
      long elapsedNano = currentTime.tv_nsec - startTime.tv_nsec;

      if (elapsedNano < 0) {
        elapsedSec--;
        elapsedNano += NANOSECONDS_IN_SECOND;
      }

      actualTime->seconds = elapsedSec;
      actualTime->nanoseconds = elapsedNano;

      simClock->nanoseconds += simIncrementPerCycle;
      while (simClock->nanoseconds >= NANOSECONDS_IN_SECOND) {
        simClock->seconds++;
        simClock->nanoseconds -= NANOSECONDS_IN_SECOND;
      }

      log_message(
          LOG_LEVEL_INFO, "Actual time: %ld.%09ld, Simulated time: %lu.%09lu",
          elapsedSec, elapsedNano, simClock->seconds, simClock->nanoseconds);

      sem_post(clockSem);
    } else {
      log_message(LOG_LEVEL_ERROR, "Failed to lock semaphore: %s",
                  strerror(errno));
    }
  }

  log_message(LOG_LEVEL_INFO, "Timekeeping service ending.");
}

int main(void) {
  gProcessType = PROCESS_TYPE_TIMEKEEPER;

  setupSignalHandlers();
  runTimekeeper();

  cleanupSharedResources();

  return EXIT_SUCCESS;
}
