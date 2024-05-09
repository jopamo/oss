#include "cleanup.h"
#include "globals.h"
#include "resource.h"
#include "shared.h"
#include "unity.c"
#include "unity.h"

#include <pthread.h>

// Structure used for threading in resource release tests
typedef struct {
  int resourceType;
  int quantity;
  int pid;
} ThreadArgs;

#define NUM_THREADS 5

void initializeResources() {
  for (int i = 0; i < MAX_RESOURCES; i++) {
    for (int j = 0; j < MAX_RESOURCES; j++) {
      resourceTable[i].total[j] = INSTANCES_PER_RESOURCE;
      resourceTable[i].available[j] = INSTANCES_PER_RESOURCE;
      resourceTable[i].availableAfter[j] = INSTANCES_PER_RESOURCE;
    }
    memset(resourceTable[i].allocated, 0, sizeof(resourceTable[i].allocated));
  }
}

void setUp(void) {
  log_message(LOG_LEVEL_INFO, "Starting setUp.");

  MLFQ mlfq;
  initializeQueues(&mlfq);

  semUnlinkCreate();
  initializeSharedResources();
  initializeResources();
  freeQueues(&mlfq);

  log_message(LOG_LEVEL_INFO, "Resource table initialized successfully.");
}

void tearDown(void) {
  log_message(LOG_LEVEL_INFO, "Starting tearDown.");
  freeQueues(&mlfq);
  cleanupSharedResources();
}

void test_initializeResourceTable(void) {
  for (int i = 0; i < MAX_RESOURCES; i++) {
    for (int j = 0; j < MAX_RESOURCES; j++) {
      TEST_ASSERT_EQUAL_MESSAGE(INSTANCES_PER_RESOURCE,
                                resourceTable[i].total[j],
                                "Mismatch in total resources initialization");
      TEST_ASSERT_EQUAL_MESSAGE(
          INSTANCES_PER_RESOURCE, resourceTable[i].available[j],
          "Mismatch in available resources initialization");
      TEST_ASSERT_EQUAL_MESSAGE(
          INSTANCES_PER_RESOURCE, resourceTable[i].availableAfter[j],
          "Mismatch in availableAfter resources initialization");
    }
  }
}

void test_requestResource_SufficientAvailable(void) {
  log_message(LOG_LEVEL_DEBUG,
              "Testing requestResource with sufficient resources available.");
  int pid = 1, resourceType = 0, quantity = 5;

  int beforeAvailable =
      resourceTable[resourceType]
          .available[resourceType]; // Correctly access the array element
  int result = requestResource(resourceType, quantity, pid);

  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(beforeAvailable - quantity,
                    resourceTable[resourceType].available[resourceType]);
  TEST_ASSERT_EQUAL(quantity,
                    resourceTable[resourceType].allocated[pid][resourceType]);
}

void test_releaseResource(void) {
  log_message(LOG_LEVEL_DEBUG, "Testing releaseResource.");
  int pid = 1, resourceType = 0, quantity = 5;

  resourceTable[resourceType].available[resourceType] =
      INSTANCES_PER_RESOURCE - quantity;
  resourceTable[resourceType].allocated[pid][resourceType] = quantity;

  int result = releaseResource(resourceType, quantity, pid);
  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE,
                    resourceTable[resourceType].available[resourceType]);
  TEST_ASSERT_EQUAL(0,
                    resourceTable[resourceType].allocated[pid][resourceType]);
}

void test_initializeResourceDescriptors_ValidInput(void) {
  log_message(LOG_LEVEL_DEBUG,
              "Testing initializeResourceDescriptors with valid input.");
  ResourceDescriptor rd[MAX_RESOURCES];
  initializeResourceDescriptors(rd);

  for (int i = 0; i < MAX_RESOURCES; i++) {
    for (int j = 0; j < MAX_RESOURCES; j++) {
      TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE, rd[i].total[j]);
      TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE, rd[i].available[j]);
      TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE, rd[i].availableAfter[j]);
    }
    for (int k = 0; k < MAX_USER_PROCESSES; k++) {
      for (int m = 0; m < MAX_RESOURCES; m++) {
        TEST_ASSERT_EQUAL(0, rd[i].allocated[k][m]);
      }
    }
  }
}

