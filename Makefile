TARGET ?= milandr_dfu_bootloader
BUILD_DIR ?= build

# Toolchain settings
PREFIX ?= arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size

# Linker script
LDSCRIPT = MDR32F9Q2I_bootloader.ld

# Optimization and CPU flags
MCU = -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
OPT ?= -O2

# C sources
C_SOURCES = \
src/main.c \
src/flash.c \
src/usb_dfu.c \
platforms/milandr/dep/common/src/board_usb.c \
platforms/milandr/dep/common/Drivers/CMSIS/DeviceSupport/startup/system_K1986VE9xI.c \
platforms/milandr/dep/common/Drivers/SPL/src/MDR32FxQI_rst_clk.c \
platforms/milandr/dep/common/Drivers/SPL/src/MDR32FxQI_eeprom.c \
platforms/milandr/dep/common/Drivers/SPL/src/MDR32FxQI_bkp.c \
platforms/milandr/dep/common/Drivers/SPL/src/MDR32FxQI_port.c \
platforms/milandr/dep/common/Drivers/SPL/src/MDR32FxQI_usb.c \
platforms/milandr/dep/common/Drivers/SPL/src/USB_Library/MDR32FxQI_usb_device.c

# Assembly sources
ASM_SOURCES_s = \
platforms/milandr/dep/common/Startup/startup_gcc_MDR32F9Q2I.s

ASM_SOURCES_S = \
platforms/milandr/dep/common/Drivers/SPL/src/MDR32FxQI_asm_GCC.S

# Include paths
C_INCLUDES = \
-Isrc \
-Iplatforms/milandr \
-Iplatforms/milandr/sdk \
-Iplatforms/milandr/sdk/USB_Library \
-Iplatforms/milandr/dep/common/inc \
-Iplatforms/milandr/dep/common/Drivers/CMSIS/CoreSupport \
-Iplatforms/milandr/dep/common/Drivers/CMSIS/DeviceSupport/inc \
-Iplatforms/milandr/dep/common/Drivers/CMSIS/DeviceSupport/startup \
-Iplatforms/milandr/dep/common/Drivers/SPL \
-Iplatforms/milandr/dep/common/Drivers/SPL/inc \
-Iplatforms/milandr/dep/common/Drivers/SPL/inc/IRQ_Handler_Template \
-Iplatforms/milandr/dep/common/Drivers/SPL/inc/USB_Library

# Compiler and Assembler flags
CFLAGS = $(MCU) $(C_INCLUDES) $(OPT) -Wall -Wextra \
         -ffunction-sections -fdata-sections -fno-common -fmessage-length=0 \
         -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-deprecated \
         -MMD -MP

ASFLAGS = $(MCU) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections -MMD -MP

# Linker flags
LDFLAGS = $(MCU) -mthumb-interwork --specs=nano.specs -T$(LDSCRIPT) \
          -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections \
          -Wl,--print-memory-usage -Wl,--no-warn-rwx-segments

# Flashing settings (OpenOCD)
OPENOCD ?= openocd
PROBE_CONFIG ?= platforms/milandr/dep/probe/jlink4swd.cfg

# Cross-platform helpers for directory creation and removal
ifneq ($(findstring sh,$(SHELL)),)
  MKDIR = mkdir -p $(1)
  RMDIR = rm -rf $(1)
else
  FIX_PATH = $(subst /,\,$(1))
  MKDIR = if not exist "$(call FIX_PATH,$(1))" mkdir "$(call FIX_PATH,$(1))"
  RMDIR = if exist "$(call FIX_PATH,$(1))" rmdir /s /q "$(call FIX_PATH,$(1))"
endif

# Object files mapping (flat inside $(BUILD_DIR))
OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SOURCES_s:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES_s)))

OBJECTS += $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SOURCES_S:.S=.o)))
vpath %.S $(sort $(dir $(ASM_SOURCES_S)))

# --- Targets ---
.PHONY: all bootloader blink clean clean-all flash help

all: bootloader

bootloader: $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex

blink:
	$(MAKE) -C example_blink

$(BUILD_DIR):
	@$(call MKDIR,$(BUILD_DIR))

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	@echo Assembling $<
	@$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)
	@echo Assembling $<
	@$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	@echo Linking $@
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	@$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	@echo Generating $@
	@$(CP) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	@echo Generating $@
	@$(CP) -O binary -S $< $@

flash: $(BUILD_DIR)/$(TARGET).bin
	$(OPENOCD) -f $(PROBE_CONFIG) -c "init" -c "halt" \
		-c "program $< verify reset 0x08000000" -c "exit"

clean:
	@$(call RMDIR,$(BUILD_DIR))
	@echo Cleaned $(BUILD_DIR)

clean-all: clean
	@$(MAKE) -C example_blink clean

help:
	@echo "Available targets:"
	@echo "  make              - Build bootloader (ELF, BIN, HEX)"
	@echo "  make bootloader   - Build bootloader"
	@echo "  make blink        - Build blink example application"
	@echo "  make flash        - Flash bootloader via OpenOCD"
	@echo "  make clean        - Remove bootloader build artifacts"
	@echo "  make clean-all    - Remove both bootloader and example build artifacts"

-include $(wildcard $(BUILD_DIR)/*.d)
