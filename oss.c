#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>

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

int main(int argc, char *argv[]) {
  int option;
  int totalProcesses = 5; // Default total process count
  int simultaneousProcesses = 3; // Default simultaneous process limit
  int iterations = 7; // Default iteration count per process
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
      case 't': // Set iterations per process
        iterations = strtol(optarg, &endptr, 10);
        if (*endptr != '\0' || iterations <= 0) {
          fprintf(stderr, "Invalid iteration count: %s\n", optarg);
          return 1;
        }
        break;
      default: // Invalid option or lack thereof prompts help display
        printHelp();
        return 1;
    }
  }

  int activeProcesses = 0; // Counter for currently active child processes
  int launchedProcesses = 0; // Counter for total launched child processes
  pid_t pid;

  while (keepRunning && launchedProcesses < totalProcesses) {
    if (activeProcesses < simultaneousProcesses) {
      pid = fork();
      if (pid == 0) { // Child process path
        char iterParam[10];
        sprintf(iterParam, "%d", iterations); // Prepare iteration parameter
        execl("./user", "user", iterParam, (char *)NULL); // Execute user process
        perror("execl"); // Exec failure reporting
        exit(1); // Exit child process on exec failure
      } else if (pid > 0) { // Parent process path
        activeProcesses++;
        launchedProcesses++;
      } else { // Fork failure handling
        perror("fork");
        exit(1); // Exit on fork failure
      }
    }

    // Non-blocking wait for any child process to exit
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
      activeProcesses--;
    }
  }

  // Block until all active child processes have exited
  while ((pid = waitpid(-1, NULL, 0)) > 0);

  return 0; // Successful program termination
}
