#include "ringbuf.h"

void ringbuf_init(ringbuf_t *rb)
{
    rb->head = 0u;
    rb->tail = 0u;
}

uint32_t ringbuf_count(const ringbuf_t *rb)
{
    return rb->head - rb->tail; /* unsigned difference handles wraparound */
}

bool ringbuf_empty(const ringbuf_t *rb)
{
    return rb->head == rb->tail;
}

bool ringbuf_put(ringbuf_t *rb, uint8_t byte)
{
    uint32_t head = rb->head;
    if (head - rb->tail == RINGBUF_SIZE) return false;
    rb->data[head & (RINGBUF_SIZE - 1u)] = byte;
    /* Publish the byte before the index. Both are plain stores to normal
     * memory on a single core; the compiler barrier keeps their order. */
    __asm__ volatile("" ::: "memory");
    rb->head = head + 1u;
    return true;
}

bool ringbuf_get(ringbuf_t *rb, uint8_t *byte)
{
    uint32_t tail = rb->tail;
    if (rb->head == tail) return false;
    *byte = rb->data[tail & (RINGBUF_SIZE - 1u)];
    __asm__ volatile("" ::: "memory");
    rb->tail = tail + 1u;
    return true;
}
