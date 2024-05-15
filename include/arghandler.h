#ifndef ARGHANDLER_H
#define ARGHANDLER_H

#include "globals.h"
#include "shared.h"

int isPositiveNumber(const char *str, int *outValue);
int psmgmtArgs(int argc, char *argv[]);
// int workerArgs(int argc, char *argv[], WorkerConfig *config);
void printUsage(const char *programName);

#endif
