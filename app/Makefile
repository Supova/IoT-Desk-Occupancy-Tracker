# =============================================================================
# app/Makefile — portable CLI build for the Desk Occupancy Tracker app
# =============================================================================
#
# Usage:
#   make              — build slot A (default, FLASH ORIGIN=0x08020000)
#   make SLOT=B       — build slot B (FLASH ORIGIN=0x08040000)
#   make clean        — remove all build outputs
#   make size         — print section sizes for the current slot
#
# Output:
#   build/slot<SLOT>/app.elf   — ELF with debug info (flash with OpenOCD/STLink)
#   build/slot<SLOT>/app.bin   — raw binary for OTA or CubeProgrammer
#
# Toolchain:
#   arm-none-eabi-gcc  (GNU Tools for STM32 13.3.rel1 or any compatible version)
#   Ubuntu install:  sudo apt-get install gcc-arm-none-eabi binutils-arm-none-eabi
#   Windows install: https://developer.arm.com/downloads/-/gnu-rm
#
# CubeIDE coexistence:
#   CubeIDE's generated files live in app/Debug/ and are NOT used here.
#   This Makefile is entirely independent.  CubeIDE regeneration will not
#   affect this file.
#
# Slot handling:
#   VECT_TAB_OFFSET is passed as a compiler -D flag so it overrides the
#   #if !defined(VECT_TAB_OFFSET) guard in system_stm32f4xx.c without
#   touching the source file.
#   The linker script FLASH ORIGIN is patched via sed into a build-local copy
#   (build/slot<SLOT>/linker.ld), so the original .ld is never modified.
#
# application_config.h:
#   Not committed (contains credentials).  Copy from
#   IOT_SDK/config/application_config_example.h and fill in your values
#   before running make.  In CI, the file is generated from GitHub Secrets.
# =============================================================================

# ---------------------------------------------------------------------------
# Slot selection — A or B
# ---------------------------------------------------------------------------
SLOT ?= A

ifeq ($(SLOT),A)
  FLASH_ORIGIN    := 0x08020000
  VECT_TAB_OFFSET := 0x00020000U
else ifeq ($(SLOT),B)
  FLASH_ORIGIN    := 0x08040000
  VECT_TAB_OFFSET := 0x00040000U
else
  $(error SLOT must be A or B, got: $(SLOT))
endif

BUILD_DIR := build/slot$(SLOT)

# ---------------------------------------------------------------------------
# Toolchain
# ---------------------------------------------------------------------------
CC      := arm-none-eabi-gcc
AS      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size

# ---------------------------------------------------------------------------
# Source files — C
# ---------------------------------------------------------------------------
C_SRCS := \
  Core/Src/boot_descriptor.c \
  Core/Src/custom_job_parser.c \
  Core/Src/main.c \
  Core/Src/ota_application.c \
  Core/Src/stm32f4xx_hal_msp.c \
  Core/Src/stm32f4xx_hal_timebase_tim.c \
  Core/Src/stm32f4xx_it.c \
  Core/Src/syscalls.c \
  Core/Src/sysmem.c \
  Core/Src/system_stm32f4xx.c \
  \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c \
  Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c \
  \
  IOT_SDK/BSP/esp32_at.c \
  IOT_SDK/BSP/esp32_at_io.c \
  IOT_SDK/BSP/utils/ota_flash.c \
  IOT_SDK/mqtt_helper/mqtt_helper.c \
  \
  IOT_SDK/Thirdparty/FreeRTOS-Kernel/croutine.c \
  IOT_SDK/Thirdparty/FreeRTOS-Kernel/event_groups.c \
  IOT_SDK/Thirdparty/FreeRTOS-Kernel/freertos_hooks.c \
  IOT_SDK/Thirdparty/FreeRTOS-Kernel/list.c \
  IOT_SDK/Thirdparty/FreeRTOS-Kernel/queue.c \
  IOT_SDK/Thirdparty/FreeRTOS-Kernel/stream_buffer.c \
  IOT_SDK/Thirdparty/FreeRTOS-Kernel/tasks.c \
  IOT_SDK/Thirdparty/FreeRTOS-Kernel/timers.c \
  IOT_SDK/Thirdparty/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c \
  IOT_SDK/Thirdparty/FreeRTOS-Kernel/portable/MemMang/heap_4.c \
  \
  IOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/3rdparty/tinycbor/src/cborencoder.c \
  IOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/3rdparty/tinycbor/src/cborencoder_close_container_checked.c \
  IOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/3rdparty/tinycbor/src/cborerrorstrings.c \
  IOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/3rdparty/tinycbor/src/cborparser.c \
  IOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/3rdparty/tinycbor/src/cborparser_dup_string.c \
  IOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/3rdparty/tinycbor/src/cborvalidation.c \
  \
  IOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/aws/Jobs-for-AWS-IoT-embedded-sdk/source/jobs.c \
  IOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/aws/Jobs-for-AWS-IoT-embedded-sdk/source/otaJobParser/job_parser.c \
  IOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/aws/Jobs-for-AWS-IoT-embedded-sdk/source/otaJobParser/ota_job_handler.c \
  \
  IOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/aws/aws-iot-core-mqtt-file-streams/source/MQTTFileDownloader.c \
  IOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/aws/aws-iot-core-mqtt-file-streams/source/MQTTFileDownloader_base64.c \
  IOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/aws/aws-iot-core-mqtt-file-streams/source/MQTTFileDownloader_cbor.c \
  \
  IOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/standard/coreJSON/source/core_json.c

