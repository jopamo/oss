#ifndef RESOURCE_H
#define RESOURCE_H

#include "globals.h"
#include "shared.h"

#define MSG_REQUEST_RESOURCE 1
#define MSG_RELEASE_RESOURCE 0

#define MAX_RESOURCES 10
#define INSTANCES_PER_RESOURCE 20
#define MAX_RESOURCE_TYPES 10
#define REQUEST_INTERVAL_NANOSECONDS 500000000 // Half a second in nanoseconds

typedef struct {
  int total;
  int available;
  int allocated[MAX_USER_PROCESSES];
  int availableAfter[MAX_RESOURCES];
} ResourceDescriptor;

typedef enum {
  REQUEST_RESOURCE,
  RELEASE_RESOURCE,
  TERMINATE_PROCESS
} ActionType;

extern pthread_mutex_t resourceTableMutex;

extern ResourceDescriptor *resourceTable;

extern Queue resourceWaitQueue[MAX_RESOURCES];

void log_resource_state(const char *operation, int pid, int resourceType,
                        int quantity, int availableBefore, int availableAfter);
int requestResource(int resourceType, int quantity, int pid);
int releaseResource(int resourceType, int quantity, int pid);

int initializeResourceTable(void);
int initializeResourceDescriptors(ResourceDescriptor *rd);

bool unsafeSystem(void);
void resolveDeadlocks(void);

void releaseResources(int pid, int resourceType, int quantity);
int getAvailable(int resourceType);
int getAvailableAfter(int resourceType);
int initializeResourceTable(void);

int isSystemSafe();

void releaseAllResourcesForProcess(pid_t pid);

bool canProcessFinish(int process, int available[]);

#endif
