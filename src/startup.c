/* Minimal Cortex-M4 startup for blink driven by the SysTick exception.
 * All 16 core vectors are present. External IRQ vectors are not included;
 * SysTick is a core exception and does not need an external IRQ vector.
 */
#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

int main(void);
void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);

/* uintptr_t is one 32-bit vector word on the target Cortex-M4 ABI. */
__attribute__((section(".isr_vector"), used, aligned(128)))
const uintptr_t vector_table[16] = {
    (uintptr_t)&_estack,
    (uintptr_t)Reset_Handler,
    (uintptr_t)Default_Handler, /* NMI */
    (uintptr_t)Default_Handler, /* HardFault */
    (uintptr_t)Default_Handler, /* MemManage */
    (uintptr_t)Default_Handler, /* BusFault */
    (uintptr_t)Default_Handler, /* UsageFault */
    0, 0, 0, 0,                /* Reserved */
    (uintptr_t)Default_Handler, /* SVCall */
    (uintptr_t)Default_Handler, /* DebugMonitor */
    0,                         /* Reserved */
    (uintptr_t)Default_Handler, /* PendSV */
    (uintptr_t)SysTick_Handler, /* SysTick, exception 15 */
};

void Default_Handler(void)
{
    for (;;) {
    }
}

void Reset_Handler(void)
{
    uintptr_t data_begin = (uintptr_t)&_sdata;
    uintptr_t data_end = (uintptr_t)&_edata;
    uintptr_t bss_begin = (uintptr_t)&_sbss;
    uintptr_t bss_end = (uintptr_t)&_ebss;
    uintptr_t source = (uintptr_t)&_sidata;

    /* Word loops require aligned, ordered linker section boundaries. */
    if (((source | data_begin | data_end | bss_begin | bss_end) & 3u) != 0u ||
        data_begin > data_end || bss_begin > bss_end) {
        Default_Handler();
    }

    const uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while ((uintptr_t)dst < data_end) {
        *dst++ = *src++;
    }

    dst = &_sbss;
    while ((uintptr_t)dst < bss_end) {
        *dst++ = 0u;
    }

    (void)main();
    for (;;) {
    }
}
