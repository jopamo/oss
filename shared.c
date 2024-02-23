#include <ctype.h>

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

/**
 * Checks if a string represents a numeric value
 *
 * @param str Pointer to the string to be checked
 * @return 1 if the string is numeric, 0 otherwise
 */
int isNumeric(const char *str) {
    // Iterate through each character of the string
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit(str[i])) {
            // If a non-digit character is found, return 0
            return 0;
        }
    }
    // All characters are digits, return 1
    return 1;
}
