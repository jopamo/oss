#include <time.h>
#include <unistd.h>

#include "process.h"
#include "shared.h"

void runTimekeeper(void) {
  log_message(LOG_LEVEL_INFO, "Starting timekeeping service.");

  key_t simClockKey = ftok(SHM_PATH, SHM_PROJ_ID);

  if (initSharedMemorySegment(simClockKey, sizeof(SimulatedClock), &shmId,
                              "Simulated Clock") < 0) {
    log_message(LOG_LEVEL_ERROR,
                "Failed to initialize shared memory for Simulated Clock.");
    exit(EXIT_FAILURE);
  }

  simClock = (SimulatedClock *)attachSharedMemorySegment(
      shmId, sizeof(SimulatedClock), "Simulated Clock");

  if (simClock == NULL) {
    log_message(LOG_LEVEL_ERROR,
                "Failed to attach to shared memory for Simulated Clock.");
    exit(EXIT_FAILURE);
  }

  key_t actualTimeKey = ftok(SHM_PATH, SHM_PROJ_ID + 1);
  if (initSharedMemorySegment(actualTimeKey, sizeof(ActualTime),
                              &actualTimeShmId, "Actual Time") < 0) {

    log_message(LOG_LEVEL_ERROR,
                "Failed to initialize shared memory for Actual Time.");
    exit(EXIT_FAILURE);
  }

  actualTime = (ActualTime *)attachSharedMemorySegment(
      actualTimeShmId, sizeof(ActualTime), "Actual Time");

  if (actualTime == NULL) {
    log_message(LOG_LEVEL_ERROR,
                "Failed to attach to shared memory for Actual Time.");
    exit(EXIT_FAILURE);
  }

  clockSem = sem_open(clockSemName, 0);
  if (clockSem == SEM_FAILED) {
    clockSem = sem_open(clockSemName, O_CREAT | O_EXCL, SEM_PERMISSIONS, 1);
    if (clockSem == SEM_FAILED) {
      log_message(LOG_LEVEL_ERROR, "Failed to open semaphore: %s",
                  strerror(errno));
      exit(EXIT_FAILURE);
    } else {
      log_message(LOG_LEVEL_INFO, "New semaphore created.");
    }
  } else {
    log_message(LOG_LEVEL_INFO, "Existing semaphore opened.");
  }

  simClock->seconds = 0;
  simClock->nanoseconds = 0;
  simClock->initialized = 1;

  struct timespec startTime, currentTime;
  clock_gettime(CLOCK_MONOTONIC, &startTime);

  double simSpeedFactor = 0.28;
  long simIncrementPerCycle = (long)(simSpeedFactor * NANOSECONDS_IN_SECOND);

  while (keepRunning) {
    nanosleep(&(struct timespec){0, 250000000L}, NULL);

    if (sem_wait(clockSem) != -1) {
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

    if (actualTime->seconds >= 60) {
      keepRunning = 0;
    }
  }
}

int main(void) {
  gProcessType = PROCESS_TYPE_TIMEKEEPER;

  setupSignalHandlers();
  runTimekeeper();

  cleanupSharedResources();

  return EXIT_SUCCESS;
}
