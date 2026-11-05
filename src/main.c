#include <stdint.h>

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

static void delay_ms(uint32_t ms)
{
    uint32_t start = ticks;
    /* Unsigned subtraction handles wraparound. The wait must not span a
     * complete 2^32-tick cycle. Keep interrupts enabled while waiting.
     * Busy-wait until WFI and sleeping-clock progression are implemented.
     */
    while ((uint32_t)(ticks - start) < ms) {
    }
}

int main(void)
{
    RCC_AHB1ENR |= GPIOAEN;
    (void)RCC_AHB1ENR; /* Read back after enabling the peripheral clock. */

    /* Clear only PA5's two mode bits, then select general-purpose output. */
    GPIOA_MODER = (GPIOA_MODER & ~PA5_MODE_MASK) | PA5_OUTPUT;

    systick_init();

    for (;;) {
        GPIOA_BSRR = 1u << PA5;
        delay_ms(BLINK_DELAY_MS);

        GPIOA_BSRR = 1u << (PA5 + 16u);
        delay_ms(BLINK_DELAY_MS);
    }
}
