#ifndef RESOURCE_H
#define RESOURCE_H

#include "globals.h" // Assuming this includes necessary definitions like ResourceDescriptor

// Function to initialize the resource table in shared memory
void initializeResourceTable(void);

// Function to initialize resource descriptors
void initializeResourceDescriptors(ResourceDescriptor *rd);

// Checks if the current state is safe or if there's a potential for deadlock
int checkSafety(int pid, int resourceType, int request);

// Logs the state change of resources for debugging or record-keeping
void log_resource_state(const char *operation, int pid, int resourceType,
                        int quantity, int availableBefore, int availableAfter);

// Attempts to allocate resources to a process
int requestResource(int resourceType, int quantity, int pid);

// Releases allocated resources from a process
int releaseResource(int resourceType, int quantity, int pid);

// Initializes queues for managing process scheduling
void initializeQueues(void);

// Checks if it is time to check for deadlocks based on simulation conditions
int timeToCheckDeadlock(void);

// Checks for the presence of deadlocks in the system
int checkForDeadlocks(void);

// Resolves detected deadlocks by possibly terminating processes or reallocating
// resources
void resolveDeadlocks(void);

// Releases specific resources from a process, used for message handling
void releaseResources(int pid, int resourceType, int quantity);

// Handles incoming requests for resources, encapsulating logic to handle
// message passing
void handleResourceRequest(int pid, int resourceType, int quantity);

#endif // RESOURCE_H
