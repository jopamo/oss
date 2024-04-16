#include "process.h"
#include "shared.h"

pid_t timekeeperPid = 0;
pid_t tableprinterPid = 0;
pid_t parentPid;

void signalSafeLog(const char *msg) {
  size_t len = 0;
  while (msg[len] != '\0')
    len++;
  write(STDERR_FILENO, msg, len);
  write(STDERR_FILENO, "\n", 1);
}

void waitForChildProcesses(void) {
  int status;
  if (timekeeperPid != 0)
    waitpid(timekeeperPid, &status, 0);
  if (tableprinterPid != 0)
    waitpid(tableprinterPid, &status, 0);
}

void signalHandler(int sig) {
  char buffer[256];
  if (sig == SIGINT || sig == SIGTERM) {
    snprintf(buffer, sizeof(buffer),
             "Child process (PID: %d): Termination requested by signal %d.",
             getpid(), sig);
    signalSafeLog(buffer);
    keepRunning = 0;
  }
}

void setupSignalHandlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signalHandler;
  sigfillset(&sa.sa_mask);

  int signals[] = {SIGINT, SIGTERM};
  for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
    if (sigaction(signals[i], &sa, NULL) == -1) {
      signalSafeLog("Error setting up parent signal handler. Exiting.");
      _exit(EXIT_FAILURE);
    }
  }
}
