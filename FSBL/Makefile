#### Target and Hardware Details ####

BUILD := bin
TARGET := CivicAlertAI
LOADER := CivicAlertAiLoader.stldr
EXECUTABLE := $(BUILD)/$(TARGET).elf
BINARY := $(BUILD)/$(TARGET).bin
SIGNED := $(BUILD)/$(TARGET)-signed.bin
MAP_FILE := $(BUILD)/$(TARGET).map
SIZE_OUTPUT := default.size.stdout
OBJDUMP_LIST := $(BUILD)/$(TARGET).list
LINKER_FILE := STM32N647L0HXQ_AXISRAM2_fsbl.ld
AI_LINKER_FILE := NetworkRuntime1020_CM55_GCC

ifndef STM32_PRG_PATH
ifeq ($(OS),Windows_NT)
STM32_PRG_PATH := $(dir $(shell where $(FLASH)))
else
STM32_PRG_PATH := $(dir $(shell which $(FLASH)))
endif
endif

ifndef STM32_PRG_PATH
$(error You must specify the STM32_PRG_PATH to continue)
endif

CPU := cortex-m55
FPU := fpv5-d16
FABI := hard

CC := arm-none-eabi-gcc
SIZE := arm-none-eabi-size
OBJDUMP := arm-none-eabi-objdump
OBJCOPY := arm-none-eabi-objcopy
MKDIR := mkdir -p
RM := rm -rf
FLASH := STM32_Programmer_CLI
SIGN := STM32_SigningTool_CLI

DEFINES := -DUSE_HAL_DRIVER -DSTM32N647xx -DARM_MATH_CM55 -DLL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6 -DLL_ATON_RT_MODE=LL_ATON_RT_ASYNC -DLL_ATON_OSAL=LL_ATON_OSAL_BARE_METAL -DLL_ATON_DBG_BUFFER_INFO_EXCLUDED=1 -DLL_ATON_SW_FALLBACK -DFX_INCLUDE_USER_DEFINE_FILE

VPATH := Core/Src FileX/App FileX/Target ../AiModel ../Drivers/STM32N6xx_HAL_Driver/Src ../Middlewares/ST/AI/Npu/Devices/STM32N6XX ../Middlewares/ST/AI/Npu/ll_aton ../Middlewares/ST/filex/common/drivers ../Middlewares/ST/filex/common/src
INCLUDES := -ICore/Inc -IFileX/App -IFileX/Target -I../AiModel -I../Drivers/STM32N6xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Include -I../Middlewares/ST/AI/Inc -I../Middlewares/ST/AI/Npu/Devices/STM32N6XX -I../Middlewares/ST/AI/Npu/ll_aton -I../Middlewares/ST/filex/common/inc -I../Middlewares/ST/filex/ports/generic/inc
SOURCES := $(foreach SOURCE,$(foreach DIR,$(VPATH),$(wildcard $(DIR)/*.c)),$(patsubst %.c,%.c,$(notdir $(SOURCE))))
OBJECTS := $(SOURCES:%.c=$(BUILD)/%.o) $(BUILD)/startup_stm32n647l0hxq_fsbl.o

CFLAGS  = -mcpu=$(CPU) -mtune=$(CPU) -std=gnu11 $(DEFINES) $(INCLUDES) -ffunction-sections -fdata-sections -falign-functions=16 -Wall -fstack-usage -mcmse -MMD -MP
CFLAGS += --specs=nano.specs -mfpu=$(FPU) -mfloat-abi=$(FABI) -mthumb
LFLAGS  = -mcpu=$(CPU) -mtune=$(CPU) --specs=nosys.specs -Wl,-Map=$(MAP_FILE) -Wl,--gc-sections -static -Wl,--cmse-implib -Wl,--out-implib=$(BUILD)/secure_nsclib.o
LFLAGS += --specs=nano.specs -mfpu=$(FPU) -mfloat-abi=$(FABI) -mthumb -Wl,--start-group -lc -lm -Wl,--end-group


#### Build Targets ####

all: CFLAGS += -Ofast
all: directories $(EXECUTABLE) secondary-outputs
	$(SIGN) -bin $(BINARY) -nk -of 0x80000000 -t fsbl -o $(SIGNED) -hv 2.3 -dump $(SIGNED)

debug: CFLAGS += -g3 -DDEBUG -O0
debug: directories $(EXECUTABLE) secondary-outputs
	$(SIGN) -bin $(BINARY) -nk -of 0x80000000 -t fsbl -o $(SIGNED) -hv 2.3 -dump $(SIGNED)

directories: $(BUILD)

$(BUILD):
	-$(MKDIR) $@
	
secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST) $(BINARY)

$(EXECUTABLE) $(MAP_FILE): $(OBJECTS)
	$(CC) -o $@ -T$(LINKER_FILE) $(OBJECTS) $(LFLAGS) -L../Middlewares/ST/AI/Lib -l$(AI_LINKER_FILE)

$(SIZE_OUTPUT): $(EXECUTABLE)
	$(SIZE) $(EXECUTABLE)

$(OBJDUMP_LIST): $(EXECUTABLE)
	$(OBJDUMP) -h -S $(EXECUTABLE) > $@

$(BINARY): $(EXECUTABLE)
	$(OBJCOPY) -O binary $(EXECUTABLE) $@

$(BUILD)/startup_stm32n647l0hxq_fsbl.o: Core/Startup/startup_stm32n647l0hxq_fsbl.s
	$(CC) -mcpu=$(CPU) -c -x assembler-with-cpp -MMD -MP --specs=nano.specs -mfpu=$(FPU) -mfloat-abi=$(FABI) -mthumb $< -o $@

$(BUILD)/%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	-$(RM) $(BUILD)

flash: all
	$(STM32_PRG_PATH)/$(FLASH) -c port=SWD mode=UR -el $(STM32_PRG_PATH)/ExternalLoader/$(LOADER) -d $(SIGNED) 0x70000000 -v

.PHONY: all directories secondary-outputs clean flash
