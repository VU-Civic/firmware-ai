#### Target and Hardware Details ####

BUILD := bin
TARGET := CivicAlertAiLoader
EXECUTABLE := $(BUILD)/$(TARGET).elf
BINARY := $(BUILD)/$(TARGET).bin
MAP_FILE := $(BUILD)/$(TARGET).map
SIZE_OUTPUT := default.size.stdout
OBJDUMP_LIST := $(BUILD)/$(TARGET).list
LINKER_FILE := STM32N647_CivicAlert_ExtMemLoader.ld

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
COPY := cp

DEFINES := -DUSE_HAL_DRIVER -DSTM32N647xx -DSTM32_EXTMEMLOADER_STM32CUBETARGET

VPATH := Middlewares/ST/STM32_ExtMem_Manager Middlewares/ST/STM32_ExtMem_Manager/nor_sfdp Middlewares/ST/STM32_ExtMem_Manager/psram Middlewares/ST/STM32_ExtMem_Manager/sal Middlewares/ST/STM32_ExtMem_Manager/sdcard Middlewares/ST/STM32_ExtMem_Manager/user Middlewares/ST/STM32_ExtMem_Loader/core Middlewares/ST/STM32_ExtMem_Loader/MDK-ARM Middlewares/ST/STM32_ExtMem_Loader/STM32Cube ../Drivers/STM32N6xx_HAL_Driver/Src Core/Src
INCLUDES := -ICore/Inc -I../Drivers/STM32N6xx_HAL_Driver/Inc -I../Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -IDrivers/CMSIS/Device/ST/STM32N6xx/Include -IDrivers/CMSIS/Include -IMiddlewares/ST/STM32_ExtMem_Loader/core -IMiddlewares/ST/STM32_ExtMem_Loader/MDK-ARM -IMiddlewares/ST/STM32_ExtMem_Loader/STM32Cube -IMiddlewares/ST/STM32_ExtMem_Manager -IMiddlewares/ST/STM32_ExtMem_Manager/sal -IMiddlewares/ST/STM32_ExtMem_Manager/nor_sfdp -IMiddlewares/ST/STM32_ExtMem_Manager/psram -IMiddlewares/ST/STM32_ExtMem_Manager/sdcard -IMiddlewares/ST/STM32_ExtMem_Manager/user
SOURCES := $(foreach SOURCE,$(foreach DIR,$(VPATH),$(wildcard $(DIR)/*.c)),$(patsubst %.c,%.c,$(notdir $(SOURCE))))
OBJECTS := $(SOURCES:%.c=$(BUILD)/%.o)

CFLAGS  = -mcpu=$(CPU) -std=gnu11 $(DEFINES) $(INCLUDES) -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -mcmse -MMD -MP
CFLAGS += --specs=nano.specs -mfpu=$(FPU) -mfloat-abi=$(FABI) -mthumb
LFLAGS  = -mcpu=$(CPU) --specs=nosys.specs -Wl,-Map=$(MAP_FILE) -Wl,--gc-sections -static -Wl,--cmse-implib -Wl,--out-implib=$(BUILD)/secure_nsclib.o
LFLAGS += --specs=nano.specs -mfpu=$(FPU) -mfloat-abi=$(FABI) -mthumb -Wl,--start-group -lc -lm -Wl,--end-group


#### Build Targets ####

all: directories $(EXECUTABLE) secondary-outputs
	$(COPY) $(EXECUTABLE) $(STM32_PRG_PATH)/ExternalLoader/$(TARGET).stldr

directories: $(BUILD)

$(BUILD):
	-$(MKDIR) $@

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST) $(BINARY)

$(EXECUTABLE) $(MAP_FILE): $(OBJECTS)
	$(CC) -o $@ -T$(LINKER_FILE) $(OBJECTS) $(LFLAGS)

$(SIZE_OUTPUT): $(EXECUTABLE)
	$(SIZE) $(EXECUTABLE)

$(OBJDUMP_LIST): $(EXECUTABLE)
	$(OBJDUMP) -h -S $(EXECUTABLE) > $@

$(BINARY): $(EXECUTABLE)
	$(OBJCOPY) -O binary $(EXECUTABLE) $@

$(BUILD)/%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	-$(RM) $(BUILD)

.PHONY: all directories secondary-outputs clean
