#ifndef SHARED_H
#define SHARED_H

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
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

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3

#define NANOSECONDS_IN_SECOND 1000000000

typedef struct {
  unsigned int lifespanSeconds;
  unsigned int lifespanNanoSeconds;
} WorkerConfig;

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

typedef enum {
  PROCESS_TYPE_OSS,
  PROCESS_TYPE_WORKER,
  PROCESS_TYPE_TIMEKEEPER
} ProcessType;

extern int logLevel;
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

extern sem_t *clockSem;
extern const char *clockSemName;

int getCurrentChildren(void);
void setCurrentChildren(int value);

void log_message(int level, const char *format, ...);

key_t getSharedMemoryKey(void);
int initSharedMemory(size_t size);
SimulatedClock *attachSharedMemory();
int detachSharedMemory(void);

int initMessageQueue(void);
int sendMessage(int msqId, Message *msg);
int receiveMessage(int msqId, Message *msg, long msgType, int flags);

#endif
