/* USART2 on PA2 (TX) / PA3 (RX), AF7, 115200 8N1 at PCLK1 = 16 MHz.
 * Interrupt driven: the main loop and the USART2 ISR share one ring buffer
 * per direction (see ringbuf.h).
 */
#ifndef SENSOR_UART_H
#define SENSOR_UART_H
#include <stdbool.h>
#include <stdint.h>

void uart_init(void);
/* Queue text for transmission; waits only while the TX buffer is full. */
void uart_puts(const char *text);
/* Wait until every queued byte has left the shift register (TC). */
void uart_flush(void);
/* Next received byte, or false if none is waiting. */
bool uart_getc(uint8_t *byte);
/* Bytes lost because the RX buffer was full, and hardware overruns (ORE). */
uint32_t uart_rx_dropped(void);
uint32_t uart_rx_overruns(void);

void USART2_IRQHandler(void);
#endif
