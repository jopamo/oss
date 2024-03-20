#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include "init.h"
#include "ipc.h"
#include "shared.h"
#include "util.h"

extern int shmId;
extern int msqId;
extern PCB processTable[MAX_WORKERS];
extern FILE* logFile;
extern char logFileName[256];

int lastChildSent = -1;

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

int sendMessage(int msqId, Message* msg) {
    if (DEBUG) {
        printf("Sending message: Type=%ld, Text=%d, Queue ID=%d\n", msg->mtype, msg->mtext, msqId);
    }

    if (msgsnd(msqId, (void*)msg, sizeof(Message) - sizeof(long), IPC_NOWAIT) < 0) {
        perror("msgsnd failed");
        printf("Failed to send message: Type=%ld, Text=%d, Queue ID=%d, Error=%s\n", msg->mtype, msg->mtext, msqId, strerror(errno));
        return -1;
    }

    log_debug("Message sent successfully");
    return 0;
}

int receiveMessage(int msqId, Message* msg, long msgType) {
    if (DEBUG) {
        printf("Attempting to receive message: Type=%ld, Queue ID=%d\n", msgType, msqId);
    }

    if (msgrcv(msqId, (void*)msg, sizeof(Message) - sizeof(long), msgType, 0) < 0) {
        if (errno != EINTR) {  // Do not report error if interrupted by a signal
            perror("msgrcv failed");
            printf("Failed to receive message: Type=%ld, Queue ID=%d, Error=%s\n", msgType, msqId, strerror(errno));
        }
        return -1;
    }

    log_debug("Message received successfully");
    return 0;
}

void sendMessageToNextChild() {
    Message msg = { .mtype = 1, .mtext = 1 };
    int startIndex = (lastChildSent + 1) % MAX_PROCESSES;
    int found = 0;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        int index = (startIndex + i) % MAX_PROCESSES;
        if (processTable[index].occupied) {
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
        printf("Received message from child with PID %d\n", msg.mtext);
        if (msg.mtext == 0) {
            printf("Child process has decided to terminate\n");
        }
    }
}


int cleanupMessageQueue(int msqId) {
  if (msgctl(msqId, IPC_RMID, NULL) == -1) {
    perror("msgctl");
    return -1;
  }
  log_debug("Cleaned up message queue successfully");
  return 0;
}

