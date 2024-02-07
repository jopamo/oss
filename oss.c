#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

void printHelp() {
  printf("Usage: oss [-h] [-n proc] [-s simul] [-t iter]\n");
  printf("  -h    Display this help message\n");
  printf("  -n proc   Number of total children to launch\n");
  printf("  -s simul  Number of children to run simultaneously\n");
  printf("  -t iter   Number of iterations for each user process\n");
}

int main(int argc, char *argv[]) {
  int option;
  int totalProcesses = 5;
  int simultaneousProcesses = 3;
  int iterations = 7;

  while ((option = getopt(argc, argv, "hn:s:t:")) != -1) {
    switch (option) {
      case 'h':
        printHelp();
        return 0;
      case 'n':
        totalProcesses = atoi(optarg);
        break;
      case 's':
        simultaneousProcesses = atoi(optarg);
        break;
      case 't':
        iterations = atoi(optarg);
        break;
      default:
        printHelp();
        return 1;
    }
  }

  int activeProcesses = 0;
  int launchedProcesses = 0;

  while (launchedProcesses < totalProcesses) {
    if (activeProcesses < simultaneousProcesses) {
      pid_t pid = fork();
      if (pid == 0) {
        char iterParam[10];
        sprintf(iterParam, "%d", iterations);
        execl("./user", "user", iterParam, (char *)NULL);
        perror("execl");
        exit(1);
      } else if (pid > 0) {
        activeProcesses++;
        launchedProcesses++;
      } else {
        perror("fork");
        exit(1);
      }
    }

    if (activeProcesses >= simultaneousProcesses || launchedProcesses == totalProcesses) {
      wait(NULL);
      activeProcesses--;
    }
  }

  while (activeProcesses > 0) {
    wait(NULL);
    activeProcesses--;
  }

  return 0;
}
