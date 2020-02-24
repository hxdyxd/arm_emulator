
all:
	gcc -Os -Wall -std=c99 -I./ -D_BSD_SOURCE -D_DEFAULT_SOURCE  -o arm9  arm9.c
