TARGET ?= dfu_bootloader_updater
BUILD_DIR ?= build

PREFIX ?= arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
PYTHON ?= python3

LDSCRIPT = ../example_blink/MDR32F9Q2I_app.ld
BOOTLOADER_BIN ?= ../build/milandr_dfu_bootloader.bin

MCU = -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
OPT ?= -Os -flto
DEFS = -DMCU_milandr

PLATFORM_DIR = ../platforms/milandr
COMMON_DIR = $(PLATFORM_DIR)/dep/common
DRIVERS_DIR = $(COMMON_DIR)/Drivers

C_SOURCES = \
  main.c \
  bootloader_image.c \
  $(DRIVERS_DIR)/CMSIS/DeviceSupport/startup/system_K1986VE9xI.c \
  $(DRIVERS_DIR)/SPL/src/MDR32FxQI_rst_clk.c \
  $(DRIVERS_DIR)/SPL/src/MDR32FxQI_eeprom.c \
  $(DRIVERS_DIR)/SPL/src/MDR32FxQI_port.c

ASM_SOURCES_s = \
  $(COMMON_DIR)/Startup/startup_gcc_MDR32F9Q2I.s

ASM_SOURCES_S = \
  $(DRIVERS_DIR)/SPL/src/MDR32FxQI_asm_GCC.S

C_INCLUDES = \
  -I. \
  -I$(BUILD_DIR) \
  -I../src \
  -I$(PLATFORM_DIR) \
  -I$(PLATFORM_DIR)/sdk \
  -I$(COMMON_DIR)/inc \
  -I$(DRIVERS_DIR)/CMSIS/CoreSupport \
  -I$(DRIVERS_DIR)/CMSIS/DeviceSupport/inc \
  -I$(DRIVERS_DIR)/CMSIS/DeviceSupport/startup \
  -I$(DRIVERS_DIR)/SPL \
  -I$(DRIVERS_DIR)/SPL/inc \
  -I$(DRIVERS_DIR)/SPL/inc/IRQ_Handler_Template

CFLAGS = $(MCU) $(DEFS) $(C_INCLUDES) $(OPT) -Wall -Wextra \
         -ffunction-sections -fdata-sections -fno-common \
         -Wno-unused-parameter -Wno-deprecated -MMD -MP

ASFLAGS = $(MCU) $(DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections -MMD -MP

LDFLAGS = $(MCU) $(OPT) -mthumb-interwork --specs=nano.specs -T$(LDSCRIPT) \
          -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections \
          -Wl,--print-memory-usage -Wl,--no-warn-rwx-segments

ifneq ($(findstring sh,$(SHELL)),)
  MKDIR = mkdir -p $(1)
  RMDIR = rm -rf $(1)
else
  FIX_PATH = $(subst /,\,$(1))
  MKDIR = if not exist "$(call FIX_PATH,$(1))" mkdir "$(call FIX_PATH,$(1))"
  RMDIR = if exist "$(call FIX_PATH,$(1))" rmdir /s /q "$(call FIX_PATH,$(1))"
endif

OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(C_SOURCES:.c=.o))) \
          $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SOURCES_s:.s=.o))) \
          $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SOURCES_S:.S=.o)))

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES_s)))
vpath %.S $(sort $(dir $(ASM_SOURCES_S)))

all: $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex
	@$(SZ) $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/bootloader_image.inc: $(BOOTLOADER_BIN) | $(BUILD_DIR)
	xxd -i < $< > $@ || $(PYTHON) -c 'import sys; data=open(sys.argv[1],"rb").read(); open(sys.argv[2],"w").write(", ".join(f"0x{b:02x}" for b in data))' $< $@

$(BUILD_DIR)/bootloader_image.o: bootloader_image.c $(BUILD_DIR)/bootloader_image.inc | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) $(LDSCRIPT)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(CP) -O binary $< $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(CP) -O ihex $< $@

$(BUILD_DIR):
	@$(call MKDIR,$(BUILD_DIR))

clean:
	@$(call RMDIR,$(BUILD_DIR))

-include $(wildcard $(BUILD_DIR)/*.d)
.PHONY: all clean
