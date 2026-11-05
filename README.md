# stm32-baremetal-sensor-node

Bare-metal C firmware for an STM32F446RE (NUCLEO-F446RE), no HAL: register-level GPIO and SysTick, interrupt-driven USART2 TX/RX with ring buffers, an I2C1 BME280 temperature driver and an SSD1306 128x64 OLED driver.

**Status:** All software items of the four-week plan are done; the two hardware items (first SWD/GDB session on the board, logic analyzer captures) are not, because nothing has been run on hardware yet.
- On the [cortex-m-emu](https://github.com/Tofuhsu/cortex-m-emu) emulator the `-O0` build prints `temperature: 25.00`, answers UART commands, and draws `SENSOR NODE` and `T 25.00 C` on the emulated SSD1306 (`make test-emu` compares the frame with `tests/emu/oled_25.00.pbm`).
- In Renode 1.17, all 7 Robot tests pass, using a patched I2C1 model, `renode/STM32F4_I2C_AckModel.cs`. Renode's stock `STM32F1_I2C` does not model CR1.ACK and starts receiving before ADDR is cleared, so a one-byte read never finishes and its BME280 model drops the next register address; with the stock model the firmware rejects the all-zero calibration and prints `bme280 init error`. Only the Renode model is patched, not the firmware. Details: [cortex-m-emu `docs/renode_differences.md`](https://github.com/Tofuhsu/cortex-m-emu/blob/main/docs/renode_differences.md).
- Renode has no SSD1306 model, so there the display is reported missing and the firmware carries on.

## Build

Requires `arm-none-eabi-gcc` and `openocd`.

```bash
make          # build/firmware.elf + .bin, prints section sizes
make flash    # program over the on-board ST-LINK (SWD)
make debug    # OpenOCD GDB server on :3333
```

## What the firmware does

1. Prints `sensor-node boot` on USART2 (PA2/PA3, AF7, 115200 8N1).
2. Initializes I2C1 (PB8/PB9), the BME280 at 0x76 and the SSD1306 at 0x3C. Prints `ssd1306 not found` if the display does not ACK.
3. Blinks LD2 (PA5) every 500 SysTick ticks and, every second half-period, prints `temperature: 25.00` and updates the display.
4. Reads lines from USART2 (CR or LF ends a line). `temp` prints a reading at once; any other line is echoed as `echo: <line>`.

USART2 TX and RX are interrupt driven (IRQ 38). The main loop and the ISR share one 128-byte single-producer/single-consumer ring buffer per direction (`src/ringbuf.c`). The ISR sends one byte per TXE interrupt and turns TXEIE off when the TX buffer is empty; received bytes that do not fit are counted, as are overruns.

## Tests

```bash
make test-unit # Unity tests on the host: ring buffer, SSD1306 driver (fake I2C)
make test-host # BME280/I2C C driver against cortex-m-emu's peripheral models
make test-emu  # ARM build on cortex-m-emu: UART lines, RX command, OLED frame
make test      # Renode Robot tests CI enforces (skips tests tagged todo)
make test-all  # all Robot tests
make sim       # interactive Renode run with a USART2 window
```

`test-host` and `test-emu` need a cortex-m-emu checkout, `../cortex-m-emu` by default (`EMU_DIR=` to change it).

- `tests/unity/`: Unity v2.6.0, unmodified, MIT license (see `tests/unity/README.md` for hashes).
- `tests/renode/sensor_node.robot`: vector table, LED, boot banner, BME280 at 25.00 and 30.50 C, RX echo, the `temp` command (answered within 0.2 s of virtual time, so it cannot be the periodic reading), and the missing-display message.
- `renode/stm32f446_nucleo.repl`: board description. Renode has no STM32F446 file, so it extends the generic STM32F4 file and sets the F446RE flash/SRAM sizes, LD2 on PA5, B1 on PC13 and a BME280 at 0x76 on I2C1 (Renode's built-in `I2C.BME280` model). It replaces the stock I2C1 with `i2c1ack`, so the sensor is `i2c1ack.bme280` in the monitor.
- `renode/STM32F4_I2C_AckModel.cs`: MIT-licensed copy of Renode's `STM32F1_I2C` (the version pinned by Renode 1.17.0). Each change is marked `ACK model` in the source and follows RM0390: CR1.ACK/POS are stored; another byte is received only after an ACKed one; in receive mode the first byte is received only after ADDR is cleared; in transmit mode TxE is set only after ADDR is cleared, and BTF only after a data byte. An `InstantTransfers` option (off by default, so not used by these tests) completes bytes at once for cortex-m-emu's instruction-by-instruction comparison. Loaded with `include` before the platform file.
- `.github/workflows/ci.yml`: builds with `gcc-arm-none-eabi`, then runs `test-unit`, `test-host`, `test-emu` (checking out cortex-m-emu) and `make test` in Renode.

Limits: neither emulator models timing, electrical levels or real sensor noise, and passing tests do not replace logic analyzer captures on the real board. The SSD1306 init sequence and font have only been checked against cortex-m-emu's SSD1306 model, not a real module. The ring buffer relies on single-core, aligned 32-bit index loads and stores; it is not safe with more than one producer or consumer.

## Plan

| Week | Scope | Status |
| --- | --- | --- |
| 1 | Toolchain, linker script, startup code, register-level LED blink, first SWD/GDB session | Done except the SWD/GDB session on a board |
| 2 | SysTick timebase, UART TX/RX with interrupts and a ring buffer | Done |
| 3 | I2C driver, BME280 reading with fixed-point compensation, logic analyzer captures | Done except the logic analyzer captures |
| 4 | SSD1306 OLED, host-side Unity tests, GitHub Actions build | Done (OLED on the emulator model only) |

## Implementation and validation scope

PB8/PB9 use AF4/open-drain. At unchanged reset PCLK1=16 MHz, I2C FREQ=16, CCR=80 and TRISE=17 select standard-mode 100 kHz hardware configuration. Temperature uses Bosch's int32 operation order with overflow checks; pathological calibration is rejected. No floating printf, int64 compensation or pressure/humidity output is linked into the target.

`ctrl_hum` is written before `ctrl_meas`; normal mode permits repeated temperature readout. The message is `temperature: 25.00\r\n` for 2500 centidegrees C. Main reads once per two LED half-periods (1000 SysTick ticks with the default 500-tick half-period). Recurring deadlines avoid accumulating service overhead, provided I2C/UART work completes before the next deadline. The emulator's instruction ticks are not hardware milliseconds.

I2C1 code shared by both I2C drivers is in `src/i2c1.c`; `src/mmio.h` is the MMIO seam that lets the same C drivers run against host bus tests. Host tests validate 1/2/N reads, NACK recovery, saved/restored PRIMASK and compensation/formatting. They do not validate emitted ARM instructions, baud timing, electrical signals or Renode behavior. See [cortex-m-emu `docs/acceptance.md`](https://github.com/Tofuhsu/cortex-m-emu/blob/main/docs/acceptance.md).

`same()` in `src/main.c` is written with explicit branches because `return *a == *b` makes GCC emit an IT block, which cortex-m-emu does not implement.
