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
LINKER_FILE := STM32N647_CivicAlert.ld
AI_LINKER_FILE := NetworkRuntime1100_CM55_GCC

SD_TARGET := Sd$(TARGET)
SD_EXECUTABLE := $(BUILD)/$(SD_TARGET).elf
SD_BINARY := $(BUILD)/$(SD_TARGET).bin
SD_SIGNED := $(BUILD)/$(SD_TARGET)-signed.bin

TEST_TARGET := Test$(TARGET)
TEST_EXECUTABLE := $(BUILD)/$(TEST_TARGET).elf
TEST_BINARY := $(BUILD)/$(TEST_TARGET).bin
TEST_SIGNED := $(BUILD)/$(TEST_TARGET)-signed.bin

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

ifndef BOARD_REV
$(error You must specify a BOARD_REV (A, B, etc) to continue)
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

DEFINES := -D_DATETIME=$(shell date +%s) -DBOARD_REV=$(BOARD_REV) -D_FW_REVISION=$(shell cd .. && git log --pretty=format:'%H' -n 1 | cut -c 1-8) -DSTM32N647xx -DPERIPH_GET_TICK -DARM_MATH_HELIUM -DLL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6 -DLL_ATON_RT_MODE=LL_ATON_RT_ASYNC -DLL_ATON_OSAL=LL_ATON_OSAL_BARE_METAL -DLL_ATON_DBG_BUFFER_INFO_EXCLUDED=1 -DLL_ATON_SW_FALLBACK

