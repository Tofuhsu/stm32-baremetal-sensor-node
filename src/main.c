#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define RCC_AHB1ENR REG32(0x40023830u)
#define GPIOA_MODER REG32(0x40020000u)
#define GPIOA_BSRR  REG32(0x40020018u)

#define GPIOAEN       (1u << 0)
#define PA5           5u
#define PA5_MODE_MASK (3u << (PA5 * 2u))
#define PA5_OUTPUT    (1u << (PA5 * 2u))

#ifndef BLINK_DELAY_ITERS
#define BLINK_DELAY_ITERS 500000u
#endif

static void delay(uint32_t iterations)
{
    for (volatile uint32_t i = 0; i < iterations; ++i) {
    }
}

int main(void)
{
    RCC_AHB1ENR |= GPIOAEN;
    (void)RCC_AHB1ENR;

    GPIOA_MODER =
        (GPIOA_MODER & ~PA5_MODE_MASK) | PA5_OUTPUT;

    for (;;) {
        GPIOA_BSRR = 1u << PA5;
        delay(BLINK_DELAY_ITERS);

        GPIOA_BSRR = 1u << (PA5 + 16u);
        delay(BLINK_DELAY_ITERS);
    }
}