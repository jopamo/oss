#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if(argc != 2) {
    fprintf(stderr, "Usage: %s <iterations>\n", argv[0]);
    return 1;
  }

  int iterations = atoi(argv[1]);

  for(int i = 1; i <= iterations; i++) {
    printf("USER PID:%d PPID:%d Iteration:%d before sleeping\n", getpid(), getppid(), i);
    sleep(1);
    printf("USER PID:%d PPID:%d Iteration:%d after sleeping\n", getpid(), getppid(), i);
  }

  return 0;
}
