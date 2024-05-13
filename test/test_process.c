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

void test_findFreeProcessTableEntry_Should_ReturnValidIndex_When_Called(void) {
  int index = findFreeProcessTableEntry();
  TEST_ASSERT_NOT_EQUAL(-1, index);
}

void test_registerChildProcess_Should_Register_When_CalledWithValidPID(void) {
  int index = findFreeProcessTableEntry();
  TEST_ASSERT_NOT_EQUAL(-1, index);

  registerChildProcess(mock_pid);

  TEST_ASSERT_EQUAL(mock_pid, processTable[index].pid);
  TEST_ASSERT_EQUAL(1, processTable[index].occupied);
}

void test_registerChildProcess_Should_HandleFailure_When_NoFreeEntry(void) {
  for (int i = 0; i < maxProcesses; i++) {
    processTable[i].occupied = 1;
  }

  int prev_pid = mock_pid;
  registerChildProcess(mock_pid);
  TEST_ASSERT_TRUE(prev_pid != mock_pid ||
                   processTable[maxProcesses - 1].pid != mock_pid);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_findFreeProcessTableEntry_Should_ReturnValidIndex_When_Called);
  RUN_TEST(test_registerChildProcess_Should_Register_When_CalledWithValidPID);
  RUN_TEST(test_registerChildProcess_Should_HandleFailure_When_NoFreeEntry);
  return UNITY_END();
}
