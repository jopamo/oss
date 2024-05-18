#include "queue.h"

MLFQ mlfq;

void freeQueue(Queue *q) {
  if (q->queue != NULL) {
    free(q->queue);
  }
  q->queue = NULL;
  q->front = 0;
  q->rear = 0;
  q->capacity = 0;
  q->size = 0;
}

int initQueue(Queue *q, int capacity) {
  if (capacity <= 0) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Invalid queue capacity: %d. Must be greater than zero.",
                capacity);
    return -1;
  }
  freeQueue(q); // Ensure any old queue is freed before initializing

  q->queue = (Message *)calloc(capacity, sizeof(Message));
  if (q->queue == NULL) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to allocate memory for queue.");
    return -1;
  }

  q->front = 0;
  q->rear = 0;
  q->capacity = capacity;
  q->size = 0;
  log_message(LOG_LEVEL_INFO, 0, "Queue initialized with capacity %d.",
              capacity);
  return 0;
}

int enqueue(Queue *q, Message item) {
  if (isQueueFull(q)) {
    log_message(LOG_LEVEL_WARN, 0, "Queue is full. Cannot enqueue PID %ld.",
                (long)item.senderPid);
    return -1;
  }
  q->queue[q->rear] = item;
  q->rear = (q->rear + 1) % q->capacity;
  q->size++;
  log_message(LOG_LEVEL_INFO, 0, "Item enqueued successfully: PID %ld.",
              (long)item.senderPid);
  return 0;
}

int dequeue(Queue *q, Message *item) {
  if (isQueueEmpty(q)) {
    log_message(LOG_LEVEL_WARN, 0, "Attempt to dequeue from an empty queue.");
    return -1;
  }
  *item = q->queue[q->front];
  q->front = (q->front + 1) % q->capacity;
  q->size--;
  log_message(LOG_LEVEL_INFO, 0, "Item dequeued successfully: PID %ld.",
              (long)item->senderPid);
  return 0;
}

int isQueueEmpty(Queue *q) { return q->size == 0; }

int isQueueFull(Queue *q) { return q->size == q->capacity; }

int removeFromQueue(Queue *q, pid_t pid) {
  int index = -1;
  for (int i = 0; i < q->capacity; i++) {
    if (q->queue[i].senderPid == pid) {
      index = i;
      break;
    }
  }
  if (index != -1) {
    for (int i = index; i < q->capacity - 1; i++) {
      q->queue[i] = q->queue[i + 1];
    }
    q->rear = (q->rear - 1 + q->capacity) % q->capacity;
    q->size--;
    log_message(LOG_LEVEL_INFO, 0, "Removed PID %ld from queue.", (long)pid);
    return 0; // Successfully removed
  } else {
    log_message(LOG_LEVEL_WARN, 0, "PID %ld not found in queue.", (long)pid);
    return -1; // PID not found
  }
}

int initMLFQ(MLFQ *mlfq, int capacity) {
  if (initQueue(&mlfq->q0, capacity) == -1 ||
      initQueue(&mlfq->q1, capacity) == -1 ||
      initQueue(&mlfq->q2, capacity) == -1 ||
      initQueue(&mlfq->blockedQueue, capacity) == -1) {
    return -1;
  }
  return 0;
}

void freeMLFQ(MLFQ *mlfq) {
  freeQueue(&mlfq->q0);
  freeQueue(&mlfq->q1);
  freeQueue(&mlfq->q2);
  freeQueue(&mlfq->blockedQueue);
}

int addProcessToMLFQ(MLFQ *mlfq, Message item, int queueLevel) {
  switch (queueLevel) {
  case 0:
    return enqueue(&mlfq->q0, item);
  case 1:
    return enqueue(&mlfq->q1, item);
  case 2:
    return enqueue(&mlfq->q2, item);
  default:
    log_message(LOG_LEVEL_ERROR, 0, "Invalid queue level: %d.", queueLevel);
    return -1;
  }
}

int getNextProcessFromMLFQ(MLFQ *mlfq, Message *item) {
  if (dequeue(&mlfq->q0, item) == 0) {
    return 0;
  } else if (dequeue(&mlfq->q1, item) == 0) {
    return 0;
  } else if (dequeue(&mlfq->q2, item) == 0) {
    return 0;
  }
  return -1;
}

int promoteProcessInMLFQ(MLFQ *mlfq, Message item) {
  removeFromQueue(&mlfq->q1, item.senderPid);
  return enqueue(&mlfq->q0, item);
}

int demoteProcessInMLFQ(MLFQ *mlfq, Message item) {
  if (removeFromQueue(&mlfq->q0, item.senderPid) == 0) {
    return enqueue(&mlfq->q1, item);
  } else if (removeFromQueue(&mlfq->q1, item.senderPid) == 0) {
    return enqueue(&mlfq->q2, item);
  } else {
    return enqueue(&mlfq->q2, item);
  }
}
