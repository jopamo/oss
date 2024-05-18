#include "arghandler.h"
#include "globals.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool isPositiveNumber(const char *str, int *outValue) {
  if (str == NULL)
    return false;
  char *end;
  long val = strtol(str, &end, 10);
  if (*end != '\0' || val <= 0 || val > INT_MAX)
    return false;
  *outValue = (int)val;
  return true;
}

void printUsage(const char *programName) {
  printf("Usage: %s [-h] [-n num_procs] [-i interval_ms] [-f log_filename] [-r "
         "num_resources] [-u instances_per_resource] [-p project_version]\n",
         programName);
  printf("Options:\n");
  printf("  -h                Show this help message.\n");
  printf("  -n num_procs      Set the number of total child processes to spawn "
         "(max: %d).\n",
         MAX_PROCESSES);
  printf("  -i interval_ms    Set the interval in milliseconds to launch "
         "children.\n");
  printf("  -f log_filename   Set the filename for OSS output logs.\n");
  printf("  -r num_resources  Set the total number of resources available "
         "(max: %d).\n",
         MAX_RESOURCES);
  printf("  -u instances_per_resource Set the maximum number of instances per "
         "resource (max: %d).\n",
         MAX_INSTANCES);
  printf(
      "  -p project_version Set the project version to run (3, 4, 5, or 6).\n");
}

bool isNumber(const char *str) {
  if (str == NULL || *str == '\0')
    return false;
  char *end;
  strtol(str, &end, 10);
  return *end == '\0';
}

int workerArgs(int argc, char *argv[], WorkerConfig *config) {
  if (argc != 3 || !isNumber(argv[1]) || !isNumber(argv[2])) {
    return -1;
  }
  config->lifespanSeconds = atoi(argv[1]);
  config->lifespanNanoSeconds = atoi(argv[2]);
  return 0;
}

int psmgmtArgs(int argc, char *argv[], int *projectVersion) {
  optind = 1; // Reset getopt's 'optind'
  int opt;
  int tempValue;
  int optionCount = 0;

  while ((opt = getopt(argc, argv, "hn:i:f:r:u:p:")) != -1) {
    optionCount++;
    switch (opt) {
    case 'h':
      printUsage(argv[0]);
      return 1;
    case 'n':
      if (!isPositiveNumber(optarg, &tempValue) || tempValue > MAX_PROCESSES) {
        fprintf(stderr, "Invalid or too many processes specified: %s\n",
                optarg);
        return -1;
      }
      maxProcesses = tempValue;
      break;
    case 'i':
      if (!isPositiveNumber(optarg, &tempValue)) {
        fprintf(stderr, "Invalid launch interval specified: %s\n", optarg);
        return -1;
      }
      launchInterval = tempValue;
      break;
    case 'f':
      strncpy(logFileName, optarg, sizeof(logFileName) - 1);
      logFileName[sizeof(logFileName) - 1] = '\0';
      logFile = fopen(logFileName, "w+");
      if (!logFile) {
        fprintf(stderr, "Failed to open log file: %s\n", logFileName);
        return -1;
      }
      break;
    case 'r':
      if (!isPositiveNumber(optarg, &tempValue) || tempValue > MAX_RESOURCES) {
        fprintf(stderr, "Invalid or too many resources specified: %s\n",
                optarg);
        return -1;
      }
      maxResources = tempValue;
      break;
    case 'u':
      if (!isPositiveNumber(optarg, &tempValue) || tempValue > MAX_INSTANCES) {
        fprintf(stderr,
                "Invalid or too many instances per resource specified: %s\n",
                optarg);
        return -1;
      }
      maxInstances = tempValue;
      break;
    case 'p':
      if (!isPositiveNumber(optarg, &tempValue) || tempValue < 3 ||
          tempValue > 6) {
        fprintf(stderr, "Invalid project version specified: %s\n", optarg);
        return -1;
      }
      *projectVersion = tempValue;
      break;
    default:
      printUsage(argv[0]);
      return -1;
    }
  }

  // If no options were provided, set the defaults
  if (optionCount == 0) {
    maxProcesses = DEFAULT_MAX_PROCESSES;
    launchInterval = DEFAULT_LAUNCH_INTERVAL;
    maxResources = DEFAULT_MAX_RESOURCES;
    maxInstances = DEFAULT_MAX_INSTANCES;
    *projectVersion = 3; // Default to project version 3
    strncpy(logFileName, DEFAULT_LOG_FILE_NAME, sizeof(logFileName) - 1);
    logFileName[sizeof(logFileName) - 1] = '\0';
    logFile = fopen(logFileName, "w+");
    if (!logFile) {
      fprintf(stderr, "Failed to open default log file: %s\n", logFileName);
      return -1;
    }
  }

  return 0;
}
