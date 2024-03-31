#include <time.h>

#include "arghandler.h"
#include "cleanup.h"
#include "shared.h"

#define INT_MAX_STR_SIZE (sizeof(char) * ((CHAR_BIT * sizeof(int) - 1) / 3 + 2))

#include <sys/types.h>

void initializeSimulationEnvironment(void);
void launchWorkerProcesses(void);
void launchWorkerProcessesIfNeeded(struct timespec *lastLaunchTime,
                                   struct timespec *currentTime);
void manageSimulation(void);
void communicateWithWorkersRoundRobin(void);
void signalHandler(int sig);

int findFreeProcessTableEntry(void);
void checkWorkerStatuses(void);
int findProcessIndexByPID(pid_t pid);

void incrementClockPerChild(void);

void logProcessTable(void);

void updateProcessTableOnFork(int index, pid_t pid);
void updateProcessTableOnTerminate(int index);

int main(int argc, char *argv[]) {
  gProcessType = PROCESS_TYPE_OSS;

  ossArgs(argc, argv);

  setupSignalHandlers();
  atexit(atexitHandler);
  initializeSimulationEnvironment();

  manageSimulation();

  cleanupAndExit();
  return EXIT_SUCCESS;
}

void initializeSimulationEnvironment(void) {

  msqId = initMessageQueue();
  if (msqId < 0) {
    log_message(LOG_LEVEL_ERROR, "[OSS] Failed to initialize message queue.");
    exit(EXIT_FAILURE);
  }

  shmId = initSharedMemory(sizeof(SimulatedClock));
  if (shmId < 0) {
    log_message(LOG_LEVEL_ERROR, "[OSS] Failed to initialize shared memory.");
    exit(EXIT_FAILURE);
  }

  simClock = attachSharedMemory(sizeof(SimulatedClock));

  if (!simClock) {
    log_message(LOG_LEVEL_ERROR, "[OSS] Failed to attach shared memory.");
    exit(EXIT_FAILURE);
  }

  logFile = fopen(logFileName, "w+");
  if (!logFile) {
    log_message(LOG_LEVEL_ERROR, "[OSS] Failed to open log file %s",
                logFileName);
    exit(EXIT_FAILURE);
  }
}

void launchWorkerProcesses(void) {
  struct timespec launchDelay = {0, launchInterval * 1000000};

  while (keepRunning && getCurrentChildren() < maxSimultaneous) {
    int index = findFreeProcessTableEntry();
    if (index != -1) {
      unsigned int lifespanSec = (rand() % childTimeLimit) + 1;
      unsigned int lifespanNSec = rand() % 1000000000;

      pid_t pid = fork();
      if (pid == 0) {
        char secStr[32], nsecStr[32];
        snprintf(secStr, sizeof(secStr), "%u", lifespanSec);
        snprintf(nsecStr, sizeof(nsecStr), "%u", lifespanNSec);
        execl("./worker", "worker", secStr, nsecStr, (char *)NULL);

        log_message(LOG_LEVEL_ERROR, "[Worker] execl failed: %s",
                    strerror(errno));
        _exit(EXIT_FAILURE);
      } else if (pid > 0) {
        updateProcessTableOnFork(index, pid);
        log_message(LOG_LEVEL_INFO,
                    "[OSS] Launched worker PID: %d, Lifespan: %u.%u seconds",
                    pid, lifespanSec, lifespanNSec);
        nanosleep(&launchDelay, NULL);
      } else {
        log_message(LOG_LEVEL_ERROR, "[OSS] Fork failed: %s", strerror(errno));
        break;
      }
    } else {
      log_message(LOG_LEVEL_INFO,
                  "[OSS] Waiting for a free process table entry...");
      sleep(1);
    }
  }
}

void manageSimulation(void) {
  struct timespec lastLaunchTime;
  clock_gettime(CLOCK_MONOTONIC, &lastLaunchTime);
  log_message(LOG_LEVEL_INFO, "Simulation management started.");

  while (keepRunning) {
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime);

    launchWorkerProcessesIfNeeded(&lastLaunchTime, &currentTime);
    communicateWithWorkersRoundRobin();
    checkWorkerStatuses();
    incrementClockPerChild();
    logProcessTable();

    usleep(100000);
  }

  log_message(LOG_LEVEL_INFO, "Simulation management ended.");
}

void launchWorkerProcessesIfNeeded(struct timespec *lastLaunchTime,
                                   struct timespec *currentTime) {
  long timeDiff = (currentTime->tv_sec - lastLaunchTime->tv_sec) * 1000 +
                  (currentTime->tv_nsec - lastLaunchTime->tv_nsec) / 1000000;

  if (timeDiff >= launchInterval) {
    log_message(LOG_LEVEL_DEBUG, "Launching worker processes as needed.");
    launchWorkerProcesses();
    *lastLaunchTime = *currentTime;
  }
}

