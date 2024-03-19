#ifndef SHARED_H
#define SHARED_H

#ifndef DEBUG
#define DEBUG 1
#endif

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

#define SHM_KEY 0x1234
#define MSG_KEY 0x5678
#define MAX_PROCESSES 20

typedef struct {
  unsigned int seconds;
  unsigned int nanoseconds;
} SystemClock;

typedef struct {
  long mtype;
  int mtext;
} Message;

typedef struct PCB {
  int occupied;
  pid_t pid;
  int startSeconds;
  int startNano;
} PCB;

int initSharedMemory();
SystemClock* attachSharedMemory(int shmId);
int initMessageQueue();
int sendMessage(int msqId, Message* msg);
int receiveMessage(int msqId, Message* msg, long msgType);
int detachSharedMemory(SystemClock* shmPtr);
int cleanupMessageQueue(int msqId);
int cleanupSharedMemory(int shmId, SystemClock* shmPtr);
#endif
