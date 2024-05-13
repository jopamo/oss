#include "arghandler.h"
#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "process.h"
#include "queue.h"
#include "resource.h"
#include "shared.h"
#include "timeutils.h"
#include "user_process.h"

#define ACTION_INTERVAL_NS 250000000L // Action interval in nanoseconds (250ms)

void initializeSimulationEnvironment(void);
void manageSimulation(void);
void manageChildTerminations(void);
void manageResourceRequests(int msqId);
bool shouldLaunchNextChild();

int stillChildrenToLaunch() {
  return totalLaunched < maxProcesses && currentChildren < maxSimultaneous;
}

int main(int argc, char *argv[]) {
  setupParentSignalHandlers();
  semUnlinkCreate();
  initializeSharedResources();

  if (initializeProcessTable() == -1 || initializeResourceTable() == -1) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to initialize all tables");
    exit(EXIT_FAILURE);
  }

  initializeTimeTracking();

  setupTimeout(MAX_RUNTIME);

  if (psmgmtArgs(argc, argv) != 0) {
    exit(EXIT_FAILURE);
  }

  atexit(cleanupResources);
  initializeSimulationEnvironment();
  manageSimulation();
  cleanupAndExit();
  return EXIT_SUCCESS;
}

void initializeSimulationEnvironment(void) {
  if (!simClock) {
    fprintf(stderr, "Error: Shared memory not attached.\n");
    exit(EXIT_FAILURE);
  }

  // Assuming simClock is already initialized in shared memory setup
  logFile = fopen(logFileName, "w+");
  if (!logFile) {
    perror("Failed to open log file");
    exit(EXIT_FAILURE);
  }
}

void manageSimulation(void) {
  unsigned long lastResourceCheckTimeSec =
      0; // For managing resources and deadlocks once per second
  unsigned long lastLogTimeSec = 0;            // For logging twice per second
  const unsigned long halfSecond = 500000000L; // Half second in nanoseconds

  while (keepRunning && (stillChildrenToLaunch() || currentChildren > 0)) {
    manageChildTerminations();

    if (shouldLaunchNextChild()) {
      pid_t pid = forkAndExecute("./worker");
      if (pid > 0) {
        registerChildProcess(pid);
        totalLaunched++;
        currentChildren++;
      }
    }

    simulateTimeProgression();
    trackActualTime();

    better_sem_wait(clockSem);
    unsigned long currentTimeSec = simClock->seconds;
    unsigned long currentTimeNano = simClock->nanoseconds;
    better_sem_post(clockSem);

    // Log resource and process tables twice per second
    if ((currentTimeSec > lastLogTimeSec) ||
        (currentTimeSec == lastLogTimeSec && currentTimeNano >= halfSecond &&
         lastLogTimeSec * 1000000000L + halfSecond <= currentTimeNano)) {
      lastLogTimeSec = currentTimeSec;
      logResourceTable();
      logProcessTable();
    }

    // Manage resource requests and deadlock checking once per second
    if (currentTimeSec > lastResourceCheckTimeSec) {
      manageResourceRequests(msqId);

      if (unsafeSystem()) { // Check for deadlocks only once per second
        resolveDeadlocks();
      }

      lastResourceCheckTimeSec = currentTimeSec;
    }

    // Calculate the sleep time based on elapsed time since the last action
    unsigned long elapsedNanoSinceLastAction =
        (currentTimeSec - lastResourceCheckTimeSec) * 1000000000L +
        currentTimeNano;
    long sleepNano = ACTION_INTERVAL_NS - elapsedNanoSinceLastAction;
    if (sleepNano > 0) {
      better_sleep(0, sleepNano);
    }
  }
}

void manageResourceRequests(int msqId) {
  MessageA5 msg;
  int result;

  // Non-blocking check for messages
  while (true) {
    result = receiveMessage(msqId, &msg, sizeof(msg), 0, IPC_NOWAIT);

    if (result == 0) { // Adjusted to check for success as return '0'
      log_message(
          LOG_LEVEL_INFO, 0,
          "Received message from PID %d: Command %d, ResourceType %d, Count %d",
          msg.senderPid, msg.commandType, msg.resourceType, msg.count);

      // Handle resource request or release based on the command type
      if (msg.commandType == MSG_REQUEST_RESOURCE) {
        if (requestResource(msg.senderPid) == 0) {
          log_message(LOG_LEVEL_INFO, 0, "Resource allocated to PID %d",
                      msg.senderPid);
        } else {
          log_message(LOG_LEVEL_WARN, 0,
                      "Failed to allocate resource to PID %d", msg.senderPid);
          // Optionally enqueue the request if it cannot be granted immediately
          enqueue(&resourceQueues[msg.resourceType], msg);
        }
      } else if (msg.commandType == MSG_RELEASE_RESOURCE) {
        if (releaseResource(msg.senderPid) == 0) {
          log_message(LOG_LEVEL_INFO, 0, "Resource released by PID %d",
                      msg.senderPid);
        } else {
          log_message(LOG_LEVEL_ERROR, 0,
                      "Failed to release resource by PID %d", msg.senderPid);
        }
      }
    } else if (result == -1 && errno != ENOMSG) {
      // Handle unexpected errors
      log_message(LOG_LEVEL_ERROR, 0, "Failed to receive messages: %s",
                  strerror(errno));
      break; // Exit the loop if a serious error occurs
    } else {
      // No more messages to process or other non-critical errors
      break;
    }
  }
}

void manageChildTerminations(void) {
  int status;
  pid_t pid;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    releaseAllResourcesForProcess(pid);
    currentChildren--;
  }
}

bool shouldLaunchNextChild() {
  static unsigned long lastLaunchSecond = 0;
  if (currentChildren < maxSimultaneous &&
      simClock->seconds > lastLaunchSecond + 1) {
    lastLaunchSecond = simClock->seconds;
    return true;
  }
  return false;
}
