# Makefile of arm_emulator
# Copyright (C) 2019-2020  hxdyxd <hxdyxd@gmail.com>
CC = gcc


TARGET = arm_emulator

C_SRCS = \
emulator.c\
armv4.c\
peripheral.c\
kfifo.c\
slip_tun.c


C_INCLUDES =  \
-I./

CFLAGS = -Os -Wall -std=gnu99 -D_BSD_SOURCE -D_DEFAULT_SOURCE $(C_INCLUDES) -DTUN_SUPPORT


OBJS = $(patsubst %.c,%.o,$(C_SRCS))

all: $(OBJS)
	$(CC) $(CFLAGS)  -o $(TARGET)   $(OBJS) -lpthread

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) $(TUN_OBJS)
