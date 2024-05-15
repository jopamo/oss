#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "process.h"
#include "queue.h"
#include "shared.h"
#include "unity.c"
#include "unity.h"
#include <string.h>

void setUp(void) {
  semUnlinkCreate();
  initializeSharedResources();

  if (initializeProcessTable() == -1 || initializeResourceTable() == -1) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to initialize all tables");
    exit(EXIT_FAILURE);
  }
}

void tearDown(void) {
  log_message(LOG_LEVEL_INFO, 0, "Starting tearDown.");
  cleanupSharedResources();
  cleanupResources();
}

void test_isProcessRunning(void) {
  processTable[0].pid = 1234;
  processTable[0].occupied = 1;
  processTable[0].state = PROCESS_RUNNING;
  TEST_ASSERT_TRUE(isProcessRunning(1234));
  processTable[0].state = PROCESS_TERMINATED;
  TEST_ASSERT_FALSE(isProcessRunning(1234));
  TEST_ASSERT_FALSE(isProcessRunning(9999));
}

void test_requestResource(void) {
  pid_t pid = 1234;
  processTable[0].pid = pid;
  processTable[0].occupied = 1;
  processTable[0].state = PROCESS_RUNNING;
  int result = requestResource(pid, 0, 5);
  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_INT(5, resourceTable[0].allocated[0]);
  TEST_ASSERT_EQUAL_INT(15,
                        resourceTable[0].available); // Adjusted to match setup
  TEST_ASSERT_EQUAL_INT(1, immediateGrantedRequests);
  TEST_ASSERT_EQUAL_INT(1, totalRequests);
}

void test_requestResource_NotEnoughResources(void) {
  pid_t pid = 1234;
  processTable[0].pid = pid;
  processTable[0].occupied = 1;
  processTable[0].state = PROCESS_RUNNING;
  resourceTable[0].available = 2;
  int result = requestResource(pid, 0, 5);
  TEST_ASSERT_EQUAL_INT(-1, result);
  TEST_ASSERT_EQUAL_INT(0, resourceTable[0].allocated[0]);
  TEST_ASSERT_EQUAL_INT(2, resourceTable[0].available);
}

void test_releaseResource(void) {
  pid_t pid = 1234;
  processTable[0].pid = pid;
  processTable[0].occupied = 1;
  processTable[0].state = PROCESS_RUNNING;
  resourceTable[0].allocated[0] = 5;
  resourceTable[0].available = 15;
  int result = releaseResource(pid, 0, 3);
  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_INT(2, resourceTable[0].allocated[0]);
  TEST_ASSERT_EQUAL_INT(18,
                        resourceTable[0].available); // Adjusted to match setup
}

void test_releaseResource_InvalidRelease(void) {
  pid_t pid = 1234;
  processTable[0].pid = pid;
  processTable[0].occupied = 1;
  processTable[0].state = PROCESS_RUNNING;
  resourceTable[0].allocated[0] = 2;
  resourceTable[0].available = 18;
  int result = releaseResource(pid, 0, 3);
  TEST_ASSERT_EQUAL_INT(-1, result);
  TEST_ASSERT_EQUAL_INT(2, resourceTable[0].allocated[0]);
  TEST_ASSERT_EQUAL_INT(18, resourceTable[0].available);
}

void test_releaseAllResourcesForProcess(void) {
  pid_t pid = 1234;
  processTable[0].pid = pid;
  processTable[0].occupied = 1;
  processTable[0].state = PROCESS_RUNNING;
  resourceTable[0].allocated[0] = 5;
  resourceTable[0].available = 15;
  releaseAllResourcesForProcess(pid);
  TEST_ASSERT_EQUAL_INT(0, resourceTable[0].allocated[0]);
  TEST_ASSERT_EQUAL_INT(20, resourceTable[0].available);
}

void test_unsafeSystem(void) {
  resourceTable[0].allocated[0] = 1;
  resourceTable[1].allocated[0] = 1;
  resourceTable[2].allocated[0] = 1;
  resourceTable[3].allocated[0] = 1;
  resourceTable[4].allocated[0] = 1;
  resourceTable[5].allocated[0] = 1;
  resourceTable[6].allocated[0] = 1;
  resourceTable[7].allocated[0] = 1;
  resourceTable[8].allocated[0] = 1;
  resourceTable[9].allocated[0] = 1;
  TEST_ASSERT_TRUE(unsafeSystem());
}

