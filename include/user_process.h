#ifndef USER_PROCESS_H
#define USER_PROCESS_H

#include <sys/mman.h>

#include "globals.h"
#include "shared.h"

void setupSignalHandlers(void);
void signalHandler(int sig);
int better_sem_wait(sem_t *sem);
int better_sem_post(sem_t *sem);
int better_sleep(time_t sec, long nsec);
int better_mlock(const void *addr, size_t len);
void *safe_shmat(int shmId, const void *shmaddr, int shmflg);
void signalSafeLog(int level, const char *message);

#endif