VPATH := Core/Src ../AiModel Middlewares/ST/AI/Npu/Devices/STM32N6XX Middlewares/ST/AI/Npu/ll_aton Middlewares/FatFS
INCLUDES := -ICore/Inc -I../AiModel -I../Drivers/STM32N6xx_HAL_Driver/Inc -I../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -IDrivers/CMSIS/Device/ST/STM32N6xx/Include -IDrivers/CMSIS/Include -IDrivers/CMSIS_DSP/Include -IDrivers/CMSIS_DSP/Include/dsp -IDrivers/CMSIS_DSP/PrivateInclude -IMiddlewares/ST/AI/Inc -IMiddlewares/ST/AI/Npu/Devices/STM32N6XX -IMiddlewares/ST/AI/Npu/ll_aton -IMiddlewares/FatFS
SOURCES := $(filter-out main.c, $(foreach SOURCE,$(foreach DIR,$(VPATH),$(wildcard $(DIR)/*.c)),$(patsubst %.c,%.c,$(notdir $(SOURCE)))))
CMSIS_DSP_SOURCES := arm_common_tables.c arm_const_structs.c arm_mve_tables.c arm_dot_prod_f32.c arm_mult_f32.c arm_cmplx_mag_f32.c arm_cmplx_mag_squared_f32.c arm_copy_f32.c arm_copy_q7.c arm_copy_q15.c arm_offset_f32.c arm_q15_to_float.c arm_cfft_f32.c arm_cfft_init_f32.c arm_rfft_fast_f32.c arm_rfft_fast_init_f32.c arm_hanning_f32.c arm_max_no_idx_f32.c arm_min_no_idx_f32.c arm_scale_f32.c
VPATH +=  Testing ../Drivers/STM32N6xx_HAL_Driver/Src Drivers/CMSIS_DSP/Source/BasicMathFunctions Drivers/CMSIS_DSP/Source/ComplexMathFunctions Drivers/CMSIS_DSP/Source/CommonTables Drivers/CMSIS_DSP/Source/FastMathFunctions Drivers/CMSIS_DSP/Source/StatisticsFunctions Drivers/CMSIS_DSP/Source/SupportFunctions Drivers/CMSIS_DSP/Source/TransformFunctions Drivers/CMSIS_DSP/Source/WindowFunctions
OBJECTS := $(SOURCES:%.c=$(BUILD)/%.o) $(CMSIS_DSP_SOURCES:%.c=$(BUILD)/%.o) $(BUILD)/startup_stm32n647l0hxq_fsbl.o $(BUILD)/stm32n6xx_hal_xspi.o $(BUILD)/stm32n6xx_ll_sdmmc.o

CFLAGS  = -mcpu=$(CPU) -mtune=$(CPU) -std=gnu11 $(DEFINES) $(INCLUDES) -flax-vector-conversions -fno-short-wchar -funsigned-char -fshort-enums -ffunction-sections -fdata-sections -falign-functions=16 -Wall -fstack-usage -mcmse -MMD -MP
CFLAGS += --specs=nano.specs -mfpu=$(FPU) -mfloat-abi=$(FABI) -mthumb
LFLAGS  = -mcpu=$(CPU) -mtune=$(CPU) --specs=nosys.specs -Wl,-Map=$(MAP_FILE) -Wl,--gc-sections -static -Wl,--cmse-implib -Wl,--out-implib=$(BUILD)/secure_nsclib.o
LFLAGS += --specs=nano.specs -mfpu=$(FPU) -mfloat-abi=$(FABI) -mthumb -z noexecstack -u _printf_float -Wl,--start-group -lc -lm -Wl,--end-group


#### Build Targets ####

all: CFLAGS += -Ofast
all: DEFINES += -DUSE_FLAC_ENCODER
all: directories $(EXECUTABLE) secondary-outputs
	$(SIGN) -bin $(BINARY) -nk -of 0x80000000 -t fsbl -o $(SIGNED) -hv 2.3 -dump $(SIGNED) -s --align

debug: CFLAGS += -g3 -DDEBUG -O0
debug: DEFINES += -DUSE_FLAC_ENCODER
debug: directories $(EXECUTABLE) secondary-outputs
	$(SIGN) -bin $(BINARY) -nk -of 0x80000000 -t fsbl -o $(SIGNED) -hv 2.3 -dump $(SIGNED) -s --align

sd: CFLAGS += -Ofast
sd: DEFINES += -DPACKET_FULL_AUDIO
sd: directories $(SD_EXECUTABLE) $(SD_BINARY)
	$(SIGN) -bin $(SD_BINARY) -nk -of 0x80000000 -t fsbl -o $(SD_SIGNED) -hv 2.3 -dump $(SD_SIGNED) -s --align
	$(STM32_PRG_PATH)/$(FLASH) -c port=SWD mode=UR -el $(STM32_PRG_PATH)/ExternalLoader/$(LOADER) -d $(SD_SIGNED) 0x70000000 -v

test: CFLAGS += -Ofast
test: DEFINES += -DPERIPHERAL_TESTS -DUSE_FLAC_ENCODER
test: directories $(TEST_EXECUTABLE) $(TEST_BINARY)
	$(SIGN) -bin $(TEST_BINARY) -nk -of 0x80000000 -t fsbl -o $(TEST_SIGNED) -hv 2.3 -dump $(TEST_SIGNED) -s --align
	$(STM32_PRG_PATH)/$(FLASH) -c port=SWD mode=UR -el $(STM32_PRG_PATH)/ExternalLoader/$(LOADER) -d $(TEST_SIGNED) 0x70000000 -v

directories: $(BUILD)

$(BUILD):
	-$(MKDIR) $@
	
secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST) $(BINARY)

$(EXECUTABLE) $(MAP_FILE): $(OBJECTS) $(BUILD)/main.o
	$(CC) -o $@ -T$(LINKER_FILE) $^ $(LFLAGS) -LMiddlewares/ST/AI/Lib -l$(AI_LINKER_FILE)

$(SD_EXECUTABLE): $(OBJECTS) $(BUILD)/sdcard.o
	$(CC) -o $@ -T$(LINKER_FILE) $^ $(LFLAGS) -LMiddlewares/ST/AI/Lib -l$(AI_LINKER_FILE)

$(TEST_EXECUTABLE): $(OBJECTS) $(BUILD)/manufacturing.o
	$(CC) -o $@ -T$(LINKER_FILE) $^ $(LFLAGS) -LMiddlewares/ST/AI/Lib -l$(AI_LINKER_FILE)

$(SIZE_OUTPUT): $(EXECUTABLE)
	$(SIZE) $(EXECUTABLE)

$(OBJDUMP_LIST): $(EXECUTABLE)
	$(OBJDUMP) -h -S $(EXECUTABLE) > $@

$(BINARY): $(EXECUTABLE)
	$(OBJCOPY) -O binary $(EXECUTABLE) $@

$(SD_BINARY): $(SD_EXECUTABLE)
	$(OBJCOPY) -O binary $(SD_EXECUTABLE) $@

$(TEST_BINARY): $(TEST_EXECUTABLE)
	$(OBJCOPY) -O binary $(TEST_EXECUTABLE) $@

$(BUILD)/startup_stm32n647l0hxq_fsbl.o: Core/Startup/startup_stm32n647l0hxq_fsbl.s
	$(CC) -mcpu=$(CPU) -c -x assembler-with-cpp -MMD -MP --specs=nano.specs -mfpu=$(FPU) -mfloat-abi=$(FABI) -mthumb $< -o $@

$(BUILD)/%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	-$(RM) $(BUILD)

flash: all
	$(STM32_PRG_PATH)/$(FLASH) -c port=SWD mode=UR -el $(STM32_PRG_PATH)/ExternalLoader/$(LOADER) -d $(SIGNED) 0x70000000 -v

.PHONY: all debug sd test directories secondary-outputs clean flash
