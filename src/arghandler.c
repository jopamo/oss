#include "arghandler.h"
#include "globals.h"

// Helper function to validate if a string is a positive integer
int isNumber(const char *str) {
  if (str == NULL)
    return 0;
  char *end;
  long val = strtol(str, &end, 10);
  return *end == '\0' && val > 0 && val <= INT_MAX;
}

// Process the command-line arguments for OSS
int ossArgs(int argc, char *argv[]) {
  optind = 1; // Reset getopt's 'optind' for unit testing or multiple parsing
  int opt;

  while ((opt = getopt(argc, argv, "hn:s:i:f:")) != -1) {
    switch (opt) {
    case 'h':
      printOSSUsage(argv[0]);
      return 1;
    case 'n':
      if (!isNumber(optarg)) {
        log_message(LOG_LEVEL_ERROR, 0, "Invalid number of processes: %s",
                    optarg);
        return ERROR_INVALID_ARGS;
      }
      maxProcesses = atoi(optarg);
      break;
    case 's':
      if (!isNumber(optarg)) {
        log_message(LOG_LEVEL_ERROR, 0,
                    "Invalid number of simultaneous processes: %s", optarg);
        return ERROR_INVALID_ARGS;
      }
      maxSimultaneous = atoi(optarg);
      break;
    case 'i':
      if (!isNumber(optarg)) {
        log_message(LOG_LEVEL_ERROR, 0, "Invalid interval: %s", optarg);
        return ERROR_INVALID_ARGS;
      }
      launchInterval = atoi(optarg);
      break;
    case 'f':
      strncpy(logFileName, optarg, sizeof(logFileName) - 1);
      logFileName[sizeof(logFileName) - 1] = '\0';
      logFile = fopen(logFileName, "w+");
      if (!logFile) {
        log_message(LOG_LEVEL_ERROR, 0, "Failed to open log file: %s",
                    logFileName);
        return ERROR_FILE_OPEN;
      }
      break;
    default:
      printOSSUsage(argv[0]);
      return ERROR_INVALID_ARGS;
    }
  }

  log_message(LOG_LEVEL_INFO, 0,
              "OSS configured with maxProcesses: %d, maxSimultaneous: %d, "
              "launchInterval: %d, logFileName: %s",
              maxProcesses, maxSimultaneous, launchInterval, logFileName);

  return 0; // Success
}

// Print the usage information
void printOSSUsage(const char *programName) {
  log_message(LOG_LEVEL_INFO, 0,
              "Usage: %s -n <num_procs> -s <simul_procs> -i <interval_ms> -f "
              "<log_filename>",
              programName);
  log_message(LOG_LEVEL_INFO, 0, "Options:");
  log_message(LOG_LEVEL_INFO, 0,
              "  -n <num_procs>    Number of total child processes to spawn.");
  log_message(
      LOG_LEVEL_INFO, 0,
      "  -s <simul_procs>  Number of child processes to spawn simultaneously.");
  log_message(
      LOG_LEVEL_INFO, 0,
      "  -i <interval_ms>  Interval in milliseconds to launch children.");
  log_message(LOG_LEVEL_INFO, 0,
              "  -f <log_filename> Logfile name for OSS output.");
  log_message(LOG_LEVEL_INFO, 0, "  -h                Show this help message.");
}

/*
int workerArgs(int argc, char *argv[], WorkerConfig *config) {
  if (argc != 3 || !isNumber(argv[1]) || !isNumber(argv[2])) {
    return ERROR_INVALID_ARGS;
  }
  config->lifespanSeconds = atoi(argv[1]);
  config->lifespanNanoSeconds = atoi(argv[2]);
  return SUCCESS;
}
*/
