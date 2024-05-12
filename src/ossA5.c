#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define LAUNCH_INTERVAL 1000 // Value in milliseconds
#define MAX_CHILDREN 18

#include "arghandler.h"
#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "process.h"
#include "resource.h"
#include "shared.h"
#include "user_process.h"

void initializeSimulationEnvironment(void);
void manageSimulation(void);
void checkForMessagesAndHandleRequests(int msqId);
void attemptProcessLaunch(void);
void performPeriodicChecks(void);
void simulateTimeProgression(struct timespec *lastCheck, double simSpeedFactor);

int shouldLaunchNextChild(struct timespec *lastLaunchTime);

int main(int argc, char *argv[]) {
  gProcessType = PROCESS_TYPE_OSS;
  parentPid = getpid();

  setupParentSignalHandlers();
  semUnlinkCreate();
  initializeSharedResources();
  ossArgs(argc, argv);
  atexit(atexitHandler);

  initializeSimulationEnvironment();
  manageSimulation();

  cleanupAndExit();
  return EXIT_SUCCESS;
}

void initializeSimulationEnvironment(void) {
  if (!simClock || !actualTime) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Shared memory segments not attached properly.");
    exit(EXIT_FAILURE);
  }

  if (!simClock->initialized) {
    simClock->seconds = 0;
    simClock->nanoseconds = 0;
    simClock->initialized = 1;
    log_message(LOG_LEVEL_DEBUG, 0, "Simulated clock initialized.");
  }

  logFile = fopen(logFileName, "w+");
  if (!logFile) {
    perror("Failed to open log file");
    exit(EXIT_FAILURE);
  }
}

void manageSimulation(void) {
  struct timespec lastTimeCheck;
  clock_gettime(CLOCK_MONOTONIC, &lastTimeCheck);

  while (stillChildrenToLaunch() || getCurrentChildren() > 0) {
    pid_t pid;
    int status;

    // Non-blocking check if any child has terminated
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
      handleTermination(pid);
      releaseAllResourcesForProcess(pid);
    }

    if (shouldLaunchNextChild(&lastTimeCheck)) {
      pid = forkAndExecute("./worker");
      if (pid > 0) {
        registerChildProcess(pid);
      }
    }

    // Initialize the message queue
    int msqid = initMessageQueue();
    if (msqid == -1) {
      fprintf(stderr, "Failed to initialize message queue\n");
      exit(EXIT_FAILURE);
    }
    checkForMessagesAndHandleRequests(msqId);
    simulateTimeProgression(&lastTimeCheck, TIMEKEEPER_SIM_SPEED_FACTOR);
    performPeriodicChecks();

    usleep(100000); // Sleep to reduce CPU usage
  }
}

int shouldLaunchNextChild(struct timespec *lastLaunchTime) {
  struct timespec currentTime;
  clock_gettime(CLOCK_MONOTONIC, &currentTime);
  if (currentTime.tv_sec - lastLaunchTime->tv_sec > 1) {
    *lastLaunchTime = currentTime;
    return 1;
  }
  return 0;
}

void simulateTimeProgression(struct timespec *lastCheck,
                             double simSpeedFactor) {
  struct timespec currentTime;
  clock_gettime(CLOCK_MONOTONIC, &currentTime);
  long elapsedNano =
      (currentTime.tv_sec - lastCheck->tv_sec) * NANOSECONDS_IN_SECOND +
      (currentTime.tv_nsec - lastCheck->tv_nsec);

  if (elapsedNano >= 250000000L) { // Update every 250ms
    if (better_sem_wait(clockSem) == 0) {
      simClock->nanoseconds += (long)(simSpeedFactor * elapsedNano);
      while (simClock->nanoseconds >= NANOSECONDS_IN_SECOND) {
        simClock->seconds++;
        simClock->nanoseconds -= NANOSECONDS_IN_SECOND;
      }
      sem_post(clockSem);
      *lastCheck = currentTime;
    }
  }
}

void performPeriodicChecks(void) {
  static unsigned long lastDeadlockCheck = 0;
  if (simClock->seconds != lastDeadlockCheck) {
    if (unsafeSystem()) {
      resolveDeadlocks();
    }
    lastDeadlockCheck = simClock->seconds;
  }
}

void checkForMessagesAndHandleRequests(int msqId) {
  MessageA5 msg;
  int msgResult;
  while ((msgResult = receiveMessage(msqId, &msg, sizeof(msg), 0, IPC_NOWAIT)) >
         0) {
    switch (msg.commandType) {
    case MSG_REQUEST_RESOURCE:
      if (requestResource(msg.resourceType, msg.count, msg.senderPid) == 0) {
        log_message(LOG_LEVEL_INFO, 0, "Resource granted to process %d.",
                    msg.senderPid);
      } else {
        log_message(LOG_LEVEL_WARN, 0,
                    "Resource request by process %d could not be granted.",
                    msg.senderPid);
      }
      break;
    case MSG_RELEASE_RESOURCE:
      if (releaseResource(msg.resourceType, msg.count, msg.senderPid) == 0) {
        log_message(LOG_LEVEL_INFO, 0, "Process %d released resources.",
                    msg.senderPid);
      }
      break;
    default:
      log_message(LOG_LEVEL_WARN, 0,
                  "Received unknown message type %d from process %d.",
                  msg.commandType, msg.senderPid);
      break;
    }
  }
}
