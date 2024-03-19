#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "shared.h"

void parseCommandLineArguments(int argc, char* argv[]);
void initializeSimulationEnvironment();
void launchWorkerProcess(int index);
void incrementClock(SystemClock* clock, unsigned int increment);
int findFreeProcessTableEntry();
void updateProcessTableOnFork(int index, pid_t pid);
void updateProcessTableOnTerminate(int index);
void sendMessageToNextChild();
void receiveMessageFromChild();
void possiblyLaunchNewChild();
void logProcessTable();
void cleanupAndExit(int signum);

#define MAX_WORKERS 20

int shmId;
SystemClock* sysClock = NULL;
int msqId;

PCB processTable[MAX_WORKERS];

int lastChildSent = -1;
int currentChildren;

int maxProcesses = 5;
int maxSimultaneous = 3;
int childTimeLimit = 10;
int launchInterval = 100;
char logFileName[256] = "oss.log";

void cleanupWrapper(void) { cleanupAndExit(0); }

int main(int argc, char* argv[]) {
  parseCommandLineArguments(argc, argv);
  atexit(cleanupWrapper);
  signal(SIGINT, cleanupAndExit);

  initializeSimulationEnvironment();

  while (true) {
    incrementClock(sysClock, 1000);
    possiblyLaunchNewChild();
    sendMessageToNextChild();
    receiveMessageFromChild();
    logProcessTable();
  }

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
        break;
      default:
        fprintf(stderr, usage, argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  printf("Max Processes: %d\n", maxProcesses);
  printf("Max Simultaneous: %d\n", maxSimultaneous);
  printf("Child Time Limit: %d\n", childTimeLimit);
  printf("Launch Interval: %d\n", launchInterval);
  printf("Log File: %s\n", logFileName);
}

void initializeSimulationEnvironment() {
  shmId = initSharedMemory();
  if (shmId == -1) {
    fprintf(stderr, "Failed to initialize shared memory.\n");
    exit(EXIT_FAILURE);
  }
  sysClock = attachSharedMemory(shmId);
  if (sysClock == (void*)-1) {
    fprintf(stderr, "Failed to attach shared memory.\n");
    exit(EXIT_FAILURE);
  }

  sysClock->seconds = 0;
  sysClock->nanoseconds = 0;

  msqId = initMessageQueue();
  if (msqId == -1) {
    fprintf(stderr, "Failed to initialize message queue.\n");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < MAX_PROCESSES; i++) {
    processTable[i].occupied = 0;
    processTable[i].pid = -1;
    processTable[i].startSeconds = 0;
    processTable[i].startNano = 0;
  }

  printf("Shared memory initialized (ID: %d).\n", shmId);
  printf("Message queue initialized (ID: %d).\n", msqId);
  printf("Process table initialized.\n");
}

void launchWorkerProcess(int index) {
  if (index < 0 || index >= MAX_PROCESSES) {
    fprintf(stderr, "launchWorkerProcess: Index out of bounds.\n");
    return;
  }

  pid_t pid = fork();

  if (pid < 0) {
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    char shmIdStr[10];
    char msqIdStr[10];

    snprintf(shmIdStr, sizeof(shmIdStr), "%d", shmId);
    snprintf(msqIdStr, sizeof(msqIdStr), "%d", msqId);

    execl("./worker", "worker", shmIdStr, msqIdStr, (char*)NULL);

    perror("execl failed to run worker");
    exit(EXIT_FAILURE);
  } else {
    processTable[index].occupied = 1;
    processTable[index].pid = pid;

    printf("Launched worker process with PID %d at index %d\n", pid, index);
  }
}

int findFreeProcessTableEntry() {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (!processTable[i].occupied) {
      return i;
    }
  }

  return -1;
}

void updateProcessTableOnFork(int index, pid_t pid) {
  if (index >= 0 && index < MAX_PROCESSES) {
    processTable[index].occupied = 1;
    processTable[index].pid = pid;

    processTable[index].startSeconds = sysClock->seconds;
    processTable[index].startNano = sysClock->nanoseconds;

    printf("Process table updated at index %d with PID %d\n", index, pid);
  } else {
    fprintf(stderr, "Invalid index %d for process table.\n", index);
  }
}

void updateProcessTableOnTerminate(int index) {
  if (index >= 0 && index < MAX_PROCESSES) {
    processTable[index].occupied = 0;
    processTable[index].pid = -1;
    processTable[index].startSeconds = 0;
    processTable[index].startNano = 0;

    printf("Process table entry at index %d marked as free.\n", index);
  } else {
    fprintf(stderr, "Invalid index %d for process table.\n", index);
  }
}

void sendMessageToNextChild() {
  Message msg;
  msg.mtype = 1;
  int startIndex = (lastChildSent + 1) % MAX_PROCESSES;
  int found = 0;

  for (int i = 0; i < MAX_PROCESSES; i++) {
    int index = (startIndex + i) % MAX_PROCESSES;
    if (processTable[index].occupied) {
      msg.mtext = 1;
      sendMessage(msqId, &msg);
      printf("Message sent to child with PID %d\n", processTable[index].pid);
      lastChildSent = index;
      found = 1;
      break;
    }
  }
  if (!found) {
    printf("No active child processes to send a message to.\n");
  }
}

void receiveMessageFromChild() {
  Message msg;

  if (receiveMessage(msqId, &msg, 2) != -1) {
    if (msg.mtext == 0) {
      printf("Received termination message from a child process.\n");

    } else {
      printf("Received message from child: %d\n", msg.mtext);
    }
  } else {
    perror("Failed to receive message");
  }
}

void possiblyLaunchNewChild() {
  if (currentChildren < maxSimultaneous) {
    int index = findFreeProcessTableEntry();
    if (index != -1) {
      launchWorkerProcess(index);
      currentChildren++;
    } else {
      printf("No free process table entries available.\n");
    }
  } else {
    printf("Maximum number of child processes already running.\n");
  }
}

void logProcessTable() {
  printf("Process Table:\n");
  printf("Index | Occupied | PID | Start Time (s.ns)\n");
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      printf("%5d | %8d | %3d | %10d.%d\n", i, processTable[i].occupied,
             processTable[i].pid, processTable[i].startSeconds,
             processTable[i].startNano);
    } else {
      printf("%5d | %8d | --- | -----------\n", i, processTable[i].occupied);
    }
  }
}

void cleanupAndExit(int signum) {
  printf("Signal %d received, cleaning up and exiting...\n", signum);

  if (sysClock != NULL) {
    if (shmdt(sysClock) == -1) {
      perror("shmdt failed");
    }
    sysClock = NULL;
  }

  if (shmId != -1) {
    if (shmctl(shmId, IPC_RMID, NULL) == -1) {
      perror("shmctl IPC_RMID failed");
    }
    shmId = -1;
  }

  if (msqId != -1) {
    if (msgctl(msqId, IPC_RMID, NULL) == -1) {
      perror("msgctl IPC_RMID failed");
    }
    msqId = -1;
  }

  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      if (kill(processTable[i].pid, SIGTERM) == -1) {
        perror("kill failed");
      }

      waitpid(processTable[i].pid, NULL, 0);
      processTable[i].occupied = 0;
    }
  }

  exit(signum);
}
