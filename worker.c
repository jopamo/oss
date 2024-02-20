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
    return 1;
  }

  int durationSecs = atoi(argv[1]);
  int durationNanos = atoi(argv[2]);

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

  long termSecs = simClock->seconds + durationSecs;
  long termNanos = simClock->nanoseconds + durationNanos;

  if (termNanos >= 1000000000) {
    termSecs++;
    termNanos -= 1000000000;
  }


  printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %ld TermTimeNano: %ld --Just Starting\n",
       getpid(), getppid(), simClock->seconds, simClock->nanoseconds, termSecs, termNanos);

  int prevSecs = simClock->seconds;

  while (simClock->seconds < termSecs || (simClock->seconds == termSecs && simClock->nanoseconds < termNanos)) {

    if (simClock->seconds > prevSecs) {
      printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d --%d seconds have passed since starting\n",
           getpid(), getppid(), simClock->seconds, simClock->nanoseconds, simClock->seconds - prevSecs);
      prevSecs = simClock->seconds;
    }
  }

  printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %ld TermTimeNano: %ld --Terminating\n",
       getpid(), getppid(), simClock->seconds, simClock->nanoseconds, termSecs, termNanos);

  shmdt(simClock);

  return 0;
}
