#include "arghandler.h"
#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "process.h"
#include "queue.h"
#include "shared.h"
#include "unity.c"
#include "unity.h"

void setUp(void) { initializeSharedResources(); }

void tearDown(void) {
  if (logFile != NULL) {
    fclose(logFile);
    logFile = NULL;
  }

  cleanupSharedResources();
  cleanupResources();
}

void test_isPositiveNumber_withNullInput(void) {
  int value;
  int result = isPositiveNumber(NULL, &value);
  TEST_ASSERT_EQUAL(0, result);
}

void test_isPositiveNumber_withEmptyString(void) {
  int value;
  int result = isPositiveNumber("", &value);
  TEST_ASSERT_EQUAL(0, result);
}

void test_isPositiveNumber_withValidPositiveNumber(void) {
  int value;
  int result = isPositiveNumber("123", &value);
  TEST_ASSERT_EQUAL(1, result);
  TEST_ASSERT_EQUAL(123, value);
}

void test_isPositiveNumber_withInvalidString(void) {
  int value;
  int result = isPositiveNumber("abc", &value);
  TEST_ASSERT_EQUAL(0, result);
}

void test_isPositiveNumber_withNegativeNumber(void) {
  int value;
  int result = isPositiveNumber("-123", &value);
  TEST_ASSERT_EQUAL(0, result);
}

void test_isPositiveNumber_withZero(void) {
  int value;
  int result = isPositiveNumber("0", &value);
  TEST_ASSERT_EQUAL(0, result);
}

void test_isPositiveNumber_withMaxInt(void) {
  int value;
  char str[12];
  sprintf(str, "%d", INT_MAX);
  int result = isPositiveNumber(str, &value);
  TEST_ASSERT_EQUAL(1, result);
  TEST_ASSERT_EQUAL(INT_MAX, value);
}

void test_isPositiveNumber_withOverflowNumber(void) {
  int value;
  int result = isPositiveNumber("2147483648", &value); // INT_MAX + 1
  TEST_ASSERT_EQUAL(0, result);
}

void test_isPositiveNumber_withTrailingCharacters(void) {
  int value;
  int result = isPositiveNumber("123abc", &value);
  TEST_ASSERT_EQUAL(0, result);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_isPositiveNumber_withNullInput);
  RUN_TEST(test_isPositiveNumber_withEmptyString);
  RUN_TEST(test_isPositiveNumber_withValidPositiveNumber);
  RUN_TEST(test_isPositiveNumber_withInvalidString);
  RUN_TEST(test_isPositiveNumber_withNegativeNumber);
  RUN_TEST(test_isPositiveNumber_withZero);
  RUN_TEST(test_isPositiveNumber_withMaxInt);
  RUN_TEST(test_isPositiveNumber_withOverflowNumber);
  RUN_TEST(test_isPositiveNumber_withTrailingCharacters);
  return UNITY_END();
}
