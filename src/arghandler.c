#include "arghandler.h"

int isNumber(const char *str) {
  char *end;
  long val = strtol(str, &end, 10);
  return *end == '\0' && val > 0 && val <= INT_MAX;
}

int ossArgs(int argc, char *argv[]) {
  int opt;
  int sFlag = 0, iFlag = 0, fFlag = 0;

  while ((opt = getopt(argc, argv, "hn:s:i:f:")) != -1) {
    switch (opt) {
    case 'h':
      printOSSUsage(argv[0]);
      return 1;
    case 'n':
      if (isNumber(optarg)) {
        maxProcesses = atoi(optarg);
      } else {
        log_message(LOG_LEVEL_ERROR,
                    "[OSS] Error: Invalid number for -n option.");
        return 2;
      }
      break;
    case 's':
      sFlag = 1;
      if (isNumber(optarg)) {
        maxSimultaneous = atoi(optarg);
      } else {
        log_message(LOG_LEVEL_ERROR,
                    "[OSS] Error: Invalid number for -s option.");
        return 3;
      }
      break;
    case 'i':
      iFlag = 1;
      if (isNumber(optarg)) {
        launchInterval = atoi(optarg);
      } else {
        log_message(LOG_LEVEL_ERROR,
                    "[OSS] Error: Invalid number for -i option.");
        return 4;
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
  if (!iFlag)
    launchInterval = DEFAULT_LAUNCH_INTERVAL;
  if (!fFlag) {
    strncpy(logFileName, DEFAULT_LOG_FILE_NAME, sizeof(logFileName));
    logFileName[sizeof(logFileName) - 1] = '\0';
  }

  logFile = fopen(logFileName, "w+");
  if (!logFile) {
    log_message(LOG_LEVEL_ERROR, "[OSS] Failed to open log file");
    return 5;
  }

  log_message(LOG_LEVEL_INFO, "[OSS] Debug Info:");
  log_message(LOG_LEVEL_INFO, "\tMax Processes: %d", maxProcesses);
  log_message(LOG_LEVEL_INFO, "\tMax Simultaneous: %d", maxSimultaneous);
  log_message(LOG_LEVEL_INFO, "\tLaunch Interval: %d ms", launchInterval);
  log_message(LOG_LEVEL_INFO, "\tLog File: %s", logFileName);

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
  puts("  -n <value>   Number of total child processes to spawn.");
  puts("  -s <value>   Number of child processes to spawn simultaneously.");
  puts("  -i <value>   Interval in ms to launch children.");
  puts("  -f <filename>  Logfile name for OSS output.");
  puts("  -h           Show this help message.");
  puts("\nDescription:");
  puts("  This program simulates an operating system scheduler");
  puts("  using message queues and shared memory.");
}
