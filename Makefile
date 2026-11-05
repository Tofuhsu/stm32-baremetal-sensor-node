TARGET  := firmware
BUILD   := build
SRCS    := $(wildcard src/*.c)
OBJS    := $(SRCS:src/%.c=$(BUILD)/%.o)

CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size

CPU     := -mcpu=cortex-m4 -mthumb
CFLAGS  := $(CPU) -std=c11 -O0 -g3 -Wall -Wextra -Werror -ffreestanding -ffunction-sections -fdata-sections
LDFLAGS := $(CPU) -T linker.ld -nostdlib -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map

.PHONY: all clean size flash debug sim test test-all

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin size

$(BUILD)/%.o: src/%.c $(wildcard src/*.h) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD):
	mkdir -p $@

size: $(BUILD)/$(TARGET).elf
	$(SIZE) $<

# Needs the board connected over the on-board ST-LINK.
flash: $(BUILD)/$(TARGET).elf
	openocd -f board/st_nucleo_f4.cfg -c "program $< verify reset exit"

# Starts an OpenOCD GDB server on :3333. In another terminal: arm-none-eabi-gdb build/firmware.elf
debug:
	openocd -f board/st_nucleo_f4.cfg

# Renode (emulator). Defaults to the macOS app in ~/Applications; CI passes its own path.
RENODE      ?= $(HOME)/Applications/Renode.app/Contents/MacOS/renode
RENODE_TEST ?= $(HOME)/Applications/Renode.app/Contents/MacOS/renode-test

# Interactive run with a UART window. Monitor commands: start, pause, i2c1.bme280 Temperature 30
sim: $(BUILD)/$(TARGET).elf
	$(RENODE) renode/sensor_node.resc

# Tests CI enforces (skips features tagged `todo`).
test: $(BUILD)/$(TARGET).elf
	$(RENODE_TEST) tests/renode/sensor_node.robot --exclude todo

# Every test, including the ones for unwritten features (expected to fail for now).
test-all: $(BUILD)/$(TARGET).elf
	$(RENODE_TEST) tests/renode/sensor_node.robot

clean:
	rm -rf $(BUILD) logs snapshots robot_output.xml log.html report.html

# Pure host protocol check, deliberately separate from target/Robot acceptance.
HOST_CC ?= cc
EMU_DIR ?= ../cortex-m-emu
HOST_MODELS := $(EMU_DIR)/src/bus.c $(EMU_DIR)/src/periph/rcc.c $(EMU_DIR)/src/periph/gpio.c $(EMU_DIR)/src/periph/i2c.c $(EMU_DIR)/src/periph/bme280.c
.PHONY: test-host
test-host: | $(BUILD)
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -O2 -g -DFIRMWARE_HOST_TEST -I$(EMU_DIR)/src tests/host/test_bme280_driver.c src/temperature.c $(HOST_MODELS) -o $(BUILD)/test_host_driver
	./$(BUILD)/test_host_driver
