#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <stdbool.h>

#include "shared.h"

#define MAX_PROCESSES 20
#define NANOSECOND 10000000
#define SIMULATION_TIME_LIMIT 60
#define PRINT_INTERVAL 5
#define WORKER_LAUNCH_INTERVAL 1

SimulatedClock * simClock = NULL;
int shmId = -1;
bool cleanupDone = false;
int totalLaunched = 0;

typedef struct {
  int occupied;
  pid_t pid;
  int startSeconds;
  int startNano;
}
PCB;

PCB processTable[MAX_PROCESSES];

void parseArgs(int argc, char * argv[], int * proc, int * simul, int * timelimitForChildren, int * intervalInMsToLaunchChildren);
SimulatedClock * setupSharedMemory(void);
void initializeProcessTable(PCB processTable[]);
void incrementSimulatedClock(SimulatedClock * simClock, int increment);
void checkAndLaunchWorkers(SimulatedClock * simClock, PCB processTable[], int proc, int simul, int timelimitForChildren, int intervalInMsToLaunchChildren, int * currentSimul, time_t * lastLaunchTime);
void checkForTerminatedChildren(PCB processTable[], int * currentSimul);
void printProcessTable(SimulatedClock * simClock, PCB processTable[]);
void cleanup();
void handle_signal(int signal);
void setupSignalHandler();
void printCurrentSimulatedTime(SimulatedClock * simClock);

int main(int argc, char * argv[]) {
  setupSignalHandler();

  int proc = 5, simul = 3, timelimitForChildren = 7, intervalInMsToLaunchChildren = 100;
  int currentSimul = 0;
  int totalLaunched = 0;

  parseArgs(argc, argv, & proc, & simul, & timelimitForChildren, & intervalInMsToLaunchChildren);
  printf("Parsed command-line arguments:\n");
  printf("  - Total processes (n): %d\n", proc);
  printf("  - Simultaneous processes (s): %d\n", simul);
  printf("  - Time limit for children (t): %d seconds\n", timelimitForChildren);
  printf("  - Interval for launching children (i): %d milliseconds\n", intervalInMsToLaunchChildren);

  simClock = setupSharedMemory();
  initializeProcessTable(processTable);

  time_t startTime = time(NULL), currentTime, lastWorkerCheck = 0, lastPrintTime = 0;
  while (1) {
    currentTime = time(NULL);
    if (currentTime - startTime >= SIMULATION_TIME_LIMIT) {
      printf("Simulation ends after 60 real-world seconds.\n");
      break;
    }

    incrementSimulatedClock(simClock, 1000000);

    if (currentTime - lastWorkerCheck >= WORKER_LAUNCH_INTERVAL) {
      if (currentSimul < simul) {
        if (totalLaunched < proc) {
          checkAndLaunchWorkers(simClock, processTable, proc, simul, timelimitForChildren, intervalInMsToLaunchChildren, & currentSimul, & lastWorkerCheck);
          lastWorkerCheck = currentTime;
          totalLaunched++;
        }
        else {
          // printf("Error: Total launched processes limit (%d) reached.\n", proc);
        }
      }
      else {
        //printf("Error: Simultaneous processes limit (%d) reached.\n", simul);
      }
    }
    else {
      //printf("Error: Worker launch interval (%d milliseconds) not yet passed.\n", WORKER_LAUNCH_INTERVAL);
    }

    checkForTerminatedChildren(processTable, & currentSimul);

    if (currentTime - lastPrintTime >= PRINT_INTERVAL) {
      printProcessTable(simClock, processTable);
      lastPrintTime = currentTime;
    }

    usleep(100000);
  }

  cleanup();
  return 0;
}

