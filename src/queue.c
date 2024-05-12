#include "queue.h"

int initQueue(Queue *q, int capacity) {
  freeQueue(q);

  q->queue = (pid_t *)calloc(capacity, sizeof(pid_t));
  if (q->queue == NULL) {
    // Handle allocation failure; set capacity to 0 to indicate the queue is not
    // usable
    q->front = 0;
    q->rear = 0;
    q->capacity = 0;
    return -1;
  } else {
    // Initialization successful
    q->front = 0;
    q->rear = 0;
    q->capacity = capacity;
    return 0;
  }
}

void freeQueue(Queue *q) {
  if (q->queue != NULL) {
    free(q->queue);
    q->queue = NULL; // Set to NULL to avoid double free
  }
  q->front = 0;
  q->rear = 0;
  q->capacity = 0;
}

int initializeQueues(MLFQ *mlfq) {
  if (initQueue(&mlfq->highPriority, MAX_PROCESSES) == -1 ||
      initQueue(&mlfq->midPriority, MAX_PROCESSES) == -1 ||
      initQueue(&mlfq->lowPriority, MAX_PROCESSES) == -1) {
    freeQueues(mlfq); // Cleanup partially initialized queues
    return -1;
  }
  return 0;
}

void freeQueues(MLFQ *mlfq) {
  freeQueue(&mlfq->highPriority);
  freeQueue(&mlfq->midPriority);
  freeQueue(&mlfq->lowPriority);
}
