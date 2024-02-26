/*
Programming language: C
Programmers: Paul Moses
Repo: https://github.com/jopamo/skeleton_mp

Date: 2.22.24
Name of class: CS4760

This program simulates an operating system's process scheduling and inter-process communication
using shared memory. It consists of two main components: the OSS (Operating System Simulator) and
worker processes. The OSS manages a simulated clock and a process table, coordinating the launch
and termination of worker processes based on simulated time.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

#include "shared.h"

#define MAX_PROCESSES 20           // Maximum number of processes the simulation can handle
#define NANOSECOND 10000000        // Number of nanoseconds in a second, used for time calculations
#define SIMULATION_TIME_LIMIT 60   // Maximum duration of the simulation in real-world seconds
#define PRINT_INTERVAL 0.5         // Interval in real-world seconds for printing the process table
#define WORKER_LAUNCH_INTERVAL 1   // Interval in real-world seconds between launching worker processes

SimulatedClock *simClock = NULL;   // Pointer to the simulated clock structure in shared memory
int shmId = -1;                    // Shared memory identifier
bool cleanupDone = false;          // Flag to ensure cleanup is performed only once
int totalLaunched = 0;             // Counter for the total number of processes launched

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
void terminateAllChildren();
int isNumeric(const char *str);

int main(int argc, char * argv[]) {
  // Initialize signal handlers for graceful termination.
  setupSignalHandler();

  // Default simulation parameters.
  int proc = 5, simul = 3, timelimitForChildren = 7, intervalInMsToLaunchChildren = 100;
  int currentSimul = 0; // Tracks the number of currently running simulations.
  int totalLaunched = 0; // Counts how many processes have been launched in total.

  // Parse command-line arguments to adjust simulation parameters.
  parseArgs(argc, argv, &proc, &simul, &timelimitForChildren, &intervalInMsToLaunchChildren);

  // Echo parsed command-line arguments to ensure correct setup.
  printf("Parsed command-line arguments:\n");
  printf("  - Total processes (n): %d\n", proc);
  printf("  - Simultaneous processes (s): %d\n", simul);
  printf("  - Time limit for children (t): %d seconds\n", timelimitForChildren);
  printf("  - Interval for launching children (i): %d milliseconds\n", intervalInMsToLaunchChildren);

  // Setup shared memory for the simulated clock and initialize the process table.
  simClock = setupSharedMemory();
  initializeProcessTable(processTable);

  // Record the start time of the simulation for time limit enforcement.
  time_t startTime = time(NULL), currentTime, lastWorkerCheck = 0, lastPrintTime = 0;
  while (1) {
    currentTime = time(NULL);

    // Check if the simulation has run beyond the real-world time limit.
    if (currentTime - startTime >= SIMULATION_TIME_LIMIT) {
      printf("Simulation ends after 60 real-world seconds.\n");
      break; // Exit the simulation loop.
    }

    // Increment the simulated clock periodically.
    incrementSimulatedClock(simClock, 4800000);

    // Attempt to launch new worker processes at defined intervals, if within limits.
    if (currentTime - lastWorkerCheck >= WORKER_LAUNCH_INTERVAL && currentSimul < simul && totalLaunched < proc) {
      checkAndLaunchWorkers(simClock, processTable, proc, simul, timelimitForChildren, intervalInMsToLaunchChildren, &currentSimul, &lastWorkerCheck);
      lastWorkerCheck = currentTime;
      totalLaunched++;
    }

    // Periodically check for and handle termination of child processes.
    checkForTerminatedChildren(processTable, &currentSimul);

    // Print the process table at regular intervals to monitor simulation progress.
    if (difftime(currentTime, lastPrintTime) >= PRINT_INTERVAL) {
      printProcessTable(simClock, processTable);
      lastPrintTime = currentTime;
    }

    struct timespec req, rem;
    req.tv_sec = 0; // Seconds
    req.tv_nsec = 480000 * 1000; // Convert microseconds to nanoseconds

    if (nanosleep(&req, &rem) < 0) {
      perror("nanosleep"); // Handle errors appropriately
    }
  }

  // Perform cleanup of shared resources before program termination.
  cleanup();

  return 0;
}

/**
 * Attempts to obtain a shared memory segment using a unique key and attaches it to the
 * process's address space to allow manipulation of the simulated clock.
 *
 * @return Pointer to the SimulatedClock structure in shared memory.
 */
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


