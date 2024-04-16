#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "arghandler.h"
#include "cleanup.h"
#include "process.h"
#include "shared.h"

void initializeSimulationEnvironment(void);
void launchWorkerProcesses(void);
void manageSimulation(void);
void communicateWithWorkersRoundRobin(void);
int findFreeProcessTableEntry(void);
int findProcessIndexByPID(pid_t pid);
void updateProcessTableOnFork(int index, pid_t pid);
void updateProcessTableOnTerminate(pid_t pid);

int main(int argc, char *argv[]) {
  gProcessType = PROCESS_TYPE_OSS;
  parentPid = getpid();

  setupParentSignalHandlers();
  setupTimeout(MAX_RUNTIME);

  semUnlinkCreate();
  initializeSharedResources();

  ossArgs(argc, argv);

  atexit(atexitHandler);

  initializeSimulationEnvironment();
  manageSimulation();

  cleanupAndExit();
  waitForChildProcesses();
  return EXIT_SUCCESS;
}

void initializeSimulationEnvironment(void) {
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

  log_message(LOG_LEVEL_DEBUG, "[%d] Simulated Clock shmId: %d", getpid(),
              simulatedTimeShmId);
  log_message(LOG_LEVEL_DEBUG, "[%d] Actual Time shmId: %d", getpid(),
              actualTimeShmId);

  timekeeperPid = forkAndExecute("./timekeeper");
  tableprinterPid = forkAndExecute("./tableprinter");
}

void manageSimulation(void) {
  gProcessType = PROCESS_TYPE_OSS;
  log_message(LOG_LEVEL_INFO, "Simulation management started.");

  while (keepRunning) {
    if (sem_wait(clockSem) == -1) {
      log_message(LOG_LEVEL_ERROR, "Failed to lock semaphore: %s",
                  strerror(errno));
      if (errno != EINTR)
        break;
      continue;
    }

    if (simClock && simClock->seconds >= MAX_RUNTIME) {
      sem_post(clockSem);

      if (timekeeperPid > 0) {
        kill(timekeeperPid, SIGTERM);
        log_message(LOG_LEVEL_DEBUG, "Sent SIGTERM to Timekeeper PID: %d",
                    timekeeperPid);
      }
      if (tableprinterPid > 0) {
        kill(tableprinterPid, SIGTERM);
        log_message(LOG_LEVEL_DEBUG, "Sent SIGTERM to TablePrinter PID: %d",
                    tableprinterPid);
      }

      break;
    }

    sem_post(clockSem);

    communicateWithWorkersRoundRobin();
    launchWorkerProcesses();

    usleep(100000);
  }

  log_message(LOG_LEVEL_INFO, "Simulation management ended. Cleaning up.");

  waitForChildProcesses();
}

void checkWorkerStatuses(void) {
  Message msg;

  while (msgrcv(msqId, &msg, sizeof(msg) - sizeof(long), 0, IPC_NOWAIT) != -1) {
    int index = findProcessIndexByPID(msg.mtype);

    if (index != -1) {
      if (msg.mtext == 0) {
        log_message(LOG_LEVEL_INFO, "Worker PID %ld terminating", msg.mtype);
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

void launchWorkerProcesses(void) {
  struct timespec launchDelay = {0, launchInterval * 1000000};

  while (keepRunning && getCurrentChildren() < maxSimultaneous) {
    int index = findFreeProcessTableEntry();
    if (index == -1) {

      log_message(LOG_LEVEL_INFO, "No free process table entry available.");
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

  if (processTable == NULL) {
    log_message(LOG_LEVEL_ERROR, "processTable is not initialized");
    return;
  }

  for (int i = 0; i < maxProcesses; i++) {
    int index = (lastIndex + i) % maxProcesses;
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

      lastIndex = (index + 1) % maxProcesses;
      break;
    }
  }

  if (!foundActiveWorker) {
    log_message(LOG_LEVEL_DEBUG,
                "No active workers to communicate with at this cycle.");
  }
}

int findFreeProcessTableEntry(void) {
  for (int i = 0; i < maxProcesses; i++) {
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
  for (int i = 0; i < maxProcesses; i++) {
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
  if (index >= 0 && index < maxProcesses) {
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
