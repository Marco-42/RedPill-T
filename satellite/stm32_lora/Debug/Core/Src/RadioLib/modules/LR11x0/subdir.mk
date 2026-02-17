################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/Src/RadioLib/modules/LR11x0/LR1110.cpp \
../Core/Src/RadioLib/modules/LR11x0/LR1120.cpp \
../Core/Src/RadioLib/modules/LR11x0/LR1121.cpp \
../Core/Src/RadioLib/modules/LR11x0/LR11x0.cpp \
../Core/Src/RadioLib/modules/LR11x0/LR11x0_commands.cpp \
../Core/Src/RadioLib/modules/LR11x0/LR11x0_crypto.cpp \
../Core/Src/RadioLib/modules/LR11x0/LR11x0_gnss.cpp \
../Core/Src/RadioLib/modules/LR11x0/LR11x0_wifi.cpp 

OBJS += \
./Core/Src/RadioLib/modules/LR11x0/LR1110.o \
./Core/Src/RadioLib/modules/LR11x0/LR1120.o \
./Core/Src/RadioLib/modules/LR11x0/LR1121.o \
./Core/Src/RadioLib/modules/LR11x0/LR11x0.o \
./Core/Src/RadioLib/modules/LR11x0/LR11x0_commands.o \
./Core/Src/RadioLib/modules/LR11x0/LR11x0_crypto.o \
./Core/Src/RadioLib/modules/LR11x0/LR11x0_gnss.o \
./Core/Src/RadioLib/modules/LR11x0/LR11x0_wifi.o 

CPP_DEPS += \
./Core/Src/RadioLib/modules/LR11x0/LR1110.d \
./Core/Src/RadioLib/modules/LR11x0/LR1120.d \
./Core/Src/RadioLib/modules/LR11x0/LR1121.d \
./Core/Src/RadioLib/modules/LR11x0/LR11x0.d \
./Core/Src/RadioLib/modules/LR11x0/LR11x0_commands.d \
./Core/Src/RadioLib/modules/LR11x0/LR11x0_crypto.d \
./Core/Src/RadioLib/modules/LR11x0/LR11x0_gnss.d \
./Core/Src/RadioLib/modules/LR11x0/LR11x0_wifi.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/RadioLib/modules/LR11x0/%.o Core/Src/RadioLib/modules/LR11x0/%.su Core/Src/RadioLib/modules/LR11x0/%.cyclo: ../Core/Src/RadioLib/modules/LR11x0/%.cpp Core/Src/RadioLib/modules/LR11x0/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L496xx -c -I../Core/Inc -I"C:/Users/aless/Documents/GitHub/RedPill-T/satellite/stm32_lora/Core/Src/RadioLib" -I"C:/Users/aless/Documents/GitHub/RedPill-T/satellite/stm32_lora/Core/Src/rscode" -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-RadioLib-2f-modules-2f-LR11x0

clean-Core-2f-Src-2f-RadioLib-2f-modules-2f-LR11x0:
	-$(RM) ./Core/Src/RadioLib/modules/LR11x0/LR1110.cyclo ./Core/Src/RadioLib/modules/LR11x0/LR1110.d ./Core/Src/RadioLib/modules/LR11x0/LR1110.o ./Core/Src/RadioLib/modules/LR11x0/LR1110.su ./Core/Src/RadioLib/modules/LR11x0/LR1120.cyclo ./Core/Src/RadioLib/modules/LR11x0/LR1120.d ./Core/Src/RadioLib/modules/LR11x0/LR1120.o ./Core/Src/RadioLib/modules/LR11x0/LR1120.su ./Core/Src/RadioLib/modules/LR11x0/LR1121.cyclo ./Core/Src/RadioLib/modules/LR11x0/LR1121.d ./Core/Src/RadioLib/modules/LR11x0/LR1121.o ./Core/Src/RadioLib/modules/LR11x0/LR1121.su ./Core/Src/RadioLib/modules/LR11x0/LR11x0.cyclo ./Core/Src/RadioLib/modules/LR11x0/LR11x0.d ./Core/Src/RadioLib/modules/LR11x0/LR11x0.o ./Core/Src/RadioLib/modules/LR11x0/LR11x0.su ./Core/Src/RadioLib/modules/LR11x0/LR11x0_commands.cyclo ./Core/Src/RadioLib/modules/LR11x0/LR11x0_commands.d ./Core/Src/RadioLib/modules/LR11x0/LR11x0_commands.o ./Core/Src/RadioLib/modules/LR11x0/LR11x0_commands.su ./Core/Src/RadioLib/modules/LR11x0/LR11x0_crypto.cyclo ./Core/Src/RadioLib/modules/LR11x0/LR11x0_crypto.d ./Core/Src/RadioLib/modules/LR11x0/LR11x0_crypto.o ./Core/Src/RadioLib/modules/LR11x0/LR11x0_crypto.su ./Core/Src/RadioLib/modules/LR11x0/LR11x0_gnss.cyclo ./Core/Src/RadioLib/modules/LR11x0/LR11x0_gnss.d ./Core/Src/RadioLib/modules/LR11x0/LR11x0_gnss.o ./Core/Src/RadioLib/modules/LR11x0/LR11x0_gnss.su ./Core/Src/RadioLib/modules/LR11x0/LR11x0_wifi.cyclo ./Core/Src/RadioLib/modules/LR11x0/LR11x0_wifi.d ./Core/Src/RadioLib/modules/LR11x0/LR11x0_wifi.o ./Core/Src/RadioLib/modules/LR11x0/LR11x0_wifi.su

.PHONY: clean-Core-2f-Src-2f-RadioLib-2f-modules-2f-LR11x0

