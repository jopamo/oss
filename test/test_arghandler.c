#include "arghandler.h"
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

  // Ensure log file is closed if opened
  if (logFile) {
    fclose(logFile);
    logFile = NULL;
  }

  initializeSharedResources();
}

void tearDown(void) {
  if (logFile != NULL) {
    fclose(logFile);
    logFile = NULL;
  }

  cleanupSharedResources();
  cleanupResources();
}

void test_isNumber_ValidNumber(void) {
  int outValue;
  TEST_ASSERT_TRUE(isPositiveNumber("123", &outValue));
  TEST_ASSERT_TRUE(isPositiveNumber("99999", &outValue));
}

void test_isNumber_InvalidNumber(void) {
  int outValue;
  TEST_ASSERT_FALSE(isPositiveNumber("abc", &outValue));
  TEST_ASSERT_FALSE(isPositiveNumber("-123", &outValue));
  TEST_ASSERT_FALSE(isPositiveNumber("123abc", &outValue));
  TEST_ASSERT_FALSE(isPositiveNumber("", &outValue));
  TEST_ASSERT_FALSE(isPositiveNumber("123.456", &outValue));
}

void test_psmgmtArgs_ValidInputs(void) {
  char *argv[] = {"program", "-n",      "5",  "-s", "3",  "-i", "100",
                  "-f",      "log.txt", "-r", "10", "-u", "20", NULL};
  int argc = sizeof(argv) / sizeof(argv[0]) - 1;

  TEST_ASSERT_EQUAL(0, psmgmtArgs(argc, argv));
  TEST_ASSERT_EQUAL(5, maxProcesses);
  TEST_ASSERT_EQUAL(3, maxSimultaneous);
  TEST_ASSERT_EQUAL(100, launchInterval);
  TEST_ASSERT_EQUAL_STRING("log.txt", logFileName);
  TEST_ASSERT_EQUAL(10, maxResources);
  TEST_ASSERT_EQUAL(20, maxInstances);
}

void test_psmgmtArgs_InvalidInputs(void) {
  char *argv[] = {"program", "-n",      "200", "-s",  "20", "-i", "abc",
                  "-f",      "log.txt", "-r",  "101", "-u", "51", NULL};
  int argc = sizeof(argv) / sizeof(argv[0]) - 1;

  TEST_ASSERT_EQUAL(ERROR_INVALID_ARGS, psmgmtArgs(argc, argv));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_isNumber_ValidNumber);
  RUN_TEST(test_isNumber_InvalidNumber);
  RUN_TEST(test_psmgmtArgs_ValidInputs);
  RUN_TEST(test_psmgmtArgs_InvalidInputs);
  return UNITY_END();
}
