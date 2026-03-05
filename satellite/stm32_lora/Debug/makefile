################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/subdir.mk
-include Drivers/STM32L4xx_HAL_Driver/Src/subdir.mk
-include Core/Startup/subdir.mk
-include Core/Src/rscode/subdir.mk
-include Core/Src/RadioLib/utils/subdir.mk
-include Core/Src/RadioLib/protocols/SSTV/subdir.mk
-include Core/Src/RadioLib/protocols/RTTY/subdir.mk
-include Core/Src/RadioLib/protocols/Print/subdir.mk
-include Core/Src/RadioLib/protocols/PhysicalLayer/subdir.mk
-include Core/Src/RadioLib/protocols/Pager/subdir.mk
-include Core/Src/RadioLib/protocols/Morse/subdir.mk
-include Core/Src/RadioLib/protocols/LoRaWAN/subdir.mk
-include Core/Src/RadioLib/protocols/Hellschreiber/subdir.mk
-include Core/Src/RadioLib/protocols/FSK4/subdir.mk
-include Core/Src/RadioLib/protocols/ExternalRadio/subdir.mk
-include Core/Src/RadioLib/protocols/BellModem/subdir.mk
-include Core/Src/RadioLib/protocols/AX25/subdir.mk
-include Core/Src/RadioLib/protocols/APRS/subdir.mk
-include Core/Src/RadioLib/protocols/AFSK/subdir.mk
-include Core/Src/RadioLib/modules/nRF24/subdir.mk
-include Core/Src/RadioLib/modules/Si443x/subdir.mk
-include Core/Src/RadioLib/modules/SX128x/subdir.mk
-include Core/Src/RadioLib/modules/SX127x/subdir.mk
-include Core/Src/RadioLib/modules/SX126x/subdir.mk
-include Core/Src/RadioLib/modules/SX123x/subdir.mk
-include Core/Src/RadioLib/modules/RF69/subdir.mk
-include Core/Src/RadioLib/modules/LR11x0/subdir.mk
-include Core/Src/RadioLib/modules/LLCC68/subdir.mk
-include Core/Src/RadioLib/modules/CC1101/subdir.mk
-include Core/Src/RadioLib/hal/RPiPico/subdir.mk
-include Core/Src/RadioLib/hal/Arduino/subdir.mk
-include Core/Src/RadioLib/subdir.mk
-include Core/Src/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(CCM_DEPS)),)
-include $(CCM_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(CXXM_DEPS)),)
-include $(CXXM_DEPS)
endif
ifneq ($(strip $(C++M_DEPS)),)
-include $(C++M_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := stm32_lora
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
stm32_lora.elf \

MAP_FILES += \
stm32_lora.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
stm32_lora.list \


# All Target
all: main-build

# Main-build Target
main-build: stm32_lora.elf secondary-outputs

# Tool invocations
stm32_lora.elf stm32_lora.map: $(OBJS) $(USER_OBJS) C:\Users\aless\Documents\GitHub\RedPill-T\satellite\stm32_lora\STM32L496VGTX_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-g++ -o "stm32_lora.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m4 -T"C:\Users\aless\Documents\GitHub\RedPill-T\satellite\stm32_lora\STM32L496VGTX_FLASH.ld" -Wl,-Map="stm32_lora.map" -Wl,--gc-sections -static --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

stm32_lora.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "stm32_lora.list"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) default.size.stdout stm32_lora.elf stm32_lora.list stm32_lora.map
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
