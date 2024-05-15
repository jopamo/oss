#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "process.h"
#include "queue.h"
#include "shared.h"
#include "signals.h"
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

void test_parentSignalHandler_SIGINT(void) {
  keepRunning = 1;
  parentSignalHandler(SIGINT);
  TEST_ASSERT_EQUAL(0, keepRunning);
}

void test_parentSignalHandler_SIGTERM(void) {
  keepRunning = 1;
  parentSignalHandler(SIGTERM);
  TEST_ASSERT_EQUAL(0, keepRunning);
}

void test_parentSignalHandler_SIGALRM(void) {
  keepRunning = 1;
  parentSignalHandler(SIGALRM);
  TEST_ASSERT_EQUAL(0, keepRunning);
}

void test_parentSignalHandler_UnhandledSignal(void) {
  keepRunning = 1;
  parentSignalHandler(SIGUSR1);
  TEST_ASSERT_EQUAL(1, keepRunning);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_parentSignalHandler_SIGINT);
  RUN_TEST(test_parentSignalHandler_SIGTERM);
  RUN_TEST(test_parentSignalHandler_SIGALRM);
  RUN_TEST(test_parentSignalHandler_UnhandledSignal);
  return UNITY_END();
}
