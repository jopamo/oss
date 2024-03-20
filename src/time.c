#include "time.h"
#include "shared.h"
#include "util.h"
#include <stdio.h>
#include <sys/time.h>

extern SystemClock* sysClock;
extern struct timeval startTime;
extern PCB processTable[MAX_WORKERS];
extern FILE* logFile;
extern char logFileName[256];

void incrementClock(SystemClock* clock, unsigned int increment) {
    unsigned int nsIncrement = increment % 1000000000;
    unsigned int secIncrement = increment / 1000000000;

    clock->nanoseconds += nsIncrement;
    if (clock->nanoseconds >= 1000000000) {
        clock->nanoseconds -= 1000000000;
        clock->seconds += 1;
    }
    clock->seconds += secIncrement;

    log_debug("Clock incremented successfully");
}

void logProcessTable() {
    struct timeval now, elapsed;
    gettimeofday(&now, NULL);
    timersub(&now, &startTime, &elapsed);

    if (!logFile) {
        fprintf(stderr, "Error: logFile is null. Attempting to reopen the log file.\n");
        logFile = fopen(logFileName, "a");
        if (!logFile) {
            perror("Failed to reopen log file");
            return;
        }
    }

    fprintf(logFile, "\nCurrent Actual Time: %ld.%06ld seconds\n", elapsed.tv_sec, elapsed.tv_usec);
    fprintf(logFile, "Current Simulated Time: %u.%09u seconds\n", sysClock->seconds, sysClock->nanoseconds);
    fprintf(logFile, "Process Table:\n");
    fprintf(logFile, "Index | Occupied | PID | Start Time (s.ns)\n");

    for (int i = 0; i < MAX_WORKERS; i++) {
        if (processTable[i].occupied) {
            fprintf(logFile, "%5d | %8d | %3d | %10u.%09u\n",
                    i, processTable[i].occupied, processTable[i].pid,
                    processTable[i].startSeconds, processTable[i].startNano);
        } else {
            fprintf(logFile, "%5d | %8d | --- | -----------\n", i, processTable[i].occupied);
        }
    }

    fflush(logFile);
}
