################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/Src/RadioLib/utils/CRC.cpp \
../Core/Src/RadioLib/utils/Cryptography.cpp \
../Core/Src/RadioLib/utils/FEC.cpp \
../Core/Src/RadioLib/utils/Utils.cpp 

OBJS += \
./Core/Src/RadioLib/utils/CRC.o \
./Core/Src/RadioLib/utils/Cryptography.o \
./Core/Src/RadioLib/utils/FEC.o \
./Core/Src/RadioLib/utils/Utils.o 

CPP_DEPS += \
./Core/Src/RadioLib/utils/CRC.d \
./Core/Src/RadioLib/utils/Cryptography.d \
./Core/Src/RadioLib/utils/FEC.d \
./Core/Src/RadioLib/utils/Utils.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/RadioLib/utils/%.o Core/Src/RadioLib/utils/%.su Core/Src/RadioLib/utils/%.cyclo: ../Core/Src/RadioLib/utils/%.cpp Core/Src/RadioLib/utils/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L496xx -c -I../Core/Inc -I"C:/Users/aless/Documents/GitHub/RedPill-T/satellite/stm32_lora/Core/Src/RadioLib" -I"C:/Users/aless/Documents/GitHub/RedPill-T/satellite/stm32_lora/Core/Src/rscode" -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-RadioLib-2f-utils

clean-Core-2f-Src-2f-RadioLib-2f-utils:
	-$(RM) ./Core/Src/RadioLib/utils/CRC.cyclo ./Core/Src/RadioLib/utils/CRC.d ./Core/Src/RadioLib/utils/CRC.o ./Core/Src/RadioLib/utils/CRC.su ./Core/Src/RadioLib/utils/Cryptography.cyclo ./Core/Src/RadioLib/utils/Cryptography.d ./Core/Src/RadioLib/utils/Cryptography.o ./Core/Src/RadioLib/utils/Cryptography.su ./Core/Src/RadioLib/utils/FEC.cyclo ./Core/Src/RadioLib/utils/FEC.d ./Core/Src/RadioLib/utils/FEC.o ./Core/Src/RadioLib/utils/FEC.su ./Core/Src/RadioLib/utils/Utils.cyclo ./Core/Src/RadioLib/utils/Utils.d ./Core/Src/RadioLib/utils/Utils.o ./Core/Src/RadioLib/utils/Utils.su

.PHONY: clean-Core-2f-Src-2f-RadioLib-2f-utils

