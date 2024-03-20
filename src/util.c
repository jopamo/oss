#include "util.h"

#include <stdio.h>

#define DEBUG 0

void log_debug(const char* message) {
  if (DEBUG) {
    fprintf(stderr, "DEBUG: %s\n", message);
  }
}
