/* A thin MMIO seam lets the same C drivers run against host bus tests.
 * Target builds perform volatile 32-bit accesses at the real addresses.
 */
#ifndef SENSOR_MMIO_H
#define SENSOR_MMIO_H
#include <stdint.h>

#ifndef FIRMWARE_HOST_TEST
static inline uint32_t read32(uint32_t address) { return *(volatile uint32_t *)address; }
static inline void write32(uint32_t address, uint32_t value) { *(volatile uint32_t *)address = value; }
static inline uint32_t irq_save(void)
{
    uint32_t mask;
    __asm__ volatile("mrs %0, primask\ncpsid i" : "=r"(mask) :: "memory");
    return mask;
}
static inline void irq_restore(uint32_t mask)
{
    __asm__ volatile("msr primask, %0" :: "r"(mask) : "memory");
}
#else
/* Host verification supplies these; target builds always use real instructions. */
extern uint32_t firmware_mmio_read(uint32_t address);
extern void firmware_mmio_write(uint32_t address, uint32_t value);
extern uint32_t irq_save(void);
extern void irq_restore(uint32_t mask);
#define read32 firmware_mmio_read
#define write32 firmware_mmio_write
#endif
#endif
