#include "shared.h"

/**
 * Generates a System V IPC key for shared memory access, utilizes ftok
 * to generate a unique key based on a pathname and project identifier.
 *
 * @return A key_t value of generated key
 */
key_t getSharedMemoryKey() {
    // Generate and return a unique key for shared memory access
    return ftok(SHM_PATH, SHM_PROJ_ID);
}

