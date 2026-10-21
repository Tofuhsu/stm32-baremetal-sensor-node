# Week 1 checklist

Goal: understand what happens between power-on and `main()`, and blink LD2 using only register writes.

## Before the board arrives

1. Install the toolchain (see main conversation for commands). Check: `arm-none-eabi-gcc --version`, `openocd --version`.
2. Download the documents (free, from st.com):
   - RM0390: STM32F446xx reference manual (registers)
   - STM32F446RE datasheet (memory map, pinout)
   - UM1724: STM32 Nucleo-64 user manual (LD2 = PA5, ST-LINK)
   - ARM Cortex-M4 Generic User Guide (vector table, exceptions)
3. Read `linker.ld`. Be able to explain: VMA vs LMA, why `.data` is `> RAM AT > FLASH`, why `.bss` has no flash copy.
4. Write `Reset_Handler` in `src/startup.c`.
5. Write the register defines and blink logic in `src/main.c`.
6. `make` must build with zero warnings. Then inspect:
   - `arm-none-eabi-objdump -h build/firmware.elf` → `.isr_vector` at 0x08000000?
   - `arm-none-eabi-objdump -s -j .isr_vector build/firmware.elf` → first word = 0x20020000?
   - `build/firmware.map` → where did `_sdata`, `_sbss` land?

## After the board arrives

7. `make flash` → LD2 blinks.
8. `make debug`, then in another terminal `arm-none-eabi-gdb build/firmware.elf`:
   - `target extended-remote :3333`, `monitor reset halt`, `break main`, `continue`
   - step through the GPIO setup, `x/x 0x<GPIOA_MODER address>` before and after
9. Write down one thing that went wrong and how you found it (interview material).

## Self-check questions

- What does the CPU read from 0x08000000 and 0x08000004 at reset?
- Why is the reset vector address odd (Thumb bit)?
- What breaks if you remove `volatile` and build with `-O2`?
- Read-modify-write on MODER vs writing BSRR: which is safe if an interrupt also touches GPIOA?
