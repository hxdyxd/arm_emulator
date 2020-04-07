# Makefile of arm_emulator
# Copyright (C) 2019-2020  hxdyxd <hxdyxd@gmail.com>
CC = $(CROSS_COMPILE)gcc
LD = $(CC)
INSTALL = install
RM = rm

TARGET = armemulator

OBJS = \
emulator.o\
armv4.o\
peripheral.o\
kfifo.o\
slip_tun.o


C_INCLUDES =  \
-I.
C_DEFS += -DTUN_SUPPORT -DFS_MMAP_MODE
CFLAGS += -O3 -Wall -std=gnu99 -g $(C_DEFS)
LDFLAGS += -lpthread

quiet_CC  =      @echo "  CC      $@"; $(CC)
quiet_LD  =      @echo "  LD      $@"; $(LD)
quiet_INSTALL  = @echo "  INSTALL $?"; $(INSTALL)

V = 0
ifeq ($(V), 0)
	quiet = quiet_
else
	quiet =
endif

STATIC = 0
ifeq ($(STATIC), 1)
	LDFLAGS += -static
endif

all:$(TARGET)

$(TARGET): $(OBJS)
	$($(quiet)LD) -o $(TARGET)   $(OBJS) $(LDFLAGS)

%.o: %.c
	$($(quiet)CC) $(CFLAGS) $(C_INCLUDES) -o $@ -c $<

clean:
	$(RM) -f $(TARGET) $(OBJS)

install:$(TARGET)
	$($(quiet)INSTALL) -D $< /usr/local/bin/$<

uninstall:
	$(RM) -f /usr/local/bin/$(TARGET)
