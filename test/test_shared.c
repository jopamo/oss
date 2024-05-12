#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "process.h"
#include "shared.h"
#include "unity.c"
#include "unity.h"

key_t expectedKey = 1234;
int mock_shmget_id = 4567;
void *mock_shmaddr = (void *)12345678;

void setUp(void) {
  semUnlinkCreate();
  initializeSharedResources();
}

void tearDown(void) { cleanupSharedResources(); }

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

void test_messageSendingReceiving(void) {
  int msqId = initMessageQueue();
  MessageA5 msg = {123, 1, 5, 10};
  TEST_ASSERT_EQUAL(0, sendMessage(msqId, &msg, sizeof(msg)));
  MessageA5 receivedMsg;
  TEST_ASSERT_EQUAL(
      0, receiveMessage(msqId, &receivedMsg, sizeof(receivedMsg), 123, 0));
  TEST_ASSERT_EQUAL(msg.senderPid, receivedMsg.senderPid);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_attachSharedMemory);
  RUN_TEST(test_detachSharedMemory);
  RUN_TEST(test_detachSharedMemoryFail);
  RUN_TEST(test_messageSendingReceiving);
  return UNITY_END();
}
