#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "arghandler.h"
#include "cleanup.h"
#include "process.h"
#include "shared.h"

pid_t timekeeperPid = -1;

void initializeSimulationEnvironment(void);
void launchWorkerProcesses(void);
void manageSimulation(void);
void communicateWithWorkersRoundRobin(void);
int findFreeProcessTableEntry(void);
void checkWorkerStatuses(void);
int findProcessIndexByPID(pid_t pid);
void logProcessTable(void);
void updateProcessTableOnFork(int index, pid_t pid);
void updateProcessTableOnTerminate(pid_t pid);

void signalHandler(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    keepRunning = 0;
    if (timekeeperPid > 0) {
      kill(timekeeperPid, SIGTERM);
    }
  }
}

void setupSignalHandling(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signalHandler;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
}

void initializeSimulationEnvironment(void) {
  initializeSharedMemorySegments();

  timekeeperPid = fork();
  if (timekeeperPid == 0) {
    execl("./timekeeper", "timekeeper", (char *)NULL);
    log_message(LOG_LEVEL_ERROR, "Failed to start timekeeper: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  } else if (timekeeperPid < 0) {
    log_message(LOG_LEVEL_ERROR, "Fork failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  msqId = initMessageQueue();
  if (msqId < 0) {
    log_message(LOG_LEVEL_ERROR, "Failed to initialize message queue.");
    exit(EXIT_FAILURE);
  }

  logFile = fopen(logFileName, "w+");
  if (!logFile) {
    log_message(LOG_LEVEL_ERROR, "Failed to open log file %s", logFileName);
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char *argv[]) {
  setupSignalHandlers();
  initializeSemaphore(clockSemName);

  ossArgs(argc, argv);

  atexit(atexitHandler);
  setupTimeout(60);

  initializeSimulationEnvironment();
  manageSimulation();

  if (timekeeperPid > 0) {
    kill(timekeeperPid, SIGTERM);
    waitpid(timekeeperPid, NULL, 0);
  }

  cleanupAndExit();
  return EXIT_SUCCESS;
}

void manageSimulation(void) {
  log_message(LOG_LEVEL_INFO, "Simulation management started.");

  while (keepRunning) {
    sem_wait(clockSem);
    if (simClock->seconds >= 60) {
      sem_post(clockSem);
      break;
    }
    sem_post(clockSem);

    communicateWithWorkersRoundRobin();
    checkWorkerStatuses();

    launchWorkerProcesses();

    logProcessTable();

    usleep(100000);
  }

  log_message(LOG_LEVEL_INFO, "Simulation management ended.");
}

void launchWorkerProcesses(void) {
  struct timespec launchDelay = {0, launchInterval * 1000000};

  while (keepRunning && getCurrentChildren() < maxSimultaneous) {
    int index = findFreeProcessTableEntry();
    if (index == -1) {
      log_message(LOG_LEVEL_INFO, "Waiting for a free process table entry...");
      sleep(1);
      continue;
    }

    unsigned int lifespanSec = (rand() % childTimeLimit) + 1;
    unsigned int lifespanNSec = rand() % 1000000000;

    pid_t pid = fork();
    if (pid == 0) {
      char secStr[32], nsecStr[32];
      snprintf(secStr, sizeof(secStr), "%u", lifespanSec);
      snprintf(nsecStr, sizeof(nsecStr), "%u", lifespanNSec);
      execl("./worker", "worker", secStr, nsecStr, (char *)NULL);
      log_message(LOG_LEVEL_ERROR, "execl failed: %s", strerror(errno));
      _exit(EXIT_FAILURE);
    } else if (pid > 0) {
      updateProcessTableOnFork(index, pid);
      log_message(LOG_LEVEL_INFO,
                  "Launched worker PID: %d, Lifespan: %u.%u seconds", pid,
                  lifespanSec, lifespanNSec);
      nanosleep(&launchDelay, NULL);
    } else {
      log_message(LOG_LEVEL_ERROR, "Fork failed: %s", strerror(errno));
    }
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
                    "Message sent to worker PID %d to proceed.", pcb->pid);
      }

      lastIndex = (index + 1) % DEFAULT_MAX_PROCESSES;
      break;
    }
  }

  if (!foundActiveWorker) {
    log_message(LOG_LEVEL_DEBUG,
                "No active workers to communicate with at this cycle.");
  }
}

void checkWorkerStatuses(void) {
  Message msg;

  while (msgrcv(msqId, &msg, sizeof(msg) - sizeof(long), 0, IPC_NOWAIT) != -1) {
    int index = findProcessIndexByPID(msg.mtype);

    if (index != -1) {
      if (msg.mtext == 0) {
        updateProcessTableOnTerminate(msg.mtype);
        log_message(LOG_LEVEL_INFO, "Worker PID %ld has terminated", msg.mtype);
      } else {
        log_message(LOG_LEVEL_DEBUG,
                    "Received alive message from worker PID %ld", msg.mtype);
      }
    } else {
      log_message(LOG_LEVEL_WARN, "Received message from unknown PID %ld",
                  msg.mtype);
    }
  }
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

    log_message(LOG_LEVEL_INFO,
                "Process table entry at index %d cleared for PID %d", index,
                pid);
  } else {
    log_message(LOG_LEVEL_WARN,
                "No process table entry found for PID %d to terminate", pid);
  }
}

void logProcessTable(void) {

  unsigned long simSec, simNano;
  sem_wait(clockSem);
  simSec = simClock->seconds;
  simNano = simClock->nanoseconds;
  sem_post(clockSem);

  char tableOutput[4096];
  sprintf(tableOutput, "OSS PID:%d SysClockS: %lu SysclockNano: %lu\n",
          getpid(), simSec, simNano);
  strcat(tableOutput, "Process Table:\n");
  strcat(tableOutput, "Entry Occupied PID StartS StartN\n");

  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    PCB *pcb = &processTable[i];
    char row[256];
    if (pcb->occupied) {
      sprintf(row, "%d 1 %d %u %u\n", i, pcb->pid, pcb->startSeconds,
              pcb->startNano);

    } else {
      sprintf(row, "%d 0 0 0 0\n", i);
    }
    strcat(tableOutput, row);
  }

  log_message(LOG_LEVEL_INFO, "%s", tableOutput);
}
