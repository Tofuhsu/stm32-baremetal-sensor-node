# stm32-baremetal-sensor-node

Bare-metal C firmware for an STM32F446RE (NUCLEO-F446RE): no HAL, register-level GPIO/SysTick, polling USART2 TX and an I2C1 BME280 temperature driver. UART RX/interrupt buffering and an SSD1306 display remain planned.

**Status:** GPIO/SysTick blink and the CRLF USART2 boot banner are the previous baseline. This update adds bounded polling I2C1, BME280 initialization and checked integer temperature compensation/formatting. The actual C driver passes host model tests; the updated ARM build, new instruction coverage and Renode/Robot tests have not yet run in the authoring environment. Nothing has been verified on hardware.

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
make test-all  # all Robot tests, including the enabled BME280 acceptance test
make test-host # actual C protocol/compensation tests; sibling cortex-m-emu checkout required
```

- `renode/stm32f446_nucleo.repl`: board description. Renode has no STM32F446 file, so it extends the generic STM32F4 file and sets the F446RE flash/SRAM sizes, LD2 on PA5, B1 on PC13 and a BME280 at 0x76 on I2C1 (Renode's built-in `I2C.BME280` model).
- `tests/renode/sensor_node.robot`: checks the vector table, LED, CRLF UART banner and temperature lines at 25.00 and 30.50 C. These tests are now enabled and must be run before claiming target acceptance.
- `.github/workflows/ci.yml`: builds with `gcc-arm-none-eabi` and runs `make test`.

Limits: the emulator models register behavior, not timing, electrical levels or real sensor noise. A passing Renode test does not replace a logic analyzer capture on the real board. The updated firmware has only exercised the custom I2C model through host C tests; new ARM and Renode runs are still pending.

## Plan

| Week | Scope |
| --- | --- |
| 1 | Toolchain, linker script, startup code, register-level LED blink, first SWD/GDB session |
| 2 | SysTick timebase, UART TX/RX with interrupts and a ring buffer |
| 3 | I2C driver, BME280 reading with fixed-point compensation, logic analyzer captures |
| 4 | SSD1306 OLED, host-side Unity tests, GitHub Actions build |

## Implementation and validation scope

PB8/PB9 use AF4/open-drain. At unchanged reset PCLK1=16 MHz, I2C FREQ=16, CCR=80 and TRISE=17 select standard-mode 100 kHz hardware configuration. Temperature uses Bosch's int32 operation order with overflow checks; pathological calibration is rejected. No floating printf, int64 compensation or pressure/humidity output is linked into the target. USART2 remains polling TX only.

`ctrl_hum` is written before `ctrl_meas`; normal mode permits repeated temperature readout. The message is `temperature: 25.00\r\n` for 2500 centidegrees C. Main reads once per two LED half-periods (1000 SysTick ticks with the default 500-tick half-period). Recurring deadlines avoid accumulating service overhead, provided I2C/UART work completes before the next deadline. The emulator's instruction ticks are not hardware milliseconds.

Host tests execute the same I2C C driver via a thin MMIO seam and validate 1/2/N reads, NACK recovery, saved/restored PRIMASK and compensation/formatting. They do not validate emitted ARM instructions, baud timing, electrical signals or Renode behavior. See the sibling emulator's `docs/acceptance.md`.

To collect the actual GCC output needed to finish instruction integration:

```sh
bash tools/build_review_artifacts.sh
# produces build/firmware-review-artifacts.zip
```

A fresh `make test-all` is required after building. No passing Robot/CI result is claimed for this update.

## AI assistance

This update's `src/bme280.{c,h}`, `src/temperature.{c,h}`, changes to `src/main.c`, host tests, build/review scripts, Robot tag changes and documentation were prepared with AI assistance for author review. Earlier startup, board and infrastructure work retain their existing history. This update has not been verified on hardware.
