#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <time.h>

#include "globals.h"
#include "shared.h"
#include "user_process.h"

void initializeTimeTracking();
void simulateTimeProgression();
void trackActualTime();

#endif // TIME_H
