#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include "shared.h"

#define MAX_PROCESSES 20
#define NANOSECOND 10000000
#define SIMULATION_TIME_LIMIT 60
#define PRINT_INTERVAL 5
#define WORKER_LAUNCH_INTERVAL 1

typedef struct {
  int occupied;
  pid_t pid;
  int startSeconds;
  int startNano;
}
PCB;

PCB processTable[MAX_PROCESSES];

// Forward Declarations
void parseArgs(int argc, char * argv[], int * proc, int * simul, int * timelimitForChildren, int * intervalInMsToLaunchChildren);
SimulatedClock * setupSharedMemory(void);
void initializeProcessTable(PCB processTable[]);
void incrementSimulatedClock(SimulatedClock * simClock, int increment);
void checkAndLaunchWorkers(SimulatedClock * simClock, PCB processTable[], int proc, int simul, int timelimitForChildren, int intervalInMsToLaunchChildren, int * currentSimul, time_t * lastLaunchTime);
void checkForTerminatedChildren(PCB processTable[]);
void printProcessTable(SimulatedClock * simClock, PCB processTable[]);

void printCurrentSimulatedTime(SimulatedClock * simClock) {
  printf("Current Simulated Time: %d seconds, %d nanoseconds\n", simClock -> seconds, simClock -> nanoseconds);
}

int main(int argc, char * argv[]) {
  int proc = 5, simul = 3, timelimitForChildren = 7, intervalInMsToLaunchChildren = 100;
  PCB processTable[MAX_PROCESSES] = {
    0
  };

  int currentSimul = 0;
  time_t startTime = time(NULL), currentTime, lastWorkerCheck = 0, lastPrintTime = 0;

  parseArgs(argc, argv, & proc, & simul, & timelimitForChildren, & intervalInMsToLaunchChildren);
  SimulatedClock * simClock = setupSharedMemory();
  initializeProcessTable(processTable);

  while (1) {
    currentTime = time(NULL);

    if (currentTime - startTime >= 60) {
      printf("Simulation ends after 60 real-world seconds.\n");
      break;
    }

    incrementSimulatedClock(simClock, 1000000);

    if (currentTime - lastWorkerCheck >= 1 && currentSimul < simul) {
      checkAndLaunchWorkers(simClock, processTable, proc, simul, timelimitForChildren, intervalInMsToLaunchChildren, & currentSimul, & lastWorkerCheck);
      lastWorkerCheck = currentTime;
    }

    checkForTerminatedChildren(processTable);

    if (currentTime - lastPrintTime >= 5) {
      printCurrentSimulatedTime(simClock);
      lastPrintTime = currentTime;
    }

    usleep(100000);
  }

  if (shmdt(simClock) == -1) {
    perror("shmdt failed");
    exit(EXIT_FAILURE);
  }
  printf("Simulation completed. Resources cleaned up.\n");
  return 0;
}

SimulatedClock * setupSharedMemory() {
  int shmId;
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
    exit(1);
  }
  else if (pid == 0) {
    char secStr[20];
    char nanoStr[20];

    sprintf(secStr, "%d", waitSeconds);
    sprintf(nanoStr, "%d", waitNanoseconds);
    execl("./worker", "worker", secStr, nanoStr, (char * ) NULL);
    perror("execl error");
    exit(1);
  } else {
    addProcessToTable(pid, simClock -> seconds, simClock -> nanoseconds);

    int status;
    waitpid(pid, & status, 0);
    removeProcessFromTable(pid);
    if (WIFEXITED(status)) {
      int exit_status = WEXITSTATUS(status);

      printf("Worker process %d exited with status %d.\n", pid, exit_status);
    }
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
          char secStr[10], nanoStr[10];
          sprintf(secStr, "%d", rand() % timelimitForChildren + 1);
          sprintf(nanoStr, "%d", rand() % NANOSECOND);
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

void checkForTerminatedChildren(PCB processTable[]) {
  int status;
  pid_t pid;

  while ((pid = waitpid(-1, & status, WNOHANG)) > 0) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
      if (processTable[i].pid == pid) {
        processTable[i].occupied = 0;
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
    } else {
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
