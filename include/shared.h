#ifndef SHARED_H
#define SHARED_H

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MSG_PATH "/tmp"
#define MSG_PROJ_ID 'a'

#define SHM_PATH "/tmp"
#define SHM_PROJ_ID_SIM_CLOCK 'S'
#define SHM_PROJ_ID_ACT_TIME 'A'
#define SHM_PROJ_ID_PROCESS_TABLE 'P'

#define SEM_PERMISSIONS 0600

#define MAX_RUNTIME 60

#define TIMEKEEPER_SIM_SPEED_FACTOR 0.28

#define DEFAULT_MAX_PROCESSES 5
#define DEFAULT_MAX_SIMULTANEOUS 10
#define DEFAULT_CHILD_TIME_LIMIT 10
#define DEFAULT_LAUNCH_INTERVAL 1000
#define DEFAULT_LOG_FILE_NAME "oss.log"

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3

#define SUCCESS 0
#define ERROR_INIT_QUEUE -1
#define ERROR_INIT_SHM -2

#define NANOSECONDS_IN_SECOND 1000000000

#define LOG_BUFFER_SIZE 1024

#define QUEUE_COUNT 3
#define MAX_PROCESSES 20
#define TIME_QUANTUM_BASE 10

#define MESSAGE_TYPE_SCHEDULE 1
#define MESSAGE_TYPE_TERMINATE 2
#define MESSAGE_TYPE_BLOCKED 3

typedef struct {
  unsigned int lifespanSeconds;
  unsigned int lifespanNanoSeconds;
} WorkerConfig;

typedef struct {
  unsigned long seconds;
  unsigned long nanoseconds;
  int initialized;
} SimulatedClock, ActualTime;

typedef struct {
  long mtype;
  int mtext;
  unsigned long usedTime;
} Message;

typedef struct PCB {
  int occupied;
  pid_t pid;
  int startSeconds;
  int startNano;
  int blocked;
  int eventBlockedUntilSec;
  int eventBlockedUntilNano;
} PCB;

typedef struct {
  pid_t *queue;
  int front, rear;
  int capacity;
} Queue;

typedef struct {
  Queue highPriority;
  Queue midPriority;
  Queue lowPriority;
} MLFQ;

extern MLFQ mlfq;

extern PCB *processTable;
extern Queue queues[QUEUE_COUNT];

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
extern ActualTime *actualTime;

extern FILE *logFile;
extern char logFileName[256];

extern int msqId;
extern int simulatedTimeShmId;
extern int actualTimeShmId;
extern int processTableShmId;

extern volatile sig_atomic_t keepRunning;
extern volatile sig_atomic_t childTerminated;

extern int maxProcesses;
extern int maxSimultaneous;
extern int childTimeLimit;
extern int launchInterval;
extern int currentChildren;

extern sem_t *clockSem;
extern const char *clockSemName;

extern pthread_mutex_t logMutex;

int getCurrentChildren(void);
void setCurrentChildren(int value);
void *attachSharedMemory(const char *path, int proj_id, size_t size,
                         const char *segmentName);
int detachSharedMemory(void **shmPtr, const char *segmentName);
const char *processTypeToString(ProcessType type);
void log_message(int level, const char *format, ...);
key_t getSharedMemoryKey(const char *path, int proj_id);
int initMessageQueue(void);
int sendMessage(int msqId, Message *msg);
int receiveMessage(int msqId, Message *msg, long msgType, int flags);
void cleanupSharedResources(void);
void initializeSharedResources(void);

#endif
