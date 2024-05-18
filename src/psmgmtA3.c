#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "process.h"
#include "queue.h"
#include "shared.h"
#include "signals.h"
#include "timeutils.h"
#include "user_process.h"
#include <sys/wait.h>

bool shouldLaunchNextChild() {
  return (keepRunning && currentChildren < maxProcesses) &&
         (simClock->seconds < MAX_RUNTIME);
}

void incrementClockByDispatchAmount() {
  long increment =
      250000000L / (currentChildren > 0
                        ? currentChildren
                        : 1); // 250ms divided by the number of current children
  simClock->nanoseconds += increment;
  if (simClock->nanoseconds >= 1000000000L) {
    simClock->seconds++;
    simClock->nanoseconds -= 1000000000L;
  }
}

void incrementClockByAmountUsedByChild(long usedSeconds, long usedNanoseconds) {
  simClock->seconds += usedSeconds;
  simClock->nanoseconds += usedNanoseconds;
  if (simClock->nanoseconds >= 1000000000L) {
    simClock->seconds++;
    simClock->nanoseconds -= 1000000000L;
  }
}

void possiblyLaunchNewChild() {
  if (shouldLaunchNextChild()) {
    char *workerArgs[] = {"5", "500000"}; // Default values, can be adjusted
    pid_t nextChildPid = forkAndExecute("./worker", 2, workerArgs);
    if (nextChildPid > 0) {
      registerChildProcess(nextChildPid);
      currentChildren++;
    }
  }
}

void incrementTheClockByChildStartAmount() {
  long increment = 10000000L; // 10ms
  simClock->nanoseconds += increment;
  if (simClock->nanoseconds >= 1000000000L) {
    simClock->seconds++;
    simClock->nanoseconds -= 1000000000L;
  }
}

void manageChildTerminations() {
  int status;
  pid_t pid;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    handleTermination(pid);
  }
}

void simulateBlockedProcesses() {
  for (int i = 0; i < maxProcesses; i++) {
    if (processTable[i].blocked) {
      if ((unsigned long)simClock->seconds >
              (unsigned long)processTable[i].eventBlockedUntilSec ||
          ((unsigned long)simClock->seconds ==
               (unsigned long)processTable[i].eventBlockedUntilSec &&
           (unsigned long)simClock->nanoseconds >=
               (unsigned long)processTable[i].eventBlockedUntilNano)) {

        processTable[i].blocked = 0;
        Message msg =
            createWorkerMessage(processTable[i].pid, PROCESS_RUNNING, 0);
        addProcessToMLFQ(&mlfq, msg, 0);
        log_message(LOG_LEVEL_DEBUG, 0, "Unblocked process PID %d.",
                    processTable[i].pid);
      }
    }
  }
}

bool anyProcessRunning() {
  for (int i = 0; i < maxProcesses; i++) {
    if (processTable[i].occupied && processTable[i].state == PROCESS_RUNNING) {
      return true;
    }
  }
  return false;
}

void handleProcessScheduling() {
  Message msg;
  if (getNextProcessFromMLFQ(&mlfq, &msg) == 0) {
    log_message(LOG_LEVEL_DEBUG, 0, "Scheduling process PID %ld.",
                msg.senderPid);
    if (kill(msg.senderPid, SIGCONT) == -1) {
      log_message(LOG_LEVEL_ERROR, 0, "Failed to send SIGCONT to PID %ld: %s",
                  msg.senderPid, strerror(errno));
    }
  }
}

void terminateAllChildren() {
  for (int i = 0; i < maxProcesses; i++) {
    if (processTable[i].occupied) {
      kill(processTable[i].pid, SIGTERM);
    }
  }
}

void runPsmgmtA3() {
  log_message(LOG_LEVEL_INFO, 0, "Simulation started for project version A3.");

  while (shouldLaunchNextChild() || anyProcessRunning()) {
    possiblyLaunchNewChild();

    incrementClockByDispatchAmount();

    manageChildTerminations();
    simulateBlockedProcesses();
    handleProcessScheduling();

    if (simClock->seconds % 1 == 0) {
      printf("OSS PID:%d SysClockS:%ld SysclockNano:%ld\n", getpid(),
             simClock->seconds, simClock->nanoseconds);
      printf("Process Table:\n");
      logProcessTable();
    }

    better_sleep(0, 10000000); // Sleep for 10ms to avoid busy-waiting
  }

  logStatistics();
  cleanupAndExit(EXIT_SUCCESS);
}
