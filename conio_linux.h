/*
 * conio.h of arm_emulator
 * Copyright (C) 2019-2020  hxdyxd <hxdyxd@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _CONIO_LINUX_H
#define _CONIO_LINUX_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

static struct termios conio_orig_termios;
static int conio_oldf;

static inline void disable_raw_mode()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &conio_orig_termios);
    fcntl(STDIN_FILENO, F_SETFL, conio_oldf);
}

static inline void enable_raw_mode(void)
{
    atexit(disable_raw_mode);

    tcgetattr(STDIN_FILENO, &conio_orig_termios);
    struct termios term = conio_orig_termios;
    term.c_lflag &= ~(ICANON | ECHO); // Disable echo as well
    term.c_cc[VINTR] = 'b' & 0x9f; //^B
    term.c_cc[VSUSP] = 0; //undef
    term.c_cc[VQUIT] = 0; //undef
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    conio_oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, conio_oldf | O_NONBLOCK);
}

static inline int kbhit(void)
{
    static int inited = 0;
    int ch;

    if(!inited) {
        inited = 1;
        enable_raw_mode();
    }

    ch = getchar();

    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

static inline int getch(void)
{
     struct termios tm, tm_old;

     if (tcgetattr(STDIN_FILENO, &tm) < 0) {
          return -1;
     }

     tm_old = tm;
     cfmakeraw(&tm);
     if (tcsetattr(STDIN_FILENO, TCSANOW, &tm) < 0) {
          return -1;
     }

     int ch = getchar();
     if (tcsetattr(STDIN_FILENO, TCSANOW, &tm_old) < 0) {
          return -1;
     }

     return ch;
}


#endif
