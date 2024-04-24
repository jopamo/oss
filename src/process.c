#include "process.h"
#include "cleanup.h"
#include "shared.h"
#include "user_process.h"

pid_t timekeeperPid = 0;
pid_t tableprinterPid = 0;
pid_t parentPid;

void waitForChildProcesses(void) {
  int status;
  if (timekeeperPid != 0) {
    waitpid(timekeeperPid, &status, 0);
    timekeeperPid = 0;
  }
  if (tableprinterPid != 0) {
    waitpid(tableprinterPid, &status, 0);
    tableprinterPid = 0;
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
    signalSafeLog(buffer);
    keepRunning = 0;
    sendSignalToChildGroups(SIGTERM);
    break;
  case SIGALRM:
    snprintf(buffer, sizeof(buffer), "OSS (PID: %d): Timer expired.\n",
             getpid());
    signalSafeLog(buffer);
    keepRunning = 0;
    sendSignalToChildGroups(SIGTERM);
    break;
  case SIGCHLD:
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    snprintf(buffer, sizeof(buffer),
             "OSS (PID: %d): Child process terminated.\n", getpid());
    signalSafeLog(buffer);
    sendSignalToChildGroups(SIGTERM);
    break;
  default:
    snprintf(buffer, sizeof(buffer),
             "OSS (PID: %d): Unhandled signal %d received.\n", getpid(), sig);
    signalSafeLog(buffer);
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
      signalSafeLog("Error setting up parent signal handler. Exiting.");
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

        log_message(LOG_LEVEL_INFO, "Child process PID: %d terminated.", pid);

        memset(&processTable[i], 0, sizeof(processTable[i]));

        break;
      }
    }
  }
}

void atexitHandler(void) { cleanupResources(); }

void sendSignalToChildGroups(int sig) {
  if (timekeeperPid > 0) {
    kill(-timekeeperPid, sig);
  }
  if (tableprinterPid > 0) {
    kill(-tableprinterPid, sig);
  }
}

void timeoutHandler(int signum) {
  (void)signum;
  log_message(LOG_LEVEL_INFO, "Timeout signal received.");
  keepRunning = 0;
  cleanupAndExit();
}

void setupTimeout(int seconds) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = timeoutHandler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGALRM, &sa, NULL);

  alarm(seconds);
  log_message(LOG_LEVEL_INFO, "Timeout setup for %d seconds.", seconds);
}
