#include "arghandler.h"
#include "cleanup.h"
#include "process.h"
#include "shared.h"
#include "time.h"

void sendTerminationMessages();
void incrementClockPerChild();

int main(int argc, char* argv[]) {
  srand(time(NULL));

  gProcessType = PROCESS_TYPE_OSS;

  setupSignalHandlers();
  atexit(atexitHandler);

#ifdef DEBUG

#endif

  int parseStatus = ossArgs(argc, argv);
  if (parseStatus != 0) {
    cleanupResources();
    exit(parseStatus == 1 ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  msqId = initMessageQueue();
  shmId = initSharedMemory();
  simClock = attachSharedMemory(shmId);

  initializeSimulationEnvironment();

  logFile = fopen(logFileName, "w+");
  if (!logFile) {
    perror("[OSS] Failed to open log file");
    exit(EXIT_FAILURE);
  }

  while (keepRunning) {
    manageProcesses();
    incrementClockPerChild();
    usleep(500000);
  }

  fclose(logFile);
  cleanupResources();
  return EXIT_SUCCESS;
}

void sendTerminationMessages() {
  Message msg = {.mtype = 1, .mtext = -1};
  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      sendMessage(&msg);
      processTable[i].occupied = 0;
    }
  }
}

void incrementClockPerChild() {
  static struct timeval lastUpdateTime = {0};
  if (lastUpdateTime.tv_sec == 0 && lastUpdateTime.tv_usec == 0) {
    gettimeofday(&lastUpdateTime, NULL);
  }

  struct timeval currentTime, elapsedTime;
  gettimeofday(&currentTime, NULL);
  timersub(&currentTime, &lastUpdateTime, &elapsedTime);

  int activeChildren = 0;
  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      activeChildren++;
    }
  }

  const double timeMultiplier = 14.0;

  double totalIncrementSec = elapsedTime.tv_sec * timeMultiplier +
                             elapsedTime.tv_usec / 1000000.0 * timeMultiplier;
  if (activeChildren > 0) {
    totalIncrementSec *= (250.0 / 1000.0) / activeChildren;
  }

  long incrementSec = (long)totalIncrementSec;
  long incrementNano = (long)((totalIncrementSec - incrementSec) * 1000000000);

  simClock->seconds += incrementSec;
  simClock->nanoseconds += incrementNano;

  if (simClock->nanoseconds >= 1000000000) {
    simClock->seconds++;
    simClock->nanoseconds -= 1000000000;
  }

  lastUpdateTime = currentTime;
}
