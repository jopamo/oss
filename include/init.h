#ifndef INIT_H
#define INIT_H

#include "globals.h"
#include "queue.h"
#include "resource.h"
#include "shared.h"
#include "user_process.h"

int initializeResourceQueues(void);
int initMessageQueue(void);
int initializeSemaphore(void);
int initializeClockAndTime(void);
int initializeResourceTable(void);
int initializeProcessTable(void);
void initializeSharedResources(void);

#endif
