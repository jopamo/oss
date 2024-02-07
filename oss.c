#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

// Display usage guidelines for the program
void printHelp() {
  printf("Usage: oss [-h] [-n proc] [-s simul] [-t iter]\n");
  printf("  -h    Display this help message\n");
  printf("  -n proc   Total children to launch\n");
  printf("  -s simul  Children allowed simultaneously\n");
  printf("  -t iter   Iterations per user process\n");
}

int main(int argc, char *argv[]) {
  // Command line option parsing variables
  int option;
  int totalProcesses = 5; // Default total process count
  int simultaneousProcesses = 3; // Default simultaneous process limit
  int iterations = 7; // Default iteration count per process

  // Parse command line options to customize process management
  while ((option = getopt(argc, argv, "hn:s:t:")) != -1) {
    switch (option) {
      case 'h': // Help option.
        printHelp();
        return 0;
      case 'n': // Set total processes
        totalProcesses = atoi(optarg);
        break;
      case 's': // Set simultaneous process limit
        simultaneousProcesses = atoi(optarg);
        break;
      case 't': // Set iterations per process
        iterations = atoi(optarg);
        break;
      default: // Invalid option or lack thereof prompts help display
        printHelp();
        return 1;
    }
  }

  // Process creation and management
  int activeProcesses = 0; // Counter for currently active child processes
  int launchedProcesses = 0; // Counter for total launched child processes

  // Main loop to fork and exec user processes based on command line parameters
  while (launchedProcesses < totalProcesses) {
    // Check against simultaneous process limit before forking
    if (activeProcesses < simultaneousProcesses) {
      pid_t pid = fork(); // Fork a new process

      if (pid == 0) { // Child process path
        char iterParam[10];
        sprintf(iterParam, "%d", iterations); // Prepare iteration parameter
        execl("./user", "user", iterParam, (char *)NULL); // Execute user process
        perror("execl"); // Exec failure reporting
        exit(1); // Exit child process on exec failure
      }
      else if (pid > 0) { // Parent process path
        activeProcesses++; // Increment active process count
        launchedProcesses++; // Increment total launched process count
      }
      else { // Fork failure handling
        perror("fork"); // Fork failure reporting
        exit(1); // Exit on fork failure
      }
    }

    // Wait for a child process to exit before attempting to launch another, if limit reached
    if (activeProcesses >= simultaneousProcesses || launchedProcesses == totalProcesses) {
      wait(NULL); // Wait for any child process to exit
      activeProcesses--; // Decrement active process count post wait
    }
  }

  // Final wait loop for remaining active child processes
  while (activeProcesses > 0) {
    wait(NULL); // Ensure all child processes have exited
    activeProcesses--; // Update active process count
  }

  return 0; // Successful program termination
}
