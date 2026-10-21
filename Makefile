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

.PHONY: all clean size flash debug

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin size

$(BUILD)/%.o: src/%.c | $(BUILD)
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

clean:
	rm -rf $(BUILD)
