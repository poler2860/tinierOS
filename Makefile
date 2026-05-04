# TinierOS Bare-Metal Makefile

# Target architecture
ARCH ?= x86_64

# Toolchain
CC = gcc
AS = as
LD = ld

# Directories
BUILD_DIR = build
SRC_DIR = src
INC_DIR = include

# Compilation flags
INCLUDE_DIRS = -I$(INC_DIR) -I$(INC_DIR)/kernel -I$(INC_DIR)/arch/$(ARCH) -I$(INC_DIR)/lib
BASICFLAGS = -std=c11 -fno-builtin -ffreestanding -nostdlib -Wall -Wextra -fno-stack-protector
DEBUGFLAGS = -g3
CFLAGS = $(BASICFLAGS) $(DEBUGFLAGS) $(INCLUDE_DIRS)

# Linker flags
LDFLAGS = -nostdlib

# Source files
KERNEL_SRC = $(wildcard $(SRC_DIR)/kernel/*.c)
ARCH_SRC = $(wildcard $(SRC_DIR)/arch/$(ARCH)/*.c)
LIB_SRC = $(wildcard $(SRC_DIR)/lib/*.c)
AS_SRC = $(wildcard $(SRC_DIR)/arch/$(ARCH)/*.s)

# Object files (placed in BUILD_DIR mirroring SRC_DIR structure)
OBJ = $(KERNEL_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) \
      $(ARCH_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) \
      $(LIB_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) \
      $(AS_SRC:$(SRC_DIR)/%.s=$(BUILD_DIR)/%.o)

.PHONY: all clean

all: $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: $(OBJ)
	@echo "LD $@"
	@$(LD) $(LDFLAGS) -o $@ $^

# Rule for C files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Rule for Assembly files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(dir $@)
	@echo "AS $<"
	@$(AS) $< -o $@

clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)
