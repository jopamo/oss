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

void test_queueInitialization(void) {
  Queue q;
  int initResult = initQueue(&q, 10);

  TEST_ASSERT_EQUAL_INT(0, initResult);
  TEST_ASSERT_NOT_NULL(q.queue);
  TEST_ASSERT_EQUAL_INT(0, q.front);
  TEST_ASSERT_EQUAL_INT(0, q.rear);
  TEST_ASSERT_EQUAL_INT(10, q.capacity);

  freeQueue(&q); // Free the queue after testing
}

void test_queueEnqueueDequeue(void) {
  Queue q;
  initQueue(&q, 5);

  MessageA5 msg = {.senderID = 123,
                   .senderPid = 1,
                   .commandType = 2,
                   .resourceType = 3,
                   .count = 4};
  enqueue(&q, msg);
  TEST_ASSERT_EQUAL_INT(1, q.rear);

  MessageA5 dequeuedMsg;
  int dequeued = dequeue(&q, &dequeuedMsg);
  TEST_ASSERT_EQUAL_INT(0, dequeued);
  TEST_ASSERT_EQUAL_INT(123, dequeuedMsg.senderID);
  TEST_ASSERT_EQUAL_INT(1, dequeuedMsg.senderPid);
  TEST_ASSERT_EQUAL_INT(2, dequeuedMsg.commandType);
  TEST_ASSERT_EQUAL_INT(3, dequeuedMsg.resourceType);
  TEST_ASSERT_EQUAL_INT(4, dequeuedMsg.count);
  TEST_ASSERT_EQUAL_INT(1, q.front);

  freeQueue(&q); // Free the queue after testing
}

void test_queueFull(void) {
  Queue q;
  initQueue(&q, 2);

  MessageA5 msg1 = {.senderID = 101,
                    .senderPid = 1,
                    .commandType = 2,
                    .resourceType = 3,
                    .count = 4};
  MessageA5 msg2 = {.senderID = 102,
                    .senderPid = 1,
                    .commandType = 2,
                    .resourceType = 3,
                    .count = 4};
  enqueue(&q, msg1);
  enqueue(&q, msg2);

  // The queue should now be full, and the next enqueue should not change
  // `rear`.
  MessageA5 msg3 = {.senderID = 103,
                    .senderPid = 1,
                    .commandType = 2,
                    .resourceType = 3,
                    .count = 4};
  enqueue(&q, msg3);
  TEST_ASSERT_EQUAL_INT(
      1, q.rear); // The rear should not advance since the queue is full

  freeQueue(&q); // Free the queue after testing
}

void test_queueEmpty(void) {
  Queue q;
  initQueue(&q, 2);

  MessageA5 dequeuedMsg;
  int dequeued = dequeue(&q, &dequeuedMsg);
  TEST_ASSERT_EQUAL_INT(-1, dequeued); // No elements to dequeue

  freeQueue(&q); // Free the queue after testing
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_queueInitialization);
  RUN_TEST(test_queueEnqueueDequeue);
  RUN_TEST(test_queueFull);
  RUN_TEST(test_queueEmpty);
  return UNITY_END();
}
