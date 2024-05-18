#include "signals.h"

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

  for (int i = 0; i < maxProcesses; i++) {
    if (processTable[i].occupied && processTable[i].state == PROCESS_RUNNING) {
      kill(processTable[i].pid, SIGTERM); // Send SIGTERM to each running child
      log_message(LOG_LEVEL_INFO, 0, "Sent SIGTERM to PID: %d.",
                  processTable[i].pid);
    }
  }

  cleanupAndExit(1);
}
