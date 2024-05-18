#include "worker.h"
#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "shared.h"
#include "signals.h"
#include "timeutils.h"
#include "user_process.h"

int main(int argc, char *argv[]) {
  gProcessType = PROCESS_TYPE_WORKER;
  setupSignalHandlers();
  initializeSharedResources();

  long lifespanSeconds = DEFAULT_LIFESPAN_SECONDS;
  long lifespanNanoSeconds = DEFAULT_LIFESPAN_NANOSECONDS;

  if (argc == 3) {
    lifespanSeconds = atol(argv[1]);
    lifespanNanoSeconds = atol(argv[2]);
  }

  return runWorkerA3(lifespanSeconds, lifespanNanoSeconds);
}