void printUsage(const char *programName) {
    printf("Usage: %s [options]\n", programName);
    printf("Options:\n");
    printf("  -h                        Display this help message and exit\n");
    printf("  -n <total_processes>      Set the number of total processes to spawn (default: 5)\n");
    printf("  -s <simultaneous>         Set the number of processes to spawn simultaneously (default: 3)\n");
    printf("  -t <time_limit>           Set the time limit (in seconds) for each child process (default: 7)\n");
    printf("  -i <interval>             Set the interval (in milliseconds) between launching children (default: 100)\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s -n 10 -s 5 -t 10 -i 50\n", programName);
    printf("This command sets up the simulation with 10 total processes, allowing up to 5\n");
    printf("processes running simultaneously, each with a time limit of 10 seconds, and\n");
    printf("launching new processes at 50-millisecond intervals.\n");
}

/**
 * Parses command-line arguments to configure the simulation parameters:
 *
 * @param proc Pointer to store the total number of processes
 * @param simul Pointer to store the maximum number of simultaneous processes
 * @param timelimitForChildren Pointer to store the time limit for child processes
 * @param intervalInMsToLaunchChildren Pointer to store the interval for launching children
 */
void parseArgs(int argc, char *argv[], int *proc, int *simul, int *timelimitForChildren, int *intervalInMsToLaunchChildren) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    int option;
    while ((option = getopt(argc, argv, "hn:s:t:i:")) != -1) {
        switch (option) {
        case 'h': //just in case
            printUsage(argv[0]);
            exit(EXIT_SUCCESS);
        case 'n':
            if (isNumeric(optarg)) {
                *proc = atoi(optarg);
                printf("Total processes set to: %d\n", *proc);
            } else {
                fprintf(stderr, "Option -n requires a numeric argument.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 's':
            if (isNumeric(optarg)) {
                *simul = atoi(optarg);
                printf("Simultaneous processes set to: %d\n", *simul);
            } else {
                fprintf(stderr, "Option -s requires a numeric argument.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 't':
            if (isNumeric(optarg)) {
                *timelimitForChildren = atoi(optarg);
                printf("Time limit for children set to: %d seconds\n", *timelimitForChildren);
            } else {
                fprintf(stderr, "Option -t requires a numeric argument.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'i':
            if (isNumeric(optarg)) {
                *intervalInMsToLaunchChildren = atoi(optarg);
                printf("Interval to launch children set to: %d milliseconds\n", *intervalInMsToLaunchChildren);
            } else {
                fprintf(stderr, "Option -i requires a numeric argument.\n");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            printUsage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * Initializes the process table by setting all entries to their default, unoccupied state
 *
 * @param processTable An array of PCB structs representing the process control block table
 */
void initializeProcessTable(PCB processTable[]) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    processTable[i].occupied = 0; // Mark as not in use
    processTable[i].pid = 0; // Reset PID
    processTable[i].startSeconds = 0; // Reset start time in seconds
    processTable[i].startNano = 0; // Reset start time in nanoseconds
  }
  printf("Process table initialized.\n");
}

/**
 * Adds a process to the first unoccupied entry in the process table
 *
 * @param pid The process ID of the worker process
 * @param startSeconds The start time in seconds of the process
 * @param startNano The start time in nanoseconds of the process
 * @return The index of the process table where the process was added or -1 if the table is full
 */
int addProcessToTable(pid_t pid, int startSeconds, int startNano) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (!processTable[i].occupied) { // Look for an unoccupied slot
      processTable[i].pid = pid; // Set PID
      processTable[i].startSeconds = startSeconds; // Set start seconds
      processTable[i].startNano = startNano; // Set start nanoseconds
      processTable[i].occupied = 1; // Mark as occupied
      printf("Process %d added to table at index %d.\n", pid, i);
      return i; // Return the index where the process was added
    }
  }
  printf("Failed to add process %d; table is full.\n", pid);
  return -1; // Indicate failure to add the process
}

/**
 * Removes a process from the process table based on its process ID
 *
 * @param pid The process ID of the worker process to remove.
 */
void removeProcessFromTable(pid_t pid) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (processTable[i].occupied && processTable[i].pid == pid) { // Match found
      processTable[i].occupied = 0; // Mark as unoccupied
      printf("Process %d removed from table.\n", pid);
      break; // Exit loop once the process is found and removed
    }
  }
}

/**
 * Launches a worker process and manages its lifecycle within the simulation:
 *
 * @param simClock Pointer to the simulated clock, shared between OSS and workers
 * @param waitSeconds Seconds component of the worker's lifespan
 * @param waitNanoseconds Nanoseconds component of the worker's lifespan
 */
void launchWorker(SimulatedClock *simClock, int waitSeconds, int waitNanoseconds) {
    pid_t pid = fork(); // Create a new process

    if (pid == -1) {
        perror("fork error"); // Report if the fork operation fails
        exit(EXIT_FAILURE);
    }
    else if (pid == 0) {
        // Child process: prepare and execute the worker
        char secStr[20]; // Buffer for seconds argument
        char nanoStr[20]; // Buffer for nanoseconds argument

        // Convert wait time to strings for passing as arguments
        snprintf(secStr, sizeof(secStr), "%d", waitSeconds);
        snprintf(nanoStr, sizeof(nanoStr), "%d", waitNanoseconds);

        // Execute worker program with time parameters
        execl("./worker", "worker", secStr, nanoStr, (char *)NULL);

        // If execl returns, report an error and exit
        perror("execl error");
        exit(EXIT_FAILURE);
    }
    else {
        // Parent process: manage the worker
        // Add worker to process table and record its start time
        addProcessToTable(pid, simClock->seconds, simClock->nanoseconds);

        int status; // For storing the termination status of the child

        // Wait for the worker to terminate
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid error"); // Report if waiting fails
        }
        else if (WIFEXITED(status)) {
            // Worker exited normally, report its exit status
            int exit_status = WEXITSTATUS(status);
            printf("Worker process %d exited with status %d.\n", pid, exit_status);
        }
        else {
            // Worker terminated abnormally, report it
            printf("Worker process %d terminated abnormally.\n", pid);
        }

        // Remove the worker from the process table
        removeProcessFromTable(pid);
    }
}

/**
 * Periodically checks if conditions allow for launching new worker processes and initiates their creation.
 *
 * @param simClock Shared simulation clock
 * @param processTable Array representing the process control blocks
 * @param proc Maximum number of processes to be launched in total
 * @param simul Maximum number of processes to be running simultaneously
 * @param timelimitForChildren Upper limit for the random lifespan of child processes
 * @param intervalInMsToLaunchChildren Minimum interval in milliseconds between launching new child processes
 * @param currentSimul Pointer to the count of currently running child processes
 * @param lastLaunchTime Timestamp of the last child process launch
 */
void checkAndLaunchWorkers(SimulatedClock *simClock, PCB processTable[], int proc, int simul, int timelimitForChildren, int intervalInMsToLaunchChildren, int *currentSimul, time_t *lastLaunchTime) {
    // Guard clause to prevent launching if limits are reached
    if (*currentSimul >= simul || *currentSimul >= proc) return;

    time_t currentTime = time(NULL); // Capture the current time

    // Only proceed if enough time has elapsed since the last launch
    if (difftime(currentTime, *lastLaunchTime) * 1000 >= intervalInMsToLaunchChildren) {
        // Iterate over the process table to find an unoccupied slot
        for (int i = 0; i < MAX_PROCESSES && *currentSimul < simul; i++) {
            if (!processTable[i].occupied) {
                pid_t pid = fork(); // Fork a new process
                if (pid == 0) { // Child process logic
                    // Prepare and execute the worker with random lifespan
                    char secStr[12], nanoStr[12];
                    snprintf(secStr, sizeof(secStr), "%d", rand() % timelimitForChildren + 1);
                    snprintf(nanoStr, sizeof(nanoStr), "%d", rand() % NANOSECOND);
                    execl("./worker", "worker", secStr, nanoStr, NULL);
                    perror("execl failed"); // Execution failed
                    exit(EXIT_FAILURE);
                } else if (pid > 0) { // Parent process logic
                    // Update the process table with the new child's details
                    processTable[i].occupied = 1;
                    processTable[i].pid = pid;
                    processTable[i].startSeconds = simClock->seconds;
                    processTable[i].startNano = simClock->nanoseconds;
                    (*currentSimul)++; // Increment the count of current simulations
                    *lastLaunchTime = currentTime; // Update the last launch timestamp
                    printf("Launched worker PID: %d\n", pid); // Log the launch
                } else {
                    perror("fork failed"); // Fork failed
                }
                break; // Exit the loop after launching a worker
            }
        }
    }
}

/**
 * Checks for terminated child processes in a non-blocking manner and updates the process table accordingly
 *
 * @param processTable Array of process control blocks to track child processes
 * @param currentSimul Pointer to the count of currently running child processes
 */
void checkForTerminatedChildren(PCB processTable[], int *currentSimul) {
    int status; // Variable to capture the exit status of terminated children
    pid_t pid; // Variable to store the PID of the terminated child

    // Non-blocking check for terminated child processes
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Iterate through the process table to find the terminated child
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (processTable[i].occupied && processTable[i].pid == pid) {
                processTable[i].occupied = 0; // Mark the slot as unoccupied
                (*currentSimul)--; // Decrement the count of running processes
                printf("Child PID %d terminated.\n", pid); // Log the termination
                break; // Exit the loop once the terminated child is handled
            }
        }
    }
}

