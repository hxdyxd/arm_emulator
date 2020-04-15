/*
 * console.c of arm_emulator
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
#include <console.h>

#include <stdio.h>
#include <stdint.h>
#include <config.h>

#define LOG_NAME   "console"
#define PRINTF(...)           printf(LOG_NAME ": " __VA_ARGS__)
#define DEBUG_PRINTF(...)     printf("\033[0;32m" LOG_NAME "\033[0m: " __VA_ARGS__)
#define ERROR_PRINTF(...)     printf("\033[1;31m" LOG_NAME "\033[0m: " __VA_ARGS__)


#ifdef USE_UNIX_TERMINAL_API
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>


static struct termios conio_orig_termios;
static int conio_oldf;

static void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &conio_orig_termios);
    fcntl(STDIN_FILENO, F_SETFL, conio_oldf);
}

static uint8_t enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &conio_orig_termios);
    struct termios term = conio_orig_termios;
    term.c_lflag &= ~(ICANON | ECHO); // Disable echo as well
    term.c_cc[VINTR] = 'b' & 0x9f; //^B
    term.c_cc[VSUSP] = 0; //undef
    term.c_cc[VQUIT] = 0; //undef
    if(tcsetattr(STDIN_FILENO, TCSANOW, &term) < 0) {
        ERROR_PRINTF("set attr err\n");
        return 0;
    }

    conio_oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, conio_oldf | O_NONBLOCK);
    return 1;
}

static uint8_t kbhit(void)
{
    int ch;

    ch = getchar();

    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

static uint8_t getch(void)
{
    struct termios tm, tm_old;

    if (tcgetattr(STDIN_FILENO, &tm) < 0) {
        ERROR_PRINTF("get attr err\n");
        return -1;
    }

    tm_old = tm;
    cfmakeraw(&tm);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &tm) < 0) {
        ERROR_PRINTF("set attr err\n");
        return -1;
    }

    uint8_t ch = getchar();
    if (tcsetattr(STDIN_FILENO, TCSANOW, &tm_old) < 0) {
        ERROR_PRINTF("set attr err\n");
        return -1;
    }

    return ch;
}


#else /* !USE_UNIX_TERMINAL_API */

#include <conio.h>

static void disable_raw_mode(void)
{

}

static uint8_t enable_raw_mode(void)
{

}

#endif /* USE_UNIX_TERMINAL_API */


static uint8_t putch(uint8_t ch)
{
    return putchar(ch);
}



const static struct charwr_interface console_interface = {
    .init = enable_raw_mode,
    .exit = disable_raw_mode,
    .readable = kbhit,
    .read = getch,
    .writeable = uart_8250_rw_enable,
    .write = putch,
};

int console_register(struct uart_register *uart)
{
    uart_8250_register(uart, &console_interface);
    return 0;
}
