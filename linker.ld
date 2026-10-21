/*
 * Linker script for STM32F446RE (NUCLEO-F446RE).
 * Flash: 512 KiB at 0x08000000, SRAM: 128 KiB at 0x20000000.
 * Values from the STM32F446xx datasheet memory map. Re-check them if the board changes.
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Stack grows down from the end of RAM. The vector table's first word points here. */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    /* Vector table must be at the start of flash: the CPU reads SP and the reset vector from here. */
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* .data lives in RAM at run time, but its initial values are stored in flash (LMA).
       Reset_Handler must copy [_sidata, ...) to [_sdata, _edata). */
    _sidata = LOADADDR(.data);
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    /* .bss has no stored image; Reset_Handler must zero [_sbss, _ebss). */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM
}
