#ifndef USER_PROCESS_H
#define USER_PROCESS_H

#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

void signalSafeLog(int level, const char *message);
void setupSignalHandlers(void);
int better_sem_wait(sem_t *sem);

#endif
