#include "shared.h"

key_t getSharedMemoryKey() {
    return ftok(SHM_PATH, SHM_PROJ_ID);
}

