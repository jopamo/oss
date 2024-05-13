#ifndef RESOURCE_H
#define RESOURCE_H

#include "globals.h"
#include "process.h"
#include "shared.h"
#include "user_process.h"

void releaseAllResourcesForProcess(int pid);

// Defines the structure for a resource descriptor
typedef struct {
  int total;     // Total units of the resource available in the system
  int available; // Units currently available for allocation
  int allocated[MAX_INSTANCES]; // Array tracking units allocated to each
                                // process
} ResourceDescriptor;

// Enum for different types of actions that can be requested by processes
typedef enum {
  REQUEST_RESOURCE, // Request for a resource allocation
  RELEASE_RESOURCE, // Release of a resource
  TERMINATE_PROCESS // Signal to terminate a process
} ActionType;

extern pthread_mutex_t resourceTableMutex;
extern ResourceDescriptor *resourceTable;

void log_resource_state(const char *operation, int pid, int total,
                        int available);
int requestResource(int pid);
int releaseResource(int pid);
void releaseResources(int pid);
int initializeResourceTable(void);
bool unsafeSystem(void);
void resolveDeadlocks(void);
void logResourceTable(void);

#endif // RESOURCE_H
