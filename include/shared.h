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

#define SHM_KEY 0x6234
#define MSG_KEY 0x5678

#define SHM_PATH "/etc/passwd"
#define SHM_PROJ_ID 'b'

#define DEFAULT_MAX_PROCESSES 5
#define DEFAULT_MAX_SIMULTANEOUS 10
#define DEFAULT_CHILD_TIME_LIMIT 10
#define DEFAULT_LAUNCH_INTERVAL 1000
#define DEFAULT_LOG_FILE_NAME "oss.log"

typedef struct {
  long seconds;
  long nanoseconds;
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
extern SimulatedClock* simClock;
extern PCB processTable[DEFAULT_MAX_PROCESSES];
extern FILE* logFile;
extern char logFileName[256];
extern volatile sig_atomic_t cleanupInitiated;
extern volatile sig_atomic_t keepRunning;
extern int msqId;
extern int shmId;

extern int maxProcesses;
extern int maxSimultaneous;
extern int childTimeLimit;
extern int launchInterval;
extern int currentChildren;

void initializeSimulationEnvironment();

void log_debug(const char* format, ...);

int initSharedMemory();
key_t getSharedMemoryKey();
SimulatedClock* attachSharedMemory();
int detachSharedMemory(SimulatedClock* shmPtr);

int initMessageQueue();
int sendMessage(Message* msg);
int receiveMessage(Message* msg, long msgType, int flags);

int getCurrentChildren();
void setCurrentChildren(int value);
void sendMessageToNextChild();
void receiveMessageFromChild();

ElapsedTime elapsedTimeSince(const struct timeval* lastTime);

#endif
