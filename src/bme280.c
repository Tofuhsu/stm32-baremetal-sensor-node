#include "bme280.h"
#include "temperature.h"

#define RCC_AHB1ENR 0x40023830u
#define RCC_APB1ENR 0x40023840u
#define GPIOB_MODER 0x40020400u
#define GPIOB_OTYPER 0x40020404u
#define GPIOB_AFRH 0x40020424u
#define CR1 0x40005400u
#define CR2 0x40005404u
#define DR 0x40005410u
#define SR1 0x40005414u
#define SR2 0x40005418u
#define CCR 0x4000541Cu
#define TRISE 0x40005420u
#define PE 1u
#define START (1u << 8)
#define STOP (1u << 9)
#define ACK (1u << 10)
#define POS (1u << 11)
#define SB 1u
#define ADDR 2u
#define BTF 4u
#define RXNE 64u
#define TXE 128u
#define AF 1024u
#define BME_WRITE 0xECu
#define BME_READ 0xEDu

/* A thin MMIO seam allows the SAME C driver to run against host bus tests.
 * Target builds still perform volatile 32-bit accesses at the real addresses.
 */
#ifndef FIRMWARE_HOST_TEST
static uint32_t read32(uint32_t address) { return *(volatile uint32_t *)address; }
static void write32(uint32_t address, uint32_t value) { *(volatile uint32_t *)address = value; }
#else
extern uint32_t firmware_mmio_read(uint32_t address);
extern void firmware_mmio_write(uint32_t address, uint32_t value);
#define read32 firmware_mmio_read
#define write32 firmware_mmio_write
#endif

static uint32_t t1;
static int32_t t2, t3;

#ifndef FIRMWARE_HOST_TEST
static uint32_t irq_save(void)
{
    uint32_t mask;
    __asm__ volatile("mrs %0, primask\ncpsid i" : "=r"(mask) :: "memory");
    return mask;
}
static void irq_restore(uint32_t mask)
{
    __asm__ volatile("msr primask, %0" :: "r"(mask) : "memory");
}
#else
/* Host verification supplies these; target builds always use real instructions. */
extern uint32_t irq_save(void);
extern void irq_restore(uint32_t mask);
#endif

static bool wait_flag(uint32_t flag)
{
    /* Bounded failure: never spin forever on an unplugged/NACKing sensor. */
    for (uint32_t attempts = 0; attempts < 100000u; ++attempts) {
        uint32_t status = read32(SR1);
        if (status & AF) return false;
        if (status & flag) return true;
    }
    return false;
}

static void abort_transfer(void)
{
    write32(CR1, read32(CR1) | STOP);
    write32(SR1, ~AF);
    write32(CR1, PE | ACK);
}

static bool address(uint32_t byte)
{
    write32(CR1, read32(CR1) | START);
    if (!wait_flag(SB)) return false;
    write32(DR, byte);
    return wait_flag(ADDR);
}

static void clear_addr(void)
{
    (void)read32(SR1);
    (void)read32(SR2);
}

static bool write_register(uint8_t reg, uint8_t value)
{
    write32(CR1, PE | ACK);
    if (!address(BME_WRITE)) goto fail;
    clear_addr();
    if (!wait_flag(TXE)) goto fail;
    write32(DR, reg);
    if (!wait_flag(TXE)) goto fail;
    write32(DR, value);
    if (!wait_flag(BTF)) goto fail;
    write32(CR1, read32(CR1) | STOP);
    return true;
fail:
    abort_transfer();
    return false;
}

/* RM0390 24.3.3: the distinct 1/2/N>2-byte master-receiver endings.
 * The critical windows restore the caller's PRIMASK, including an outer mask.
 * ISR never accesses I2C. Delays/flag polling remain outside critical windows.
 */
