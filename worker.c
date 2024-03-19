#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shared.h"

typedef struct {
  unsigned int lifespanSeconds;
  unsigned int lifespanNanoSeconds;
} WorkerConfig;

typedef struct {
  SystemClock *sysClock;
  int msqId;
  int shmId;
} SharedResources;

void parseCommandLineArguments(int argc, char *argv[], WorkerConfig *config);
void attachToSharedResources(SharedResources *resources);
void operateWorkerCycle(const WorkerConfig *config, SharedResources *resources);
int determineTermination(const WorkerConfig *config,
                         const SystemClock *sysClock);
void logActivity(const char *message, const char *extraInfo);
void cleanUpResources(SharedResources *resources);

int main(int argc, char *argv[]) {
  WorkerConfig config = {0};
  SharedResources resources = {NULL, -1, -1};

  parseCommandLineArguments(argc, argv, &config);
  attachToSharedResources(&resources);

  if (resources.sysClock && resources.msqId != -1) {
    operateWorkerCycle(&config, &resources);
  }

  cleanUpResources(&resources);
  return EXIT_SUCCESS;
}

void parseCommandLineArguments(int argc, char *argv[], WorkerConfig *config) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s lifespanSeconds lifespanNanoSeconds\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  char *endptr;
  config->lifespanSeconds = (unsigned int)strtoul(argv[1], &endptr, 10);
  if (*endptr != '\0') {
    fprintf(stderr, "Invalid input for lifespanSeconds\n");
    exit(EXIT_FAILURE);
  }
  config->lifespanNanoSeconds = (unsigned int)strtoul(argv[2], &endptr, 10);
  if (*endptr != '\0') {
    fprintf(stderr, "Invalid input for lifespanNanoSeconds\n");
    exit(EXIT_FAILURE);
  }
}

void attachToSharedResources(SharedResources *resources) {
  resources->shmId = initSharedMemory();
  if (resources->shmId == -1) {
    perror("initSharedMemory failed");
    exit(EXIT_FAILURE);
  }

  resources->sysClock = attachSharedMemory(resources->shmId);
  if (resources->sysClock == (void *)-1) {
    perror("attachSharedMemory failed");
    cleanupSharedMemory(resources->shmId, resources->sysClock);
    exit(EXIT_FAILURE);
  }

  resources->msqId = initMessageQueue();
  if (resources->msqId == -1) {
    perror("initMessageQueue failed");
    cleanupSharedMemory(resources->shmId, resources->sysClock);
    exit(EXIT_FAILURE);
  }
}

void operateWorkerCycle(const WorkerConfig *config,
                        SharedResources *resources) {
  Message msg = {.mtype = 1, .mtext = 1};
  while (1) {
    if (receiveMessage(resources->msqId, &msg, 1) == -1) {
      logActivity("Failed to receive message", strerror(errno));
      break;
    }

    if (determineTermination(config, resources->sysClock)) {
      logActivity("Terminating.", "");
      msg.mtext = 0;

      if (sendMessage(resources->msqId, &msg) == -1) {
        logActivity("Failed to send termination message", strerror(errno));
      }
      break;
    } else {
      logActivity("Continuing operation.", "");

      msg.mtext = 1;
      if (sendMessage(resources->msqId, &msg) == -1) {
        logActivity("Failed to send continue operation message",
                    strerror(errno));
      }
    }
  }
}

int determineTermination(const WorkerConfig *config,
                         const SystemClock *sysClock) {
  unsigned long totalLifespanNS =
      config->lifespanSeconds * 1000000000UL + config->lifespanNanoSeconds;
  unsigned long currentNS =
      sysClock->seconds * 1000000000UL + sysClock->nanoseconds;
  return currentNS >= totalLifespanNS;
}

void logActivity(const char *message, const char *extraInfo) {
  printf("Worker PID: %d - %s %s\n", getpid(), message, extraInfo);
}

void cleanUpResources(SharedResources *resources) {
  if (resources->sysClock) {
    detachSharedMemory(resources->sysClock);
  }
  if (resources->msqId != -1) {
    cleanupMessageQueue(resources->msqId);
  }
  if (resources->shmId != -1) {
    cleanupSharedMemory(resources->shmId, resources->sysClock);
  }
}
