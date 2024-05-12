#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "resource.h"
#include "shared.h"
#include "unity.c"
#include "unity.h"

void setUp(void) {
  semUnlinkCreate();
  initializeSharedResources();

  resourceTable = malloc(MAX_RESOURCES * sizeof(ResourceDescriptor));

  for (int i = 0; i < MAX_RESOURCES; i++) {
    resourceTable[i].total = INSTANCES_PER_RESOURCE;
    resourceTable[i].available = INSTANCES_PER_RESOURCE;
    memset(resourceTable[i].allocated, 0, sizeof(resourceTable[i].allocated));
  }

  log_message(LOG_LEVEL_INFO, 0, "Resource table initialized successfully.");
}

void tearDown(void) {
  log_message(LOG_LEVEL_INFO, 0, "Starting tearDown.");
  free(resourceTable);
  resourceTable = NULL;
  cleanupSharedResources();
}

void test_initializeResourceTable(void) {
  for (int i = 0; i < MAX_RESOURCES; i++) {
    TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE, resourceTable[i].total);
    TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE, resourceTable[i].available);
    for (int j = 0; j < MAX_USER_PROCESSES; j++) {
      TEST_ASSERT_EQUAL(0, resourceTable[i].allocated[j]);
    }
  }
}

void test_requestResource_SufficientAvailable(void) {
  int result = requestResource(0, 5, 1);
  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(15, getAvailable(0));
}

void test_requestResource_InsufficientAvailable(void) {
  requestResource(0, 15, 1);
  int result = requestResource(0, 6, 2);
  TEST_ASSERT_EQUAL(-1, result);
  TEST_ASSERT_EQUAL(5, getAvailable(0));
}

void test_releaseResource(void) {
  int pid = 1, resourceType = 0, quantity = 5;
  requestResource(resourceType, quantity, pid);

  int result = releaseResource(resourceType, quantity, pid);
  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE,
                    resourceTable[resourceType].available);
  TEST_ASSERT_EQUAL(0, resourceTable[resourceType].allocated[pid]);
}

void test_unsafeSystem(void) {
  int pid1 = 1, pid2 = 2;

  // Stage a deadlock
  TEST_ASSERT_EQUAL(
      0, requestResource(0, 10, pid1)); // Process 1 fully occupies Resource 0
  TEST_ASSERT_EQUAL(
      0, requestResource(1, 10, pid2)); // Process 2 fully occupies Resource 1

  // Both processes request more resources of type other process holds
  TEST_ASSERT_EQUAL(
      -1, requestResource(1, 20,
                          pid1)); // Process 1 requests 1 unit of Resource 1
  TEST_ASSERT_EQUAL(
      -1, requestResource(0, 20,
                          pid2)); // Process 2 requests 1 unit of Resource 0

  // The system should now be in a state of deadlock
  bool deadlockDetected = unsafeSystem();
  TEST_ASSERT_EQUAL(1, deadlockDetected);
}

void test_resolveDeadlocks(void) {
  test_unsafeSystem();
  resolveDeadlocks();

  // Verify all resources are available and no process holds any resources
  for (int i = 0; i < MAX_RESOURCES; i++) {
    TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE, resourceTable[i].available);
    for (int j = 0; j < MAX_USER_PROCESSES; j++) {
      TEST_ASSERT_EQUAL(0, resourceTable[i].allocated[j]);
    }
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_initializeResourceTable);
  RUN_TEST(test_requestResource_SufficientAvailable);
  RUN_TEST(test_releaseResource);
  RUN_TEST(test_unsafeSystem);
  RUN_TEST(test_resolveDeadlocks);
  return UNITY_END();
}
