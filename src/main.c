#include <stdint.h>
#include "bme280.h"
#include "i2c1.h"
#include "ssd1306.h"
#include "temperature.h"
#include "uart.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define RCC_AHB1ENR REG32(0x40023830u)
#define GPIOA_MODER REG32(0x40020000u)
#define GPIOA_BSRR  REG32(0x40020018u)

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
#define SYSTICK_HZ    1000u
#define SYSTICK_RELOAD (RESET_HCLK_HZ / SYSTICK_HZ - 1u)

_Static_assert(RESET_HCLK_HZ % SYSTICK_HZ == 0u, "SysTick period must be exact");
_Static_assert(SYSTICK_RELOAD > 0u && SYSTICK_RELOAD <= 0x00FFFFFFu,
               "SysTick reload must fit its nonzero 24-bit range");

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

static bool sensor_ready, display_ready;

static void show_temperature(const char *text)
{
    if (!display_ready) return;
    char shown[16] = "T ";
    uint32_t n = 2u;
    while (*text != '\0' && n < sizeof shown - 3u) shown[n++] = *text++;
    shown[n++] = ' ';
    shown[n++] = 'C';
    shown[n] = '\0';
    ssd1306_text(2u, 0u, "                     "); /* clear the old reading */
    ssd1306_text(2u, 0u, shown);
    (void)ssd1306_flush(2u, 2u);
}

static void print_temperature(void)
{
    int32_t value;
    if (!sensor_ready) { uart_puts("bme280 not ready\r\n"); return; }
    if (!bme280_read_temperature(&value)) { uart_puts("bme280 read error\r\n"); return; }
    char message[8];
    if (!sensor_format_temperature(value, message)) { uart_puts("temperature format error\r\n"); return; }
    uart_puts("temperature: "); uart_puts(message); uart_puts("\r\n");
    show_temperature(message);
}

/* Received characters are collected into a line; CR or LF ends it.
 * "temp" prints a reading now; any other line is echoed back. */
static char line[32];
static uint32_t line_length;

/* Written with explicit branches: `return *a == *b` makes GCC emit an IT
 * block, which cortex-m-emu does not implement. */
static bool same(const char *a, const char *b)
{
    for (;;) {
        if (*a != *b) return false;
        if (*a == '\0') return true;
        ++a;
        ++b;
    }
}

static void handle_line(void)
{
    line[line_length] = '\0';
    if (same(line, "temp")) print_temperature();
    else { uart_puts("echo: "); uart_puts(line); uart_puts("\r\n"); }
    line_length = 0u;
}

static void poll_commands(void)
{
    uint8_t byte;
    while (uart_getc(&byte)) {
        if (byte == '\r' || byte == '\n') {
            if (line_length > 0u) handle_line();
        } else if (line_length < sizeof line - 1u) {
            line[line_length++] = (char)byte;
        } /* longer lines are cut to 31 characters */
    }
}

static void wait_period(uint32_t *start, uint32_t ms)
{
    /* Unsigned subtraction handles wraparound. The wait must not span a
     * complete 2^32-tick cycle. Keep interrupts enabled while waiting.
     * Busy-wait until WFI and sleeping-clock progression are implemented.
     */
    while ((uint32_t)(ticks - *start) < ms) {
        poll_commands();
    }
    *start += ms; /* absolute period: sensor/UART work does not accumulate drift */
}

int main(void)
{
    RCC_AHB1ENR |= GPIOAEN;
    (void)RCC_AHB1ENR; /* Read back after enabling the peripheral clock. */

    /* Clear only PA5's two mode bits, then select general-purpose output. */
    GPIOA_MODER = (GPIOA_MODER & ~PA5_MODE_MASK) | PA5_OUTPUT;

    uart_init();
    uart_puts("sensor-node boot\r\n");
    i2c1_init();
    sensor_ready = bme280_init();
    display_ready = ssd1306_init();
    if (display_ready) {
        ssd1306_clear();
        ssd1306_text(0u, 0u, "SENSOR NODE");
        display_ready = ssd1306_flush(0u, SSD1306_PAGES - 1u);
    }
    if (!display_ready) uart_puts("ssd1306 not found\r\n");
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
