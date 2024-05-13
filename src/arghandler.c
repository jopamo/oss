#include "arghandler.h"
#include "globals.h"

int isPositiveNumber(const char *str, int *outValue) {
  if (str == NULL)
    return 0;
  char *end;
  long val = strtol(str, &end, 10);
  if (*end != '\0' || val <= 0 || val > INT_MAX)
    return 0;
  *outValue = (int)val;
  return 1;
}

int psmgmtArgs(int argc, char *argv[]) {
  optind = 1; // Reset getopt's 'optind'
  int opt;
  int tempValue;

  while ((opt = getopt(argc, argv, "hn:s:i:f:r:u:")) != -1) {
    switch (opt) {
    case 'h':
      printUsage(argv[0]);
      return 1;
    case 'n':
      if (!isPositiveNumber(optarg, &tempValue) || tempValue > MAX_PROCESSES) {
        fprintf(stderr, "Invalid or too many processes specified: %s\n",
                optarg);
        return ERROR_INVALID_ARGS;
      }
      maxProcesses = tempValue;
      break;
    case 's':
      if (!isPositiveNumber(optarg, &tempValue) ||
          tempValue > MAX_SIMULTANEOUS) {
        fprintf(stderr,
                "Invalid or too many simultaneous processes specified: %s\n",
                optarg);
        return ERROR_INVALID_ARGS;
      }
      maxSimultaneous = tempValue;
      break;
    case 'i':
      if (!isPositiveNumber(optarg, &tempValue)) {
        fprintf(stderr, "Invalid launch interval specified: %s\n", optarg);
        return ERROR_INVALID_ARGS;
      }
      launchInterval = tempValue;
      break;
    case 'f':
      strncpy(logFileName, optarg, sizeof(logFileName) - 1);
      logFileName[sizeof(logFileName) - 1] = '\0';
      logFile = fopen(logFileName, "w+");
      if (!logFile) {
        fprintf(stderr, "Failed to open log file: %s\n", logFileName);
        return ERROR_FILE_OPEN;
      }
      break;
    case 'r':
      if (!isPositiveNumber(optarg, &tempValue) || tempValue > MAX_RESOURCES) {
        fprintf(stderr, "Invalid or too many resources specified: %s\n",
                optarg);
        return ERROR_INVALID_ARGS;
      }
      maxResources = tempValue;
      break;
    case 'u':
      if (!isPositiveNumber(optarg, &tempValue) || tempValue > MAX_INSTANCES) {
        fprintf(stderr,
                "Invalid or too many instances per resource specified: %s\n",
                optarg);
        return ERROR_INVALID_ARGS;
      }
      maxInstances = tempValue;
      break;
    default:
      printUsage(argv[0]);
      return ERROR_INVALID_ARGS;
    }
  }
  return 0;
}

void printUsage(const char *programName) {
  printf("Usage: %s [-h] [-n num_procs] [-s simul_procs] [-i interval_ms] [-f "
         "log_filename] [-r num_resources] [-u instances_per_resource]\n",
         programName);
  printf("Options:\n");
  printf("  -h                Show this help message.\n");
  printf("  -n num_procs      Set the number of total child processes to spawn "
         "(max: %d).\n",
         MAX_PROCESSES);
  printf("  -s simul_procs    Set the number of child processes to spawn "
         "simultaneously (max: %d).\n",
         MAX_SIMULTANEOUS);
  printf("  -i interval_ms    Set the interval in milliseconds to launch "
         "children.\n");
  printf("  -f log_filename   Set the filename for OSS output logs.\n");
  printf("  -r num_resources  Set the total number of resources available "
         "(max: %d).\n",
         MAX_RESOURCES);
  printf("  -u instances_per_resource Set the maximum number of instances per "
         "resource (max: %d).\n",
         MAX_INSTANCES);
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
