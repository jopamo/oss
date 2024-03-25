#include "arghandler.h"

int isNumber(const char *str) {
  char *end;
  long val = strtol(str, &end, 10);
  return *end == '\0' && val > 0 && val <= INT_MAX;
}

int ossArgs(int argc, char *argv[]) {
  int opt;
  int sFlag = 0, tFlag = 0, iFlag = 0, fFlag = 0;

  while ((opt = getopt(argc, argv, "hn:s:t:i:f:")) != -1) {
    switch (opt) {
    case 'h':
      printOSSUsage(argv[0]);
      return 1;
    case 'n':
      if (isNumber(optarg)) {
        maxProcesses = atoi(optarg);
      } else {
        fprintf(stderr, "[OSS] Error: Invalid number for -n option.\n");
        return 2;
      }
      break;
    case 's':
      sFlag = 1;
      if (isNumber(optarg)) {
        maxSimultaneous = atoi(optarg);
      } else {
        fprintf(stderr, "[OSS] Error: Invalid number for -s option.\n");
        return 3;
      }
      break;
    case 't':
      tFlag = 1;
      if (isNumber(optarg)) {
        childTimeLimit = atoi(optarg);
      } else {
        fprintf(stderr, "[OSS] Error: Invalid number for -t option.\n");
        return 4;
      }
      break;
    case 'i':
      iFlag = 1;
      if (isNumber(optarg)) {
        launchInterval = atoi(optarg);
      } else {
        fprintf(stderr, "[OSS] Error: Invalid number for -i option.\n");
        return 5;
      }
      break;
    case 'f':
      fFlag = 1;
      strncpy(logFileName, optarg, sizeof(logFileName) - 1);
      logFileName[sizeof(logFileName) - 1] = '\0';
      break;
    default:
      printOSSUsage(argv[0]);
      return -1;
    }
  }

  if (!sFlag)
    maxSimultaneous = DEFAULT_MAX_SIMULTANEOUS;
  if (!tFlag)
    childTimeLimit = DEFAULT_CHILD_TIME_LIMIT;
  if (!iFlag)
    launchInterval = DEFAULT_LAUNCH_INTERVAL;
  if (!fFlag) {
    strncpy(logFileName, DEFAULT_LOG_FILE_NAME, sizeof(logFileName));
    logFileName[sizeof(logFileName) - 1] = '\0';
  }

  logFile = fopen(logFileName, "w+");
  if (!logFile) {
    perror("[OSS] Failed to open log file");
    return 6;
  }

#ifdef DEBUG
  printf("[OSS] Debug Info:\n");
  printf("\tMax Processes: %d\n", maxProcesses);
  printf("\tMax Simultaneous: %d\n", maxSimultaneous);
  printf("\tChild Time Limit: %d\n", childTimeLimit);
  printf("\tLaunch Interval: %d ms\n", launchInterval);
  printf("\tLog File: %s\n", logFileName);
#endif

  return 0;
}

void workerArgs(int argc, char *argv[], WorkerConfig *config) {
  if (argc != 3) {
    fprintf(stderr, "[Worker] Usage: %s <seconds> <nanoseconds>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  config->lifespanSeconds = strtoul(argv[1], NULL, 10);
  config->lifespanNanoSeconds = strtoul(argv[2], NULL, 10);
}

void printOSSUsage(const char *programName) {
  printf("Usage: %s [OPTIONS]\n", programName);
  puts("Options:");
  puts("  -n, --numproc <value>      Number of total child");
  puts("                             processes to spawn.");
  puts("  -s, --simulproc <value>    Number of child processes");
  puts("                             to spawn simultaneously.");
  puts("  -t, --timelimit <value>    Time limit for child processes.");
  puts("  -i, --interval <value>     Interval in ms to launch children.");
  puts("  -f, --logfile <filename>   Logfile name for OSS output.");
  puts("  -h, --help                 Show this help message.");
  puts("\nDescription:");
  puts("  This program simulates an operating system scheduler");
  puts("  using message queues and shared memory.");
}