void test_resolveDeadlocks(void) {
  // Initialize the process table entries for the processes
  pid_t pids[] = {1234, 2345, 3456};
  for (int i = 0; i < 3; i++) {
    processTable[i].pid = pids[i];
    processTable[i].occupied = 1;
    processTable[i].state = PROCESS_RUNNING;
  }

  // Allocate resources to create a deadlock scenario
  resourceTable[0].allocated[0] = 19; // Process 0 holds 5 units of resource 0
  resourceTable[1].allocated[1] = 19; // Process 1 holds 5 units of resource 1
  resourceTable[2].allocated[2] = 19; // Process 2 holds 5 units of resource 2

  resourceTable[0].available = 1; // 15 units of resource 0 available
  resourceTable[1].available = 1; // 15 units of resource 1 available
  resourceTable[2].available = 1; // 15 units of resource 2 available

  log_message(LOG_LEVEL_DEBUG, 0, "Before resolving deadlocks:");
  logResourceTable();

  // Call resolveDeadlocks to detect and handle the deadlock
  resolveDeadlocks();

  log_message(LOG_LEVEL_DEBUG, 0, "After resolving deadlocks:");
  logResourceTable();

  // Validate that the resources were released and the processes were terminated
  for (int i = 0; i < 3; i++) {
    TEST_ASSERT_EQUAL_INT(0, resourceTable[i].allocated[i]);
    TEST_ASSERT_EQUAL_INT(
        20,
        resourceTable[i].available); // All resources should be available again
  }
  TEST_ASSERT_EQUAL_INT(
      3,
      terminatedByDeadlock); // Three processes should be terminated by deadlock
  for (int i = 0; i < 3; i++) {
    TEST_ASSERT_EQUAL_INT(
        PROCESS_TERMINATED,
        processTable[i].state); // Process state should be terminated
  }
}

void test_logResourceTable(void) { logResourceTable(); }

void test_logStatistics(void) {
  totalRequests = 10;
  immediateGrantedRequests = 5;
  waitingGrantedRequests = 3;
  terminatedByDeadlock = 2;
  successfullyTerminated = 4;
  deadlockDetectionRuns = 2;
  logStatistics();
}

void test_multipleResourceRequests(void) {
  pid_t pid1 = 1234, pid2 = 5678;
  processTable[0].pid = pid1;
  processTable[0].occupied = 1;
  processTable[0].state = PROCESS_RUNNING;
  processTable[1].pid = pid2;
  processTable[1].occupied = 1;
  processTable[1].state = PROCESS_RUNNING;

  int result1 = requestResource(pid1, 0, 5);
  TEST_ASSERT_EQUAL_INT(0, result1);
  TEST_ASSERT_EQUAL_INT(5, resourceTable[0].allocated[0]);
  TEST_ASSERT_EQUAL_INT(15, resourceTable[0].available);

  int result2 = requestResource(pid2, 0, 10);
  TEST_ASSERT_EQUAL_INT(0, result2);
  TEST_ASSERT_EQUAL_INT(10, resourceTable[0].allocated[1]);
  TEST_ASSERT_EQUAL_INT(5, resourceTable[0].available);

  TEST_ASSERT_EQUAL_INT(2, totalRequests);
  TEST_ASSERT_EQUAL_INT(2, immediateGrantedRequests);
}

void test_requestAndRelease(void) {
  pid_t pid = 1234;
  processTable[0].pid = pid;
  processTable[0].occupied = 1;
  processTable[0].state = PROCESS_RUNNING;

  int result1 = requestResource(pid, 0, 10);
  TEST_ASSERT_EQUAL_INT(0, result1);
  TEST_ASSERT_EQUAL_INT(10, resourceTable[0].allocated[0]);
  TEST_ASSERT_EQUAL_INT(10, resourceTable[0].available);

  int result2 = releaseResource(pid, 0, 5);
  TEST_ASSERT_EQUAL_INT(0, result2);
  TEST_ASSERT_EQUAL_INT(5, resourceTable[0].allocated[0]);
  TEST_ASSERT_EQUAL_INT(15, resourceTable[0].available);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_isProcessRunning);
  RUN_TEST(test_requestResource);
  RUN_TEST(test_requestResource_NotEnoughResources);
  RUN_TEST(test_releaseResource);
  RUN_TEST(test_releaseResource_InvalidRelease);
  RUN_TEST(test_releaseAllResourcesForProcess);
  RUN_TEST(test_unsafeSystem);
  // RUN_TEST(test_resolveDeadlocks);
  RUN_TEST(test_logResourceTable);
  RUN_TEST(test_logStatistics);
  // RUN_TEST(test_multipleResourceRequests);
  // RUN_TEST(test_requestAndRelease);
  return UNITY_END();
}
