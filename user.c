#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

typedef struct {
  int seconds;
  int nanoseconds;
} SimulatedClock;

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s seconds nanoseconds\n", argv[0]);
    return 1; // Exit with error if the argument count is incorrect
  }

  int durationSecs = atoi(argv[1]);
  int durationNanos = atoi(argv[2]);

  // Attach to the shared memory segment for the simulated system clock
  key_t key = ftok("oss.c", 'R');
  int shmId = shmget(key, sizeof(SimulatedClock), 0666);
  if (shmId < 0) {
    perror("shmget");
    exit(1);
  }

  SimulatedClock *simClock = (SimulatedClock *)shmat(shmId, NULL, 0);
  if (simClock == (void *)-1) {
    perror("shmat");
    exit(1);
  }

  // Calculate termination time
  int termSecs = simClock->seconds + durationSecs;
  int termNanos = simClock->nanoseconds + durationNanos;
  if (termNanos >= 1000000000) {
    termSecs += 1;
    termNanos -= 1000000000;
  }

  printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n",
       getpid(), getppid(), simClock->seconds, simClock->nanoseconds, termSecs, termNanos);
  printf("--Just Starting\n");

  int prevSecs = simClock->seconds;
  // Loop until termination time is reached
  while (simClock->seconds < termSecs || (simClock->seconds == termSecs && simClock->nanoseconds < termNanos)) {
    if (simClock->seconds > prevSecs) {
      printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n",
           getpid(), getppid(), simClock->seconds, simClock->nanoseconds, termSecs, termNanos);
      printf("--%d seconds have passed since starting\n", simClock->seconds - prevSecs);
      prevSecs = simClock->seconds;
    }
  }

  printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n",
       getpid(), getppid(), simClock->seconds, simClock->nanoseconds, termSecs, termNanos);
  printf("--Terminating\n");

  // Detach from the shared memory segment
  shmdt(simClock);

  return 0;
}
