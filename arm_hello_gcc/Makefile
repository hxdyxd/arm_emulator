
######################################
# target
######################################
TARGET = hello

# optimization
OPT = -O2

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

CPU = -mcpu=arm926ej-s
FLOAT-ABI = -mfloat-abi=soft

C_DEFS = 

ASFLAGS = $(CPU) 
CFLAGS = $(CPU) $(FLOAT-ABI) -std=c99  -Wall  $(C_DEFS)

#######################################
# LDFLAGS
#######################################
# link script
LDSCRIPT = main.lds

# libraries
LIBS = 
LIBDIR = 
LDFLAGS = -T $(LDSCRIPT) $(LIBDIR) $(LIBS) -Map $(TARGET).map


all:
	$(CC) -c $(CFLAGS) -g main.c -o main.o
	$(AS)    $(ASFLAGS) -g startup.s -o startup.o
	$(LD)  $(LDFLAGS)  main.o  startup.o  -o $(TARGET).elf
	$(SZ)  $(TARGET).elf
	$(BIN)  $(TARGET).elf $(TARGET).bin


dump:
	$(OBJDUMP) -d $(TARGET).elf


clean:
	rm -rf *.o

# *** EOF ***