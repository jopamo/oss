#include "user_process.h"
#include "shared.h"

void signalSafeLog(const char *msg) {
  size_t len = 0;
  while (msg[len] != '\0')
    len++;
  write(STDERR_FILENO, msg, len);
  write(STDERR_FILENO, "\n", 1);
}

void signalHandler(int sig) {
  char buffer[256];
  if (sig == SIGINT || sig == SIGTERM) {
    snprintf(buffer, sizeof(buffer),
             "Child process (PID: %d): Termination requested by signal %d.\n",
             getpid(), sig);
    signalSafeLog(buffer);
    keepRunning = 0;
  } else if (sig == SIGCHLD) {

    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {
    }
  }
}

void setupSignalHandlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signalHandler;
  sigfillset(&sa.sa_mask);

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGALRM);

  int signals[] = {SIGINT, SIGTERM, SIGCHLD};
  for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
    if (sigaction(signals[i], &sa, NULL) == -1) {
      signalSafeLog("Error setting up signal handler. Exiting.");
      _exit(EXIT_FAILURE);
    }
  }
}
