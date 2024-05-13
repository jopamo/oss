#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "process.h"
#include "queue.h"
#include "shared.h"
#include "unity.c"
#include "unity.h"

void setUp(void) {
  maxResources = DEFAULT_MAX_RESOURCES;
  maxProcesses = DEFAULT_MAX_PROCESSES;
  maxSimultaneous = DEFAULT_MAX_SIMULTANEOUS;
  maxInstances = DEFAULT_MAX_INSTANCES;
  launchInterval = DEFAULT_LAUNCH_INTERVAL;
  strcpy(logFileName, DEFAULT_LOG_FILE_NAME);

  semUnlinkCreate();
  initializeSharedResources();

  if (initializeProcessTable() == -1 || initializeResourceTable() == -1) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to initialize all tables");
    exit(EXIT_FAILURE);
  }

  log_message(LOG_LEVEL_INFO, 0, "Resource table initialized successfully.");
}

void tearDown(void) {
  log_message(LOG_LEVEL_INFO, 0, "Starting tearDown.");
  cleanupSharedResources();
  cleanupResources();
}

void test_initializeResourceTable(void) {
  for (int i = 0; i < maxResources; i++) {
    TEST_ASSERT_EQUAL(maxInstances, resourceTable[i].total);
    TEST_ASSERT_EQUAL(maxInstances, resourceTable[i].available);
    for (int j = 0; j < maxSimultaneous; j++) {
      TEST_ASSERT_EQUAL(0, resourceTable[i].allocated[j]);
    }
  }
}

void test_requestResource_ValidRequest_ShouldAllocate(void) {
  int pid = 1;
  initializeResourceTable(); // Make sure resources are set to a known state
  TEST_ASSERT_EQUAL(0, requestResource(pid)); // Test allocation
  TEST_ASSERT_EQUAL(
      1, resourceTable->allocated[pid]); // Check if resource was allocated
  TEST_ASSERT_EQUAL(
      resourceTable->total - 1,
      resourceTable->available); // Ensure available resources are decremented
}

void test_requestResource_InvalidPid_ShouldFail(void) {
  int invalidPid = maxProcesses; // Use an out-of-range PID
  TEST_ASSERT_EQUAL(
      -1, requestResource(invalidPid)); // Should fail due to invalid PID
}

void test_releaseResource_ValidRelease_ShouldDeallocate(void) {
  int pid = 1;
  requestResource(pid); // Allocate first to set up the scenario
  TEST_ASSERT_EQUAL(0, releaseResource(pid)); // Test release
  TEST_ASSERT_EQUAL(
      0, resourceTable->allocated[pid]); // Check if resource was deallocated
  TEST_ASSERT_EQUAL(
      resourceTable->total,
      resourceTable->available); // Ensure available resources are incremented
}

void test_releaseResource_InvalidPid_ShouldFail(void) {
  int invalidPid = maxProcesses;                      // Use an out-of-range PID
  TEST_ASSERT_EQUAL(-1, releaseResource(invalidPid)); // Should fail
}

void test_releaseResource_NoResourceAllocated_ShouldFail(void) {
  int pid = 1; // Assuming PID 1 hasn't allocated any resource yet
  TEST_ASSERT_EQUAL(
      -1, releaseResource(pid)); // Should fail since nothing to release
}

void test_unsafeSystem(void) {
  int pid1 = 1, pid2 = 2;

  // Initialize resource table for testing purposes
  for (int i = 0; i < maxResources; i++) {
    resourceTable[i].available =
        1; // Assume each resource initially has only 1 unit available
  }

  // Process 1 requests and is allocated Resource 0
  TEST_ASSERT_EQUAL(0, requestResource(pid1));
  // Process 2 requests and is allocated Resource 1
  TEST_ASSERT_EQUAL(0, requestResource(pid2));

  // Now, attempt to cross-request resources leading to deadlock
  // Process 1 requests Resource 1, which is held by Process 2
  TEST_ASSERT_EQUAL(
      -1, requestResource(
              pid1)); // Expect failure due to lack of available resources
  // Process 2 requests Resource 0, which is held by Process 1
  TEST_ASSERT_EQUAL(
      -1, requestResource(
              pid2)); // Expect failure due to lack of available resources

  // The system should now be in a state of deadlock
  bool deadlockDetected = unsafeSystem();
  TEST_ASSERT_TRUE(deadlockDetected); // Assert that a deadlock is detected
}

void test_resolveDeadlocks(void) {
  test_unsafeSystem(); // Ensure system is in deadlock state before resolution
  resolveDeadlocks();

  // After resolving deadlocks, no resources should be allocated, and all should
  // be available
  for (int i = 0; i < maxResources; i++) {
    TEST_ASSERT_EQUAL(
        maxInstances,
        resourceTable[i].available); // Check if all resources are available
    for (int j = 0; j < maxSimultaneous; j++) {
      TEST_ASSERT_EQUAL(
          0,
          resourceTable[i].allocated[j]); // Ensure no resources are allocated
    }
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_initializeResourceTable);
  RUN_TEST(test_requestResource_ValidRequest_ShouldAllocate);
  RUN_TEST(test_requestResource_InvalidPid_ShouldFail);
  // RUN_TEST(test_releaseResource_ValidRelease_ShouldDeallocate);
  RUN_TEST(test_releaseResource_InvalidPid_ShouldFail);
  RUN_TEST(test_releaseResource_NoResourceAllocated_ShouldFail);
  // RUN_TEST(test_unsafeSystem);
  // RUN_TEST(test_resolveDeadlocks);
  return UNITY_END();
}
