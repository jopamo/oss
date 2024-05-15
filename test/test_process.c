#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "process.h"
#include "queue.h"
#include "shared.h"
#include "unity.c"
#include "unity.h"

int mock_pid = 1234;

void setUp(void) {
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

void test_registerChildProcess(void) {
  pid_t pid = 1234;
  processTable[0].occupied = 0;
  TEST_ASSERT_EQUAL_INT(0, processTable[0].occupied);

  registerChildProcess(pid);

  TEST_ASSERT_EQUAL_INT(1, processTable[0].occupied);
  TEST_ASSERT_EQUAL_INT(pid, processTable[0].pid);
  TEST_ASSERT_EQUAL_INT(PROCESS_RUNNING, processTable[0].state);
  TEST_ASSERT_EQUAL_INT(simClock->seconds, processTable[0].startSeconds);
  TEST_ASSERT_EQUAL_INT(simClock->nanoseconds, processTable[0].startNano);
}

void test_registerChildProcess_NoFreeEntry(void) {
  pid_t pid = 1234;
  for (int i = 0; i < maxProcesses; i++) {
    processTable[i].occupied = 1;
  }

  // Mock function call for kill
  TEST_ASSERT_EQUAL(-1, kill(pid, SIGTERM));

  registerChildProcess(pid);

  // No changes should be made to the process table
  for (int i = 0; i < maxProcesses; i++) {
    TEST_ASSERT_EQUAL_INT(1, processTable[i].occupied);
  }
}

void test_findFreeProcessTableEntry(void) {
  processTable[0].occupied = 1;
  processTable[1].occupied = 0;

  int index = findFreeProcessTableEntry();
  TEST_ASSERT_EQUAL_INT(1, index);
}

void test_findFreeProcessTableEntry_NoFreeEntry(void) {
  for (int i = 0; i < maxProcesses; i++) {
    processTable[i].occupied = 1;
  }

  int index = findFreeProcessTableEntry();
  TEST_ASSERT_EQUAL_INT(-1, index);
}

void test_clearProcessEntry(void) {
  processTable[0].occupied = 1;
  processTable[0].state = PROCESS_RUNNING;

  clearProcessEntry(0);

  TEST_ASSERT_EQUAL_INT(1, processTable[0].occupied);
  TEST_ASSERT_EQUAL_INT(PROCESS_TERMINATED, processTable[0].state);
}

void test_processStateToString(void) {
  TEST_ASSERT_EQUAL_STRING("Running ", processStateToString(PROCESS_RUNNING));
  TEST_ASSERT_EQUAL_STRING("Waiting", processStateToString(PROCESS_WAITING));
  TEST_ASSERT_EQUAL_STRING("Terminated",
                           processStateToString(PROCESS_TERMINATED));
  TEST_ASSERT_EQUAL_STRING("Unknown", processStateToString(-1));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_registerChildProcess);
  RUN_TEST(test_registerChildProcess_NoFreeEntry);
  RUN_TEST(test_findFreeProcessTableEntry);
  RUN_TEST(test_findFreeProcessTableEntry_NoFreeEntry);
  RUN_TEST(test_clearProcessEntry);
  RUN_TEST(test_processStateToString);
  return UNITY_END();
}
