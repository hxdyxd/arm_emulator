# Makefile of arm_emulator
# Copyright (C) 2019-2020  hxdyxd <hxdyxd@gmail.com>

######################################
# target
######################################
TARGET = hello

# optimization
OPT = -O3

#######################################
# paths
#######################################
# Build path
BUILD_DIR = build

PREFIX = arm-none-eabi-


CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump
SZ = $(PREFIX)size
BIN = $(OBJCOPY) -O binary -S

CPU = -mcpu=arm920t
FLOAT-ABI = -mfloat-abi=soft


C_SRCS = \
src/main.c


ASM_SOURCES = \
startup.s

# C includes
C_INCLUDES =  \
-Isrc


C_DEFS =

ASFLAGS = $(CPU) $(FLOAT-ABI)
CFLAGS = $(CPU) $(FLOAT-ABI) $(OPT) -std=c99  -Wall  $(C_DEFS) $(C_INCLUDES)
CFLAGS += -ffunction-sections -fdata-sections

#######################################
# LDFLAGS
#######################################
# link script
LDSCRIPT = main.lds

# libraries
LIBS = -lc -lnosys
LIBDIR = -L/root/gcc-arm-none-eabi-8-2018-q4-major/arm-none-eabi/lib/
LDFLAGS = --specs=nosys.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(TARGET).map,--cref
LDFLAGS += -Wl,--gc-sections


OBJS = $(patsubst %.c,%.o,$(C_SRCS))
AS_OBJS = $(patsubst %.s,%.o,$(ASM_SOURCES))


all: $(OBJS) $(AS_OBJS)
	$(CC)  $(LDFLAGS) $(AS_OBJS) $(OBJS) -o $(TARGET).elf -lm
	$(SZ)  $(TARGET).elf
	$(BIN)  $(TARGET).elf $(TARGET).bin


%.o:%.c
	$(CC) -c $(CFLAGS)  $< -o $@

%.o:%.s
	$(AS)  $(ASFLAGS)  $< -o $@


dump:
	$(OBJDUMP) -d $(TARGET).elf -S > dump.map

readelf:
	readelf -S  $(TARGET).elf

clean:
	rm -rf ./*.o ./src/*.o *.bin *.elf *.map

# *** EOF ***
