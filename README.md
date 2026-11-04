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

## Emulation (Renode)

The firmware also runs on an emulated board in [Renode](https://renode.io) 1.17, so it can be tested without hardware and on every push.

```bash
make sim       # interactive run with a USART2 window
make test      # Robot tests CI enforces
make test-all  # also runs tests for features not written yet (tagged `todo`, expected to fail)
```

- `renode/stm32f446_nucleo.repl`: board description. Renode has no STM32F446 file, so it extends the generic STM32F4 file and sets the F446RE flash/SRAM sizes, LD2 on PA5, B1 on PC13 and a BME280 at 0x76 on I2C1 (Renode's built-in `I2C.BME280` model).
- `tests/renode/sensor_node.robot`: checks the vector table now; LED, UART and BME280 tests are tagged `todo` until the matching driver exists. The expected UART strings are a proposed format.
- `.github/workflows/ci.yml`: builds with `gcc-arm-none-eabi` and runs `make test`.

Limits: the emulator models register behavior, not timing, electrical levels or real sensor noise. A passing Renode test does not replace a logic analyzer capture on the real board. The STM32F4 I2C model has not yet been exercised by this firmware.

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

Written with an AI assistant: Makefile, linker script, file layout, the Renode board/script files, the Robot tests and the CI workflow.
Written by the author: startup code (`Reset_Handler`), register definitions and every driver in `src/`. This note is kept accurate as the project grows.