SimulatedClock * setupSharedMemory() {
  SimulatedClock * simClock;

  key_t key = getSharedMemoryKey();
  shmId = shmget(key, sizeof(SimulatedClock), 0644 | IPC_CREAT);
  if (shmId < 0) {
    perror("shmget error");
    exit(1);
  }

  simClock = (SimulatedClock * ) shmat(shmId, NULL, 0);
  if (simClock == (void * ) - 1) {
    perror("shmat error");
    exit(1);
  }

  printf("Shared memory setup complete.\n");
  return simClock;
}

void printUsage(const char * programName) {
  fprintf(stderr, "Usage: %s [-h] [-n proc] [-s simul] [-t timelimitForChildren] [-i intervalInMsToLaunchChildren]\n", programName);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -h  Display this help message and exit\n");
  fprintf(stderr, "  -n  Set the number of total processes to spawn (default: 5)\n");
  fprintf(stderr, "  -s  Set the number of processes to spawn simultaneously (default: 3)\n");
  fprintf(stderr, "  -t  Set the time limit (in seconds) for each child process (default: 7)\n");
  fprintf(stderr, "  -i  Set the interval (in milliseconds) between launching children (default: 100)\n");
}

void parseArgs(int argc, char * argv[], int * proc, int * simul, int * timelimitForChildren, int * intervalInMsToLaunchChildren) {
  int option;
  while ((option = getopt(argc, argv, "hn:s:t:i:")) != -1) {
    switch (option) {
    case 'h':
      printUsage(argv[0]);
      exit(EXIT_SUCCESS);
    case 'n':
      *
      proc = atoi(optarg);
      printf("Total processes set to: %d\n", * proc);
      break;
    case 's':
      *
      simul = atoi(optarg);
      printf("Simultaneous processes set to: %d\n", * simul);
      break;
    case 't':
      *
      timelimitForChildren = atoi(optarg);
      printf("Time limit for children set to: %d seconds\n", * timelimitForChildren);
      break;
    case 'i':
      *
      intervalInMsToLaunchChildren = atoi(optarg);
      printf("Interval to launch children set to: %d milliseconds\n", * intervalInMsToLaunchChildren);
      break;
    default:
      printUsage(argv[0]);
      exit(EXIT_FAILURE);
    }
  }
}

void initializeProcessTable(PCB processTable[]) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    processTable[i].occupied = 0;
    processTable[i].pid = 0;
    processTable[i].startSeconds = 0;
    processTable[i].startNano = 0;
  }
  printf("Process table initialized.\n");
}

int addProcessToTable(pid_t pid, int startSeconds, int startNano) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (!processTable[i].occupied) {
      processTable[i].pid = pid;
      processTable[i].startSeconds = startSeconds;
      processTable[i].startNano = startNano;
      processTable[i].occupied = 1;
      printf("Process %d added to table at index %d.\n", pid, i);
      return i;
    }
  }
  printf("Failed to add process %d; table is full.\n", pid);
  return -1;
}

void removeProcessFromTable(pid_t pid) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (processTable[i].occupied && processTable[i].pid == pid) {
      processTable[i].occupied = 0;
      printf("Process %d removed from table.\n", pid);
      break;
    }
  }
}

void launchWorker(SimulatedClock * simClock, int waitSeconds, int waitNanoseconds) {
  pid_t pid = fork();

  if (pid == -1) {
    perror("fork error");
    exit(EXIT_FAILURE);
  }
  else if (pid == 0) {
    char secStr[20];
    char nanoStr[20];

    snprintf(secStr, sizeof(secStr), "%d", waitSeconds);
    snprintf(nanoStr, sizeof(nanoStr), "%d", waitNanoseconds);

    execl("./worker", "worker", secStr, nanoStr, (char * ) NULL);

    perror("execl error");
    exit(EXIT_FAILURE);
  }
  else {
    addProcessToTable(pid, simClock -> seconds, simClock -> nanoseconds);
    int status;

    if (waitpid(pid, & status, 0) == -1) {
      perror("waitpid error");

    }
    else if (WIFEXITED(status)) {
      int exit_status = WEXITSTATUS(status);
      printf("Worker process %d exited with status %d.\n", pid, exit_status);
    }
    else {
      printf("Worker process %d terminated abnormally.\n", pid);
    }

    removeProcessFromTable(pid);
  }
}

