#include "process.h"
#include "shared.h"

void launchWorkerProcess(int index, unsigned int lifespanSec,
                         unsigned int lifespanNSec) {
  if (index < 0 || index >= DEFAULT_MAX_PROCESSES) {
    fprintf(stderr, "launchWorkerProcess: Index out of bounds.\n");
    return;
  }

  char lifespanSecStr[20];
  char lifespanNSecStr[20];

  snprintf(lifespanSecStr, sizeof(lifespanSecStr), "%u", lifespanSec);
  snprintf(lifespanNSecStr, sizeof(lifespanNSecStr), "%u", lifespanNSec);

  pid_t pid = fork();
  if (pid < 0) {
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    execl("./worker", "worker", lifespanSecStr, lifespanNSecStr, (char *)NULL);
    perror("execl failed to run worker");
    exit(EXIT_FAILURE);
  } else {
    ElapsedTime elapsed = elapsedTimeSince(&startTime);
    processTable[index].occupied = 1;
    processTable[index].pid = pid;
    processTable[index].startSeconds = elapsed.seconds;
    processTable[index].startNano = elapsed.nanoseconds;

    printf(
        "Launched worker process with PID %d at index %d, start time: %ld sec, "
        "%ld ns\n",
        pid, index, elapsed.seconds, elapsed.nanoseconds);
  }
}

int findFreeProcessTableEntry() {
  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (!processTable[i].occupied) {
      return i;
    }
  }
  return -1;
}

void updateProcessTableOnFork(int index, pid_t pid) {
  if (index >= 0 && index < DEFAULT_MAX_PROCESSES) {
    processTable[index].occupied = 1;
    processTable[index].pid = pid;

    processTable[index].startSeconds = simClock->seconds;
    processTable[index].startNano = simClock->nanoseconds;

    printf("Process table updated at index %d with PID %d\n", index, pid);
  } else {
    fprintf(stderr, "Invalid index %d for process table.\n", index);
  }
}

void updateProcessTableOnTerminate(int index) {
  if (index >= 0 && index < DEFAULT_MAX_PROCESSES) {
    processTable[index].occupied = 0;
    processTable[index].pid = -1;
    processTable[index].startSeconds = 0;
    processTable[index].startNano = 0;

    printf("Process table entry at index %d marked as free.\n", index);
  } else {
    fprintf(stderr, "Invalid index %d for process table.\n", index);
  }
}

void possiblyLaunchNewChild() {
  if (currentChildren < maxSimultaneous) {
    int index = findFreeProcessTableEntry();
    if (index != -1) {
      unsigned int lifespanSec = rand() % 10 + 1;
      unsigned int lifespanNSec = rand() % 1000000000;

      launchWorkerProcess(index, lifespanSec, lifespanNSec);
      currentChildren++;
    } else {
      printf("No free process table entries available.\n");
    }
  } else {
    printf("Maximum number of child processes already running.\n");
  }
}

void logProcessTable() {
  struct timeval now, elapsed;
  gettimeofday(&now, NULL);
  timersub(&now, &startTime, &elapsed);

  if (!logFile) {
    fprintf(stderr,
            "Error: logFile is null. Attempting to reopen the log file.\n");
    logFile = fopen(logFileName, "a");
    if (!logFile) {
      perror("Failed to reopen log file");
      return;
    }
  }

  fprintf(logFile, "\nCurrent Actual Time: %ld.%06ld seconds\n", elapsed.tv_sec,
          elapsed.tv_usec);
  fprintf(logFile, "Current Simulated Time: %lu.%09lu seconds\n",
          simClock->seconds, simClock->nanoseconds);
  fprintf(logFile, "Process Table:\n");
  fprintf(logFile, "Index | Occupied | PID | Start Time (s.ns)\n");

  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      fprintf(logFile, "%5d | %8d | %3d | %10u.%09u\n", i,
              processTable[i].occupied, processTable[i].pid,
              processTable[i].startSeconds, processTable[i].startNano);
    } else {
      fprintf(logFile, "%5d | %8d | --- | -----------\n", i,
              processTable[i].occupied);
    }
  }

  fflush(logFile);
}

void manageProcesses() {
  static struct timeval lastLaunchTime = {0}, lastLogTime = {0};
  struct timeval currentTime, diffTime;
  gettimeofday(&currentTime, NULL);

  timersub(&currentTime, &lastLaunchTime, &diffTime);
  int diffMs = diffTime.tv_sec * 1000 + diffTime.tv_usec / 1000;

  if (diffMs >= launchInterval && getCurrentChildren() < maxSimultaneous) {
#ifdef DEBUG
    printf(
        "DEBUG: Attempting to launch a new child process. Current children: "
        "%d, Max simultaneous: %d\n",
        getCurrentChildren(), maxSimultaneous);
#endif

    possiblyLaunchNewChild();
    lastLaunchTime = currentTime;

#ifdef DEBUG
    assert(getCurrentChildren() <= maxSimultaneous);
    printf(
        "DEBUG: New child process launched successfully. Total children now: "
        "%d\n",
        getCurrentChildren());
#endif
  }

  receiveMessageFromChild();

  timersub(&currentTime, &lastLogTime, &diffTime);
  if ((diffTime.tv_sec * 1000 + diffTime.tv_usec / 1000) >= 500) {
#ifdef DEBUG
    printf("DEBUG: Logging process table.\n");
#endif

    logProcessTable();
    lastLogTime = currentTime;
  }
}
