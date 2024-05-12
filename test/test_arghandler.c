#include "arghandler.h"
#include "globals.h"
#include "init.h"
#include "shared.h"
#include "unity.c"
#include "unity.h"

void setUp(void) {
  maxProcesses = DEFAULT_MAX_PROCESSES;
  maxSimultaneous = DEFAULT_MAX_SIMULTANEOUS;
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
}

void test_isNumber_ValidNumber(void) {
  TEST_ASSERT_TRUE(isNumber("123"));
  TEST_ASSERT_TRUE(isNumber("99999"));
}

void test_isNumber_InvalidNumber(void) {
  TEST_ASSERT_FALSE(isNumber("abc"));
  TEST_ASSERT_FALSE(isNumber("-123"));
  TEST_ASSERT_FALSE(isNumber("123abc"));
  TEST_ASSERT_FALSE(isNumber(""));
  TEST_ASSERT_FALSE(isNumber("123.456"));
}

void test_ossArgs_ValidInputs(void) {
  char *argv[] = {"program", "-n",  "5",  "-s",      "3",
                  "-i",      "100", "-f", "log.txt", NULL};
  int argc = sizeof(argv) / sizeof(argv[0]) - 1; // Adjust for NULL

  TEST_ASSERT_EQUAL(0, ossArgs(argc, argv));
  TEST_ASSERT_EQUAL(5, maxProcesses);
  TEST_ASSERT_EQUAL(3, maxSimultaneous);
  TEST_ASSERT_EQUAL(100, launchInterval);
  TEST_ASSERT_EQUAL_STRING("log.txt", logFileName);
}

void test_ossArgs_MissingValues(void) {
  char *argv[] = {"program", "-n", "-s", "3", "-i", "100", "-f", NULL};
  int argc = sizeof(argv) / sizeof(argv[0]) - 1;

  TEST_ASSERT_EQUAL(ERROR_INVALID_ARGS, ossArgs(argc, argv));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_isNumber_ValidNumber);
  RUN_TEST(test_isNumber_InvalidNumber);
  RUN_TEST(test_ossArgs_ValidInputs);
  RUN_TEST(test_ossArgs_MissingValues);
  return UNITY_END();
}

/*
void test_workerArgs_ValidInputs(void) {
  char *argv[] = {"worker", "10", "500", NULL};
  int argc = sizeof(argv) / sizeof(argv[0]) -
             1; // Correctly calculate argc excluding NULL
  WorkerConfig config;

  workerArgs(argc, argv, &config);
  TEST_ASSERT_EQUAL(10, config.lifespanSeconds);
  TEST_ASSERT_EQUAL(500, config.lifespanNanoSeconds);
}

void test_workerArgs_InvalidInputs(void) {
  char *argv[] = {"worker", "not_a_number", "also_not_a_number", NULL};
  int argc = sizeof(argv) / sizeof(argv[0]) -
             1; // Correctly calculate argc excluding NULL
  WorkerConfig config;

  int result = workerArgs(argc, argv, &config);
  TEST_ASSERT_EQUAL(ERROR_INVALID_ARGS, result);
}
*/
