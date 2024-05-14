#ifndef RESOURCE_H
#define RESOURCE_H

#include <pthread.h>

#include "process.h"
#include "shared.h"

typedef struct {
  int total;                       // Total number of instances
  int available;                   // Number of available instances
  int allocated[MAX_SIMULTANEOUS]; // Number of allocated instances for each
                                   // process
} ResourceDescriptor;

typedef enum {
  REQUEST_RESOURCE, // Request for a resource allocation
  RELEASE_RESOURCE, // Release of a resource
  TERMINATE_PROCESS // Signal to terminate a process
} ActionType;

extern pthread_mutex_t resourceTableMutex;
extern ResourceDescriptor *resourceTable;

extern int totalRequests;
extern int immediateGrantedRequests;
extern int waitingGrantedRequests;
extern int terminatedByDeadlock;
extern int successfullyTerminated;
extern int deadlockDetectionRuns;
extern int processesTerminatedByDeadlockDetection;

bool isProcessRunning(int pid);
void log_resource_state(const char *operation, int pid, int resourceType,
                        int count, int availableBefore, int availableAfter);
int requestResource(int pid, int resourceType, int count);
int releaseResource(int pid, int resourceType, int count);
void releaseAllResourcesForProcess(int pid);
void logResourceTable(void);
bool unsafeSystem(void);
void resolveDeadlocks(void);
void logStatistics(void);

#endif
