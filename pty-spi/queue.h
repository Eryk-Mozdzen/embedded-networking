#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>

#define QUEUE_NO_WAIT      0u
#define QUEUE_WAIT_FOREVER UINT32_MAX

struct queue;
typedef struct queue queue_t;

queue_t *queue_create(const uint32_t size);
void queue_free(queue_t *queue);
uint32_t
queue_write(queue_t *queue, const uint8_t *src, const uint32_t len, const uint32_t timeout);
uint32_t queue_read(queue_t *queue, uint8_t *dest, const uint32_t capacity, const uint32_t timeout);
void queue_close(queue_t *queue);

#endif
