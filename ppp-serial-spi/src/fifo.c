#include <stdint.h>

#include "fifo.h"

void fifo_init(fifo_t *fifo) {
    fifo->rd = 0;
    fifo->wr = 0;
}

uint32_t fifo_write(fifo_t *fifo, const uint8_t *src, uint32_t src_len) {
    uint32_t num = 0;
    while(num < src_len) {
        uint32_t next = fifo->wr + 1;
        if(next >= sizeof(fifo->buffer)) {
            next = 0;
        }
        if(next == fifo->rd) {
            break;
        }
        fifo->buffer[fifo->wr] = src[num];
        fifo->wr = next;
        num++;
    }
    return num;
}

uint32_t fifo_read(fifo_t *fifo, uint8_t *dst, const uint32_t dst_capacity) {
    uint32_t num = 0;
    while((fifo->rd != fifo->wr) && (num < dst_capacity)) {
        dst[num] = fifo->buffer[fifo->rd];
        fifo->rd++;
        num++;
        if(fifo->rd >= sizeof(fifo->buffer)) {
            fifo->rd = 0;
        }
    }
    return num;
}
