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

void test_findFreeProcessTableEntry_should_return_valid_index(void) {
  processTable[0].occupied = 0;
  processTable[1].occupied = 1;
  processTable[2].occupied = 1;

  int index = findFreeProcessTableEntry();

  TEST_ASSERT_EQUAL(0, index);
}

void test_findFreeProcessTableEntry_should_return_negative_when_full(void) {
  processTable[0].occupied = 1;
  processTable[1].occupied = 1;
  processTable[2].occupied = 1;

  int index = findFreeProcessTableEntry();

  TEST_ASSERT_EQUAL(-1, index);
}

void test_registerChildProcess_should_register_when_table_not_full(void) {
  pid_t test_pid = 12345;
  processTable[0].occupied = 0;

  registerChildProcess(test_pid);

  TEST_ASSERT_EQUAL(test_pid, processTable[0].pid);
  TEST_ASSERT_EQUAL(1, processTable[0].occupied);
  TEST_ASSERT_EQUAL(PROCESS_RUNNING, processTable[0].state);
}

void test_registerChildProcess_should_terminate_when_table_full(void) {
  pid_t test_pid = 12345;
  processTable[0].occupied = 1;
  processTable[1].occupied = 1;
  processTable[2].occupied = 1;

  registerChildProcess(test_pid);
}

void test_handleTermination_should_clean_up_process(void) {
  pid_t test_pid = 12345;
  processTable[0].pid = test_pid;
  processTable[0].occupied = 1;

  handleTermination(test_pid);

  TEST_ASSERT_EQUAL(0, processTable[0].occupied);
  TEST_ASSERT_EQUAL(PROCESS_TERMINATED, processTable[0].state);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_findFreeProcessTableEntry_should_return_valid_index);
  RUN_TEST(test_findFreeProcessTableEntry_should_return_negative_when_full);
  RUN_TEST(test_registerChildProcess_should_register_when_table_not_full);
  RUN_TEST(test_registerChildProcess_should_terminate_when_table_full);
  RUN_TEST(test_handleTermination_should_clean_up_process);

  return UNITY_END();
}
