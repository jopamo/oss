#include "timeutils.h"

static double simSpeedFactor = TIMEKEEPER_SIM_SPEED_FACTOR;

static struct timespec startTime;
static long unsigned int lastSimLogTimeSec = 0;
static long unsigned int lastActualLogTimeSec = 0;

void initializeTimeTracking(void) {
  if (better_sem_wait(clockSem) == 0) {
    if (!simClock->initialized) {
      simClock->seconds = 0;
      simClock->nanoseconds = 0;
      simClock->initialized = 1;

      // Set the start time for actual time tracking
      if (clock_gettime(CLOCK_MONOTONIC, &startTime) != 0) {
        log_message(LOG_LEVEL_ERROR, 0, "Failed to initialize start time.");
      } else {
        log_message(LOG_LEVEL_INFO, 0, "Start time initialized: %ld s, %ld ns",
                    startTime.tv_sec, startTime.tv_nsec);
      }

      log_message(LOG_LEVEL_INFO, 0, "Simulation clock initialized.");
    }
    better_sem_post(clockSem);
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

    // Log only if the second has changed since the last log
    if (simClock->seconds != lastSimLogTimeSec) {
      log_message(LOG_LEVEL_INFO, 0, "Simulated time updated: %lu s, %lu ns",
                  simClock->seconds, simClock->nanoseconds);
      lastSimLogTimeSec = simClock->seconds;
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

    if (actualTime->seconds > lastActualLogTimeSec) {
      log_message(LOG_LEVEL_INFO, 0, "Actual time elapsed: %ld s, %ld ns",
                  elapsedSec, elapsedNano);
      lastActualLogTimeSec = actualTime->seconds;
    }

    better_sem_post(clockSem);
  }
}
