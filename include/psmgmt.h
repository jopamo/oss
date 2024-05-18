#ifndef PSMGMT_H
#define PSMGMT_H

#include "cleanup.h"
#include "globals.h"
#include "init.h"
#include "process.h"
#include "queue.h"
#include "shared.h"
#include "signals.h"
#include "timeutils.h"

void manageChildTerminations();
void handleProcessScheduling();
void simulateBlockedProcesses();
int shouldLaunchNextChild();
void runPsmgmtA3();

#endif // PSMGMT_H
