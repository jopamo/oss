#include "process.h"
#include "shared.h"

void signalSafeLog(const char *msg) {
  write(STDERR_FILENO, msg, strlen(msg));
  write(STDERR_FILENO, "\n", 1);
}

void genericSignalHandler(int sig) {
  switch (sig) {
  case SIGINT:
  case SIGTERM:
    signalSafeLog("Shutdown requested.");
    keepRunning = 0;
    break;
  case SIGALRM:
    signalSafeLog("Timer expired.");
    keepRunning = 0;
    break;
  case SIGCHLD:
    signalSafeLog("Child process terminated.");
    childTerminated = 1;

    break;
  default:
    signalSafeLog("Unhandled signal received.");
    keepRunning = 0;
    break;
  }
}

void setupSignalHandlers(void) {
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
      signalSafeLog("Error setting up signal handler.");
      _exit(EXIT_FAILURE);
    }
  }
}
