#ifndef INIT_H
#define INIT_H

#include "globals.h"
#include "shared.h"

void *safe_shmat(int shmId, const void *shmaddr, int shmflg);
int initializeSimulatedClock(void);
int initializeActualTime(void);
int initializeProcessTable(void);
void initializeSharedResources(void);
int initMessageQueue(void);

#endif
