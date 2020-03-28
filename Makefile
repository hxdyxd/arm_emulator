# Makefile of arm_emulator
# Copyright (C) 2019-2020  hxdyxd <hxdyxd@gmail.com>
CC = $(CROSS_COMPILE)gcc


TARGET = arm_emulator

C_SRCS = \
emulator.c\
armv4.c\
peripheral.c\
kfifo.c\
slip_tun.c


C_INCLUDES =  \
-I./
C_FLAGS = -O3 -Wall -std=gnu99 -g 
C_DFLAG =  -D_BSD_SOURCE -D_DEFAULT_SOURCE -DTUN_SUPPORT -DFS_MMAP_MODE


LIB_NAME = -lpthread


OBJS = $(patsubst %.c,%.o,$(C_SRCS))

all: $(OBJS)
	$(CC) $(C_FLAGS) $(C_DFLAG)  -o $(TARGET)   $(OBJS) $(LIB_NAME)

%.o:%.c
	$(CC) $(C_FLAGS) $(C_DFLAG) $(C_INCLUDES) -o $@ -c $<

clean:
	rm -f $(TARGET) $(OBJS) $(TUN_OBJS)
