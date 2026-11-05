#include <stdint.h>
#include "bme280.h"
#include "temperature.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define RCC_AHB1ENR REG32(0x40023830u)
#define RCC_APB1ENR REG32(0x40023840u)
#define GPIOA_MODER REG32(0x40020000u)
#define GPIOA_OTYPER REG32(0x40020004u)
#define GPIOA_BSRR  REG32(0x40020018u)
#define GPIOA_AFRL  REG32(0x40020020u)

#define USART2_SR  REG32(0x40004400u)
#define USART2_DR  REG32(0x40004404u)
#define USART2_BRR REG32(0x40004408u)
#define USART2_CR1 REG32(0x4000440Cu)

#define USART2EN   (1u << 17)
#define USART_TXE  (1u << 7)
#define USART_TC   (1u << 6)
#define USART_TE   (1u << 3)
#define USART_UE   (1u << 13)
#define PA2       2u
#define PA2_MODE_MASK (3u << (PA2 * 2u))
#define PA2_AF_MODE   (2u << (PA2 * 2u))
#define PA2_AF_MASK   (15u << (PA2 * 4u))
#define PA2_AF7       (7u << (PA2 * 4u))

#define SYST_CSR REG32(0xE000E010u)
#define SYST_RVR REG32(0xE000E014u)
#define SYST_CVR REG32(0xE000E018u)

#define SYST_ENABLE    (1u << 0)
#define SYST_TICKINT   (1u << 1)
#define SYST_CLKSOURCE (1u << 2)

/* RM0390 6.2.2 and 6.3.3: reset selects HSI, with AHB undivided.
 * This firmware leaves the reset clock configuration unchanged.
 */
#define RESET_HCLK_HZ 16000000u
/* APB1 is also undivided at reset; USART2 uses PCLK1, not a timer clock. */
#define RESET_PCLK1_HZ RESET_HCLK_HZ
#define UART_BAUD 115200u
/* OVER8=0: BRR encodes round(PCLK1 / baud), here 0x008B. */
#define UART_BRR ((RESET_PCLK1_HZ + UART_BAUD / 2u) / UART_BAUD)
#define SYSTICK_HZ    1000u
#define SYSTICK_RELOAD (RESET_HCLK_HZ / SYSTICK_HZ - 1u)

_Static_assert(RESET_HCLK_HZ % SYSTICK_HZ == 0u, "SysTick period must be exact");
_Static_assert(SYSTICK_RELOAD > 0u && SYSTICK_RELOAD <= 0x00FFFFFFu,
               "SysTick reload must fit its nonzero 24-bit range");
_Static_assert(UART_BRR >= 16u && UART_BRR <= 0xFFFFu,
               "USART divider must fit OVER8=0 BRR with USARTDIV >= 1");

#define GPIOAEN     (1u << 0)
#define PA5         5u
#define PA5_MODE_MASK (3u << (PA5 * 2u))
#define PA5_OUTPUT    (1u << (PA5 * 2u))

#ifndef BLINK_DELAY_MS
#define BLINK_DELAY_MS 500u
#endif

/* Zeroed by Reset_Handler. After initialization, only SysTick_Handler writes.
 * A naturally aligned 32-bit SRAM load is atomic on Cortex-M4; volatile makes
 * the foreground reload it. Volatile alone does not make ++ atomic.
 */
static volatile uint32_t ticks;

void SysTick_Handler(void)
{
    ++ticks;
}

static void uart_init(void)
{
    /* main() has already enabled GPIOA. Open and read back the APB1 gate. */
    RCC_APB1ENR |= USART2EN;
    (void)RCC_APB1ENR;

    USART2_CR1 = 0u;
    /* Preserve PA5, SWD pins and the other AF fields. PA2 is push-pull AF7. */
    GPIOA_MODER = (GPIOA_MODER & ~PA2_MODE_MASK) | PA2_AF_MODE;
    GPIOA_OTYPER &= ~(1u << PA2);
    GPIOA_AFRL = (GPIOA_AFRL & ~PA2_AF_MASK) | PA2_AF7;

    USART2_BRR = UART_BRR;
    /* OVER8=0, M=0, PCE=0: 8 data bits, no parity. CR2's reset STOP=00
     * supplies one stop bit; this firmware leaves CR2/CR3 at reset values.
     */
    USART2_CR1 = USART_UE | USART_TE;
}

static void uart_puts(const char *text)
{
    while (*text != '\0') {
        while ((USART2_SR & USART_TXE) == 0u) {
        }
        USART2_DR = (uint8_t)*text++;
    }
    /* TXE only means TDR is empty. Wait for the last frame to finish too. */
    while ((USART2_SR & USART_TC) == 0u) {
    }
}

static void systick_init(void)
{
    /* PM0214 4.5.5: stop, set reload, clear current, then enable.
     * CVR writes also clear COUNTFLAG. CALIB is read-only and unused here.
     */
    SYST_CSR = 0u;
    SYST_RVR = SYSTICK_RELOAD;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CLKSOURCE | SYST_TICKINT | SYST_ENABLE;
}

static void wait_period(uint32_t *start, uint32_t ms)
{
    /* Unsigned subtraction handles wraparound. The wait must not span a
     * complete 2^32-tick cycle. Keep interrupts enabled while waiting.
     * Busy-wait until WFI and sleeping-clock progression are implemented.
     */
    while ((uint32_t)(ticks - *start) < ms) {
    }
    *start += ms; /* absolute period: sensor/UART work does not accumulate drift */
}

static void print_temperature(void)
{
    int32_t value;
    if (!bme280_read_temperature(&value)) { uart_puts("bme280 read error\r\n"); return; }
    char message[8];
    if (!sensor_format_temperature(value, message)) { uart_puts("temperature format error\r\n"); return; }
    uart_puts("temperature: "); uart_puts(message); uart_puts("\r\n");
}

int main(void)
{
    RCC_AHB1ENR |= GPIOAEN;
    (void)RCC_AHB1ENR; /* Read back after enabling the peripheral clock. */

    /* Clear only PA5's two mode bits, then select general-purpose output. */
    GPIOA_MODER = (GPIOA_MODER & ~PA5_MODE_MASK) | PA5_OUTPUT;

    uart_init();
    uart_puts("sensor-node boot\r\n");
    bool sensor_ready = bme280_init();
    if (sensor_ready) print_temperature();
    else uart_puts("bme280 init error\r\n");
    systick_init();
    uint32_t phase = ticks;

    for (;;) {
        GPIOA_BSRR = 1u << PA5;
        wait_period(&phase, BLINK_DELAY_MS);

        GPIOA_BSRR = 1u << (PA5 + 16u);
        wait_period(&phase, BLINK_DELAY_MS);
        if (sensor_ready) print_temperature();
    }
}
