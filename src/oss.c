#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cleanup.h"
#include "init.h"
#include "ipc.h"
#include "process.h"
#include "shared.h"
#include "time.h"

extern SystemClock* sysClock;
extern PCB processTable[MAX_WORKERS];
extern FILE* logFile;
extern char logFileName[256];
extern volatile sig_atomic_t cleanupInitiated;
extern int msqId;

int maxProcesses = 5;
int maxSimultaneous = 10;
int childTimeLimit = 10;
int launchInterval = 1000;
volatile sig_atomic_t keepRunning = 1;

void parseCommandLineArguments(int argc, char* argv[]);
void manageProcesses();
void sendTerminationMessages();

void alarmHandler(int sig) {
  sendTerminationMessages();
  keepRunning = 0;
}

int main(int argc, char* argv[]) {
  parseCommandLineArguments(argc, argv);

  setupSignalHandlers();
  atexit(atexitHandler);

  initializeSimulationEnvironment();
  logFile = fopen(logFileName, "w+");

  pid_t timeKeeperPid = fork();
  if (timeKeeperPid == 0) {
    while (keepRunning) {
      incrementClock(sysClock, 3.8);
    }
    _Exit(EXIT_SUCCESS);
  }

  while (keepRunning) {
    manageProcesses();
    usleep(500000);
  }

  waitpid(timeKeeperPid, NULL, 0);

  return EXIT_SUCCESS;
}

void parseCommandLineArguments(int argc, char* argv[]) {
  int opt;
  const char* usage =
      "Usage: %s [-h] [-n proc] [-s simul] [-t timelimitForChildren] "
      "[-i intervalInMsToLaunchChildren] [-f logfile]\n";

  while ((opt = getopt(argc, argv, "hn:s:t:i:f:")) != -1) {
    switch (opt) {
      case 'h':
        printf(usage, argv[0]);
        exit(EXIT_SUCCESS);
      case 'n':
        maxProcesses = atoi(optarg);
        break;
      case 's':
        maxSimultaneous = atoi(optarg);
        break;
      case 't':
        childTimeLimit = atoi(optarg);
        break;
      case 'i':
        launchInterval = atoi(optarg);
        break;
      case 'f':
        strncpy(logFileName, optarg, sizeof(logFileName) - 1);
        logFile = fopen(logFileName, "w+");
        if (!logFile) {
          perror("Failed to open log file");
          exit(EXIT_FAILURE);
        }
        break;
      default:
        fprintf(stderr, usage, argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  printf("Max Processes: %d\n", maxProcesses);
  printf("Max Simultaneous: %d\n", maxSimultaneous);
  printf("Child Time Limit: %d\n", childTimeLimit);
  printf("Launch Interval: %d ms\n", launchInterval);
  printf("Log File: %s\n", logFileName);
}

void manageProcesses() {
  static struct timeval lastLaunchTime = {0};
  struct timeval currentTime, diffTime;
  gettimeofday(&currentTime, NULL);

  timersub(&currentTime, &lastLaunchTime, &diffTime);
  int diffMs = diffTime.tv_sec * 1000 + diffTime.tv_usec / 1000;

  if (diffMs >= launchInterval) {
    possiblyLaunchNewChild();
    lastLaunchTime = currentTime;
  }

  static struct timeval lastLogTime = {0};
  timersub(&currentTime, &lastLogTime, &diffTime);
  if ((diffTime.tv_sec * 1000 + diffTime.tv_usec / 1000) >= 500) {
    logProcessTable();
    lastLogTime = currentTime;
  }
}

void sendTerminationMessages() {
  Message msg = {.mtype = 1, .mtext = -1};
  for (int i = 0; i < MAX_WORKERS; i++) {
    if (processTable[i].occupied) {
      sendMessage(msqId, &msg);
    }
  }
}
