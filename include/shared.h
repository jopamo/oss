#ifndef SHARED_H
#define SHARED_H

#include <stdio.h>
#include <sys/types.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#define SHM_KEY 0x1234
#define MSG_KEY 0x5678

#define MAX_WORKERS 10
#define MAX_PROCESSES 25

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
  unsigned int startSeconds;
  unsigned int startNano;
} PCB;

int getCurrentChildren();
void setCurrentChildren(int value);
int initSharedMemory();

#endif
