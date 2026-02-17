################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/Src/RadioLib/modules/Si443x/Si4430.cpp \
../Core/Src/RadioLib/modules/Si443x/Si4431.cpp \
../Core/Src/RadioLib/modules/Si443x/Si4432.cpp \
../Core/Src/RadioLib/modules/Si443x/Si443x.cpp 

OBJS += \
./Core/Src/RadioLib/modules/Si443x/Si4430.o \
./Core/Src/RadioLib/modules/Si443x/Si4431.o \
./Core/Src/RadioLib/modules/Si443x/Si4432.o \
./Core/Src/RadioLib/modules/Si443x/Si443x.o 

CPP_DEPS += \
./Core/Src/RadioLib/modules/Si443x/Si4430.d \
./Core/Src/RadioLib/modules/Si443x/Si4431.d \
./Core/Src/RadioLib/modules/Si443x/Si4432.d \
./Core/Src/RadioLib/modules/Si443x/Si443x.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/RadioLib/modules/Si443x/%.o Core/Src/RadioLib/modules/Si443x/%.su Core/Src/RadioLib/modules/Si443x/%.cyclo: ../Core/Src/RadioLib/modules/Si443x/%.cpp Core/Src/RadioLib/modules/Si443x/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L496xx -c -I../Core/Inc -I"C:/Users/aless/Documents/GitHub/RedPill-T/satellite/stm32_lora/Core/Src/RadioLib" -I"C:/Users/aless/Documents/GitHub/RedPill-T/satellite/stm32_lora/Core/Src/rscode" -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-RadioLib-2f-modules-2f-Si443x

clean-Core-2f-Src-2f-RadioLib-2f-modules-2f-Si443x:
	-$(RM) ./Core/Src/RadioLib/modules/Si443x/Si4430.cyclo ./Core/Src/RadioLib/modules/Si443x/Si4430.d ./Core/Src/RadioLib/modules/Si443x/Si4430.o ./Core/Src/RadioLib/modules/Si443x/Si4430.su ./Core/Src/RadioLib/modules/Si443x/Si4431.cyclo ./Core/Src/RadioLib/modules/Si443x/Si4431.d ./Core/Src/RadioLib/modules/Si443x/Si4431.o ./Core/Src/RadioLib/modules/Si443x/Si4431.su ./Core/Src/RadioLib/modules/Si443x/Si4432.cyclo ./Core/Src/RadioLib/modules/Si443x/Si4432.d ./Core/Src/RadioLib/modules/Si443x/Si4432.o ./Core/Src/RadioLib/modules/Si443x/Si4432.su ./Core/Src/RadioLib/modules/Si443x/Si443x.cyclo ./Core/Src/RadioLib/modules/Si443x/Si443x.d ./Core/Src/RadioLib/modules/Si443x/Si443x.o ./Core/Src/RadioLib/modules/Si443x/Si443x.su

.PHONY: clean-Core-2f-Src-2f-RadioLib-2f-modules-2f-Si443x

