#include "process.h"
#include "shared.h"

void setupSignalHandlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = genericSignalHandler;
  sigemptyset(&sa.sa_mask);

  int signals[] = {SIGINT, SIGTERM, SIGALRM, SIGCHLD};
  for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
    if (sigaction(signals[i], &sa, NULL) == -1) {
      log_message(LOG_LEVEL_ERROR, "Error setting up handler for signal %d: %s",
                  signals[i], strerror(errno));
      exit(EXIT_FAILURE);
    }
  }
}

void genericSignalHandler(int sig) {
  switch (sig) {
  case SIGINT:
  case SIGTERM:
    log_message(LOG_LEVEL_INFO,
                "Termination signal (%d) received, initiating cleanup...", sig);
    keepRunning = 0;
    break;
  case SIGALRM:
    log_message(LOG_LEVEL_INFO, "Timer expired. Exiting...");
    keepRunning = 0;
    break;
  case SIGCHLD:
    log_message(LOG_LEVEL_INFO, "Child process terminated.");
    while (waitpid(-1, NULL, WNOHANG) > 0)
      ;
    keepRunning = 0;
    break;
  default:
    log_message(LOG_LEVEL_WARN, "Unhandled signal (%d) received", sig);
    keepRunning = 0;
    break;
  }
}
