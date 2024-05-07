#include "globals.h"
#include "resource.h"
#include "shared.h"
#include "unity.c"
#include "unity.h"

void setUp(void) {
  // Initialize necessary global structures and variables
  initializeSharedResources();
}

void tearDown(void) {
  // Cleanup resources to prevent leaks and ensure clean test starts
  cleanupSharedResources();
}

void test_InitializeSemaphore(void) {
  // Check if semaphore is not null and is available
  TEST_ASSERT_NOT_NULL(clockSem);

  // Verify semaphore value is set to 1
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
  // Simulate attaching to shared memory
  SimulatedClock *simClock = (SimulatedClock *)attachSharedMemory(
      SHM_PATH, SHM_PROJ_ID_SIM_CLOCK, sizeof(SimulatedClock),
      "Simulated Clock");
  TEST_ASSERT_NOT_NULL(simClock);

  // Test detaching from shared memory
  TEST_ASSERT_EQUAL(0,
                    detachSharedMemory((void **)&simClock, "Simulated Clock"));
  TEST_ASSERT_NULL(simClock);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_InitializeSemaphore);
  RUN_TEST(test_InitializeSimulatedClock);
  RUN_TEST(test_InitializeActualTime);
  RUN_TEST(test_InitializeProcessTable);
  RUN_TEST(test_AttachDetachSharedMemory);
  return UNITY_END();
}
