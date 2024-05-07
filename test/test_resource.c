#include "globals.h"
#include "resource.h"
#include "shared.h"
#include "unity.c"
#include "unity.h"

#include <pthread.h>

typedef struct {
  int resourceType;
  int quantity;
  int pid;
} ThreadArgs;

#define NUM_THREADS 5

void setUp(void) {
  // Initialize necessary global structures and variables
  initializeSharedResources();
  resourceTable->available[1] = INSTANCES_PER_RESOURCE;
}

void tearDown(void) {
  // Cleanup resources to prevent leaks and ensure clean test starts
  cleanupSharedResources();
}

void *release_resource_thread(void *args) {
  ThreadArgs *targs = (ThreadArgs *)args;
  releaseResource(targs->resourceType, targs->quantity, targs->pid);
  return NULL;
}

void test_initializeResourceDescriptors_ValidInput(void) {
  ResourceDescriptor rd;
  initializeResourceDescriptors(&rd);

  for (int i = 0; i < MAX_RESOURCES; i++) {
    printf("Testing resource %d: total = %d, expected = %d\n", i, rd.total[i],
           INSTANCES_PER_RESOURCE);
    TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE, rd.total[i]);
    TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE, rd.available[i]);
  }
}

void redirect_stderr_to_buffer(char *buffer, int size, FILE **temp_file) {
  fflush(stderr);
  *temp_file = tmpfile();
  int fd = fileno(*temp_file);
  dup2(fd, STDERR_FILENO);
  setvbuf(stderr, buffer, _IOFBF, size);
}

void restore_stderr(int original_fd) {
  fflush(stderr);
  dup2(original_fd, STDERR_FILENO);
}

void test_initializeResourceDescriptors_NullPointer(void) {
  char buffer[1024] = {0};
  FILE *temp_file;
  int original_stderr = dup(STDERR_FILENO);

  redirect_stderr_to_buffer(buffer, sizeof(buffer), &temp_file);

  initializeResourceDescriptors(NULL);

  restore_stderr(original_stderr);

  // Find the substring irrespective of prefix
  TEST_ASSERT_NOT_NULL(strstr(
      buffer, "Null pointer provided to initializeResourceDescriptors."));

  fclose(temp_file);
}

void setupResourceState(int pid, int resourceType, int available, int allocated,
                        int maxDemand) {
  resourceTable->available[resourceType] = available;
  resourceTable->allocated[pid][resourceType] = allocated;
  maximum[pid][resourceType] = maxDemand;
}

void test_SafetyCheck(void) {
  // Define process identifiers and resource request amounts
  int safePid = 1, unsafePid = 2, resourceType = 0, safeRequest = 3,
      unsafeRequest = 10;

  // Configure initial resource states for safe and unsafe scenarios
  setupResourceState(safePid, resourceType, 10, 2,
                     5); // Config for safe scenario
  setupResourceState(unsafePid, resourceType, 10, 2,
                     10); // Config for unsafe scenario

  // Validate handling of a safe resource request
  TEST_ASSERT_EQUAL(0, requestResource(resourceType, safeRequest, safePid));

  // Validate handling of an unsafe resource request
  TEST_ASSERT_EQUAL(-1,
                    requestResource(resourceType, unsafeRequest, unsafePid));

  // Verify resource availability after processing the safe request
  TEST_ASSERT_EQUAL(7, resourceTable->available[resourceType]);

  // Reset resource states to ensure test isolation
  setupResourceState(safePid, resourceType, 10, 0, 5);
  setupResourceState(unsafePid, resourceType, 10, 0, 10);
}

