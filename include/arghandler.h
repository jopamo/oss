#ifndef ARGHANDLER_H
#define ARGHANDLER_H

#include "shared.h"

int isNumber(const char *str);
int ossArgs(int argc, char *argv[]);
void workerArgs(int argc, char *argv[], WorkerConfig *config);
void printOSSUsage(const char *programName);

#endif
