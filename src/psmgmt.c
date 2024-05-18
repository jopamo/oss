#include "psmgmt.h"
#include "arghandler.h"
#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "process.h"
#include "queue.h"
#include "resource.h"
#include "shared.h"
#include "signals.h"
#include "timeutils.h"
#include "user_process.h"

void initializeAll(void) {
  setupParentSignalHandlers();
  semUnlinkCreate();
  initializeSharedResources();

  if (initializeProcessTable() == -1 || initializeResourceTable() == -1) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to initialize all tables");
    cleanupAndExit(EXIT_FAILURE);
  }

  if (initMLFQ(&mlfq, maxProcesses) == -1) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to initialize MLFQ.");
    cleanupAndExit(EXIT_FAILURE);
  }
}

int main(int argc, char *argv[]) {
  struct sigaction sa;
  sa.sa_handler = handleSigInt;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);

  int projectVersion = 0;
  if (psmgmtArgs(argc, argv, &projectVersion) != 0) {
    cleanupAndExit(EXIT_FAILURE);
  }

  initializeAll();
  initializeTimeTracking();
  setupTimeout(MAX_RUNTIME);

  switch (projectVersion) {
  case 3:
    runPsmgmtA3();
    break;
  default:
    fprintf(stderr, "Unsupported project version: %d\n", projectVersion);
    return EXIT_FAILURE;
  }

  atexit(cleanupResources);
  cleanupAndExit(EXIT_SUCCESS);
}
