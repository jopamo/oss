#include "process.h"
#include "shared.h"

pid_t parentPid;

void signalSafeLog(const char *msg) {
  size_t len = 0;
  while (msg[len] != '\0')
    len++;
  write(STDERR_FILENO, msg, len);
  write(STDERR_FILENO, "\n", 1);
}

void genericSignalHandler(int sig) {
  if (getpid() == parentPid) {
    switch (sig) {
    case SIGINT:
    case SIGTERM:
      signalSafeLog("Shutdown requested by parent.");
      keepRunning = 0;
      break;
    case SIGALRM:
      signalSafeLog("Timer expired in parent.");
      keepRunning = 0;
      break;
    case SIGCHLD:
      while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
      childTerminated = 1;
      break;
    default:
      signalSafeLog("Unhandled signal received by parent.");
      keepRunning = 0;
      break;
    }
  } else {

    if (sig == SIGINT || sig == SIGTERM) {
      signalSafeLog("Child process termination requested.");
      keepRunning = 0;
    }
  }
}

void setupSignalHandlers(pid_t pid) {
  parentPid = pid;

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = genericSignalHandler;
  sigemptyset(&sa.sa_mask);

  sigaddset(&sa.sa_mask, SIGINT);
  sigaddset(&sa.sa_mask, SIGTERM);
  sigaddset(&sa.sa_mask, SIGALRM);
  sigaddset(&sa.sa_mask, SIGCHLD);

  int signals[] = {SIGINT, SIGTERM, SIGALRM, SIGCHLD};
  for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
    if (sigaction(signals[i], &sa, NULL) == -1) {
      signalSafeLog("Error setting up signal handler. Exiting.");
      _exit(EXIT_FAILURE);
    }
  }
}
