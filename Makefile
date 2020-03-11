
TARGET = armv4


all:
	gcc -Os -Wall -std=c99 -I./ -D_BSD_SOURCE -D_DEFAULT_SOURCE  -o $(TARGET) -I./  emulator.c armv4.c peripheral.c

clean:
	rm -f $(TARGET)