void checkAndLaunchWorkers(SimulatedClock * simClock, PCB processTable[], int proc, int simul, int timelimitForChildren, int intervalInMsToLaunchChildren, int * currentSimul, time_t * lastLaunchTime) {
  if ( * currentSimul >= simul || * currentSimul >= proc) {
    return;
  }

  time_t currentTime = time(NULL);

  if (difftime(currentTime, * lastLaunchTime) * 1000 >= intervalInMsToLaunchChildren) {
    for (int i = 0; i < MAX_PROCESSES && * currentSimul < simul; i++) {
      if (!processTable[i].occupied) {
        pid_t pid = fork();
        if (pid == 0) {
          char secStr[12], nanoStr[12];
          snprintf(secStr, sizeof(secStr), "%d", rand() % timelimitForChildren + 1);
          snprintf(nanoStr, sizeof(nanoStr), "%d", rand() % NANOSECOND);
          execl("./worker", "worker", secStr, nanoStr, NULL);
          perror("execl failed");
          exit(EXIT_FAILURE);
        }
        else if (pid > 0) {
          processTable[i].occupied = 1;
          processTable[i].pid = pid;
          processTable[i].startSeconds = simClock -> seconds;
          processTable[i].startNano = simClock -> nanoseconds;
          * currentSimul += 1;
          * lastLaunchTime = currentTime;
          printf("Launched worker PID: %d\n", pid);
        }
        else {
          perror("fork failed");
        }
        break;
      }
    }
  }
}

void checkForTerminatedChildren(PCB processTable[], int * currentSimul) {
  int status;
  pid_t pid;

  while ((pid = waitpid(-1, & status, WNOHANG)) > 0) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
      if (processTable[i].pid == pid) {
        processTable[i].occupied = 0;
        * currentSimul -= 1;
        printf("Child PID %d terminated.\n", pid);
        break;
      }
    }
  }
}

void printProcessTable(SimulatedClock * simClock, PCB processTable[]) {
  printf("OSS PID: %d, SysClockS: %d, SysClockNano: %d\n", getpid(), simClock -> seconds, simClock -> nanoseconds);
  printf("Process Table:\n");
  printf("Entry Occupied PID StartS StartN\n");
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      printf("%d 1 %d %d %d\n", i, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano);
    }
    else {
      printf("%d 0 - - -\n", i);
    }
  }
}

void incrementSimulatedClock(SimulatedClock * simClock, int increment) {
  simClock -> nanoseconds += increment;
  while (simClock -> nanoseconds >= NANOSECOND) {
    simClock -> nanoseconds -= NANOSECOND;
    simClock -> seconds += 1;
  }
}

void cleanup() {
  if (!cleanupDone) {
    if (simClock != NULL && shmdt(simClock) == -1) {
      perror("Cleanup shmdt failed");
    }
    if (shmId != -1 && shmctl(shmId, IPC_RMID, NULL) == -1) {
      perror("Cleanup shmctl failed");
    }
    printf("Cleanup complete.\n");
    cleanupDone = true;
  }
}

void handleSignal(int signal) {
  printf("Caught signal %d. Cleaning up...\n", signal);
  cleanup();
  exit(EXIT_FAILURE);
}

void setupSignalHandler() {
  struct sigaction sa;
  sa.sa_handler = handleSignal;
  sigemptyset( & sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, & sa, NULL) == -1) {
    perror("Error setting up SIGINT handler");
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGTERM, & sa, NULL) == -1) {
    perror("Error setting up SIGTERM handler");
    exit(EXIT_FAILURE);
  }
  atexit(cleanup);
}

void printCurrentSimulatedTime(SimulatedClock * simClock) {
  printf("Current Simulated Time: %d seconds, %d nanoseconds\n", simClock -> seconds, simClock -> nanoseconds);
}