# ---------------------------------------------------------------------------
# Source files — Assembly
# ---------------------------------------------------------------------------
ASM_SRCS := \
  Core/Startup/startup_stm32f407vgtx.s

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
INCLUDES := \
  -ICore/Inc \
  -ICore/Src \
  -IDrivers/STM32F4xx_HAL_Driver/Inc \
  -IDrivers/STM32F4xx_HAL_Driver/Inc/Legacy \
  -IDrivers/CMSIS/Device/ST/STM32F4xx/Include \
  -IDrivers/CMSIS/Include \
  -IIOT_SDK/BSP \
  -IIOT_SDK/BSP/utils \
  -IIOT_SDK/config \
  -IIOT_SDK/mqtt_helper \
  -IIOT_SDK/Thirdparty/FreeRTOS-Kernel/include \
  -IIOT_SDK/Thirdparty/FreeRTOS-Kernel/portable/GCC/ARM_CM4F \
  -IIOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/standard/coreJSON/source/include \
  -IIOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/standard/logging-stack \
  -IIOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/aws/aws-iot-core-mqtt-file-streams/source/include \
  -IIOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/3rdparty/tinycbor/src \
  -IIOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/aws/Jobs-for-AWS-IoT-embedded-sdk/source/include \
  -IIOT_SDK/Thirdparty/aws-iot-device-sdk-libraries/aws/Jobs-for-AWS-IoT-embedded-sdk/source/otaJobParser/include \
  -IMiddlewares/Third_Party/Infineon_Wireless_Connectivity/aws-iot-device-sdk-embedded-C/platform/include \
  -IMiddlewares/Third_Party/Infineon_Wireless_Connectivity/aws-iot-device-sdk-embedded-C/libraries/standard/coreMQTT \
  -IMiddlewares/Third_Party/Infineon_Wireless_Connectivity/aws-iot-device-sdk-embedded-C/libraries/standard/coreMQTT/source/include \
  -IMiddlewares/Third_Party/Infineon_Wireless_Connectivity/aws-iot-device-sdk-embedded-C/libraries/standard/coreMQTT/source/interface \
  -IMiddlewares/Third_Party/Infineon_Wireless_Connectivity/aws-iot-device-sdk-embedded-C/libraries/standard/backoffAlgorithm/source/include

# ---------------------------------------------------------------------------
# Flags
# ---------------------------------------------------------------------------
MCU := -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb

DEFINES := \
  -DDEBUG \
  -DUSE_HAL_DRIVER \
  -DSTM32F407xx \
  '-DMBEDTLS_CONFIG_FILE="mbedtls_config.h"' \
  -DVECT_TAB_OFFSET=$(VECT_TAB_OFFSET)

# -DVECT_TAB_OFFSET on the command line takes precedence over the
# #if !defined(VECT_TAB_OFFSET) guard in system_stm32f4xx.c, so the
# correct value is used for each slot without touching the source file.

CFLAGS := \
  $(MCU) \
  -std=gnu11 \
  -g3 \
  -O0 \
  $(DEFINES) \
  $(INCLUDES) \
  -ffunction-sections \
  -fdata-sections \
  -Wall \
  -fstack-usage \
  -MMD -MP \
  --specs=nano.specs

ASFLAGS := \
  $(MCU) \
  -g3 \
  -x assembler-with-cpp \
  -MMD -MP \
  --specs=nano.specs

# The linker script is generated per-slot (see the linker.ld rule below).
LDFLAGS := \
  $(MCU) \
  -T$(BUILD_DIR)/linker.ld \
  --specs=nosys.specs \
  --specs=nano.specs \
  -Wl,-Map=$(BUILD_DIR)/app.map \
  -Wl,--gc-sections \
  -static \
  -Wl,--start-group -lc -lm -Wl,--end-group

# ---------------------------------------------------------------------------
# Derived object and dependency file lists
# ---------------------------------------------------------------------------
OBJS := \
  $(addprefix $(BUILD_DIR)/, $(C_SRCS:.c=.o)) \
  $(addprefix $(BUILD_DIR)/, $(ASM_SRCS:.s=.o))

DEPS := $(OBJS:.o=.d)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.PHONY: all clean size

all: $(BUILD_DIR)/app.bin

# ---- Raw binary (used for OTA and CRC computation) ------------------------
$(BUILD_DIR)/app.bin: $(BUILD_DIR)/app.elf
	$(OBJCOPY) -O binary $< $@
	@echo ""
	@echo "Built: $@ (slot $(SLOT), FLASH ORIGIN=$(FLASH_ORIGIN))"
	$(SIZE) $<

# ---- Link ELF --------------------------------------------------------------
$(BUILD_DIR)/app.elf: $(OBJS) $(BUILD_DIR)/linker.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@

# ---- Slot-specific linker script -------------------------------------------
# Patch the FLASH ORIGIN in a build-local copy so the source .ld is never
# modified.  The sed substitution replaces the slot A default; if SLOT=A
# this is a no-op (same value), which is fine.
$(BUILD_DIR)/linker.ld: STM32F407VGTX_FLASH.ld | $(BUILD_DIR)
	sed 's/ORIGIN = 0x08020000/ORIGIN = $(FLASH_ORIGIN)/' $< > $@

# ---- Compile C sources ------------------------------------------------------
# Object files mirror the source tree under BUILD_DIR.
# -MMD generates a .d file alongside each .o for dependency tracking.
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ---- Assemble startup file --------------------------------------------------
$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# ---- Create build directory -------------------------------------------------
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ---- Clean ------------------------------------------------------------------
clean:
	rm -rf build/

# ---- Print section sizes ----------------------------------------------------
size: $(BUILD_DIR)/app.elf
	$(SIZE) $<

# ---------------------------------------------------------------------------
# Include auto-generated dependency files
# ---------------------------------------------------------------------------
# The leading dash suppresses errors on first build (before .d files exist).
-include $(DEPS)
