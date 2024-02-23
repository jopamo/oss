#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/shm.h>
#include <string.h>

#include "shared.h"

void processArgs(int argc, char *argv[], int *addSeconds, int *addNanoseconds) {
    if (argc != 3) {
        fprintf(stderr, "Error: Incorrect number of arguments.\n");
        fprintf(stderr, "Usage: %s <seconds> <nanoseconds>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Validate that arguments are numeric before converting
    if (!isNumeric(argv[1]) || !isNumeric(argv[2])) {
        fprintf(stderr, "Error: Both arguments must be numeric.\n");
        exit(EXIT_FAILURE);
    }

    long long seconds = atoll(argv[1]);
    long long nanoseconds = atoll(argv[2]);

    // Additional range checking remains unchanged
    if (seconds < 0 || nanoseconds < 0 || nanoseconds >= 1000000000) {
        fprintf(stderr, "Error: Seconds must be >= 0, nanoseconds must be between 0 and 999999999.\n");
        exit(EXIT_FAILURE);
    }

    *addSeconds = (int)seconds;
    *addNanoseconds = (int)nanoseconds;
}

int main(int argc, char *argv[]) {
    int addSeconds, addNanoseconds;

    processArgs(argc, argv, &addSeconds, &addNanoseconds);

    key_t key = getSharedMemoryKey();
    int shmId = shmget(key, sizeof(SimulatedClock), 0666);
    if (shmId < 0) {
        perror("shmget error");
        exit(EXIT_FAILURE);
    }

    SimulatedClock *simClock = (SimulatedClock *)shmat(shmId, NULL, 0);
    if (simClock == (void *)-1) {
        perror("shmat error");
        exit(EXIT_FAILURE);
    }

    // Calculate and wait for the termination time
    long long termTimeS = simClock->seconds + addSeconds;
    long long termTimeN = simClock->nanoseconds + addNanoseconds;
    while (termTimeN >= 1000000000) {
        termTimeN -= 1000000000;
        termTimeS += 1;
    }

    // Busy-wait loop until the calculated termination time
    while (simClock->seconds < termTimeS || (simClock->seconds == termTimeS && simClock->nanoseconds < termTimeN)) {
        sched_yield();
    }

    // Detach from shared memory
    if (shmdt(simClock) == -1) {
        perror("shmdt error");
        exit(EXIT_FAILURE);
    }

    return 0;
}
