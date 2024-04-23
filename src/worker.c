#include "process.h"
#include "shared.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

void logStatus(long currentSeconds, long currentNanoSeconds, long targetSeconds,
               long targetNanoSeconds, int iterations);
bool isTimeToTerminate(unsigned long currentSeconds,
                       unsigned long currentNanoSeconds,
                       unsigned long targetSeconds,
                       unsigned long targetNanoSeconds);
void normalizeTime(unsigned long *seconds, unsigned long *nanoseconds);
void sendStatusMessage(int msqId, pid_t pid, int status,
                       unsigned long usedTime);

int main(int argc, char *argv[]) {
  gProcessType = PROCESS_TYPE_WORKER;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <lifespan seconds> <lifespan nanoseconds>\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }

  initializeSharedResources();
  setupSignalHandlers();

  unsigned long lifespanSeconds = strtoul(argv[1], NULL, 10);
  unsigned long lifespanNanoSeconds = strtoul(argv[2], NULL, 10);

  sem_wait(clockSem);
  unsigned long startSeconds = simClock->seconds;
  unsigned long startNanoSeconds = simClock->nanoseconds;
  sem_post(clockSem);

  unsigned long targetSeconds = startSeconds + lifespanSeconds;
  unsigned long targetNanoSeconds = startNanoSeconds + lifespanNanoSeconds;
  normalizeTime(&targetSeconds, &targetNanoSeconds);

  srand(time(NULL) ^ (getpid() << 16));

  while (keepRunning) {
    sem_wait(clockSem);
    unsigned long currentSeconds = simClock->seconds;
    unsigned long currentNanoSeconds = simClock->nanoseconds;
    sem_post(clockSem);

    if (isTimeToTerminate(currentSeconds, currentNanoSeconds, targetSeconds,
                          targetNanoSeconds)) {
      unsigned long usedTime = rand() % 1000000000;
      sendStatusMessage(msqId, getpid(), 0, usedTime);
      break;
    } else {
      unsigned long usedTime = rand() % 1000000000;
      sendStatusMessage(msqId, getpid(), 1, usedTime);
    }
    usleep(100000);
  }

  cleanupSharedResources();
  return 0;
}

void sendStatusMessage(int msqId, pid_t pid, int status,
                       unsigned long usedTime) {
  Message msg;
  msg.mtype = pid;
  msg.mtext = status;
  msg.usedTime = usedTime;

  if (msgsnd(msqId, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
    perror("msgsnd failed");
    exit(EXIT_FAILURE);
  }
}

void normalizeTime(unsigned long *seconds, unsigned long *nanoseconds) {
  while (*nanoseconds >= 1000000000) {
    (*seconds)++;
    *nanoseconds -= 1000000000;
  }
}

bool isTimeToTerminate(unsigned long currentSeconds,
                       unsigned long currentNanoSeconds,
                       unsigned long targetSeconds,
                       unsigned long targetNanoSeconds) {
  return (currentSeconds > targetSeconds ||
          (currentSeconds == targetSeconds &&
           currentNanoSeconds >= targetNanoSeconds));
}
