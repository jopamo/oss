#include "globals.h"
#include "resource.h"
#include "shared.h"
#include "unity.c"
#include "unity.h"

void setUp(void) {
  // Initialize necessary global structures and variables before each test
  initializeSharedResources();
}

void tearDown(void) {
  // Cleanup resources to prevent leaks and ensure clean test starts
  cleanupSharedResources();
}

// Existing tests
void test_InitializeSemaphore(void) {
  TEST_ASSERT_NOT_NULL(clockSem);
  int sem_val;
  sem_getvalue(clockSem, &sem_val);
  TEST_ASSERT_EQUAL(1, sem_val);
}

void test_InitializeSimulatedClock(void) {
  struct shmid_ds shm_info;
  shmctl(simulatedTimeShmId, IPC_STAT, &shm_info);
  TEST_ASSERT_EQUAL(sizeof(SimulatedClock), shm_info.shm_segsz);
  TEST_ASSERT_NOT_EQUAL(-1, simulatedTimeShmId);
  TEST_ASSERT_NOT_EQUAL((void *)-1, simClock);
}

void test_InitializeActualTime(void) {
  struct shmid_ds shm_info;
  shmctl(actualTimeShmId, IPC_STAT, &shm_info);
  TEST_ASSERT_EQUAL(sizeof(ActualTime), shm_info.shm_segsz);
  TEST_ASSERT_NOT_EQUAL(-1, actualTimeShmId);
  TEST_ASSERT_NOT_EQUAL((void *)-1, actualTime);
}

void test_InitializeProcessTable(void) {
  struct shmid_ds shm_info;
  shmctl(processTableShmId, IPC_STAT, &shm_info);
  TEST_ASSERT_EQUAL(DEFAULT_MAX_PROCESSES * sizeof(PCB), shm_info.shm_segsz);
  TEST_ASSERT_NOT_EQUAL(-1, processTableShmId);
  TEST_ASSERT_NOT_EQUAL((void *)-1, processTable);
}

void test_AttachDetachSharedMemory(void) {
  SimulatedClock *simClock = (SimulatedClock *)attachSharedMemory(
      SHM_PATH, SHM_PROJ_ID_SIM_CLOCK, sizeof(SimulatedClock),
      "Simulated Clock");
  TEST_ASSERT_NOT_NULL(simClock);
  TEST_ASSERT_EQUAL(0,
                    detachSharedMemory((void **)&simClock, "Simulated Clock"));
  TEST_ASSERT_NULL(simClock);
}

// New test functions
void test_initializeResourceTable(void) {
  // Check if the resource table is properly initialized
  TEST_ASSERT_NOT_NULL(resourceTable);
  for (int i = 0; i < MAX_RESOURCES; i++) {
    TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE, resourceTable[i].total[i]);
    TEST_ASSERT_EQUAL(INSTANCES_PER_RESOURCE, resourceTable[i].available[i]);
  }
}

void test_checkSafety(void) {
  // Example: Ensure function correctly identifies safe state
  // You will need specific states to test against
  TEST_ASSERT_EQUAL(
      1, checkSafety(0, 0,
                     1)); // Assuming PID 0, resourceType 0, request 1 is safe
}

void test_requestResource(void) {
  // Test resource request logic, assuming initial conditions are set
  int pid = 1, resourceType = 1, quantity = 1;
  int result = requestResource(resourceType, quantity, pid);
  TEST_ASSERT_EQUAL(0, result); // Assuming request is successful
}

void test_releaseResource(void) {
  // Test resource release logic
  int pid = 1, resourceType = 1, quantity = 1;
  requestResource(resourceType, quantity, pid); // Assume request was successful
  TEST_ASSERT_EQUAL(0, releaseResource(resourceType, quantity,
                                       pid)); // Assuming release is successful
}

// Main function to run all tests
int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_InitializeSemaphore);
  RUN_TEST(test_InitializeSimulatedClock);
  RUN_TEST(test_InitializeActualTime);
  RUN_TEST(test_InitializeProcessTable);
  RUN_TEST(test_AttachDetachSharedMemory);
  RUN_TEST(test_initializeResourceTable);
  RUN_TEST(test_checkSafety);
  RUN_TEST(test_requestResource);
  RUN_TEST(test_releaseResource);
  return UNITY_END();
}
