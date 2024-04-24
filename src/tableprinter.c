#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "shared.h"
#include "user_process.h"

void logProcessTable(void) {
  unsigned long simSec, simNano;

  sem_wait(clockSem);
  simSec = simClock->seconds;
  simNano = simClock->nanoseconds;
  sem_post(clockSem);

  char tableOutput[4096] = "";
  snprintf(tableOutput, sizeof(tableOutput),
           "OSS PID:%d SysClockS: %lu SysclockNano: %lu\nProcess Table:\nEntry "
           "Occupied PID StartS StartN\n",
           getpid(), simSec, simNano);

  for (int i = 0; i < maxProcesses; i++) {
    PCB *pcb = &processTable[i];
    char row[256];
    if (pcb->occupied) {
      snprintf(row, sizeof(row), "%d 1 %d %u %u\n", i, pcb->pid,
               pcb->startSeconds, pcb->startNano);
    } else {
      snprintf(row, sizeof(row), "%d 0 - - -\n", i);
    }
    strncat(tableOutput, row, sizeof(tableOutput) - strlen(tableOutput) - 1);
  }

  log_message(LOG_LEVEL_INFO, "%s", tableOutput);
}

int main(void) {
  setupSignalHandlers();
  initializeSharedResources();

  while (keepRunning) {
    logProcessTable();
    usleep(500000);
  }

  cleanupSharedResources();
  return EXIT_SUCCESS;
}
