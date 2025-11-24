AI_MODEL_DIR := AiModel
AI_MODEL := $(AI_MODEL_DIR)/CivicAlert.onnx
AI_WEIGHTS := $(AI_MODEL_DIR)/CivicAlert.hex
AI_WEIGHTS_ADDRESS := 0x71000000
LOADER := CivicAlertAiLoader.stldr

WEIGHTS_GEN := stedgeai
FLASH := STM32_Programmer_CLI
OBJCOPY := arm-none-eabi-objcopy
PRINT := @echo
COPY := @cp -a
RM := @rm -rf

ifndef STM32_PRG_PATH
ifeq ($(OS),Windows_NT)
STM32_PRG_PATH := $(dir $(shell where $(FLASH)))
else
STM32_PRG_PATH := $(dir $(shell which $(FLASH)))
endif
endif
ifndef STM32_PRG_PATH
$(error You must add $(FLASH) to the PATH or specify the STM32_PRG_PATH to continue)
endif

ifndef STM32_AI_PATH
ifeq ($(OS),Windows_NT)
STM32_AI_PATH := $(dir $(shell where $(WEIGHTS_GEN)))
else
STM32_AI_PATH := $(dir $(shell which $(WEIGHTS_GEN)))
endif
endif
ifndef STM32_AI_PATH
$(error You must add $(WEIGHTS_GEN) to the PATH or specify the STM32_AI_PATH to continue)
endif

.PHONY: all debug loader checkloader weights flash flashw clean

all:
	$(MAKE) -C FSBL all

debug:
	$(MAKE) -C FSBL debug

loader:
	$(MAKE) -C ExtMemLoader all

checkloader:
ifeq (,$(wildcard $(STM32_PRG_PATH)/ExternalLoader/$(LOADER)))
	$(MAKE) -C ExtMemLoader all
endif

weights:
	$(STM32_AI_PATH)/$(WEIGHTS_GEN) generate --model $(AI_MODEL) --target stm32n6 --st-neural-art civicalert@neural_art.json --name CivicAlert
	$(OBJCOPY) -I binary st_ai_output/network_atonbuf.xSPI2.raw --change-addresses $(AI_WEIGHTS_ADDRESS) -O ihex $(AI_WEIGHTS)
	$(COPY) st_ai_output/network.* $(AI_MODEL_DIR)
	$(RM) st_ai_*
	$(PRINT) "\n*** AI MODEL HAS CHANGED! FIRMWARE SHOULD BE REBUILT AND REFLASHED! ***\n"

flash: checkloader
	$(MAKE) -C FSBL flash

flashw: checkloader
	$(STM32_PRG_PATH)/$(FLASH) -c port=SWD mode=UR -el $(STM32_PRG_PATH)/ExternalLoader/$(LOADER) -d $(AI_WEIGHTS) $(AI_WEIGHTS_ADDRESS) -v

clean:
	$(MAKE) -C FSBL clean
	$(MAKE) -C ExtMemLoader clean
	$(RM) st_ai_*
