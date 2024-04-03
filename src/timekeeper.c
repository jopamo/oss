#include "cleanup.h"
#include "shared.h"

void runTimekeeper() {
  log_message(LOG_LEVEL_INFO, "Starting timekeeping service.");

  simClock = attachSharedMemory();
  if (simClock == (void *)-1) {
    log_message(LOG_LEVEL_ERROR, "Failed to attach to shared memory.");
    exit(EXIT_FAILURE);
  }

  clockSem = sem_open(clockSemName, O_CREAT | O_RDWR, 0644, 1);
  if (clockSem == SEM_FAILED) {
    log_message(LOG_LEVEL_ERROR, "Failed to open semaphore: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }

  double speedFactor = 0.28;
  long incrementPerCycle = (long)(speedFactor * NANOSECONDS_IN_SECOND);

  while (keepRunning) {
    usleep(250000);

    sem_wait(clockSem);
    simClock->nanoseconds += incrementPerCycle;

    if (simClock->nanoseconds >= NANOSECONDS_IN_SECOND) {
      simClock->seconds += simClock->nanoseconds / NANOSECONDS_IN_SECOND;
      simClock->nanoseconds %= NANOSECONDS_IN_SECOND;
    }

    sem_post(clockSem);

    log_message(LOG_LEVEL_DEBUG, "Simulated Time updated: %lu sec, %lu nsec",
                simClock->seconds, simClock->nanoseconds);
  }
}

int main() {
  gProcessType = PROCESS_TYPE_TIMEKEEPER;

  setupSignalHandlers();
  runTimekeeper();
  return EXIT_SUCCESS;
}
