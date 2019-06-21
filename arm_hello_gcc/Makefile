
######################################
# target
######################################
TARGET = hello

# optimization
OPT = -O0

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

ASFLAGS = $(CPU) $(FLOAT-ABI)
CFLAGS = $(CPU) $(FLOAT-ABI) $(OPT) -std=c99  -Wall  $(C_DEFS)
CFLAGS += -ffunction-sections -fdata-sections

#######################################
# LDFLAGS
#######################################
# link script
LDSCRIPT = main.lds

# libraries
LIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS = -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(TARGET).map,--cref
LDFLAGS += -Wl,--gc-sections


all:
	$(CC) -c $(CFLAGS) -g main.c -o main.o
	$(CC) -c $(CFLAGS) -g syscalls.c -o syscalls.o
	$(AS)  $(ASFLAGS) -g startup.s -o startup.o
	$(CC)  $(LDFLAGS)  main.o  syscalls.o  startup.o  -o $(TARGET).elf
	$(SZ)  $(TARGET).elf
	$(BIN)  $(TARGET).elf $(TARGET).bin


dump:
	$(OBJDUMP) -d $(TARGET).elf


clean:
	rm -rf *.o *.bin *.elf

# *** EOF ***