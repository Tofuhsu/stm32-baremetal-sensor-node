# stm32-baremetal-sensor-node

Bare-metal C firmware for an STM32F446RE (NUCLEO-F446RE): no HAL, register-level drivers for GPIO, SysTick, UART (interrupt + ring buffer) and I2C (BME280), with an SSD1306 OLED display.

**Status:** week 1, in progress. Nothing has been run on hardware yet.

## Build

Requires `arm-none-eabi-gcc` and `openocd`.

```bash
make          # build/firmware.elf + .bin, prints section sizes
make flash    # program over the on-board ST-LINK (SWD)
make debug    # OpenOCD GDB server on :3333
```

## Plan

| Week | Scope |
| --- | --- |
| 1 | Toolchain, linker script, startup code, register-level LED blink, first SWD/GDB session |
| 2 | SysTick timebase, UART TX/RX with interrupts and a ring buffer |
| 3 | I2C driver, BME280 reading with fixed-point compensation, logic analyzer captures |
| 4 | SSD1306 OLED, host-side Unity tests, GitHub Actions build |

## Measurements and limits

To be filled in with method, conditions and raw captures as results are produced.

## AI assistance

Build scaffolding (Makefile, linker script, file layout) was set up with an AI assistant. Startup code, register access and drivers are written by the author; this note will be kept accurate as the project grows.
