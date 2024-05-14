#include "user_process.h"

#define MAX_RETRY_COUNT 10

void signalSafeLog(int level, const char *message) {
  if (level < currentLogLevel) {
    return;
  }

  char buffer[LOG_BUFFER_SIZE];
  int msgLength = strlen(message);

  if (msgLength > LOG_BUFFER_SIZE - 2) {
    // Ensure there's room for newline and null terminator
    msgLength = LOG_BUFFER_SIZE - 2;
  }

  memcpy(buffer, message, msgLength);
  buffer[msgLength] = '\n';
  buffer[msgLength + 1] = '\0';

  // msgLength + 1 to include the newline
  ssize_t written = write(STDERR_FILENO, buffer, msgLength + 1);
  if (written == -1) {
    // If write fails, try to handle error minimally
    static const char errorMsg[] = "Error writing log\n";
    ssize_t errorWritten =
        write(STDERR_FILENO, errorMsg,
              sizeof(errorMsg) - 1); // -1 to avoid writing the null terminator
    (void)errorWritten; // Explicitly ignore the result to avoid warnings
  }
}

void signalHandler(int sig) {
  char buffer[256];
  if (sig == SIGINT || sig == SIGTERM) {
    snprintf(buffer, sizeof(buffer),
             "Child process (PID: %d): Termination requested by signal %d.\n",
             getpid(), sig);
    signalSafeLog(LOG_LEVEL_INFO, buffer);
    _exit(EXIT_SUCCESS);
  } else if (sig == SIGCHLD) {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {
    }
  }
}

void setupSignalHandlers(void) {
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = signalHandler;

  if (sigaction(SIGINT, &sa, NULL) == -1 ||
      sigaction(SIGTERM, &sa, NULL) == -1 ||
      sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("Failed to set signal handlers");
    exit(EXIT_FAILURE);
  }
}

int better_sem_wait(sem_t *sem) {
  if (!sem) {
    log_message(LOG_LEVEL_ERROR, 0, "Invalid semaphore pointer provided.");
    return -1;
  }

  int result;
  log_message(LOG_LEVEL_ANNOY, 0, "Attempting to acquire semaphore...");
  while ((result = sem_wait(sem)) == -1 && errno == EINTR) {
    log_message(LOG_LEVEL_WARN, 0,
                "Semaphore wait was interrupted. Retrying...");
    continue; // Handle interruption by signals and retry
  }
  if (result == 0) {
    log_message(LOG_LEVEL_ANNOY, 0, "Semaphore acquired successfully.");
  } else {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to acquire semaphore: %s",
                strerror(errno));
  }
  return result;
}

int better_sem_post(sem_t *sem) {
  if (sem_post(sem) == -1) {
    switch (errno) {
    case EINVAL:
      log_message(LOG_LEVEL_ERROR, 0,
                  "Failed to release semaphore: The semaphore is invalid.");
      break;
    case EOVERFLOW:
      log_message(
          LOG_LEVEL_ERROR, 0,
          "Failed to release semaphore: The semaphore value would overflow.");
      break;
    default:
      log_message(LOG_LEVEL_ERROR, 0,
                  "Failed to release semaphore: Unknown error %s",
                  strerror(errno));
    }
    return -1;
  }
  log_message(LOG_LEVEL_ANNOY, 0, "Semaphore released successfully.");
  return 0;
}

int better_sleep(time_t sec, long nsec) {
  struct timespec req = {sec, nsec}, rem;
  while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
    log_message(LOG_LEVEL_DEBUG, 0,
                "Sleep interrupted. Sleeping again for remaining time...");
    req = rem; // Continue sleeping for the remaining time
  }
  if (errno != EINTR) {
    log_message(LOG_LEVEL_ANNOY, 0, "Sleep completed successfully.");
    return 0;
  } else {
    log_message(LOG_LEVEL_DEBUG, 0, "Failed to complete sleep: %s",
                strerror(errno));
    return -1;
  }
}

int better_mlock(const void *addr, size_t len) {
  if (mlock(addr, len) == 0) {
    log_message(LOG_LEVEL_ANNOY, 0, "Memory locked successfully.");
    return 0;
  } else {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to lock memory: %s",
                strerror(errno));
    return -1;
  }
}

void *safe_shmat(int shmId, const void *shmaddr, int shmflg) {
  void *addr;
  int retries = 5;
  while (retries--) {
    addr = shmat(shmId, shmaddr, shmflg);
    if (addr == (void *)-1 && errno == EINTR && retries) {
      log_message(LOG_LEVEL_WARN, 0, "shmat interrupted, retrying...");
      continue;
    }
    break;
  }
  return addr;
}
