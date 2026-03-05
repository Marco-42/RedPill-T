################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/Src/RadioLib/modules/SX126x/STM32WLx.cpp \
../Core/Src/RadioLib/modules/SX126x/STM32WLx_Module.cpp \
../Core/Src/RadioLib/modules/SX126x/SX1261.cpp \
../Core/Src/RadioLib/modules/SX126x/SX1262.cpp \
../Core/Src/RadioLib/modules/SX126x/SX1268.cpp \
../Core/Src/RadioLib/modules/SX126x/SX126x.cpp \
../Core/Src/RadioLib/modules/SX126x/SX126x_LR_FHSS.cpp \
../Core/Src/RadioLib/modules/SX126x/SX126x_commands.cpp \
../Core/Src/RadioLib/modules/SX126x/SX126x_config.cpp 

OBJS += \
./Core/Src/RadioLib/modules/SX126x/STM32WLx.o \
./Core/Src/RadioLib/modules/SX126x/STM32WLx_Module.o \
./Core/Src/RadioLib/modules/SX126x/SX1261.o \
./Core/Src/RadioLib/modules/SX126x/SX1262.o \
./Core/Src/RadioLib/modules/SX126x/SX1268.o \
./Core/Src/RadioLib/modules/SX126x/SX126x.o \
./Core/Src/RadioLib/modules/SX126x/SX126x_LR_FHSS.o \
./Core/Src/RadioLib/modules/SX126x/SX126x_commands.o \
./Core/Src/RadioLib/modules/SX126x/SX126x_config.o 

CPP_DEPS += \
./Core/Src/RadioLib/modules/SX126x/STM32WLx.d \
./Core/Src/RadioLib/modules/SX126x/STM32WLx_Module.d \
./Core/Src/RadioLib/modules/SX126x/SX1261.d \
./Core/Src/RadioLib/modules/SX126x/SX1262.d \
./Core/Src/RadioLib/modules/SX126x/SX1268.d \
./Core/Src/RadioLib/modules/SX126x/SX126x.d \
./Core/Src/RadioLib/modules/SX126x/SX126x_LR_FHSS.d \
./Core/Src/RadioLib/modules/SX126x/SX126x_commands.d \
./Core/Src/RadioLib/modules/SX126x/SX126x_config.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/RadioLib/modules/SX126x/%.o Core/Src/RadioLib/modules/SX126x/%.su Core/Src/RadioLib/modules/SX126x/%.cyclo: ../Core/Src/RadioLib/modules/SX126x/%.cpp Core/Src/RadioLib/modules/SX126x/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L496xx -c -I../Core/Inc -I"C:/Users/aless/Documents/GitHub/RedPill-T/satellite/stm32_lora/Core/Src/RadioLib" -I"C:/Users/aless/Documents/GitHub/RedPill-T/satellite/stm32_lora/Core/Src/rscode" -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-RadioLib-2f-modules-2f-SX126x

clean-Core-2f-Src-2f-RadioLib-2f-modules-2f-SX126x:
	-$(RM) ./Core/Src/RadioLib/modules/SX126x/STM32WLx.cyclo ./Core/Src/RadioLib/modules/SX126x/STM32WLx.d ./Core/Src/RadioLib/modules/SX126x/STM32WLx.o ./Core/Src/RadioLib/modules/SX126x/STM32WLx.su ./Core/Src/RadioLib/modules/SX126x/STM32WLx_Module.cyclo ./Core/Src/RadioLib/modules/SX126x/STM32WLx_Module.d ./Core/Src/RadioLib/modules/SX126x/STM32WLx_Module.o ./Core/Src/RadioLib/modules/SX126x/STM32WLx_Module.su ./Core/Src/RadioLib/modules/SX126x/SX1261.cyclo ./Core/Src/RadioLib/modules/SX126x/SX1261.d ./Core/Src/RadioLib/modules/SX126x/SX1261.o ./Core/Src/RadioLib/modules/SX126x/SX1261.su ./Core/Src/RadioLib/modules/SX126x/SX1262.cyclo ./Core/Src/RadioLib/modules/SX126x/SX1262.d ./Core/Src/RadioLib/modules/SX126x/SX1262.o ./Core/Src/RadioLib/modules/SX126x/SX1262.su ./Core/Src/RadioLib/modules/SX126x/SX1268.cyclo ./Core/Src/RadioLib/modules/SX126x/SX1268.d ./Core/Src/RadioLib/modules/SX126x/SX1268.o ./Core/Src/RadioLib/modules/SX126x/SX1268.su ./Core/Src/RadioLib/modules/SX126x/SX126x.cyclo ./Core/Src/RadioLib/modules/SX126x/SX126x.d ./Core/Src/RadioLib/modules/SX126x/SX126x.o ./Core/Src/RadioLib/modules/SX126x/SX126x.su ./Core/Src/RadioLib/modules/SX126x/SX126x_LR_FHSS.cyclo ./Core/Src/RadioLib/modules/SX126x/SX126x_LR_FHSS.d ./Core/Src/RadioLib/modules/SX126x/SX126x_LR_FHSS.o ./Core/Src/RadioLib/modules/SX126x/SX126x_LR_FHSS.su ./Core/Src/RadioLib/modules/SX126x/SX126x_commands.cyclo ./Core/Src/RadioLib/modules/SX126x/SX126x_commands.d ./Core/Src/RadioLib/modules/SX126x/SX126x_commands.o ./Core/Src/RadioLib/modules/SX126x/SX126x_commands.su ./Core/Src/RadioLib/modules/SX126x/SX126x_config.cyclo ./Core/Src/RadioLib/modules/SX126x/SX126x_config.d ./Core/Src/RadioLib/modules/SX126x/SX126x_config.o ./Core/Src/RadioLib/modules/SX126x/SX126x_config.su

.PHONY: clean-Core-2f-Src-2f-RadioLib-2f-modules-2f-SX126x

