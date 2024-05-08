#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "arghandler.h"
#include "cleanup.h"
#include "globals.h"
#include "process.h"
#include "resource.h"
#include "shared.h"
#include "user_process.h"

void initializeSimulationEnvironment(void);
void manageSimulation(void);
unsigned long randRange(unsigned long min, unsigned long max);
void launchWorkerProcesses(void);
void scheduleNextProcess(void);
int isEmptyQueue(Queue *queue);
void enqueue(Queue *queue, pid_t pid);
pid_t dequeue(Queue *queue);
void handleTermination(pid_t pid);
void moveToBlockedQueue(pid_t pid);
void checkMessages(void);
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

  logFile = fopen(logFileName, "w+");
  if (!logFile) {
    perror("Failed to open log file");
    exit(EXIT_FAILURE);
  }

  initializeSimulationEnvironment();
  manageSimulation();

  cleanupAndExit();
  waitForChildProcesses();
  fclose(logFile);
  return EXIT_SUCCESS;
}

void initializeSimulationEnvironment(void) {
  MLFQ mlfq;
  initializeQueues(&mlfq);
  initializeResourceTable();

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

  unsigned long lastDeadlockCheck =
      0; // to keep track of time since last deadlock check

  while (keepRunning &&
         (totalLaunched < MAX_PROCESSES || getCurrentChildren() > 0)) {
    if (better_sem_wait(clockSem) == -1) {
      log_message(LOG_LEVEL_ERROR, "Semaphore lock failed: %s",
                  strerror(errno));
      if (errno != EINTR)
        break;
      continue;
    }

    // Check for deadlocks every simulated second
    if (simClock->seconds > lastDeadlockCheck) {
      lastDeadlockCheck = simClock->seconds;
      if (checkForDeadlocks()) {
        resolveDeadlocks();
      }
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

    // Handle scheduling and process management
    scheduleNextProcess();
    launchWorkerProcesses();
    checkMessages();

    usleep(100000); // Reduce CPU usage
  }

  log_message(LOG_LEVEL_INFO, "Simulation management ended. Cleaning up.");
  cleanupAndExit();
}

void launchWorkerProcesses(void) {
  while (keepRunning && getCurrentChildren() < maxSimultaneous &&
         totalLaunched < MAX_PROCESSES) {
    int index = findFreeProcessTableEntry();
    if (index == -1) {
      log_message(LOG_LEVEL_INFO, "No free process table entry available.");
      sleep(1); // Prevent tight looping.
      continue;
    }

    pid_t pid = fork();
    if (pid > 0) { // Parent process
      updateProcessTableOnFork(index, pid);
      setCurrentChildren(getCurrentChildren() + 1);
      totalLaunched++;
      log_message(LOG_LEVEL_INFO, "Launched worker PID: %d", pid);
    } else if (pid == 0) {               // Child process
      execl("./worker", "worker", NULL); // Execute worker program
      perror("execl failed");
      exit(EXIT_FAILURE);
    } else {
      perror("Fork failed");
      continue;
    }
  }
}

void checkMessages(void) {
  Message msg;
  while (receiveMessage(msqId, &msg, 0, IPC_NOWAIT) != -1) {
    switch (msg.mtext) {
    case MESSAGE_TYPE_REQUEST:
      if (requestResource(msg.resourceType, msg.amount, msg.mtype) == 0) {
        log_resource_state("REQUEST", msg.mtype, msg.resourceType, msg.amount,
                           getAvailable(msg.resourceType),
                           getAvailableAfter(msg.resourceType));
      }
      break;
    case MESSAGE_TYPE_RELEASE:
      releaseResource(msg.resourceType, msg.amount, msg.mtype);
      log_resource_state("RELEASE", msg.mtype, msg.resourceType, msg.amount,
                         getAvailable(msg.resourceType),
                         getAvailableAfter(msg.resourceType));
      break;
    case MESSAGE_TYPE_TERMINATE:
      terminateProcess(msg.mtype);
      break;
    default:
      log_message(LOG_LEVEL_WARN, "Received unknown message type: %d",
                  msg.mtext);
      break;
    }
  }
}

void scheduleNextProcess(void) {
  pid_t selectedProcessPid = -1;
  Queue *selectedQueue = NULL;
  int selectedQueueLevel = -1;

  if (!isEmptyQueue(&mlfq.highPriority)) {
    selectedQueue = &mlfq.highPriority;
    selectedQueueLevel = 0;
  } else if (!isEmptyQueue(&mlfq.midPriority)) {
    selectedQueue = &mlfq.midPriority;
    selectedQueueLevel = 1;
  } else if (!isEmptyQueue(&mlfq.lowPriority)) {
    selectedQueue = &mlfq.lowPriority;
    selectedQueueLevel = 2;
  }

  if (selectedQueue) {
    selectedProcessPid = dequeue(selectedQueue);

    int timeQuantum = TIME_QUANTUM_BASE * (1 << selectedQueueLevel);

    Message msg;
    msg.mtype = selectedProcessPid;
    msg.mtext = timeQuantum;

    if (sendMessage(msqId, &msg) != SUCCESS) {
      log_message(LOG_LEVEL_ERROR,
                  "Failed to send scheduling message to PID %d.",
                  selectedProcessPid);
      return;
    }

    Message responseMsg;
    if (receiveMessage(msqId, &responseMsg, selectedProcessPid, 0) == SUCCESS) {
      if (responseMsg.mtext < 0) {
        handleTermination(selectedProcessPid);
      } else if (responseMsg.mtext == 0) {
        moveToBlockedQueue(selectedProcessPid);
      } else {
        Queue *targetQueue = (responseMsg.mtext >= timeQuantum &&
                              selectedQueueLevel < QUEUE_COUNT - 1)
                                 ? (selectedQueueLevel == 0 ? &mlfq.midPriority
                                                            : &mlfq.lowPriority)
                                 : selectedQueue;
        enqueue(targetQueue, selectedProcessPid);
      }
    } else {
      log_message(LOG_LEVEL_ERROR, "Failed to receive message from PID %d.",
                  selectedProcessPid);
    }
  } else {
    log_message(LOG_LEVEL_INFO, "No processes are ready to be scheduled.");
  }
}

int isEmptyQueue(Queue *queue) { return queue->front == queue->rear; }

void enqueue(Queue *queue, pid_t pid) {
  queue->queue[queue->rear] = pid;
  queue->rear = (queue->rear + 1) % MAX_PROCESSES;
}

pid_t dequeue(Queue *queue) {
  if (isEmptyQueue(queue)) {
    return -1;
  }
  pid_t pid = queue->queue[queue->front];
  queue->front = (queue->front + 1) % MAX_PROCESSES;
  return pid;
}

void handleTermination(pid_t pid) {
  for (int i = 0; i < maxProcesses; i++) {
    if (processTable[i].pid == pid && processTable[i].occupied) {
      processTable[i].occupied = 0;
      processTable[i].pid = -1; // Reset PID to an invalid value
      setCurrentChildren(getCurrentChildren() - 1); // Decrement children count
      log_message(LOG_LEVEL_INFO,
                  "Process PID: %d has terminated and cleared from table.",
                  pid);
      break;
    }
  }
}

void moveToBlockedQueue(pid_t pid) {
  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (processTable[i].pid == pid) {
      log_message(LOG_LEVEL_INFO, "Process PID: %d is now blocked.", pid);
      processTable[i].blocked = 1;

      break;
    }
  }
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
