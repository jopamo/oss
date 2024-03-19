#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

#include "shared.h"

void log_debug(const char* message) {
  if (DEBUG) {
    fprintf(stderr, "DEBUG: %s\n", message);
  }
}

int initSharedMemory() {
  int shmId = shmget(SHM_KEY, sizeof(SystemClock), IPC_CREAT | 0666);
  if (shmId < 0) {
    perror("shmget");
    return -1;
  }
  log_debug("Shared memory initialized successfully");
  return shmId;
}

SystemClock* attachSharedMemory(int shmId) {
  SystemClock* shmPtr = (SystemClock*)shmat(shmId, NULL, 0);
  if (shmPtr == (SystemClock*)-1) {
    perror("shmat");
    return NULL;
  }
  log_debug("Attached shared memory successfully");
  return shmPtr;
}

int detachSharedMemory(SystemClock* shmPtr) {
  if (shmdt(shmPtr) == -1) {
    perror("shmdt");
    return -1;
  }
  log_debug("Detached shared memory successfully");
  return 0;
}

int cleanupSharedMemory(int shmId, SystemClock* shmPtr) {
  if (detachSharedMemory(shmPtr) < 0) return -1;
  if (shmctl(shmId, IPC_RMID, NULL) == -1) {
    perror("shmctl");
    return -1;
  }
  log_debug("Cleaned up shared memory successfully");
  return 0;
}

int initMessageQueue() {
  int msqId = msgget(MSG_KEY, IPC_CREAT | 0666);
  if (msqId < 0) {
    perror("msgget");
    return -1;
  }
  log_debug("Message queue initialized successfully");
  return msqId;
}

int sendMessage(int msqId, Message* msg) {
  if (DEBUG) {
    printf("Sending message: Type=%ld, Text=%d, Queue ID=%d\n", msg->mtype,
           msg->mtext, msqId);
  }

  if (msgsnd(msqId, (void*)msg, sizeof(Message) - sizeof(long), IPC_NOWAIT) <
      0) {
    perror("msgsnd failed");
    printf("Failed to send message: Type=%ld, Text=%d, Queue ID=%d, Error=%s\n",
           msg->mtype, msg->mtext, msqId, strerror(errno));
    return -1;
  }

  log_debug("Message sent successfully");
  return 0;
}

int receiveMessage(int msqId, Message* msg, long msgType) {
  if (DEBUG) {
    printf("Attempting to receive message: Type=%ld, Queue ID=%d\n", msgType,
           msqId);
  }

  if (msgrcv(msqId, (void*)msg, sizeof(Message) - sizeof(long), msgType, 0) <
      0) {
    perror("msgrcv failed");
    printf("Failed to receive message: Type=%ld, Queue ID=%d, Error=%s\n",
           msgType, msqId, strerror(errno));
    return -1;
  }

  log_debug("Message received successfully");
  return 0;
}

int cleanupMessageQueue(int msqId) {
  if (msgctl(msqId, IPC_RMID, NULL) == -1) {
    perror("msgctl");
    return -1;
  }
  log_debug("Cleaned up message queue successfully");
  return 0;
}

void incrementClock(SystemClock* clock, unsigned int increment) {
  unsigned int nsIncrement = increment % 1000000000;
  unsigned int secIncrement = increment / 1000000000;

  clock->nanoseconds += nsIncrement;
  if (clock->nanoseconds >= 1000000000) {
    clock->nanoseconds -= 1000000000;
    clock->seconds += 1;
  }
  clock->seconds += secIncrement;

  log_debug("Clock incremented successfully");
}
