/* Single-producer, single-consumer byte ring buffer.
 * One side only writes head (put), the other only writes tail (get), so an
 * ISR and the main loop can share it without disabling interrupts. Indices are
 * free-running 32-bit counters; an aligned 32-bit load or store is atomic on
 * Cortex-M4. RINGBUF_SIZE must be a power of two.
 */
#ifndef SENSOR_RINGBUF_H
#define SENSOR_RINGBUF_H
#include <stdbool.h>
#include <stdint.h>

#define RINGBUF_SIZE 128u
_Static_assert((RINGBUF_SIZE & (RINGBUF_SIZE - 1u)) == 0u, "size must be a power of two");

typedef struct {
    volatile uint32_t head; /* written by the producer */
    volatile uint32_t tail; /* written by the consumer */
    uint8_t data[RINGBUF_SIZE];
} ringbuf_t;

void ringbuf_init(ringbuf_t *rb);
/* Producer side. Returns false (and stores nothing) when full. */
bool ringbuf_put(ringbuf_t *rb, uint8_t byte);
/* Consumer side. Returns false when empty. */
bool ringbuf_get(ringbuf_t *rb, uint8_t *byte);
bool ringbuf_empty(const ringbuf_t *rb);
uint32_t ringbuf_count(const ringbuf_t *rb);
#endif
