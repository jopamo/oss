#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "process.h"
#include "queue.h"
#include "shared.h"
#include "unity.c"
#include "unity.h"

void setUp(void) {
  semUnlinkCreate();
  initializeSharedResources();
}

void tearDown(void) {
  cleanupSharedResources();
  cleanupResources();
}

void test_attachSharedMemory(void) {
  void *memory = attachSharedMemory("/somepath", 1, 1024, "TestSegment");
  TEST_ASSERT_NOT_NULL(memory);
  detachSharedMemory(&memory, "TestSegment");
}

void test_detachSharedMemory(void) {
  void *memory = attachSharedMemory("/somepath", 1, 1024, "TestSegment");
  TEST_ASSERT_EQUAL(0, detachSharedMemory(&memory, "TestSegment"));
  TEST_ASSERT_NULL(memory);
}

void test_detachSharedMemoryFail(void) {
  void *memory = NULL; // Simulating invalid pointer
  TEST_ASSERT_EQUAL(-1, detachSharedMemory(&memory, "TestSegment"));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_attachSharedMemory);
  RUN_TEST(test_detachSharedMemory);
  RUN_TEST(test_detachSharedMemoryFail);
  return UNITY_END();
}
