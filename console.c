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
#include <assert.h>
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
#include <loop.h>
#include <kfifo.h>


#define CONSOLE_FIFO_SIZE   (512)

static struct termios stdin_orig_termios;
static int conio_oldf;
static uint8_t term_escape_char = 'b' & 0x9f; /* ctrl-b is used for escape */

struct console_status_t {
    int idx_r;
    struct __kfifo recv;
    uint8_t recv_buf[CONSOLE_FIFO_SIZE];
    struct __kfifo send;
    uint8_t send_buf[CONSOLE_FIFO_SIZE];
    uint8_t term_got_escape;
    int (*term)(uint8_t escape_char, uint8_t ch);
};


static struct console_status_t con_default = {
    .idx_r = 0,
    .term = NULL,
};

static void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &stdin_orig_termios);
    fcntl(STDIN_FILENO, F_SETFL, conio_oldf);
}

static int console_escape_proc_byte(struct console_status_t *c, uint8_t ch)
{
    if (c->term_got_escape) {
        c->term_got_escape = 0;
        if (ch == term_escape_char) {
            goto send_char;
        } else if(c->term) {
            return c->term(term_escape_char, ch);
        }
    } else if (ch == term_escape_char) {
        c->term_got_escape = 1;
    } else {
send_char:
        return 1;
    }
    return 0;
}

static void console_prepare_callback(void *opaque)
{
    struct console_status_t *c = (struct console_status_t *)opaque;
    c->idx_r = loop_add_poll(&loop_default, STDIN_FILENO, POLLIN);

    if(c->send.in != c->send.out) {
        int ch;
        while(__kfifo_out(&c->send, &ch, 1) == 1) {
            putchar(ch);
        }
        fflush(stdout);
    }
}

static void console_poll_callback(void *opaque)
{
    struct console_status_t *c = (struct console_status_t *)opaque;
    int revents = loop_get_revents(&loop_default, c->idx_r);
    if(revents & POLLIN) {
        int ch;
        if(read(STDIN_FILENO, &ch, 1) == 1) {
            if(!console_escape_proc_byte(c, ch))
                return;
            while(__kfifo_in(&c->recv, &ch, 1) == 0 && LOOP_IS_RUN(&loop_default)) {
                poll(NULL, 0, 1);
            }
        }
    }
}


static const struct loopcb_t loop_console_cb = {
    .prepare = console_prepare_callback,
    .poll = console_poll_callback,
    .timer = NULL,
    .opaque = &con_default,
};


static uint8_t enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &stdin_orig_termios);
    struct termios term = stdin_orig_termios;
    term.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                         | INLCR | IGNCR | ICRNL | IXON);
    term.c_oflag |= OPOST;
    term.c_lflag &= ~(ICANON | ECHONL | ECHO | IEXTEN); // Disable echo as well
    term.c_cflag &= ~(CSIZE | PARENB);
    term.c_cflag |= CS8;
    term.c_cc[VMIN] = 1;
    term.c_cc[VTIME] = 0;
    term.c_lflag &= ~ISIG;
    if(tcsetattr(STDIN_FILENO, TCSANOW, &term) < 0) {
        ERROR_PRINTF("set attr err\n");
        return -1;
    }

    conio_oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, conio_oldf | O_NONBLOCK);

    __kfifo_init(&con_default.recv, con_default.recv_buf, CONSOLE_FIFO_SIZE, 1);
    __kfifo_init(&con_default.send, con_default.send_buf, CONSOLE_FIFO_SIZE, 1);
    loop_register(&loop_default, &loop_console_cb);
    return 1;
}

static uint8_t console_readable(void)
{
    return (con_default.recv.in != con_default.recv.out);
}

static uint8_t console_read(void)
{
    uint8_t ch;
    __kfifo_out(&con_default.recv, &ch, 1);
    return ch;
}

static uint8_t console_writeable(void)
{
    if(kfifo_unused(&con_default.send))
        return 1;
    else
        return 0;
}

static uint8_t console_write(uint8_t ch)
{
    __kfifo_in(&con_default.send, &ch, 1);
    return 0;
}


#else /* !USE_UNIX_TERMINAL_API */

#include <conio.h>

static void disable_raw_mode(void)
{

}

static uint8_t enable_raw_mode(void)
{
    return 1;
}

static uint8_t console_readable(void)
{
    return kbhit();
}

static uint8_t console_read(void)
{
    return getch();
}

static uint8_t console_writeable(void)
{
    return 1;
}

static uint8_t console_write(uint8_t ch)
{
    return putchar(ch);
}

#endif /* USE_UNIX_TERMINAL_API */




const static struct charwr_interface console_interface = {
    .init = enable_raw_mode,
    .exit = disable_raw_mode,
    .readable = console_readable,
    .read = console_read,
    .writeable = console_writeable,
    .write = console_write,
};

int console_register(struct uart_register *uart)
{
    uart_8250_register(uart, &console_interface);
    return 0;
}

void console_term_register(int (*term)(uint8_t escape_char, uint8_t ch))
{
    con_default.term = term;
}
