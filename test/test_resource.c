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

void test_requestResource_should_grant_when_available(void) {
  processTable[1].state = PROCESS_RUNNING;
  int result = requestResource(1, 0, 5);

  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(5, resourceTable[0].allocated[1]);
  TEST_ASSERT_EQUAL(5, resourceTable[0].available);
}

void test_requestResource_should_fail_when_not_enough_resources(void) {
  processTable[1].state = PROCESS_RUNNING;
  int result = requestResource(1, 0, 15);

  TEST_ASSERT_EQUAL(-1, result);
  TEST_ASSERT_EQUAL(0, resourceTable[0].allocated[1]);
  TEST_ASSERT_EQUAL(10, resourceTable[0].available);
}

void test_requestResource_should_fail_for_invalid_pid(void) {
  int result = requestResource(10, 0, 5);

  TEST_ASSERT_EQUAL(-1, result);
  TEST_ASSERT_EQUAL(10, resourceTable[0].available);
}

void test_requestResource_should_fail_for_non_running_process(void) {
  processTable[1].state = PROCESS_WAITING;
  int result = requestResource(1, 0, 5);

  TEST_ASSERT_EQUAL(-1, result);
  TEST_ASSERT_EQUAL(0, resourceTable[0].allocated[1]);
  TEST_ASSERT_EQUAL(10, resourceTable[0].available);
}

void test_releaseResource_should_release_correctly(void) {
  processTable[1].state = PROCESS_RUNNING;
  resourceTable[0].allocated[1] = 5;
  resourceTable[0].available = 5;

  int result = releaseResource(1, 0, 3);

  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(2, resourceTable[0].allocated[1]);
  TEST_ASSERT_EQUAL(8, resourceTable[0].available);
}

void test_releaseResource_should_fail_for_invalid_pid(void) {
  int result = releaseResource(10, 0, 3);

  TEST_ASSERT_EQUAL(-1, result);
  TEST_ASSERT_EQUAL(10, resourceTable[0].available);
}

void test_releaseResource_should_fail_for_non_running_process(void) {
  processTable[1].state = PROCESS_WAITING;
  int result = releaseResource(1, 0, 3);

  TEST_ASSERT_EQUAL(-1, result);
  TEST_ASSERT_EQUAL(0, resourceTable[0].allocated[1]);
  TEST_ASSERT_EQUAL(10, resourceTable[0].available);
}

void test_releaseAllResourcesForProcess_should_release_all_resources(void) {
  processTable[1].state = PROCESS_RUNNING;
  resourceTable[0].allocated[1] = 5;
  resourceTable[1].allocated[1] = 3;
  resourceTable[0].available = 5;
  resourceTable[1].available = 7;

  releaseAllResourcesForProcess(1);

  TEST_ASSERT_EQUAL(10, resourceTable[0].available);
  TEST_ASSERT_EQUAL(10, resourceTable[1].available);
  TEST_ASSERT_EQUAL(0, resourceTable[0].allocated[1]);
  TEST_ASSERT_EQUAL(0, resourceTable[1].allocated[1]);
}

void test_unsafeSystem_should_detect_safe_system(void) {
  for (int i = 0; i < maxProcesses; i++) {
    for (int j = 0; j < maxResources; j++) {
      resourceTable[j].allocated[i] = 0;
    }
  }

  bool isUnsafe = unsafeSystem();

  TEST_ASSERT_FALSE(isUnsafe);
}

void test_unsafeSystem_should_detect_unsafe_system(void) {
  resourceTable[0].allocated[1] = 5;
  bool isUnsafe = unsafeSystem();

  TEST_ASSERT_TRUE(isUnsafe);
}

void test_resolveDeadlocks_should_release_deadlocked_resources(void) {
  resourceTable[0].allocated[1] = 5;
  processTable[1].state = PROCESS_RUNNING;

  resolveDeadlocks();

  TEST_ASSERT_EQUAL(10, resourceTable[0].available);
  TEST_ASSERT_EQUAL(0, resourceTable[0].allocated[1]);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_requestResource_should_grant_when_available);
  RUN_TEST(test_requestResource_should_fail_when_not_enough_resources);
  RUN_TEST(test_requestResource_should_fail_for_invalid_pid);
  RUN_TEST(test_requestResource_should_fail_for_non_running_process);
  RUN_TEST(test_releaseResource_should_release_correctly);
  RUN_TEST(test_releaseResource_should_fail_for_invalid_pid);
  RUN_TEST(test_releaseResource_should_fail_for_non_running_process);
  RUN_TEST(test_releaseAllResourcesForProcess_should_release_all_resources);
  RUN_TEST(test_unsafeSystem_should_detect_safe_system);
  RUN_TEST(test_unsafeSystem_should_detect_unsafe_system);
  RUN_TEST(test_resolveDeadlocks_should_release_deadlocked_resources);

  return UNITY_END();
}
