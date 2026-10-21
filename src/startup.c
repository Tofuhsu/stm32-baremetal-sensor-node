/*
 * Minimal startup code: vector table + Reset_Handler.
 * No HAL, no C library startup (we link with -nostdlib).
 */
#include <stdint.h>

/* Symbols defined in linker.ld. Only their addresses matter. */
extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata;
extern uint32_t _sbss, _ebss;

int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/*
 * Cortex-M vector table. Entry 0 is the initial stack pointer, entry 1 is the reset vector.
 * Entries 2-15 are system exceptions; device IRQs (UART, I2C, ...) follow and will be added later.
 * The section name must match KEEP(*(.isr_vector)) in linker.ld.
 */
__attribute__((section(".isr_vector"), used))
const uintptr_t vector_table[16] = {
    (uintptr_t)&_estack,
    (uintptr_t)Reset_Handler,
    (uintptr_t)Default_Handler, /* NMI */
    (uintptr_t)Default_Handler, /* HardFault */
    /* Remaining entries are 0 for now. Add SysTick (entry 15) in week 2. */
};

void Reset_Handler(void)
{
    /*
     * TODO(Jeff): write this yourself.
     * 1. Copy .data from flash to RAM: word by word from _sidata into [_sdata, _edata).
     * 2. Zero .bss: write 0 to every word in [_sbss, _ebss).
     * 3. Call main().
     * 4. If main() ever returns, loop forever.
     *
     * Think about: why must step 1 and 2 happen before main()? What would a global
     * `int counter = 5;` contain if you skipped step 1?
     */
}

void Default_Handler(void)
{
    while (1) {
    }
}