/**
 * Displays the current state of the process table along with the simulated clock time
 *
 * @param simClock Pointer to the simulated clock shared between OSS and worker processes
 * @param processTable Array representing the process control blocks
 */
void printProcessTable(SimulatedClock *simClock, PCB processTable[]) {
  // Header with the current OSS PID and simulated time
  printf("OSS PID: %d, SysClockS: %d, SysClockNano: %d\n", getpid(), simClock->seconds, simClock->nanoseconds);
  // Begin process table output
  printf("\n\nProcess Table:\n");
  printf("Entry Occupied PID StartS StartN\n");
  // Loop through each process table entry and print details for occupied slots
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      printf("%d 1 %d %d %d\n", i, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano);
    }
  }
  printf("\n"); // End of process table output
}

/**
 * Advances the simulation clock by a specified increment
 *
 * @param simClock Pointer to the simulated clock structure
 * @param increment The number of nanoseconds to add to the simulated clock
 */
void incrementSimulatedClock(SimulatedClock *simClock, int increment) {
  // Add increment to current nanoseconds, adjusting seconds as needed
  simClock->nanoseconds += increment;
  while (simClock->nanoseconds >= NANOSECOND) {
    simClock->nanoseconds -= NANOSECOND;
    simClock->seconds += 1; // Increment seconds when nanoseconds overflow
  }
}