void test_initializeResourceDescriptors_NullPointer(void) {
  log_message(LOG_LEVEL_DEBUG,
              "Testing initializeResourceDescriptors with a null pointer.");
  TEST_ASSERT_EQUAL(-1, initializeResourceDescriptors(NULL));
}

void test_checkForDeadlocks_Detection(void) {
  log_message(LOG_LEVEL_INFO, "Setting up scenario likely to cause deadlock.");

  int pid1 = 1;
  int pid2 = 2;

  TEST_ASSERT_EQUAL(0, requestResource(0, 5, pid1));
  TEST_ASSERT_EQUAL(0, requestResource(1, 5, pid2));
  TEST_ASSERT_EQUAL(0, requestResource(1, 5, pid1));
  TEST_ASSERT_EQUAL(0, requestResource(0, 5, pid2));

  int deadlockDetected = checkForDeadlocks();
  TEST_ASSERT_EQUAL(1, deadlockDetected);
}

void test_resolveDeadlocks(void) {
  log_message(LOG_LEVEL_DEBUG, "Testing resolveDeadlocks.");
  test_checkForDeadlocks_Detection(); // Setup deadlock scenario

  resolveDeadlocks(); // Attempt to resolve

  for (int i = 0; i < MAX_RESOURCES; i++) {
    // Assuming there's a specific index or just the first one if only one type
    // per resource
    log_resource_state(
        "After resolving deadlocks", 0, i, 0, 0,
        resourceTable[i].available[0]); // Adjusted to access an element
    TEST_ASSERT_EQUAL_MESSAGE(
        INSTANCES_PER_RESOURCE, resourceTable[i].available[0],
        "Deadlock not fully resolved for resource."); // Adjusted to access an
                                                      // element
  }
}

void test_initQueue(void) {
  log_message(LOG_LEVEL_DEBUG, "Testing initQueue for a single queue.");

  Queue singleQueue;
  int initResult =
      initQueue(&singleQueue, 10);  // Testing a single queue initialization
  TEST_ASSERT_EQUAL(0, initResult); // Ensure initialization succeeded
  TEST_ASSERT_NOT_NULL(singleQueue.queue);     // Verify allocation
  TEST_ASSERT_EQUAL(0, singleQueue.front);     // Verify initial state
  TEST_ASSERT_EQUAL(0, singleQueue.rear);      // Verify initial state
  TEST_ASSERT_EQUAL(10, singleQueue.capacity); // Verify capacity set correctly
  freeQueue(&singleQueue);                     // Clean up single queue

  log_message(LOG_LEVEL_DEBUG, "Testing initQueue within MLFQ structure.");

  MLFQ mlfq;
  initResult = initializeQueues(&mlfq); // Initialize all queues within MLFQ
  TEST_ASSERT_EQUAL(0, initResult);     // Ensure MLFQ initialization succeeded
  TEST_ASSERT_NOT_NULL(mlfq.highPriority.queue); // Check high priority queue
  TEST_ASSERT_NOT_NULL(mlfq.midPriority.queue);  // Check mid priority queue
  TEST_ASSERT_NOT_NULL(mlfq.lowPriority.queue);  // Check low priority queue
  freeQueues(&mlfq);                             // Clean up MLFQ queues
}

void test_initializeQueues(void) {
  log_message(LOG_LEVEL_DEBUG, "Testing initializeQueues.");
  MLFQ mlfq;
  initializeQueues(&mlfq);
  TEST_ASSERT_NOT_NULL(mlfq.highPriority.queue);
  TEST_ASSERT_EQUAL(MAX_PROCESSES, mlfq.highPriority.capacity);
  TEST_ASSERT_EQUAL(MAX_PROCESSES, mlfq.midPriority.capacity);
  TEST_ASSERT_EQUAL(MAX_PROCESSES, mlfq.lowPriority.capacity);
  freeQueues(&mlfq);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_initializeResourceTable);
  RUN_TEST(test_requestResource_SufficientAvailable);
  RUN_TEST(test_releaseResource);
  RUN_TEST(test_initializeResourceDescriptors_ValidInput);
  RUN_TEST(test_initializeResourceDescriptors_NullPointer);
  RUN_TEST(test_checkForDeadlocks_Detection);
  RUN_TEST(test_resolveDeadlocks);
  RUN_TEST(test_initQueue);
  RUN_TEST(test_initializeQueues);
  return UNITY_END();
}
