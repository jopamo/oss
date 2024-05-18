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

#define MSG_REQUEST_RESOURCE 1
#define MSG_RELEASE_RESOURCE 0

#define REQUEST_INTERVAL_NANOSECONDS 500000000 // Half a second in nanoseconds

#define HALF_SECOND 500000000L // 500 milliseconds in nanoseconds
#define ONE_SECOND 1000000000L // One second in nanoseconds

// absolute maximums
#define MAX_PROCESSES 50
#define MAX_SIMULTANEOUS 18
#define MAX_RESOURCES 20
#define MAX_INSTANCES 40
#define MAX_RESOURCE_TYPES 10

// default values assigned to variables
#define DEFAULT_MAX_RESOURCES 10
#define DEFAULT_MAX_PROCESSES 18
#define DEFAULT_MAX_INSTANCES 20

#define DEFAULT_CHILD_TIME_LIMIT 10
#define DEFAULT_LAUNCH_INTERVAL 1000
#define DEFAULT_LOG_FILE_NAME "psmgmt.log"

#define MAX_RUNTIME 60
#define TIMEKEEPER_SIM_SPEED_FACTOR 0.12

#define MSG_PATH "/tmp"
#define MSG_PROJ_ID 'a'

#define SHM_PATH "/tmp"
#define SHM_PROJ_ID_SIM_CLOCK 'S'
#define SHM_PROJ_ID_ACT_TIME 'A'
#define SHM_PROJ_ID_PROCESS_TABLE 'P'
#define SHM_PROJ_ID_RESOURCE_TABLE 'R'
#define SHM_PROJ_ID_DEADLOCK 'D'

#define SEM_PERMISSIONS 0666
#define MSQ_PERMISSIONS 0666
#define SHM_PERMISSIONS 0666

// Log levels
#define LOG_LEVEL_ANNOY 0
#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_WARN 3
#define LOG_LEVEL_ERROR 4

#define DEFAULT_LIFESPAN_SECONDS 5
#define DEFAULT_LIFESPAN_NANOSECONDS 500000000

// Time conversions
#define NANOSECONDS_IN_SECOND 1000000000

#define LOG_BUFFER_SIZE 1024

typedef struct {
  unsigned long seconds;
  unsigned long nanoseconds;
  int initialized;
} SimulatedClock, ActualTime;

typedef struct PCB {
  int occupied;
  pid_t pid;
  int startSeconds;
  int startNano;
  int blocked;
  int eventBlockedUntilSec;
  int eventBlockedUntilNano;
  int state;
} PCB;

typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR } LogLevel;

typedef enum { PROCESS_TYPE_PSMGMT, PROCESS_TYPE_WORKER } ProcessType;

extern int maxResources;
extern int maxProcesses;
extern int maxInstances;
extern int launchInterval;
extern char logFileName[256];
extern FILE *logFile;

extern PCB *processTable;
extern pthread_mutex_t processTableMutex;

extern int logLevel;
extern ProcessType gProcessType;

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
extern int currentLogLevel;

extern int totalLaunched;

extern pthread_mutex_t cleanupMutex;

#endif
