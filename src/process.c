#include "process.h"
#include "globals.h"
#include "resource.h"
#include "shared.h"

void registerChildProcess(pid_t pid) {
  pthread_mutex_lock(&processTableMutex);
  int index = findFreeProcessTableEntry();
  if (index != -1) {
    processTable[index].pid = pid;
    processTable[index].occupied = 1;
    processTable[index].state = PROCESS_RUNNING;
    processTable[index].startSeconds = simClock->seconds;
    processTable[index].startNano = simClock->nanoseconds;
    log_message(LOG_LEVEL_DEBUG, 0,
                "Registered child process with PID %d at index %d", pid, index);
  } else {
    log_message(
        LOG_LEVEL_ERROR, 0,
        "No free entries to register process PID %d. Terminating process.",
        pid);
    kill(pid, SIGTERM); // Gracefully terminate the child process
  }
  pthread_mutex_unlock(&processTableMutex);
}

int findFreeProcessTableEntry(void) {
  pthread_mutex_lock(&processTableMutex);
  if (processTable == NULL) {
    log_message(LOG_LEVEL_ERROR, 0, "Process table is not initialized.");
    pthread_mutex_unlock(&processTableMutex);
    return -1; // Indicate failure
  }

  for (int i = 0; i < maxProcesses; i++) {
    if (!processTable[i].occupied) {
      log_message(LOG_LEVEL_DEBUG, 0,
                  "Found free process table entry at index %d", i);
      pthread_mutex_unlock(&processTableMutex);
      return i;
    }
  }
  log_message(LOG_LEVEL_WARN, 0, "No free process table entries found.");
  pthread_mutex_unlock(&processTableMutex);
  return -1;
}

pid_t forkAndExecute(const char *executable, int argCount, char *args[]) {
  pid_t pid = fork();
  if (pid == 0) {
    // Create an array to hold the executable and its arguments, followed by a
    // NULL pointer.
    char *execArgs[argCount + 2];
    execArgs[0] =
        (char *)executable; // The first element should be the executable name.
    for (int i = 0; i < argCount; ++i) {
      execArgs[i + 1] = args[i]; // Fill in the arguments.
    }
    execArgs[argCount + 1] = NULL; // The last element must be NULL.

    // Replace the current process with the new executable.
    execvp(executable, execArgs);
    perror("Failed to execute child process");
    cleanupAndExit(EXIT_FAILURE);
  } else if (pid < 0) {
    perror("Failed to fork child process");
    cleanupAndExit(EXIT_FAILURE);
  }
  return pid;
}

void handleTermination(pid_t pid) {
  pthread_mutex_lock(&processTableMutex);
  int index = findProcessIndexByPID(pid);
  if (index != -1) {
    freeAllProcessResources(index);
    clearProcessEntry(index);
    log_message(LOG_LEVEL_INFO, 0,
                "Terminated process %d and cleared resources.", pid);
    decrementCurrentChildren();
  }
  pthread_mutex_unlock(&processTableMutex);
}

void freeAllProcessResources(int index) {
  releaseAllResourcesForProcess(processTable[index].pid);
}

void updateResourceAndProcessTables() {
  logResourceTable();
  logProcessTable();
}

void decrementCurrentChildren() {
  if (currentChildren > 0) {
    currentChildren--;
  }
}

const char *processStateToString(int state) {
  switch (state) {
  case PROCESS_RUNNING:
    return "Running";
  case PROCESS_WAITING:
    return "Waiting";
  case PROCESS_TERMINATED:
    return "Terminated";
  default:
    return "Unknown";
  }
}

void clearProcessEntry(int index) {
  pthread_mutex_lock(&processTableMutex);
  if (index >= 0 && index < maxProcesses && processTable[index].occupied) {
    processTable[index].occupied = 0;
    processTable[index].state = PROCESS_TERMINATED;
    log_message(LOG_LEVEL_INFO, 0, "Process terminated at table entry index %d",
                index);
  }
  pthread_mutex_unlock(&processTableMutex);
}

int killProcess(pid_t pid, int sig) { return kill(pid, sig); }

int findProcessIndexByPID(pid_t pid) {
  pthread_mutex_lock(&processTableMutex);
  for (int i = 0; i < maxProcesses; i++) {
    if (processTable[i].occupied && processTable[i].pid == pid) {
      log_message(LOG_LEVEL_DEBUG, 0,
                  "Found process table entry for PID %ld at index %d",
                  (long)pid, i);
      pthread_mutex_unlock(&processTableMutex);
      return i;
    }
  }
  log_message(LOG_LEVEL_DEBUG, 0, "No process table entry found for PID %ld",
              (long)pid);
  pthread_mutex_unlock(&processTableMutex);
  return -1;
}

void logProcessTable() {
  log_message(LOG_LEVEL_INFO, 0,
              "---------------- Process Table ----------------");
  log_message(
      LOG_LEVEL_INFO, 0,
      " Index | PID    | State        | Start Time   | Blocked | Block Until");

  pthread_mutex_lock(&processTableMutex);
  for (int i = 0; i < maxProcesses; i++) {
    if (processTable[i].occupied ||
        processTable[i].state == PROCESS_TERMINATED) {
      char startTime[64], blockTime[64];

      snprintf(startTime, sizeof(startTime), "%02d.%09d",
               processTable[i].startSeconds, processTable[i].startNano);
      snprintf(blockTime, sizeof(blockTime), "%02d.%09d",
               processTable[i].eventBlockedUntilSec,
               processTable[i].eventBlockedUntilNano);

      log_message(LOG_LEVEL_INFO, 0, "%5d  | %6d | %-12s | %s | %7s | %s", i,
                  processTable[i].pid,
                  processStateToString(processTable[i].state), startTime,
                  processTable[i].blocked ? "Yes" : "No", blockTime);
    }
  }
  pthread_mutex_unlock(&processTableMutex);
  log_message(LOG_LEVEL_INFO, 0,
              "------------------------------------------------");
}
