#ifndef GLOBALS_H
#define GLOBALS_H

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
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
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_RESOURCES 10
#define INSTANCES_PER_RESOURCE 20
#define MAX_USER_PROCESSES 18
#define MSG_PATH "/tmp"
#define MSG_PROJ_ID 'a'
#define SHM_PATH "/tmp"
#define SHM_PROJ_ID_SIM_CLOCK 'S'
#define SHM_PROJ_ID_ACT_TIME 'A'
#define SHM_PROJ_ID_PROCESS_TABLE 'P'
#define SHM_PROJ_ID_RESOURCE_TABLE 'R'
#define SHM_PROJ_ID_DEADLOCK 'D'
#define SEM_PERMISSIONS 0600
#define MAX_RUNTIME 30
#define TIMEKEEPER_SIM_SPEED_FACTOR 0.28
#define MAX_RESOURCE_TYPES 10
#define MAX_PROCESSES 100

#define DEFAULT_MAX_PROCESSES 8
#define DEFAULT_MAX_SIMULTANEOUS 10
#define DEFAULT_CHILD_TIME_LIMIT 10
#define DEFAULT_LAUNCH_INTERVAL 1000
#define DEFAULT_LOG_FILE_NAME "oss.log"

#define LOG_LEVEL_ANNOY 0
#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_WARN 3
#define LOG_LEVEL_ERROR 4

#define SUCCESS 0
#define ERROR_INVALID_ARGS -1
#define ERROR_INIT_QUEUE -2
#define ERROR_INIT_SHM -3
#define ERROR_FILE_OPEN -4

#define NANOSECONDS_IN_SECOND 1000000000

#define LOG_BUFFER_SIZE 1024

#define QUEUE_COUNT 3
#define TIME_QUANTUM_BASE 10

#define MESSAGE_TYPE_SCHEDULE 1
#define MESSAGE_TYPE_TERMINATE 2
#define MESSAGE_TYPE_BLOCKED 3
#define MESSAGE_TYPE_RELEASE 4
#define MESSAGE_TYPE_REQUEST 5

#define MSG_RESOURCE_GRANTED 1

typedef struct {
  int total[MAX_RESOURCES];     // Total instances for each resource
  int available[MAX_RESOURCES]; // Available instances for each resource
  int allocated[MAX_USER_PROCESSES]
               [MAX_RESOURCES]; // Resources allocated per process
} ResourceDescriptor;

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
  int resourceType;
  int amount;
  int resourceAmount;
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

typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR } LogLevel;

typedef enum {
  PROCESS_TYPE_OSS,
  PROCESS_TYPE_WORKER,
  PROCESS_TYPE_TABLEPRINTER,
  PROCESS_TYPE_TIMEKEEPER
} ProcessType;

extern int maxProcesses;
extern int maxSimultaneous;
extern int launchInterval;
extern char logFileName[256];
extern FILE *logFile;

extern int maxDemand[MAX_USER_PROCESSES][MAX_RESOURCES];

extern MLFQ mlfq;

extern PCB *processTable;
extern Queue queues[QUEUE_COUNT];

extern int logLevel;
extern ProcessType gProcessType;

extern struct timeval startTime;
extern struct timeval lastLogTime;
extern SimulatedClock *simClock;
extern ActualTime *actualTime;

extern int msqId;
extern int simulatedTimeShmId;
extern int actualTimeShmId;
extern int processTableShmId;
extern int resourceTableShmId;

extern volatile sig_atomic_t keepRunning;
extern volatile sig_atomic_t childTerminated;

extern int childTimeLimit;
extern int currentChildren;

extern sem_t *clockSem;
extern const char *clockSemName;

extern pthread_mutex_t logMutex;
extern pthread_mutex_t resourceTableMutex;

extern ResourceDescriptor *resourceTable;

extern int currentLogLevel;

extern Queue resourceWaitQueue[MAX_RESOURCES];

extern int available[MAX_RESOURCE_TYPES]; // Available resources of each type
extern int maximum[MAX_PROCESSES]
                  [MAX_RESOURCE_TYPES]; // Maximum demand of each process
extern int allocation[MAX_PROCESSES]
                     [MAX_RESOURCE_TYPES]; // Resources currently allocated to
                                           // each process
extern int need[MAX_PROCESSES]
               [MAX_RESOURCE_TYPES]; // Remaining resource needs of each process

#endif // GLOBALS_H
