#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

#define MAX_PROCESSES 20

typedef struct {
  int seconds;
  int nanoseconds;
} SimulatedClock;

typedef struct PCB {
  int occupied; // either true or false
  pid_t pid; // process id of this child
  int startSeconds; // time when it was forked
  int startNano; // time when it was forked
} PCB;

volatile sig_atomic_t keepRunning = 1;

void signalHandler(int sig) {
  (void)sig; // Explicitly mark the parameter as unused
  keepRunning = 0;
}

void printHelp() {
  printf("Usage: oss [-h] [-n proc] [-s simul] [-t iter]\n");
  printf("  -h  Display this help message\n");
  printf("  -n proc   Total children to launch\n");
  printf("  -s simul  Children allowed simultaneously\n");
  printf("  -t iter   Iterations per user process\n");
}

// find an empty or reusable entry in the process table
int findEmptyProcessEntry(PCB *table) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (table[i].occupied == 0) {
      return i;
    }
  }
  return -1; // No empty entry found
}

int main(int argc, char *argv[]) {
  int option;
  int totalProcesses = 5; // Default total process count
  int simultaneousProcesses = 3; // Default simultaneous process limit
  char *endptr;

  signal(SIGINT, signalHandler);

  while ((option = getopt(argc, argv, "hn:s:t:")) != -1) {
    switch (option) {
      case 'h': // Help option.
        printHelp();
        return 0;
      case 'n': // Set total processes
        totalProcesses = strtol(optarg, &endptr, 10);
        if (*endptr != '\0' || totalProcesses <= 0) {
          fprintf(stderr, "Invalid total process count: %s\n", optarg);
          return 1;
        }
        break;
      case 's': // Set simultaneous process limit
        simultaneousProcesses = strtol(optarg, &endptr, 10);
        if (*endptr != '\0' || simultaneousProcesses <= 0) {
          fprintf(stderr, "Invalid simultaneous process limit: %s\n", optarg);
          return 1;
        }
        break;
      default: // Invalid option or lack thereof prompts help display
        printHelp();
        return 1;
    }
  }

  // Shared memory for simulated clock
  int shmId;
  SimulatedClock *simClock;
  key_t key = ftok("oss.c", 'R');
  shmId = shmget(key, sizeof(SimulatedClock), IPC_CREAT | 0666);
  if (shmId < 0) {
    perror("shmget");
    exit(1);
  }
  simClock = (SimulatedClock *)shmat(shmId, NULL, 0);
  if (simClock == (void *)-1) {
    perror("shmat");
    exit(1);
  }
  simClock->seconds = 0;
  simClock->nanoseconds = 0;

  // Initialize process table
  PCB processTable[MAX_PROCESSES];
  for (int i = 0; i < MAX_PROCESSES; i++) {
    processTable[i].occupied = 0;
  }

  int activeProcesses = 0;
  while (keepRunning) {
    int index = findEmptyProcessEntry(processTable);
    if (index != -1 && activeProcesses < simultaneousProcesses) {
      pid_t pid = fork();
      if (pid == 0) {
        // Child process
        execl("./worker", "worker", NULL);
        perror("execl");
        exit(1);
      } else if (pid > 0) {
        // Parent process: Update process table entry for the new child
        processTable[index].occupied = 1;
        processTable[index].pid = pid;
        processTable[index].startSeconds = simClock->seconds;
        processTable[index].startNano = simClock->nanoseconds;
        activeProcesses++;
      } else {
        perror("fork");
        exit(1);
      }
    }

    // Update simulated clock here
    simClock->nanoseconds += 100000;
    if (simClock->nanoseconds >= 1000000000) {
      simClock->seconds += 1;
      simClock->nanoseconds -= 1000000000;
    }

    // Check for terminated processes
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
      for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i].pid == pid) {
          processTable[i].occupied = 0;
          activeProcesses--;
          break;
        }
      }
    }
  }

  // Cleanup
  shmdt((void *) simClock);
  shmctl(shmId, IPC_RMID, NULL);

  return 0;
}
