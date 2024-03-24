#ifndef ARGHANDLER_H
#define ARGHANDLER_H

#include "shared.h"

typedef struct {
  unsigned int lifespanSeconds;
  unsigned int lifespanNanoSeconds;
} WorkerConfig;

void workerArgs(int argc, char *argv[], WorkerConfig *config);
int ossArgs(int argc, char* argv[]);
int isNumber(const char* str);
void printOSSUsage(const char *programName);

#endif