void test_initializeResourceTable(void) {
  // Check if the shared memory ID is valid (indicating successful shmget call).
  TEST_ASSERT_GREATER_THAN(-1, resourceTableShmId);

  // Check if the shared memory was attached correctly (indicating successful
  // shmat call).
  TEST_ASSERT_NOT_EQUAL(NULL, resourceTable);

  // Verify each field of each ResourceDescriptor is initialized as expected.
  for (int i = 0; i < MAX_RESOURCES; i++) {
    TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE, resourceTable[i].total[i]);
    TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE, resourceTable[i].available[i]);
    for (int j = 0; j < MAX_USER_PROCESSES; j++) {
      TEST_ASSERT_EQUAL(0, resourceTable[i].allocated[j][i]);
    }
  }
}

void test_ResourceAllocation(void) {
  int pid = 1;
  int resourceType = 1;
  int quantity = 5;

  // Test allocation
  TEST_ASSERT_EQUAL(0, requestResource(resourceType, quantity, pid));
  TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE - quantity,
                    resourceTable->available[resourceType]);
  TEST_ASSERT_EQUAL(quantity, resourceTable->allocated[pid][resourceType]);

  // Test releasing
  TEST_ASSERT_EQUAL(0, releaseResource(resourceType, quantity, pid));
  TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE,
                    resourceTable->available[resourceType]);
  TEST_ASSERT_EQUAL(0, resourceTable->allocated[pid][resourceType]);
}

void test_releaseResource() {
  int pid = 1;
  int resourceType = 0;
  int quantity = 5;
  int initialQuantity = 10;
  int expectedAvailable =
      0 +
      NUM_THREADS *
          quantity; // Initial resources are 0, each thread releases 'quantity'

  // Allocate initial resources plus enough for each thread to release
  resourceTable->allocated[pid][resourceType] =
      initialQuantity + NUM_THREADS * quantity;
  resourceTable->available[resourceType] = 0;

  pthread_t threads[NUM_THREADS];
  ThreadArgs args = {resourceType, quantity, pid};

  // Launch threads to release resources concurrently
  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_create(&threads[i], NULL, release_resource_thread, &args);
  }

  // Wait for all threads to complete
  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  // The expected final allocated should consider the total released
  int expectedFinalAllocated =
      initialQuantity + NUM_THREADS * quantity - NUM_THREADS * quantity;

  // Assertions to verify the final state of resourceTable
  TEST_ASSERT_EQUAL(expectedAvailable, resourceTable->available[resourceType]);
  TEST_ASSERT_EQUAL(
      expectedFinalAllocated,
      resourceTable
          ->allocated[pid][resourceType]); // Should reflect the releases
}

void test_initQueue() {
  Queue q;
  initQueue(&q, 10);
  TEST_ASSERT_NOT_NULL(q.queue);
  TEST_ASSERT_EQUAL(0, q.front);
  TEST_ASSERT_EQUAL(0, q.rear);
  TEST_ASSERT_EQUAL(10, q.capacity);
  free(q.queue); // Clean up
}

void test_initializeQueues() {
  initializeQueues();
  TEST_ASSERT_NOT_NULL(mlfq.highPriority.queue);
  TEST_ASSERT_NOT_NULL(mlfq.midPriority.queue);
  TEST_ASSERT_NOT_NULL(mlfq.lowPriority.queue);
  TEST_ASSERT_EQUAL(MAX_PROCESSES, mlfq.highPriority.capacity);
  TEST_ASSERT_EQUAL(MAX_PROCESSES, mlfq.midPriority.capacity);
  TEST_ASSERT_EQUAL(MAX_PROCESSES, mlfq.lowPriority.capacity);
  // Assume cleanup functions exist to free these queues
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_initializeResourceTable);
  RUN_TEST(test_initializeResourceDescriptors_ValidInput);
  RUN_TEST(test_initializeResourceDescriptors_NullPointer);
  RUN_TEST(test_ResourceAllocation);
  RUN_TEST(test_SafetyCheck);
  RUN_TEST(test_releaseResource);
  RUN_TEST(test_initQueue);
  RUN_TEST(test_initializeQueues);
  return UNITY_END();
}
