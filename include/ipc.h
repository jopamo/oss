#ifndef IPC_H
#define IPC_H

#include "shared.h" 

SystemClock* attachSharedMemory(int shmId);
int detachSharedMemory(SystemClock* shmPtr);
int cleanupSharedMemory(int shmId, SystemClock* shmPtr);

int sendMessage(int msqId, Message* msg);
int receiveMessage(int msqId, Message* msg, long msgType);
int cleanupMessageQueue(int msqId);

void sendMessageToNextChild();
void receiveMessageFromChild();

#endif 
