#include "shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>

typedef struct {
    unsigned int lifespanSeconds;
    unsigned int lifespanNanoSeconds;
} WorkerConfig;

typedef struct {
    SystemClock *sysClock;
    int msqId;
    int shmId;
} SharedResources;

void parseCommandLineArguments(int argc, char *argv[], WorkerConfig *config);
void attachToSharedResources(SharedResources *resources);
void operateWorkerCycle(const WorkerConfig *config, SharedResources *resources);
void cleanUpResources(SharedResources *resources);
int shouldTerminate(const WorkerConfig *config, const SystemClock *sysClock);

int main(int argc, char *argv[]) {
    WorkerConfig config = {0};
    SharedResources resources = {NULL, -1, -1};

    parseCommandLineArguments(argc, argv, &config);

    // Handle signals for graceful shutdown
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    attachToSharedResources(&resources);
    if (resources.sysClock && resources.msqId != -1) {
        operateWorkerCycle(&config, &resources);
    }

    cleanUpResources(&resources);
    return EXIT_SUCCESS;
}

void parseCommandLineArguments(int argc, char *argv[], WorkerConfig *config) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s lifespanSeconds lifespanNanoSeconds\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    config->lifespanSeconds = strtoul(argv[1], NULL, 10);
    config->lifespanNanoSeconds = strtoul(argv[2], NULL, 10);
}

void attachToSharedResources(SharedResources *resources) {
    // Assumes that shared memory and message queue IDs are already known (set by `oss`)
    resources->shmId = shmget(SHM_KEY, sizeof(SystemClock), 0666);
    if (resources->shmId == -1) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }

    resources->sysClock = (SystemClock *)shmat(resources->shmId, NULL, 0);
    if (resources->sysClock == (void *)-1) {
        perror("shmat failed");
        exit(EXIT_FAILURE);
    }

    resources->msqId = msgget(MSG_KEY, 0666);
    if (resources->msqId == -1) {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }
}

void operateWorkerCycle(const WorkerConfig *config, SharedResources *resources) {
    unsigned long targetTimeSeconds = resources->sysClock->seconds + config->lifespanSeconds;
    unsigned long targetTimeNano = resources->sysClock->nanoseconds + config->lifespanNanoSeconds;
    if (targetTimeNano >= 1000000000) {
        targetTimeSeconds++;
        targetTimeNano -= 1000000000;
    }

    while (!shouldTerminate(config, resources->sysClock)) {
        Message msg;
        if (msgrcv(resources->msqId, &msg, sizeof(msg.mtext), 1, 0) == -1) {
            perror("msgrcv failed");
            break;
        }

        if (shouldTerminate(config, resources->sysClock)) {
            printf("WORKER PID: %d - Terminating\n", getpid());
            msg.mtype = 2;
            msg.mtext = getpid();
            msgsnd(resources->msqId, &msg, sizeof(msg.mtext), 0);
            break;
        } else {
            printf("WORKER PID: %d - Checking clock\n", getpid());
            msg.mtype = 1;
            msg.mtext = getpid();
            msgsnd(resources->msqId, &msg, sizeof(msg.mtext), 0);
        }
    }
}

int shouldTerminate(const WorkerConfig *config, const SystemClock *sysClock) {
    unsigned long currentSeconds = sysClock->seconds;
    unsigned long currentNano = sysClock->nanoseconds;
    unsigned long totalLifespanNS = config->lifespanSeconds * 1000000000UL + config->lifespanNanoSeconds;

    return (currentSeconds * 1000000000UL + currentNano) >= totalLifespanNS;
}

void cleanUpResources(SharedResources *resources) {
    if (resources->sysClock) {
        shmdt(resources->sysClock);
    }
    // Assuming the message queue and shared memory are cleaned up by `oss`
}
