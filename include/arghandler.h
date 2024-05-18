#ifndef ARGHANDLER_H
#define ARGHANDLER_H

#include "globals.h"
#include "shared.h"

bool isPositiveNumber(const char *str, int *outValue);
int psmgmtArgs(int argc, char *argv[], int *projectVersion);
int workerArgs(int argc, char *argv[], WorkerConfig *config);
void printUsage(const char *programName);
int workerArgs(int argc, char *argv[], WorkerConfig *config);

#endif
