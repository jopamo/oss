#include "arghandler.h"
#include "globals.h"

int isNumber(const char *str) {
  char *end;
  long val = strtol(str, &end, 10);
  return *end == '\0' && val > 0 && val <= INT_MAX;
}

int ossArgs(int argc, char *argv[]) {
  optind = 1; // Reset getopt's 'optind' to the start for robust unit testing
  int opt;
  int nFlag = 0, sFlag = 0, iFlag = 0, fFlag = 0;

  while ((opt = getopt(argc, argv, "hn:s:i:f:")) != -1) {
    switch (opt) {
    case 'h':
      printOSSUsage(argv[0]);
      return 1;
    case 'n':
      if (optarg && isNumber(optarg)) {
        maxProcesses = atoi(optarg);
        nFlag = 1;
      } else {
        log_message(LOG_LEVEL_ERROR,
                    "[OSS] Error: Invalid or missing number for -n option.");
        return ERROR_INVALID_ARGS;
      }
      break;
    case 's':
      if (optarg && isNumber(optarg)) {
        maxSimultaneous = atoi(optarg);
        sFlag = 1;
      } else {
        log_message(LOG_LEVEL_ERROR,
                    "[OSS] Error: Invalid or missing number for -s option.");
        return ERROR_INVALID_ARGS;
      }
      break;
    case 'i':
      if (optarg && isNumber(optarg)) {
        launchInterval = atoi(optarg);
        iFlag = 1;
      } else {
        log_message(LOG_LEVEL_ERROR,
                    "[OSS] Error: Invalid or missing number for -i option.");
        return ERROR_INVALID_ARGS;
      }
      break;
    case 'f':
      if (optarg) {
        strncpy(logFileName, optarg, sizeof(logFileName) - 1);
        logFileName[sizeof(logFileName) - 1] = '\0';
        fFlag = 1;
      } else {
        log_message(LOG_LEVEL_ERROR,
                    "[OSS] Error: Missing filename for -f option.");
        return ERROR_INVALID_ARGS;
      }
      break;
    default:
      printOSSUsage(argv[0]);
      return ERROR_INVALID_ARGS;
    }
  }

  if (!nFlag || !sFlag || !iFlag || !fFlag) {
    log_message(LOG_LEVEL_ERROR, "[OSS] Error: Missing required options.");
    return ERROR_INVALID_ARGS; // Ensure all required options are provided
  }

  logFile = fopen(logFileName, "w+");
  if (!logFile) {
    log_message(LOG_LEVEL_ERROR, "[OSS] Failed to open log file.");
    return ERROR_FILE_OPEN;
  }

  log_message(LOG_LEVEL_INFO, "[OSS] Debug Info:");
  log_message(LOG_LEVEL_INFO, "\tMax Processes: %d", maxProcesses);
  log_message(LOG_LEVEL_INFO, "\tMax Simultaneous: %d", maxSimultaneous);
  log_message(LOG_LEVEL_INFO, "\tLaunch Interval: %d ms", launchInterval);
  log_message(LOG_LEVEL_INFO, "\tLog File: %s", logFileName);

  return 0;
}

int workerArgs(int argc, char *argv[], WorkerConfig *config) {
  if (argc != 3 || !isNumber(argv[1]) || !isNumber(argv[2])) { // Improved check
    return ERROR_INVALID_ARGS;
  }
  config->lifespanSeconds = atoi(argv[1]);
  config->lifespanNanoSeconds = atoi(argv[2]);
  return SUCCESS;
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
