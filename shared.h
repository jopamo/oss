#ifndef SHARED_H
#define SHARED_H

#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_PATH "/etc/passwd"
#define SHM_PROJ_ID 'b'

typedef struct {
    int seconds;
    int nanoseconds;
} SimulatedClock;

key_t getSharedMemoryKey();

#endif
