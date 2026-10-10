#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "queue.h"

struct queue {
    uint8_t *buffer;
    uint32_t size;
    uint32_t write;
    uint32_t read;
    bool closed;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
};

static void deadline_after_ms(struct timespec *ts, const uint32_t ms) {
    clock_gettime(CLOCK_MONOTONIC, ts);
    ts->tv_sec += ms / 1000;
    ts->tv_nsec += (long)(ms % 1000) * 1000000L;
    if(ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

static bool wait_on(pthread_cond_t *cond,
                    pthread_mutex_t *lock,
                    const uint32_t timeout_ms,
                    const struct timespec *deadline) {
    if(timeout_ms == QUEUE_WAIT_FOREVER) {
        pthread_cond_wait(cond, lock);
        return true;
    }
    return (pthread_cond_timedwait(cond, lock, deadline) != ETIMEDOUT);
}

queue_t *queue_create(const uint32_t size) {
    queue_t *queue = malloc(sizeof(queue_t));

    if(queue == NULL) {
        return NULL;
    }

    queue->buffer = malloc(size);

    if(queue->buffer == NULL) {
        free(queue);
        return NULL;
    }

    if(pthread_mutex_init(&queue->lock, NULL) < 0) {
        free(queue->buffer);
        free(queue);
        return NULL;
    }

    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    const int e1 = pthread_cond_init(&queue->not_empty, &attr);
    const int e2 = pthread_cond_init(&queue->not_full, &attr);
    pthread_condattr_destroy(&attr);
    if((e1 != 0) || (e2 != 0)) {
        pthread_mutex_destroy(&queue->lock);
        free(queue->buffer);
        free(queue);
        return NULL;
    }

    queue->size = size;
    queue->write = 0;
    queue->read = 0;
    queue->closed = false;

    return queue;
}

void queue_free(queue_t *queue) {
    if(queue != NULL) {
        pthread_cond_destroy(&queue->not_empty);
        pthread_cond_destroy(&queue->not_full);
        pthread_mutex_destroy(&queue->lock);
        free(queue->buffer);
        free(queue);
    }
}

uint32_t
queue_write(queue_t *queue, const uint8_t *src, const uint32_t len, const uint32_t timeout) {
    uint32_t num = 0;
    struct timespec deadline = {0};

    if((timeout != QUEUE_NO_WAIT) && (timeout != QUEUE_WAIT_FOREVER)) {
        deadline_after_ms(&deadline, timeout);
    }

    pthread_mutex_lock(&queue->lock);

    while(num < len) {
        const uint32_t before = num;

        while(num < len) {
            uint32_t wr = queue->write;
            uint32_t next = wr + 1;
            if(next >= queue->size) {
                next = 0;
            }
            if(next == queue->read) {
                break;
            }
            queue->buffer[wr] = src[num];
            queue->write = next;
            num++;
        }

        if(num != before) {
            pthread_cond_broadcast(&queue->not_empty);
        }
        if((num == len) || (timeout == QUEUE_NO_WAIT) || queue->closed) {
            break;
        }
        if(!wait_on(&queue->not_full, &queue->lock, timeout, &deadline)) {
            break;
        }
    }

    pthread_mutex_unlock(&queue->lock);

    return num;
}

uint32_t
queue_read(queue_t *queue, uint8_t *dest, const uint32_t capacity, const uint32_t timeout) {
    uint32_t num = 0;
    struct timespec deadline = {0};

    if((timeout != QUEUE_NO_WAIT) && (timeout != QUEUE_WAIT_FOREVER)) {
        deadline_after_ms(&deadline, timeout);
    }

    pthread_mutex_lock(&queue->lock);

    while((queue->read == queue->write) && (timeout != QUEUE_NO_WAIT) && !queue->closed) {
        if(!wait_on(&queue->not_empty, &queue->lock, timeout, &deadline)) {
            break;
        }
    }

    while((queue->read != queue->write) && (num < capacity)) {
        uint32_t rd = queue->read;
        uint32_t next = rd + 1;
        dest[num] = queue->buffer[rd];
        if(next >= queue->size) {
            next = 0;
        }
        queue->read = next;
        num++;
    }

    if(num > 0) {
        pthread_cond_broadcast(&queue->not_full);
    }

    pthread_mutex_unlock(&queue->lock);

    return num;
}

void queue_close(queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    queue->closed = true;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);
}
