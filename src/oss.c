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
  shmId = initSharedMemory();
  simClock = attachSharedMemory();

  assert(msqId >= 0);
  assert(simClock != NULL);

  logFile = fopen(logFileName, "w+");
  if (!logFile) {
    perror("[OSS] Failed to open log file");
    exit(EXIT_FAILURE);
  }
}

void launchWorkerProcesses(void) {
  int activeChildren = 0;
  struct timespec launchDelay = {0, launchInterval * 1000000};

  while (keepRunning && activeChildren < maxSimultaneous) {
    if (findFreeProcessTableEntry() != -1) {
      unsigned int lifespanSec = (rand() % childTimeLimit) + 1;
      unsigned int lifespanNSec = rand() % 1000000000;

      int index = findFreeProcessTableEntry();
      if (index != -1) {
        pid_t pid = fork();
        if (pid == 0) {
          char secStr[32], nsecStr[32];
          snprintf(secStr, sizeof(secStr), "%u", lifespanSec);
          snprintf(nsecStr, sizeof(nsecStr), "%u", lifespanNSec);
          execl("./worker", "worker", secStr, nsecStr, (char *)NULL);
          perror("execl failed");
          exit(EXIT_FAILURE);
        } else if (pid > 0) {
          updateProcessTableOnFork(index, pid);
          activeChildren++;
          printf("[OSS] Launched worker PID: %d, Lifespan: %u.%u seconds\n",
                 pid, lifespanSec, lifespanNSec);

          nanosleep(&launchDelay, NULL);
        } else {
          perror("fork failed");
          break;
        }
      }
    } else {
      printf("[OSS] Waiting for free process table entry...\n");
      sleep(1);
    }
  }
}

void manageSimulation(void) {
  struct timespec lastLaunchTime;
  clock_gettime(CLOCK_MONOTONIC, &lastLaunchTime);

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
}

void launchWorkerProcessesIfNeeded(struct timespec *lastLaunchTime,
                                   struct timespec *currentTime) {
  long timeDiff = (currentTime->tv_sec - lastLaunchTime->tv_sec) * 1000 +
                  (currentTime->tv_nsec - lastLaunchTime->tv_nsec) / 1000000;

  if (timeDiff >= launchInterval) {
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
        log_error("Failed to communicate with worker PID %d", pcb->pid);
      } else {
        log_debug("[OSS] Message sent to worker PID %d to proceed.", pcb->pid);
      }

      lastIndex = (index + 1) % DEFAULT_MAX_PROCESSES;
      break;
    }
  }

  if (!foundActiveWorker) {
    log_debug("[OSS] No active workers to communicate with at this cycle.");
  }
}

void checkWorkerStatuses(void) {
  Message msg;

  while (msgrcv(msqId, &msg, sizeof(msg) - sizeof(long), 0, IPC_NOWAIT) != -1) {
    int index = findProcessIndexByPID(msg.mtype);

    printf("[OSS] Received message from Worker PID: %ld\n", msg.mtype);

    if (index != -1) {

      if (msg.mtext == 0) {
        updateProcessTableOnTerminate(index);
        printf("[OSS] Worker PID %ld has terminated\n", msg.mtype);
      } else {
      }
    } else {
      printf("[OSS ERROR] Received message from unknown PID %ld\n", msg.mtype);
    }
  }
}

void incrementClockPerChild(void) {
  int activeChildren = getCurrentChildren();

  if (activeChildren == 0)
    return;

  long incrementNano = (250000000L / activeChildren);

  simClock->nanoseconds += incrementNano;

  while (simClock->nanoseconds >= 1000000000) {
    simClock->seconds++;
    simClock->nanoseconds -= 1000000000;
  }
}

int findFreeProcessTableEntry(void) {
  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (!processTable[i].occupied) {
      return i;
    }
  }
  return -1;
}

int findProcessIndexByPID(pid_t pid) {
  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (processTable[i].occupied && processTable[i].pid == pid) {
      return i;
    }
  }
  log_debug("No process table entry found for PID %ld", (long)pid);
  return -1;
}

void updateProcessTableOnFork(int index, pid_t pid) {
  if (index >= 0 && index < DEFAULT_MAX_PROCESSES) {
    PCB *pcb = &processTable[index];
    pcb->occupied = 1;
    pcb->pid = pid;
    pcb->startSeconds = simClock->seconds;
    pcb->startNano = simClock->nanoseconds;

    log_debug("Process table updated at index %d with PID %d", index, pid);
  } else {
    log_error("Invalid index %d for process table update", index);
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

    log_debug("Process table entry at index %d cleared for PID %d", index, pid);
  } else {
    log_error("No process table entry found for PID %d", pid);
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
