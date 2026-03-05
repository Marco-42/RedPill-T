################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/rscode/berlekamp.c \
../Core/Src/rscode/crcgen.c \
../Core/Src/rscode/galois.c \
../Core/Src/rscode/rs.c 

C_DEPS += \
./Core/Src/rscode/berlekamp.d \
./Core/Src/rscode/crcgen.d \
./Core/Src/rscode/galois.d \
./Core/Src/rscode/rs.d 

OBJS += \
./Core/Src/rscode/berlekamp.o \
./Core/Src/rscode/crcgen.o \
./Core/Src/rscode/galois.o \
./Core/Src/rscode/rs.o 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/rscode/%.o Core/Src/rscode/%.su Core/Src/rscode/%.cyclo: ../Core/Src/rscode/%.c Core/Src/rscode/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L496xx -c -I../Core/Inc -I"C:/Users/aless/Documents/GitHub/RedPill-T/satellite/stm32_lora/Core/Src/RadioLib" -I"C:/Users/aless/Documents/GitHub/RedPill-T/satellite/stm32_lora/Core/Src/rscode" -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-rscode

clean-Core-2f-Src-2f-rscode:
	-$(RM) ./Core/Src/rscode/berlekamp.cyclo ./Core/Src/rscode/berlekamp.d ./Core/Src/rscode/berlekamp.o ./Core/Src/rscode/berlekamp.su ./Core/Src/rscode/crcgen.cyclo ./Core/Src/rscode/crcgen.d ./Core/Src/rscode/crcgen.o ./Core/Src/rscode/crcgen.su ./Core/Src/rscode/galois.cyclo ./Core/Src/rscode/galois.d ./Core/Src/rscode/galois.o ./Core/Src/rscode/galois.su ./Core/Src/rscode/rs.cyclo ./Core/Src/rscode/rs.d ./Core/Src/rscode/rs.o ./Core/Src/rscode/rs.su

.PHONY: clean-Core-2f-Src-2f-rscode