/**
 * Handles the cleanup of shared resources and termination of child processes
 * Ensures this operation is performed only once
 */
void cleanup() {
  if (!cleanupDone) {
    // Terminate all child processes still running
    terminateAllChildren();
    // Detach from shared memory if not already done
    if (simClock != NULL && shmdt(simClock) == -1) {
      perror("Cleanup shmdt failed");
    }
    // Mark shared memory segment for deletion
    if (shmId != -1 && shmctl(shmId, IPC_RMID, NULL) == -1) {
      perror("Cleanup shmctl failed");
    }
    printf("Cleanup complete\n");
    cleanupDone = true; // Ensure cleanup only runs once
  }
}

/**
 * Responds to signals by cleaning up and exiting the program
 *
 * @param signal The signal number received
 */
void handleSignal(int signal) {
  printf("Caught signal %d. Cleaning up\n", signal);
  cleanup();
  exit(EXIT_FAILURE); // Terminate the program with a failure status
}

/**
 * Configures signal handling for graceful shutdown
 */
void setupSignalHandler() {
  struct sigaction sa; // Signal action structure
  sa.sa_handler = handleSignal; // Assign signal handling function
  sigemptyset(&sa.sa_mask); // Initialize signal set
  sa.sa_flags = 0; // No flags

  // Setup SIGINT handling
  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("Error setting up SIGINT handler");
    exit(EXIT_FAILURE);
  }

  // Setup SIGTERM handling
  if (sigaction(SIGTERM, &sa, NULL) == -1) {
    perror("Error setting up SIGTERM handler");
    exit(EXIT_FAILURE);
  }
}

/**
 * Prints the current simulated time to stdout
 *
 * @param simClock Pointer to the simulated clock shared memory
 */
void printCurrentSimulatedTime(SimulatedClock *simClock) {
  printf("Current Simulated Time: %d seconds, %d nanoseconds", simClock->seconds, simClock->nanoseconds);
}

/**
 * Sends a termination signal to all child processes and waits for them to exit
 */
void terminateAllChildren() {
    // Iterate through all potential child processes
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i].occupied) {
            // Send termination signal
            kill(processTable[i].pid, SIGTERM);
            // Wait for the process to terminate
            waitpid(processTable[i].pid, NULL, 0);
            // Mark the process table entry as unoccupied
            processTable[i].occupied = 0;
        }
    }
}

