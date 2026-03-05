################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/Src/RadioLib/hal/Arduino/ArduinoHal.cpp 

OBJS += \
./Core/Src/RadioLib/hal/Arduino/ArduinoHal.o 

CPP_DEPS += \
./Core/Src/RadioLib/hal/Arduino/ArduinoHal.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/RadioLib/hal/Arduino/%.o Core/Src/RadioLib/hal/Arduino/%.su Core/Src/RadioLib/hal/Arduino/%.cyclo: ../Core/Src/RadioLib/hal/Arduino/%.cpp Core/Src/RadioLib/hal/Arduino/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L496xx -c -I../Core/Inc -I"C:/Users/aless/Documents/GitHub/RedPill-T/satellite/stm32_lora/Core/Src/RadioLib" -I"C:/Users/aless/Documents/GitHub/RedPill-T/satellite/stm32_lora/Core/Src/rscode" -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-RadioLib-2f-hal-2f-Arduino

clean-Core-2f-Src-2f-RadioLib-2f-hal-2f-Arduino:
	-$(RM) ./Core/Src/RadioLib/hal/Arduino/ArduinoHal.cyclo ./Core/Src/RadioLib/hal/Arduino/ArduinoHal.d ./Core/Src/RadioLib/hal/Arduino/ArduinoHal.o ./Core/Src/RadioLib/hal/Arduino/ArduinoHal.su

.PHONY: clean-Core-2f-Src-2f-RadioLib-2f-hal-2f-Arduino

