#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "shared.h"
#include "signals.h"
#include "timeutils.h"
#include "user_process.h"

int runWorkerA3(long lifespanSeconds, long lifespanNanoSeconds) {
  if (lifespanSeconds < 0 || lifespanNanoSeconds < 0) {
    log_message(LOG_LEVEL_ERROR, 0, "Invalid lifespan arguments.");
    exit(EXIT_FAILURE);
  }

  long targetSeconds = simClock->seconds + lifespanSeconds;
  long targetNanoSeconds = simClock->nanoseconds + lifespanNanoSeconds;

  if (targetNanoSeconds >= 1000000000L) {
    targetSeconds += 1;
    targetNanoSeconds -= 1000000000L;
  }

  pid_t pid = getpid();
  pid_t ppid = getppid();

  log_message(LOG_LEVEL_INFO, 0,
              "WORKER PID:%d PPID:%d SysClockS:%ld SysclockNano:%ld "
              "TermTimeS:%ld TermTimeNano:%ld --Just Starting",
              pid, ppid, simClock->seconds, simClock->nanoseconds,
              targetSeconds, targetNanoSeconds);

  int iteration = 0;
  while (keepRunning) {
    better_sleep(0, 10000000); // Sleep for 10ms

    long currentSeconds = simClock->seconds;
    long currentNanoSeconds = simClock->nanoseconds;

    if (!keepRunning || currentSeconds > targetSeconds ||
        (currentSeconds == targetSeconds &&
         currentNanoSeconds >= targetNanoSeconds)) {
      log_message(
          LOG_LEVEL_INFO, 0,
          "WORKER PID:%d PPID:%d SysClockS:%ld SysclockNano:%ld TermTimeS:%ld "
          "TermTimeNano:%ld --Terminating after %d iterations",
          pid, ppid, currentSeconds, currentNanoSeconds, targetSeconds,
          targetNanoSeconds, iteration);
      break;
    }

    log_message(
        LOG_LEVEL_INFO, 0,
        "WORKER PID:%d PPID:%d SysClockS:%ld SysclockNano:%ld TermTimeS:%ld "
        "TermTimeNano:%ld --%d iterations have passed since starting",
        pid, ppid, currentSeconds, currentNanoSeconds, targetSeconds,
        targetNanoSeconds, iteration);

    iteration++;

    if (!keepRunning) {
      log_message(LOG_LEVEL_INFO, 0, "Terminating due to external signal.");
      break;
    }
  }

  cleanupResources();
  return 0;
}
