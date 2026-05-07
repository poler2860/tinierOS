# TinierOS Bare-Metal Makefile
ARCH     = avr
MCU      = atmega328p
F_CPU    = 16000000UL
CC       = avr-gcc
AS       = avr-gcc -x assembler-with-cpp
LD       = avr-gcc
OBJCOPY  = avr-objcopy


# Directories
BUILD_DIR = build
SRC_DIR = src
INC_DIR = include

# Compilation flags
INCLUDE_DIRS = -I$(INC_DIR) -I$(INC_DIR)/kernel -I$(INC_DIR)/arch/$(ARCH) -I$(INC_DIR)/lib
CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) \
	-Os -g3 -std=c11 \
    -ffreestanding -fno-builtin -nostdlib \
    -fno-stack-protector -Wall -Wextra \
    $(INCLUDE_DIRS)

# Linker flags
LDFLAGS  = -mmcu=$(MCU) -nostdlib -T linker/atmega328p.ld -lgcc

# Source files
KERNEL_SRC = $(wildcard $(SRC_DIR)/kernel/*.c)
ARCH_SRC = $(wildcard $(SRC_DIR)/arch/$(ARCH)/*.c)
LIB_SRC = $(wildcard $(SRC_DIR)/lib/*.c)
AS_SRC = $(wildcard $(SRC_DIR)/arch/$(ARCH)/*.S)

# Object files (placed in BUILD_DIR mirroring SRC_DIR structure)
OBJ = $(KERNEL_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) \
      $(ARCH_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) \
      $(LIB_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) \
      $(AS_SRC:$(SRC_DIR)/%.S=$(BUILD_DIR)/%.o)

.PHONY: all clean size flash

all: $(BUILD_DIR)/kernel.hex

$(BUILD_DIR)/kernel.hex: $(BUILD_DIR)/kernel.elf
	@echo "OBJCOPY $@"
	@$(OBJCOPY) -O ihex -R .eeprom $< $@

$(BUILD_DIR)/kernel.elf: $(OBJ)
	@echo "LD $@"
	@$(CC) $(LDFLAGS) -o $@ $^

# Rule for C files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Rule for Assembly files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S
	@mkdir -p $(dir $@)
	@echo "AS $<"
	@$(AS) $(CFLAGS) -c $< -o $@

clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

# Show Flash/SRAM usage broken down by section
size: $(BUILD_DIR)/kernel.elf
	avr-size --format=avr --mcu=$(MCU) $<

# Flash to the board via UART bootloader (Uno R3 default)
flash: $(BUILD_DIR)/kernel.hex
	avrdude -p $(MCU) -c avrisp -P /dev/ttyACM0 -b 19200 \
	        -U flash:w:$<:i
