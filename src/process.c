#include "process.h"

void atexitHandler(void) { cleanupResources(); }

void parentSignalHandler(int sig) {
  char buffer[256];
  switch (sig) {
  case SIGINT:
  case SIGTERM:
    snprintf(buffer, sizeof(buffer),
             "OSS (PID: %d): Shutdown requested by signal %d.\n", getpid(),
             sig);
    signalSafeLog(LOG_LEVEL_INFO, buffer);
    keepRunning = 0;
    break;
  case SIGALRM:
    snprintf(buffer, sizeof(buffer), "OSS (PID: %d): Timer expired.\n",
             getpid());
    signalSafeLog(LOG_LEVEL_INFO, buffer);
    keepRunning = 0;
    break;
  case SIGCHLD:
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    snprintf(buffer, sizeof(buffer),
             "OSS (PID: %d): Child process terminated.\n", getpid());
    signalSafeLog(LOG_LEVEL_INFO, buffer);
    break;
  default:
    snprintf(buffer, sizeof(buffer),
             "OSS (PID: %d): Unhandled signal %d received.\n", getpid(), sig);
    signalSafeLog(LOG_LEVEL_INFO, buffer);
    break;
  }
}

void setupParentSignalHandlers(void) {
  struct sigaction sa_parent, sa_child;
  memset(&sa_parent, 0, sizeof(sa_parent));
  memset(&sa_child, 0, sizeof(sa_child));
  sa_parent.sa_handler = parentSignalHandler;
  sa_child.sa_handler = childExitHandler;
  sigfillset(&sa_parent.sa_mask);
  sigfillset(&sa_child.sa_mask);

  sigaction(SIGCHLD, &sa_child, NULL);

  int signals[] = {SIGINT, SIGTERM, SIGALRM};
  for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
    if (sigaction(signals[i], &sa_parent, NULL) == -1) {
      signalSafeLog(LOG_LEVEL_ERROR,
                    "Error setting up parent signal handler. Exiting.");
      _exit(EXIT_FAILURE);
    }
  }
}

void childExitHandler(int sig) {
  (void)sig;
  pid_t pid;
  int status;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    // Find the index of the process that has terminated
    int index = findProcessIndexByPID(pid);
    if (index != -1) {
      handleTermination(pid);
      log_message(LOG_LEVEL_INFO, 0,
                  "Child process PID: %d terminated and cleaned up.", pid);
    }
  }
}

void setupTimeout(int seconds) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = timeoutHandler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGALRM, &sa, NULL);

  log_message(LOG_LEVEL_DEBUG, 0, "Setting up alarm for %d seconds.", seconds);
  alarm(seconds);
}

void timeoutHandler(int signum) {
  log_message(LOG_LEVEL_INFO, 0, "Timeout signal received, signum: %d.",
              signum);
  keepRunning = 0;
  cleanupAndExit();
}

void registerChildProcess(pid_t pid) {
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
}

int findFreeProcessTableEntry(void) {
  if (processTable == NULL) {
    log_message(LOG_LEVEL_ERROR, 0, "Process table is not initialized.");
    return -1; // Indicate failure
  }

  for (int i = 0; i < maxProcesses; i++) {
    if (!processTable[i].occupied) {
      log_message(LOG_LEVEL_DEBUG, 0,
                  "Found free process table entry at index %d", i);
      return i;
    }
  }
  log_message(LOG_LEVEL_WARN, 0, "No free process table entries found.");
  return -1;
}

pid_t forkAndExecute(const char *executable) {
  pid_t pid = fork();
  if (pid == 0) {
    execl(executable, executable, (char *)NULL);
    perror("Failed to execute child process");
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    perror("Failed to fork child process");
    exit(EXIT_FAILURE);
  }
  return pid;
}

void handleTermination(pid_t pid) {
  int index = findProcessIndexByPID(pid);
  if (index != -1) {
    // Free all resources held by the process
    freeAllProcessResources(index);
    // Clear the process entry
    clearProcessEntry(index);
    // Log the event
    log_message(LOG_LEVEL_INFO, 0,
                "Terminated process %d and cleared resources.", pid);
    // Decrement the count of active children
    decrementCurrentChildren();
  }
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
    return "Running ";
  case PROCESS_WAITING:
    return "Waiting";
  case PROCESS_TERMINATED:
    return "Terminated";
  default:
    return "Unknown";
  }
}

void clearProcessEntry(int index) {
  if (index >= 0 && index < maxProcesses && processTable[index].occupied) {
    processTable[index].occupied = 1;
    processTable[index].state = PROCESS_TERMINATED;
    log_message(LOG_LEVEL_INFO, 0, "Process terminated at table entry index %d",
                index);
  }
}

int killProcess(int pid, int sig) { return kill(pid, sig); }

int findProcessIndexByPID(pid_t pid) {
  for (int i = 0; i < maxProcesses; i++) {
    if (processTable[i].occupied && processTable[i].pid == pid) {
      log_message(LOG_LEVEL_DEBUG, 0,
                  "Found process table entry for PID %ld at index %d",
                  (long)pid, i);
      return i;
    }
  }
  log_message(LOG_LEVEL_WARN, 0, "No process table entry found for PID %ld",
              (long)pid);
  return -1;
}

void logProcessTable() {
  log_message(LOG_LEVEL_INFO, 0,
              "---------------- Process Table ----------------");
  log_message(
      LOG_LEVEL_INFO, 0,
      " Index | PID    | State        | Start Time   | Blocked | Block Until");

  for (int i = 0; i < maxProcesses; i++) {
    if (processTable[i].occupied ||
        processTable[i].state == PROCESS_TERMINATED) {
      char startTime[64], blockTime[64];

      // Formatting the start and block time to include leading zeros for
      // nanoseconds and ensure uniform field widths
      snprintf(startTime, sizeof(startTime), "%02d.%09d",
               processTable[i].startSeconds, processTable[i].startNano);
      snprintf(blockTime, sizeof(blockTime), "%02d.%09d",
               processTable[i].eventBlockedUntilSec,
               processTable[i].eventBlockedUntilNano);

      // Ensure alignment in the log output by adjusting the spacing in the
      // format specifier
      log_message(LOG_LEVEL_INFO, 0, "%5d  | %6d | %-12s | %s | %7s | %s", i,
                  processTable[i].pid,
                  processStateToString(processTable[i].state), startTime,
                  processTable[i].blocked ? "Yes" : "No", blockTime);
    }
  }
  log_message(LOG_LEVEL_INFO, 0,
              "------------------------------------------------");
}
