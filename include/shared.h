#ifndef SHARED_H
#define SHARED_H

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MSG_PATH "/tmp"
#define MSG_PROJ_ID 'a'

#define SHM_PATH "/tmp"
#define SHM_PROJ_ID 'b'

#define MAX_RUNTIME 60
#define DEFAULT_MAX_PROCESSES 5
#define DEFAULT_MAX_SIMULTANEOUS 10
#define DEFAULT_CHILD_TIME_LIMIT 10
#define DEFAULT_LAUNCH_INTERVAL 1000
#define DEFAULT_LOG_FILE_NAME "oss.log"

typedef struct {
  unsigned long seconds;
  unsigned long nanoseconds;
} ElapsedTime;

typedef struct {
  unsigned long seconds;
  unsigned long nanoseconds;
} SimulatedClock;

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

typedef enum { PROCESS_TYPE_OSS, PROCESS_TYPE_WORKER } ProcessType;

extern ProcessType gProcessType;

extern struct timeval startTime;
extern struct timeval lastLogTime;
extern SimulatedClock *simClock;
extern PCB processTable[DEFAULT_MAX_PROCESSES];
extern FILE *logFile;
extern char logFileName[256];
extern volatile sig_atomic_t keepRunning;
extern int msqId;
extern int shmId;

extern int maxProcesses;
extern int maxSimultaneous;
extern int childTimeLimit;
extern int launchInterval;
extern int currentChildren;

void log_debug(const char *format, ...);
void log_error(const char *format, ...);

int initSharedMemory(void);
key_t getSharedMemoryKey(void);
SimulatedClock *attachSharedMemory(void);
int initMessageQueue(void);
int getCurrentChildren(void);
void sendMessageToNextChild(void);

int detachSharedMemory(void);

int sendMessage(int msqId, Message *msg);
int receiveMessage(int msqId, Message *msg, long msgType, int flags);

void setCurrentChildren(int value);
int receiveMessageFromChild(Message *msg, int childIndex);
void updateProcessTableEntry(int childIndex);

ElapsedTime elapsedTimeSince(const struct timeval *lastTime);

#endif
