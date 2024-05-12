#include "process.h"
#include "cleanup.h"
#include "globals.h"
#include "shared.h"
#include "user_process.h"

pid_t parentPid;

void waitForChildProcesses(void) {
  int status;
  while ((timekeeperPid > 0 && waitpid(timekeeperPid, &status, 0) > 0) ||
         (tableprinterPid > 0 && waitpid(tableprinterPid, &status, 0) > 0)) {
  }
}

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
    sendSignalToChildGroups(SIGTERM);
    break;
  case SIGALRM:
    snprintf(buffer, sizeof(buffer), "OSS (PID: %d): Timer expired.\n",
             getpid());
    signalSafeLog(LOG_LEVEL_INFO, buffer);
    keepRunning = 0;
    sendSignalToChildGroups(SIGTERM);
    break;
  case SIGCHLD:
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    snprintf(buffer, sizeof(buffer),
             "OSS (PID: %d): Child process terminated.\n", getpid());
    signalSafeLog(LOG_LEVEL_INFO, buffer);
    sendSignalToChildGroups(SIGTERM);
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

    for (int i = 0; i < MAX_PROCESSES; i++) {
      if (processTable[i].occupied && processTable[i].pid == pid) {

        log_message(LOG_LEVEL_INFO, 0, "Child process PID: %d terminated.",
                    pid);

        memset(&processTable[i], 0, sizeof(processTable[i]));

        break;
      }
    }
  }
}

void atexitHandler(void) { cleanupResources(); }

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
    log_message(LOG_LEVEL_INFO, 0,
                "Registered child process with PID %d at index %d", pid, index);
  } else {
    log_message(
        LOG_LEVEL_ERROR, 0,
        "No free entries to register process PID %d. Terminating process.",
        pid);
    kill(pid, SIGTERM); // Gracefully terminate the child process
  }
}

void terminateProcess(pid_t pid) { kill(pid, SIGTERM); }

int findFreeProcessTableEntry(void) {
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

int stillChildrenToLaunch() {
  return totalLaunched < MAX_PROCESSES && currentChildren < maxSimultaneous;
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

void sendSignalToChildGroups(int sig) {
  if (timekeeperPid > 0) {
    kill(-timekeeperPid, sig);
  }
  if (tableprinterPid > 0) {
    kill(-tableprinterPid, sig);
  }
}

void handleTermination(pid_t pid) {
  int index = findProcessIndexByPID(pid);
  if (index != -1) {
    freeAllProcessResources(
        index);               // Assume function frees resources in `resource.c`
    clearProcessEntry(index); // Cleans up the process table entry
    log_message(LOG_LEVEL_INFO, 0,
                "Terminated process %d and cleared resources.", pid);
    decrementCurrentChildren(); // Decrement the count of current children
  }
}

void freeAllProcessResources(int index) {
  releaseAllResourcesForProcess(
      processTable[index].pid); // Releases all resources held by the process
}

void updateResourceAndProcessTables() {
  logResourceTable(); // Logs current state of resources
  logProcessTable();  // Logs current state of the process table
}

// Logs the current state of the resource table
void logResourceTable() {
  for (int i = 0; i < MAX_RESOURCES; i++) {
    log_message(LOG_LEVEL_DEBUG, 0, "Resource %d: Available %d", i,
                resourceTable[i].available);
  }
}

// Logs the current state of the process table
void logProcessTable() {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      log_message(LOG_LEVEL_DEBUG, 0, "Process %d: PID %ld, State %s", i,
                  processTable[i].pid,
                  processStateToString(processTable[i].state));
    }
  }
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

// Clears a process entry in the process table
void clearProcessEntry(int index) {
  processTable[index].occupied = 0;
  processTable[index].pid = 0;
  processTable[index].state = PROCESS_TERMINATED;
}

/**
 * Attempts to terminate a process or process group with the given process ID.
 * @param pid The process ID or process group ID of the target. Negative PID for
 * process groups.
 * @param sig The signal to send.
 */
int killProcess(int pid, int sig) {
  if (kill(pid, sig) == -1) {
    perror("Error killing process or process group");
    return -1;
  }
  return SUCCESS;
}

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