static bool read_registers(uint8_t reg, uint8_t *out, uint32_t count)
{
    if (!count) return false;
    write32(CR1, PE | ACK);
    if (!address(BME_WRITE)) goto fail;
    clear_addr();
    if (!wait_flag(TXE)) goto fail;
    write32(DR, reg);
    if (!wait_flag(BTF)) goto fail;
    write32(CR1, PE | ACK | (count == 2u ? POS : 0u));
    if (!address(BME_READ)) goto fail; /* repeated START, no STOP */
    if (count == 1u) {
        write32(CR1, read32(CR1) & ~ACK);
        uint32_t mask = irq_save();
        clear_addr();
        write32(CR1, read32(CR1) | STOP);
        irq_restore(mask);
        if (!wait_flag(RXNE)) goto fail;
        *out = (uint8_t)read32(DR);
    } else if (count == 2u) {
        uint32_t mask = irq_save();
        clear_addr();
        write32(CR1, read32(CR1) & ~ACK);
        irq_restore(mask);
        if (!wait_flag(BTF)) goto fail;
        mask = irq_save();
        write32(CR1, read32(CR1) | STOP);
        *out++ = (uint8_t)read32(DR);
        *out = (uint8_t)read32(DR);
        irq_restore(mask);
    } else {
        clear_addr();
        while (count > 3u) {
            if (!wait_flag(RXNE)) goto fail;
            *out++ = (uint8_t)read32(DR);
            --count;
        }
        if (!wait_flag(BTF)) goto fail;
        write32(CR1, read32(CR1) & ~ACK);
        uint32_t mask = irq_save();
        *out++ = (uint8_t)read32(DR);
        irq_restore(mask);
        if (!wait_flag(BTF)) goto fail;
        mask = irq_save();
        write32(CR1, read32(CR1) | STOP);
        *out++ = (uint8_t)read32(DR);
        irq_restore(mask);
        if (!wait_flag(RXNE)) goto fail;
        *out = (uint8_t)read32(DR);
    }
    write32(CR1, PE | ACK);
    return true;
fail:
    abort_transfer();
    return false;
}

static uint32_t u16(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] * 256u;
}
static int32_t s16(const uint8_t *p)
{
    uint32_t value = u16(p);
    return value < 32768u ? (int32_t)value : (int32_t)value - 65536;
}

bool bme280_init(void)
{
    /* RM0390 6.3.13/24.6, DS10693 AF table: PB8/PB9 AF4, open-drain.
     * HSI/PCLK1=16 MHz: standard mode CCR=16MHz/(2*100kHz)=80,
     * TRISE=16+1=17; FREQ is the peripheral clock expressed in MHz.
     */
    write32(RCC_AHB1ENR, read32(RCC_AHB1ENR) | 2u);
    (void)read32(RCC_AHB1ENR);
    write32(GPIOB_MODER, (read32(GPIOB_MODER) & ~0xF0000u) | 0xA0000u);
    write32(GPIOB_OTYPER, read32(GPIOB_OTYPER) | 0x300u);
    write32(GPIOB_AFRH, (read32(GPIOB_AFRH) & ~0xFFu) | 0x44u);
    write32(RCC_APB1ENR, read32(RCC_APB1ENR) | (1u << 21));
    (void)read32(RCC_APB1ENR);
    write32(CR1, 0);
    write32(CR2, 16);
    write32(CCR, 80);
    write32(TRISE, 17);
    write32(CR1, PE | ACK);
    uint8_t id, cal[6];
    if (!read_registers(0xD0, &id, 1) || id != 0x60u ||
        !read_registers(0x88, cal, sizeof cal)) return false;
    /* Driver policy, not a Bosch-specified validity bit: a complete all-zero
     * trimming block is rejected instead of reporting plausible 0.00 C from
     * a failed register-pointer transfer. A real zero-temperature sample
     * with valid trimming parameters is still accepted.
     */
    if ((u16(cal) | u16(cal + 2) | u16(cal + 4)) == 0u) return false;
    t1 = u16(cal); t2 = s16(cal + 2); t3 = s16(cal + 4);
    /* Bosch 5.4.3/5.4.5: ctrl_hum first; ctrl_meas latches it. T/H x1,
     * pressure skipped. Normal mode lets host-side temperature changes appear.
     */
    return write_register(0xF2, 1) && write_register(0xF4, 0x23);
}

bool bme280_read_temperature(int32_t *centi_celsius)
{
    uint8_t raw[3];
    if (!read_registers(0xFA, raw, 3)) return false;
    uint32_t adc = (uint32_t)raw[0] * 4096u | (uint32_t)raw[1] * 16u | raw[2] / 16u;
    if (adc == 0x80000u) return false;
    return sensor_compensate_temperature(adc, t1, t2, t3, centi_celsius);
}
