# Credit: This Makefile is based off of, and modified from, the ones used in
#         https://github.com/prtzl/Embedded_videos
#
# Compiling this requires a few extra files from the STM32 Standard Peripheral
# Library. These are:
# 	- system_stm32f4xx.c
# 	- stm32f4xx_conf.h
# 	- stm32_flash.ld
# The first two above can be obtained from:
# <STM32 StdPeriphLib Dir>/Project/STM32F4xx_StdPeriph_Templates
#
# The third one above can be obtained from (may vary depending on board):
# <STM32 StdPeriphLib Dir>/Project/STM32F4xx_StdPeriph_Templates/TrueSTUDIO/STM32F429_439xx/STM32F439NI_FLASH.ld
#

# Path to stlink folder for uploading code to board
STLINK=/usr/bin

# Put your source files here (*.c)
SRCS=main.c

# NOTE: Get system_stm32f4xx.c from 
# 			/opt/stm32f4/Project/STM32F4xx_StdPeriph_Templates
SRCS += system_stm32f4xx.c

# Libraries source files, add ones that you intent to use
SRCS += stm32f4xx_rcc.c
SRCS += stm32f4xx_gpio.c

# Binaries will be generated with this name (.elf, .bin, .hex)
PROJ_NAME=leds

# Put your STM32F4 library code directory here
STM_COMMON=/opt/stm32f4

# Compiler settings. Only edit CFLAGS to include other header files.
CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy

# Compiler flags
# NOTE: For this specific board, get stm32_flash.ld from
# 		  /opt/stm32f4/Project/STM32F4xx_StdPeriph_Templates/TrueSTUDIO/STM32F429_439xx/STM32F439NI_FLASH.ld
CFLAGS  = -Tstm32_flash.ld
CFLAGS += -g -O2 -Wall 
CFLAGS += -DUSE_STDPERIPH_DRIVER
CFLAGS += -mlittle-endian -mthumb -mcpu=cortex-m4 -mthumb-interwork
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += -I.
CFLAGS += -DSTM32F429_439xx
CFLAGS += --specs=nano.specs --specs=nosys.specs

# Include files from STM libraries
CFLAGS += -isystem$(STM_COMMON)/Libraries/CMSIS/Include
CFLAGS += -isystem$(STM_COMMON)/Libraries/CMSIS/Device/ST/STM32F4xx/Include
CFLAGS += -isystem$(STM_COMMON)/Libraries/STM32F4xx_StdPeriph_Driver/inc

# add startup file to build
SRCS += $(STM_COMMON)/Libraries/CMSIS/Device/ST/STM32F4xx/Source/Templates/TrueSTUDIO/startup_stm32f429_439xx.s
OBJS = $(SRCS:.c=.o)

vpath %.c $(STM_COMMON)/Libraries/STM32F4xx_StdPeriph_Driver/src \

.PHONY: proj

# Commands
all: proj

proj: $(PROJ_NAME).elf

# NOTE: Get stm32f4xx_conf.h from
# 			/opt/stm32f4/Project/STM32F4xx_StdPeriph_Templates
$(PROJ_NAME).elf: $(SRCS) stm32f4xx_conf.h
	$(CC) $(CFLAGS) $^ -o $@
	$(OBJCOPY) -O ihex $(PROJ_NAME).elf $(PROJ_NAME).hex
	$(OBJCOPY) -O binary $(PROJ_NAME).elf $(PROJ_NAME).bin

clean:
	rm -f *.o $(PROJ_NAME).elf $(PROJ_NAME).hex $(PROJ_NAME).bin

# Flash the STM32F4
burn: proj
	$(STLINK)/st-flash write $(PROJ_NAME).bin 0x8000000
