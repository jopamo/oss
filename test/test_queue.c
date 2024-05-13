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

  semUnlinkCreate();
  initializeSharedResources();
}

void tearDown(void) {
  cleanupSharedResources();
  cleanupResources();
}

void test_queueInitialization(void) {
  Queue q;
  initQueue(&q, 10);

  TEST_ASSERT_EQUAL_INT(0, initQueue(&q, 10));
  TEST_ASSERT_NOT_NULL(q.queue);
  TEST_ASSERT_EQUAL_INT(0, q.front);
  TEST_ASSERT_EQUAL_INT(0, q.rear);
  TEST_ASSERT_EQUAL_INT(10, q.capacity);

  freeQueue(&q); // Free the queue after testing
}

void test_queueEnqueueDequeue(void) {
  Queue q;
  initQueue(&q, 5);

  MessageA5 msg = {123, 1, 2, 3};
  enqueue(&q, msg);
  TEST_ASSERT_EQUAL_INT(1, q.rear);

  MessageA5 dequeuedMsg;
  int dequeued = dequeue(&q, &dequeuedMsg);
  TEST_ASSERT_EQUAL_INT(0, dequeued);
  TEST_ASSERT_EQUAL_INT(123, dequeuedMsg.senderPid);
  TEST_ASSERT_EQUAL_INT(1, dequeuedMsg.commandType);
  TEST_ASSERT_EQUAL_INT(2, dequeuedMsg.resourceType);
  TEST_ASSERT_EQUAL_INT(3, dequeuedMsg.count);
  TEST_ASSERT_EQUAL_INT(1, q.front);

  freeQueue(&q); // Free the queue after testing
}

void test_queueFull(void) {
  Queue q;
  initQueue(&q, 2);

  MessageA5 msg1 = {101, 1, 2, 3};
  MessageA5 msg2 = {102, 1, 2, 3};
  enqueue(&q, msg1);
  enqueue(&q, msg2);

  // The queue should now be full, and the next enqueue should not change
  // `rear`.
  MessageA5 msg3 = {103, 1, 2, 3};
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
