/*
 * Week 1 goal: blink the user LED (LD2) on NUCLEO-F446RE by writing registers directly.
 * LD2 is wired to PA5 (see the Nucleo-64 user manual, UM1724).
 */
#include <stdint.h>

/*
 * TODO(Jeff): define the registers you need, using addresses from RM0390
 * (STM32F446xx reference manual). Look up:
 *   - RCC base address (memory map table) and the RCC_AHB1ENR offset -> enable the GPIOA clock
 *   - GPIOA base address, GPIOx_MODER offset -> set PA5 to general-purpose output (01)
 *   - GPIOx_ODR offset (or BSRR) -> drive PA5 high/low
 *
 * Pattern:
 *   #define REG32(addr) (*(volatile uint32_t *)(addr))
 *
 * Think about: why `volatile`? What could the compiler do to your blink loop without it?
 */

static void delay(volatile uint32_t count)
{
    while (count--) {
    }
}

int main(void)
{
    /* TODO(Jeff): 1. enable GPIOA clock  2. configure PA5 as output (clear 2 bits, then set) */

    while (1) {
        /* TODO(Jeff): toggle PA5 */
        delay(500000); /* busy-wait; replaced by SysTick in week 2 */
    }
}
