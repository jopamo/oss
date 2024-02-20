#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>

#define MAX_PROCESSES 20

typedef struct {
    int seconds;
    int nanoseconds;
} SimulatedClock;

typedef struct PCB {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
    long long termTime;
} PCB;

volatile sig_atomic_t keepRunning = 1;
SimulatedClock *simClock;
PCB processTable[MAX_PROCESSES];
int shmId;

void signalHandler(int sig) {
    (void)sig;
    keepRunning = 0;
    shmdt((void *) simClock);
    shmctl(shmId, IPC_RMID, NULL);
    exit(0);
}

void printHelp() {
    printf("Usage: oss [-h] [-n proc] [-s simul] [-t timelimitForChildren] [-i intervalInMsToLaunchChildren]\n");
    printf("  -h  Display this help message\n");
    printf("  -n proc   Total children to launch\n");
    printf("  -s simul  Children allowed simultaneously\n");
    printf("  -t timelimitForChildren  Maximum lifetime of a child in seconds\n");
    printf("  -i intervalInMsToLaunchChildren  Interval to launch children in milliseconds\n");
}

int findEmptyProcessEntry() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!processTable[i].occupied) {
            return i;
        }
    }
    return -1;
}

void setupSharedMemory() {
    key_t key = ftok("oss.c", 'R');
    shmId = shmget(key, sizeof(SimulatedClock), IPC_CREAT | 0666);
    if (shmId < 0) {
        perror("shmget");
        exit(1);
    }
    simClock = (SimulatedClock *)shmat(shmId, NULL, 0);
    if (simClock == (void *)-1) {
        perror("shmat");
        exit(1);
    }
    simClock->seconds = 0;
    simClock->nanoseconds = 0;
}

void incrementSimulatedClock(int incrementNs) {
    simClock->nanoseconds += incrementNs;
    while (simClock->nanoseconds >= 1000000000) {
        simClock->seconds += 1;
        simClock->nanoseconds -= 1000000000;
    }
}

int checkProcessTermination(PCB *process, SimulatedClock *clock) {
    long long currentTime = clock->seconds * 1000000000 + clock->nanoseconds;
    long long processTermTime = process->termTime;
    return currentTime >= processTermTime;
}

void terminateProcess(pid_t pid) {
    if (kill(pid, SIGTERM) != 0) {
        perror("Failed to terminate process");
    }
}

int main(int argc, char *argv[]) {
    int totalProcesses = 5;
    int timeLimitForChildren = 7;
    int intervalInMsToLaunchChildren = 100;
    int option, launchedProcesses = 0;
    long lastLaunchTime = 0;
    long long lastOutputTime = 0;

    signal(SIGINT, signalHandler);
    setupSharedMemory();

    char *endptr;
    while ((option = getopt(argc, argv, "hn:s:t:i:")) != -1) {
        switch (option) {
            case 'h':
                printHelp();
                return 0;
            case 'n':
                totalProcesses = strtol(optarg, &endptr, 10);
                break;
            case 't':
                timeLimitForChildren = atoi(optarg);
                break;
            case 'i':
                intervalInMsToLaunchChildren = atoi(optarg);
                break;
            default:
                printHelp();
                return 1;
        }
    }

    while (keepRunning && launchedProcesses < totalProcesses) {
        incrementSimulatedClock(100000);

        if ((simClock->seconds * 1000000000 + simClock->nanoseconds - lastLaunchTime >= intervalInMsToLaunchChildren * 1000000) &&
            findEmptyProcessEntry() != -1 && launchedProcesses < totalProcesses) {
            pid_t pid = fork();
            if (pid == 0) {
                char secArg[12], nanoArg[12];
                int durationSecs = rand() % timeLimitForChildren + 1;
                int durationNanos = rand() % 1000000000;
                sprintf(secArg, "%d", durationSecs);
                sprintf(nanoArg, "%d", durationNanos);

                execl("./worker", "worker", secArg, nanoArg, (char *)NULL);
                perror("execl failed");
                exit(1);
            } else if (pid > 0) {
                int index = findEmptyProcessEntry();
                processTable[index].occupied = 1;
                processTable[index].pid = pid;
                processTable[index].startSeconds = simClock->seconds;
                processTable[index].startNano = simClock->nanoseconds;
                processTable[index].termTime = (long long) simClock->seconds * 1000000000 + simClock->nanoseconds +
                                                timeLimitForChildren * 1000000000;
                launchedProcesses++;
                lastLaunchTime = simClock->seconds * 1000000000 + simClock->nanoseconds;
            } else {
                perror("Failed to fork");
            }
        }

        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (processTable[i].occupied && checkProcessTermination(&processTable[i], simClock)) {
                terminateProcess(processTable[i].pid);
                processTable[i].occupied = 0;
                launchedProcesses--;
            }
        }

        long long currentTime = simClock->seconds * 1000000000 + simClock->nanoseconds;
        if (currentTime - lastOutputTime >= 500000000) {
            printf("OSS PID:%d SysClockS: %d SysclockNano: %d\n", getpid(), simClock->seconds, simClock->nanoseconds);
            printf("Process Table:\n");
            printf("Entry Occupied PID StartS StartN\n");

            for (int i = 0; i < MAX_PROCESSES; i++) {
                if (processTable[i].occupied) {
                    printf("%d %d %d %d %d\n", i, processTable[i].occupied, processTable[i].pid,
                           processTable[i].startSeconds, processTable[i].startNano);
                }
            }

            lastOutputTime = currentTime;
        }

        usleep(1000);
    }

    shmdt(simClock);
    shmctl(shmId, IPC_RMID, NULL);

    return 0;
}
