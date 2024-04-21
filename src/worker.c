#include "process.h"
#include "shared.h"

#include <stdbool.h>

void logStatus(long currentSeconds, long currentNanoSeconds, long targetSeconds,
               long targetNanoSeconds, int iterations);

bool isTimeToTerminate(unsigned long currentSeconds,
                       unsigned long currentNanoSeconds,
                       unsigned long targetSeconds,
                       unsigned long targetNanoSeconds);

void normalizeTime(unsigned long *seconds, unsigned long *nanoseconds);
void sendStatusMessage(int msqId, pid_t pid, int status);

int main(int argc, char *argv[]) {
  gProcessType = PROCESS_TYPE_WORKER;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <lifespan seconds> <lifespan nanoseconds>\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }

  initializeSharedResources();
  msqId = initMessageQueue();
  setupSignalHandlers();

  unsigned long lifespanSeconds = strtoul(argv[1], NULL, 10);
  unsigned long lifespanNanoSeconds = strtoul(argv[2], NULL, 10);

  unsigned long startSeconds, startNanoSeconds;
  sem_wait(clockSem);
  startSeconds = simClock->seconds;
  startNanoSeconds = simClock->nanoseconds;
  sem_post(clockSem);

  unsigned long targetSeconds = startSeconds + lifespanSeconds;
  unsigned long targetNanoSeconds = startNanoSeconds + lifespanNanoSeconds;
  normalizeTime(&targetSeconds, &targetNanoSeconds);

  logStatus(startSeconds, startNanoSeconds, targetSeconds, targetNanoSeconds,
            0);

  int iterations = 0;
  while (keepRunning) {
    log_message(LOG_LEVEL_DEBUG,
                "[Worker %d] Loop start. Waiting for message...", getpid());

    Message msg;
    if (receiveMessage(msqId, &msg, getpid(), 0) == -1) {
      if (errno != EINTR) {
        log_message(
            LOG_LEVEL_ERROR,
            "[Worker %d] Error receiving message, exiting loop. Error: %s",
            getpid(), strerror(errno));
        break;
      }
      log_message(LOG_LEVEL_DEBUG,
                  "[Worker %d] Interrupted by signal, continuing...", getpid());
      continue;
    }

    log_message(LOG_LEVEL_DEBUG,
                "[Worker %d] Message received. Checking clock...", getpid());

    sem_wait(clockSem);
    unsigned long currentSeconds = simClock->seconds;
    unsigned long currentNanoSeconds = simClock->nanoseconds;
    sem_post(clockSem);

    log_message(LOG_LEVEL_DEBUG, "[Worker %d] Current Time: %lu.%lu", getpid(),
                currentSeconds, currentNanoSeconds);

    iterations++;
    logStatus(currentSeconds, currentNanoSeconds, targetSeconds,
              targetNanoSeconds, iterations);

    if (isTimeToTerminate(currentSeconds, currentNanoSeconds, targetSeconds,
                          targetNanoSeconds)) {
      log_message(
          LOG_LEVEL_DEBUG,
          "[Worker %d] Time to terminate. Sending termination message...",
          getpid());
      sendStatusMessage(msqId, getpid(), 0);
      log_message(LOG_LEVEL_DEBUG,
                  "[Worker %d] Termination message sent. Exiting loop.",
                  getpid());
      break;
    } else {
      log_message(
          LOG_LEVEL_DEBUG,
          "[Worker %d] Not time to terminate. Sending continue message...",
          getpid());
      sendStatusMessage(msqId, getpid(), 1);
      log_message(LOG_LEVEL_DEBUG, "[Worker %d] Continue message sent.",
                  getpid());
    }
  }
  cleanupSharedResources();
  return 0;
}

void logStatus(long currentSeconds, long currentNanoSeconds, long targetSeconds,
               long targetNanoSeconds, int iterations) {
  char statusMessage[256];
  snprintf(statusMessage, sizeof(statusMessage),
           "WORKER PID:%d PPID:%d SysClockS: %ld SysclockNano: %ld TermTimeS: "
           "%ld TermTimeNano: %ld --%d iteration(s)",
           getpid(), getppid(), currentSeconds, currentNanoSeconds,
           targetSeconds, targetNanoSeconds, iterations);

  log_message(LOG_LEVEL_INFO, "%s", statusMessage);
}

void normalizeTime(unsigned long *seconds, unsigned long *nanoseconds) {
  while (*nanoseconds >= NANOSECONDS_IN_SECOND) {
    (*seconds)++;
    *nanoseconds -= NANOSECONDS_IN_SECOND;
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

void sendStatusMessage(int msqId, pid_t pid, int status) {
  Message msg;
  msg.mtype = pid;
  msg.mtext = status;

  if (sendMessage(msqId, &msg) == -1) {
    log_message(LOG_LEVEL_ERROR, "[Worker %d] Failed to send status message.",
                getpid());
  } else {
    log_message(LOG_LEVEL_DEBUG,
                "[Worker %d] Status message sent successfully. Status: %d",
                getpid(), status);
  }
}
