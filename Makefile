# Makefile of arm_emulator
# Copyright (C) 2019-2020  hxdyxd <hxdyxd@gmail.com>
CC = gcc


TARGET = arm_emulator

C_SRCS = \
emulator.c\
armv4.c\
peripheral.c

C_INCLUDES =  \
-I./

CFLAGS = -Os -Wall -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE $(C_INCLUDES)


OBJS = $(patsubst %.c,%.o,$(C_SRCS))

all: $(OBJS)
	$(CC) $(CFLAGS)  -o $(TARGET)   $(OBJS)

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -f $(TARGET) $(OBJS)
