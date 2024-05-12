#include "globals.h"

int maxProcesses = DEFAULT_MAX_PROCESSES;
int maxSimultaneous = DEFAULT_MAX_SIMULTANEOUS;
int launchInterval = DEFAULT_LAUNCH_INTERVAL;
char logFileName[256] = DEFAULT_LOG_FILE_NAME;
FILE *logFile = NULL;

// Global variables to represent different process and system states
ProcessType gProcessType;   // Current process type
struct timeval startTime;   // System start time
struct timeval lastLogTime; // Last log entry time

SimulatedClock *simClock = NULL; // Pointer to simulated system clock
ActualTime *actualTime = NULL;   // Pointer to actual time for processes

PCB *processTable; // Pointer to process control block table

MLFQ mlfq; // Multi-level feedback queue structure

int msqId = -1; // Message queue identifier

// Shared memory identifiers for various components
int simulatedTimeShmId = -1; // Simulated time shared memory ID
int actualTimeShmId = -1;    // Actual time shared memory ID
int processTableShmId = -1;  // Process table shared memory ID
int resourceTableShmId = -1; // Resource table shared memory ID
int deadlockShmId = -1;      // Deadlock detection shared memory ID

// Volatile variables for process control
volatile sig_atomic_t childTerminated =
    0; // Flag to check if child process has terminated
volatile sig_atomic_t keepRunning = 1; // Flag to control system running state

// System limits and settings
int childTimeLimit = DEFAULT_CHILD_TIME_LIMIT; // Time limit for child processes

int currentChildren = 0; // Current number of child processes

int currentLogLevel = LOG_LEVEL_DEBUG; // Current log level

sem_t *clockSem = SEM_FAILED; // Semaphore for clock synchronization
const char *clockSemName = "/simClockSem"; // Name of the clock semaphore

pthread_mutex_t logMutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for logging

int totalLaunched = 0;

pid_t timekeeperPid = 0;
pid_t tableprinterPid = 0;

int deadlockCheckInterval = 1000;
