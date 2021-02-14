# Makefile of arm_emulator
# Copyright (C) 2019-2020  hxdyxd <hxdyxd@gmail.com>
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
LD = $(CC)
INSTALL = install
RM = rm
PKG_CONFIG ?= pkg-config

TARGET = armemulator

OBJS += \
emulator.o\
disassembly.o\
armv4.o\
peripheral.o\
kfifo.o\
slip_tun.o\
slip_user.o\
console.o\
loop.o\
slip.o

C_INCLUDES =  \
-I .

GIT_TAGS = $(shell git describe --tags)
CFLAGS += -DARMEMULATOR_VERSION_STRING=\"$(GIT_TAGS)\"
CFLAGS += -O3 -Wall -std=gnu99 -g $(C_DEFS)

LDFLAGS += -lpthread

NO_GLIB = 0
ifeq ($(NO_GLIB), 1)
	CFLAGS += -DNO_GLIB
	SLIP_USER_DEPS =
else
	C_INCLUDES += -I libslirp/src
	CFLAGS += -DUSE_SLIRP_SUPPORT
	CFLAGS += $(shell $(PKG_CONFIG) --cflags glib-2.0)
	LDFLAGS += libslirp/libslirp.a
	LDFLAGS += $(shell $(PKG_CONFIG) --libs glib-2.0)
	SLIP_USER_DEPS = libslirp/libslirp.a
endif


quiet_CC  =      @echo "  CC      $@"; $(CC)
quiet_LD  =      @echo "  LD      $@"; $(LD)
quiet_INSTALL  = @echo "  INSTALL $?"; $(INSTALL)
quiet_MAKE     = @+$(MAKE)

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
CFLAGS += $(C_INCLUDES)


all: $(TARGET)

$(TARGET): $(OBJS)
	$($(quiet)LD) -o $(TARGET)   $(OBJS) $(LDFLAGS)

%.o: %.c
	$($(quiet)CC) $(CFLAGS) -o $@ -c $<

slip_user.c: $(SLIP_USER_DEPS)

.PHONY: clean
clean: clean_slirp
	$(RM) -f $(TARGET) $(OBJS)

install: $(TARGET)
	$($(quiet)INSTALL) -D $< /usr/local/bin/$<

uninstall:
	$(RM) -f /usr/local/bin/$(TARGET)

.PHONY: libslirp/libslirp.a
libslirp/libslirp.a:
	$($(quiet)MAKE) -C libslirp CC="$(CC)" AR="$(AR)" LD="$(LD)"

.PHONY: clean_slirp
clean_slirp:
	$($(quiet)MAKE) -i -C libslirp clean
