#include "timeutils.h"

static double simSpeedFactor = TIMEKEEPER_SIM_SPEED_FACTOR;

static struct timespec startTime;

void initializeTimeTracking(void) {
  if (clockSem == SEM_FAILED || clockSem == NULL) {
    log_message(LOG_LEVEL_ERROR, 0, "Invalid semaphore pointer provided.");
    return;
  }

  if (better_sem_wait(clockSem) == 0) {
    if (!simClock->initialized) {
      simClock->seconds = 0;
      simClock->nanoseconds = 0;
      simClock->initialized = 1;

      // Set the start time for actual time tracking
      if (clock_gettime(CLOCK_MONOTONIC, &startTime) != 0) {
        log_message(LOG_LEVEL_ERROR, 0, "Failed to initialize start time.");
      } else {
        log_message(LOG_LEVEL_DEBUG, 0, "Start time initialized: %ld s, %ld ns",
                    startTime.tv_sec, startTime.tv_nsec);
      }

      log_message(LOG_LEVEL_DEBUG, 0, "Simulation clock initialized.");
    }
    better_sem_post(clockSem);
  } else {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to wait on semaphore.");
  }
}

void simulateTimeProgression(void) {
  if (better_sem_wait(clockSem) == 0) {
    long increment = (long)(250000000L * simSpeedFactor);
    simClock->nanoseconds += increment;
    while (simClock->nanoseconds >= 1000000000L) {
      simClock->seconds++;
      simClock->nanoseconds -= 1000000000L;
    }

    better_sem_post(clockSem);
  }
}

void trackActualTime(void) {
  struct timespec currentTime;
  if (clock_gettime(CLOCK_MONOTONIC, &currentTime) != 0) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to get current time.");
    return;
  }

  if (better_sem_wait(clockSem) == 0) {
    // Calculate elapsed time
    long elapsedSec = currentTime.tv_sec - startTime.tv_sec;
    long elapsedNano = currentTime.tv_nsec - startTime.tv_nsec;
    if (elapsedNano < 0) { // Handle the case where nanoseconds wrap
      elapsedSec--;
      elapsedNano += 1000000000;
    }

    // Store the elapsed time
    actualTime->seconds = elapsedSec;
    actualTime->nanoseconds = elapsedNano;

    better_sem_post(clockSem);
  }
}
