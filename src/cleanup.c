#include "process.h"

#include "cleanup.h"
#include "shared.h"

volatile sig_atomic_t cleanupInitiated = 0;

void initializeProcessTable(void) {
  processTable = malloc(DEFAULT_MAX_PROCESSES * sizeof(PCB));
  if (!processTable) {

    perror("Failed to allocate memory for processTable");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    processTable[i].occupied = 0;
  }
}

void semUnlinkCreate(void) {
  sem_unlink(clockSemName);
  clockSem = sem_open(clockSemName, O_CREAT | O_EXCL, SEM_PERMISSIONS, 1);
  if (clockSem == SEM_FAILED) {
    log_message(LOG_LEVEL_ERROR, "Failed to create semaphore: %s",
                strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void atexitHandler(void) { cleanupResources(); }

void cleanupResources(void) {
  log_message(LOG_LEVEL_INFO, "Initiating cleanup...");

  if (__sync_lock_test_and_set(&cleanupInitiated, 1)) {
    return;
  }

  semaphore_cleanup();
  logFile_cleanup();
  killAllWorkers();
  sharedMemory_cleanup();
  messageQueue_cleanup();

  cleanupInitiated = 0;

  log_message(LOG_LEVEL_INFO, "Cleanup completed. Exiting...");
}

void semaphore_cleanup(void) {
  if (clockSem != NULL && clockSem != SEM_FAILED) {
    sem_close(clockSem);
    sem_unlink(clockSemName);
    clockSem = SEM_FAILED;
  }
}

void logFile_cleanup(void) {
  if (logFile) {
    fflush(stdout);
    fclose(logFile);
    logFile = NULL;
  }
}

void cleanupSharedMemorySegment(int shmId, const char *segmentName) {
  if (shmId >= 0) {

    void *shmPtr = shmat(shmId, NULL, SHM_RDONLY);
    if (shmPtr != (void *)-1) {
      shmdt(shmPtr);
    }

    if (shmctl(shmId, IPC_RMID, NULL) == -1) {
      log_message(
          LOG_LEVEL_ERROR,
          "Failed to mark %s shared memory for deletion (shmId: %d): %s",
          segmentName, shmId, strerror(errno));
    } else {
      log_message(LOG_LEVEL_INFO,
                  "%s shared memory segment marked for deletion (shmId: %d).",
                  segmentName, shmId);
    }
  }
}

void sharedMemory_cleanup(void) {
  cleanupSharedMemorySegment(simulatedTimeShmId, "Simulated Clock");
  cleanupSharedMemorySegment(actualTimeShmId, "Actual Time");
  cleanupSharedMemorySegment(processTableShmId, "Process Table");
}

int messageQueue_cleanup(void) {
  if (msqId != -1) {
    if (msgctl(msqId, IPC_RMID, NULL) == -1) {
      perror("Message queue removal failed");
      return -1;
    }
    msqId = -1;
  }
  return 0;
}

void killAllWorkers(void) {
  log_message(LOG_LEVEL_INFO, "Sending termination signals to all processes.");

  if (timekeeperPid > 0) {
    kill(timekeeperPid, SIGTERM);
    int status;
    waitpid(timekeeperPid, &status, 0);
    log_message(LOG_LEVEL_DEBUG,
                "Termination signal sent and Timekeeper PID: %d exited.",
                timekeeperPid);
  }

  if (tableprinterPid > 0) {
    kill(tableprinterPid, SIGTERM);
    int status;
    waitpid(tableprinterPid, &status, 0);
    log_message(LOG_LEVEL_DEBUG,
                "Termination signal sent and TablePrinter PID: %d exited.",
                tableprinterPid);
  }

  Message msg = {.mtype = 1, .mtext = 0};
  for (int i = 0; i < DEFAULT_MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      msg.mtype = processTable[i].pid;
      if (sendMessage(msqId, &msg) != -1) {
        int status;
        waitpid(processTable[i].pid, &status, 0);
        log_message(LOG_LEVEL_DEBUG,
                    "Termination message sent and worker PID: %d exited.",
                    processTable[i].pid);
      } else {
        log_message(LOG_LEVEL_ERROR,
                    "Failed to send termination message to PID: %d.",
                    processTable[i].pid);
      }
      processTable[i].occupied = 0;
    }
  }
}

void timeoutHandler(int signum) {
  (void)signum;
  log_message(LOG_LEVEL_INFO, "Timeout signal received.");
  keepRunning = 0;
  cleanupAndExit();
}

void setupTimeout(int seconds) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = timeoutHandler;
  sigaction(SIGALRM, &sa, NULL);

  alarm(seconds);
  log_message(LOG_LEVEL_INFO, "Timeout setup for %d seconds.", seconds);
}

void cleanupAndExit(void) {
  cleanupResources();
  _exit(EXIT_SUCCESS);
}

pid_t forkAndExecute(const char *executable) {
  pid_t pid = fork();
  if (pid == 0) {

    execl(executable, executable, (char *)NULL);

    log_message(LOG_LEVEL_ERROR, "Failed to start process '%s': %s", executable,
                strerror(errno));
    exit(EXIT_FAILURE);
  } else if (pid < 0) {

    log_message(LOG_LEVEL_ERROR, "Failed to fork process for '%s': %s",
                executable, strerror(errno));
  }

  return pid;
}

void parentSignalHandler(int sig) {
  char buffer[256];
  switch (sig) {
  case SIGINT:
  case SIGTERM:
    snprintf(buffer, sizeof(buffer),
             "OSS (PID: %d): Shutdown requested by signal %d.", getpid(), sig);
    signalSafeLog(buffer);
    keepRunning = 0;
    break;
  case SIGALRM:
    snprintf(buffer, sizeof(buffer), "OSS (PID: %d): Timer expired.", getpid());
    signalSafeLog(buffer);
    keepRunning = 0;
    break;
  case SIGCHLD:
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    snprintf(buffer, sizeof(buffer), "OSS (PID: %d): Child process terminated.",
             getpid());
    signalSafeLog(buffer);
    break;
  default:
    snprintf(buffer, sizeof(buffer),
             "OSS (PID: %d): Unhandled signal %d received.", getpid(), sig);
    signalSafeLog(buffer);
    break;
  }
}

void setupParentSignalHandlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = parentSignalHandler;
  sigfillset(&sa.sa_mask);

  int signals[] = {SIGINT, SIGTERM, SIGALRM, SIGCHLD};
  for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
    if (sigaction(signals[i], &sa, NULL) == -1) {
      signalSafeLog("Error setting up parent signal handler. Exiting.");
      _exit(EXIT_FAILURE);
    }
  }
}
