#include "queue.h"

Queue resourceQueues[MAX_RESOURCES];

void freeQueue(Queue *q) {
  if (q->queue != NULL) {
    free(q->queue);
  }
  q->queue = NULL;
  q->front = 0;
  q->rear = 0;
  q->capacity = 0;
}

int initQueue(Queue *q, int capacity) {
  if (capacity <= 0) {
    log_message(LOG_LEVEL_ERROR, 0,
                "Invalid queue capacity: %d. Must be greater than zero.",
                capacity);
    return -1;
  }
  freeQueue(q); // Ensure any old queue is freed before initializing

  q->queue = (MessageA5 *)calloc(capacity, sizeof(MessageA5));
  if (q->queue == NULL) {
    log_message(LOG_LEVEL_ERROR, 0, "Failed to allocate memory for queue.");
    return -1;
  }

  q->front = 0;
  q->rear = 0;
  q->capacity = capacity;
  log_message(LOG_LEVEL_INFO, 0, "Queue initialized with capacity %d.",
              capacity);
  return 0;
}

void enqueue(Queue *q, MessageA5 item) {
  if (q->capacity == 0) {
    log_message(LOG_LEVEL_ERROR, 0, "Queue capacity is zero, cannot enqueue.");
    return;
  }
  int nextRear = (q->rear + 1) % q->capacity;
  if (nextRear == q->front) {
    log_message(LOG_LEVEL_WARN, 0, "Queue is full. Cannot enqueue PID %ld.",
                item.senderPid);
    return;
  }
  q->queue[q->rear] = item;
  q->rear = nextRear;
  log_message(LOG_LEVEL_INFO, 0, "Item enqueued successfully: PID %ld.",
              item.senderPid);
}

int dequeue(Queue *q, MessageA5 *item) {
  if (q->front == q->rear) {
    log_message(LOG_LEVEL_WARN, 0, "Attempt to dequeue from an empty queue.");
    return -1;
  }
  *item = q->queue[q->front];
  q->front = (q->front + 1) % q->capacity;
  log_message(LOG_LEVEL_INFO, 0, "Item dequeued successfully: PID %ld.",
              item->senderPid);
  return 0;
}