void communicateWithWorkersRoundRobin(void) {
  static int lastIndex = 0;
  int foundActiveWorker = 0;

  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    int index = (lastIndex + i) % DEFAULT_MAX_PROCESSES;
    PCB *pcb = &processTable[index];

    if (pcb->occupied) {
      foundActiveWorker = 1;
      Message msg = {.mtype = pcb->pid, .mtext = 1};

      if (sendMessage(msqId, &msg) == -1) {
        log_message(LOG_LEVEL_ERROR, "Failed to communicate with worker PID %d",
                    pcb->pid);
      } else {
        log_message(LOG_LEVEL_DEBUG,
                    "[OSS] Message sent to worker PID %d to proceed.",
                    pcb->pid);
      }

      lastIndex = (index + 1) % DEFAULT_MAX_PROCESSES;
      break;
    }
  }

  if (!foundActiveWorker) {
    log_message(LOG_LEVEL_DEBUG,
                "[OSS] No active workers to communicate with at this cycle.");
  }
}

void checkWorkerStatuses(void) {
  Message msg;

  while (msgrcv(msqId, &msg, sizeof(msg) - sizeof(long), 0, IPC_NOWAIT) != -1) {
    int index = findProcessIndexByPID(msg.mtype);

    if (index != -1) {
      if (msg.mtext == 0) {
        updateProcessTableOnTerminate(index);
        log_message(LOG_LEVEL_INFO, "[OSS] Worker PID %ld has terminated",
                    msg.mtype);
      } else {
        log_message(LOG_LEVEL_DEBUG,
                    "[OSS] Received alive message from worker PID %ld",
                    msg.mtype);
      }
    } else {
      log_message(LOG_LEVEL_ERROR,
                  "[OSS ERROR] Received message from unknown PID %ld",
                  msg.mtype);
    }
  }
}

void incrementClockPerChild(void) {
  int activeChildren = getCurrentChildren();

  if (activeChildren == 0) {
    log_message(LOG_LEVEL_DEBUG, "No active children to increment clock.");
    return;
  }

  long incrementNano = (250000000L / activeChildren);

  simClock->nanoseconds += incrementNano;

  while (simClock->nanoseconds >= 1000000000) {
    simClock->seconds++;
    simClock->nanoseconds -= 1000000000;
  }

  log_message(LOG_LEVEL_DEBUG,
              "Clock incremented: %lu seconds, %lu nanoseconds",
              simClock->seconds, simClock->nanoseconds);
}

int findFreeProcessTableEntry(void) {
  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (!processTable[i].occupied) {
      log_message(LOG_LEVEL_DEBUG, "Found free process table entry at index %d",
                  i);
      return i;
    }
  }
  log_message(LOG_LEVEL_WARN, "No free process table entries found.");
  return -1;
}

int findProcessIndexByPID(pid_t pid) {
  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (processTable[i].occupied && processTable[i].pid == pid) {
      log_message(LOG_LEVEL_DEBUG,
                  "Found process table entry for PID %ld at index %d",
                  (long)pid, i);
      return i;
    }
  }
  log_message(LOG_LEVEL_WARN, "No process table entry found for PID %ld",
              (long)pid);
  return -1;
}

void updateProcessTableOnFork(int index, pid_t pid) {
  if (index >= 0 && index < DEFAULT_MAX_PROCESSES) {
    PCB *pcb = &processTable[index];
    pcb->occupied = 1;
    pcb->pid = pid;
    pcb->startSeconds = simClock->seconds;
    pcb->startNano = simClock->nanoseconds;

    log_message(
        LOG_LEVEL_DEBUG,
        "Process table updated at index %d with PID %d, start time %lu.%09lu",
        index, pid, pcb->startSeconds, pcb->startNano);
  } else {
    log_message(LOG_LEVEL_ERROR, "Invalid index %d for process table update",
                index);
  }
}

void updateProcessTableOnTerminate(pid_t pid) {
  int index = findProcessIndexByPID(pid);
  if (index != -1) {
    PCB *pcb = &processTable[index];
    pcb->occupied = 0;
    pcb->pid = -1;
    pcb->startSeconds = 0;
    pcb->startNano = 0;

    log_message(LOG_LEVEL_DEBUG,
                "Process table entry at index %d cleared for PID %d", index,
                pid);
  } else {
    log_message(LOG_LEVEL_ERROR, "No process table entry found for PID %d",
                pid);
  }
}

void logProcessTable(void) {
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
