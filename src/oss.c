#include "cleanup.h"
#include "process.h"
#include "shared.h"
#include "time.h"

int isNumber(const char* str);
int parseCommandLineArguments(int argc, char* argv[]);
void sendTerminationMessages();

void incrementClockPerChild();

int main(int argc, char* argv[]) {
  srand(time(NULL));

  gProcessType = PROCESS_TYPE_OSS;

  setupSignalHandlers();
  atexit(atexitHandler);

#ifdef DEBUG

#endif

  int parseStatus = parseCommandLineArguments(argc, argv);
  if (parseStatus != 0) {
    cleanupResources();
    exit(parseStatus == 1 ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  shmId = initSharedMemory();
  simClock = attachSharedMemory(shmId);

  initializeSimulationEnvironment();

  logFile = fopen(logFileName, "w+");
  if (!logFile) {
    perror("Failed to open log file");
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

int isNumber(const char* str) {
  char* end;
  long val = strtol(str, &end, 10);
  return *end == '\0' && val > 0 && val <= INT_MAX;
}

int parseCommandLineArguments(int argc, char* argv[]) {
  int opt;
  int sFlag = 0, tFlag = 0, iFlag = 0, fFlag = 0;
  const char* usage =
      "Usage: %s [-h] [-n proc] [-s simul] [-t timelimitForChildren] "
      "[-i intervalInMsToLaunchChildren] [-f logfile]\n";

  while ((opt = getopt(argc, argv, "hn:s:t:i:f:")) != -1) {
    switch (opt) {
      case 'h':
        printf(usage, argv[0]);
        return 1;
      case 'n':
        if (isNumber(optarg)) {
          maxProcesses = atoi(optarg);
        } else {
          fprintf(stderr, "Error: Invalid number for -n option.\n");
          return 2;
        }
        break;
      case 's':
        sFlag = 1;
        if (isNumber(optarg)) {
          maxSimultaneous = atoi(optarg);
        } else {
          fprintf(stderr, "Error: Invalid number for -s option.\n");
          return 3;
        }
        break;
      case 't':
        tFlag = 1;
        if (isNumber(optarg)) {
          childTimeLimit = atoi(optarg);
        } else {
          fprintf(stderr, "Error: Invalid number for -t option.\n");
          return 4;
        }
        break;
      case 'i':
        iFlag = 1;
        if (isNumber(optarg)) {
          launchInterval = atoi(optarg);
        } else {
          fprintf(stderr, "Error: Invalid number for -i option.\n");
          return 5;
        }
        break;
      case 'f':
        fFlag = 1;
        strncpy(logFileName, optarg, sizeof(logFileName) - 1);
        logFileName[sizeof(logFileName) - 1] = '\0';
        break;
      default:
        fprintf(stderr, usage, argv[0]);
        return -1;
    }
  }

  if (!sFlag) maxSimultaneous = DEFAULT_MAX_SIMULTANEOUS;
  if (!tFlag) childTimeLimit = DEFAULT_CHILD_TIME_LIMIT;
  if (!iFlag) launchInterval = DEFAULT_LAUNCH_INTERVAL;
  if (!fFlag) {
    strncpy(logFileName, DEFAULT_LOG_FILE_NAME, sizeof(logFileName));
    logFileName[sizeof(logFileName) - 1] = '\0';
  }

  logFile = fopen(logFileName, "w+");
  if (!logFile) {
    perror("Failed to open log file");
    return 6;
  }

#ifdef DEBUG
  printf("Debug Info:\n");
  printf("\tMax Processes: %d\n", maxProcesses);
  printf("\tMax Simultaneous: %d\n", maxSimultaneous);
  printf("\tChild Time Limit: %d\n", childTimeLimit);
  printf("\tLaunch Interval: %d ms\n", launchInterval);
  printf("\tLog File: %s\n", logFileName);
#endif

  return 0;
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
  int activeChildren = 0;
  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      activeChildren++;
    }
  }

  if (activeChildren > 0) {
    float increment = 250.0f / activeChildren;
    int incrementSec = (int)increment / 1000;
    int incrementNano = ((int)increment % 1000) * 1000000;

    simClock->seconds += incrementSec;
    simClock->nanoseconds += incrementNano;

    if (simClock->nanoseconds >= 1000000000) {
      simClock->seconds++;
      simClock->nanoseconds -= 1000000000;
    }
  }
}
