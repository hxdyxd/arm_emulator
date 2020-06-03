/*
 * slip_tun.c of arm_emulator
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <slip_tun.h>
#include <config.h>

#define LOG_NAME   "tun"
#define DEBUG_PRINTF(...)     printf("\033[0;32m" LOG_NAME "\033[0m: " __VA_ARGS__)
#define ERROR_PRINTF(...)     printf("\033[1;31m" LOG_NAME "\033[0m: " __VA_ARGS__)



#ifdef USE_TUN_SUPPORT
#include <slip.h>
#include <loop.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/socket.h>
//struct ifreq ifr;
#include <linux/if.h>


#define BUF_SIZE      (1800)
#define FIFO_SIZE     (4096)

struct slip_tun_t {
    struct __kfifo send;
    uint8_t send_fifo_buf[FIFO_SIZE];
    struct __kfifo recv;
    uint8_t recv_fifo_buf[FIFO_SIZE];

    int idx;
    int fd;
    uint32_t recv_time_cnt;
    uint8_t buf[BUF_SIZE];
};

static struct slip_tun_t slip_tun;

static int net_tun_init(struct slip_tun_t *t);
static int net_tun_exit(struct slip_tun_t *t);

/***********************extern*******************************/
static uint8_t slip_tun_init(void)
{
    __kfifo_init(&slip_tun.send, slip_tun.send_fifo_buf, FIFO_SIZE, 1);
    __kfifo_init(&slip_tun.recv, slip_tun.recv_fifo_buf, FIFO_SIZE, 1);

    if(net_tun_init(&slip_tun) < 0) {
        return 0;
    }
    return 1;
}

static void slip_tun_exit(void)
{
    net_tun_exit(&slip_tun);
}

/* Non-blockable */
static uint8_t slip_tun_readable(void)
{
    return (slip_tun.send.in != slip_tun.send.out);
}

/* Non-blockable */
static uint8_t slip_tun_read(void)
{
    uint8_t ch;
    __kfifo_out(&slip_tun.send, &ch, 1);
    return ch;
}

/* Non-blockable */
static uint8_t slip_tun_writeable(void)
{
    if(kfifo_unused(&slip_tun.recv))
        return 1;
    else
        return 0;
}

/* Non-blockable */
static uint8_t slip_tun_write(uint8_t ch)
{
    __kfifo_in(&slip_tun.recv, &ch, 1);
    return 0;
}


/***********************extern end*******************************/

static int tun_alloc(int flags)
{
    struct ifreq ifr;
    int fd, err;
    char *clonedev = "/dev/net/tun";

    if ((fd = open(clonedev, O_RDWR)) < 0) {
        ERROR_PRINTF("open %s err\n", clonedev);
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = flags;

    if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
        close(fd);
        ERROR_PRINTF("ioctl fd = %d err\n", fd);
        return err;
    }

    DEBUG_PRINTF("open tun/tap device: %s for reading...\n", ifr.ifr_name);
    return fd;
}

static void slip_tun_prepare_callback(void *opaque)
{
    struct slip_tun_t *t = (struct slip_tun_t *)opaque;
    t->idx = loop_add_poll(&loop_default, t->fd, POLLIN);

    int rlen = slip_recv_poll(&t->recv, t->buf, BUF_SIZE);
    if(rlen) {
        int wlen;
        do {
            wlen = write(t->fd, t->buf, rlen);
        } while(wlen < 0 && wlen == EINTR);
        loop_set_timeout(&loop_default, 0);
        t->recv_time_cnt = loop_get_clock_ms(&loop_default);
    } else {
        if(loop_get_clock_ms(&loop_default) - t->recv_time_cnt >= 2) {
            loop_set_timeout(&loop_default, 1);
        } else {
            loop_set_timeout(&loop_default, 0);
        }
    }
}

static void slip_tun_poll_callback(void *opaque)
{
    struct slip_tun_t *t = (struct slip_tun_t *)opaque;
    int revents = loop_get_revents(&loop_default, t->idx);
    if(revents & POLLIN) {
        int total_len = read(t->fd, t->buf, BUF_SIZE);
        if (total_len < 0) {
            ERROR_PRINTF("Reading from interface error\n");
            return;
        }
        
        slip_send_packet(&t->send, t->buf, total_len, &LOOP_IS_RUN(&loop_default));
    }
}


static const struct loopcb_t loop_slip_tun_cb = {
    .prepare = slip_tun_prepare_callback,
    .poll = slip_tun_poll_callback,
    .timer = NULL,
    .opaque = &slip_tun,
};


static int net_tun_init(struct slip_tun_t *t)
{
    /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
     *        IFF_TAP   - TAP device
     *        IFF_NO_PI - Do not provide packet information
     */
    t->fd = tun_alloc(IFF_TUN | IFF_NO_PI);
    if (t->fd < 0)
        goto err0;

    loop_register(&loop_default, &loop_slip_tun_cb);
    return 0;

err0:
    return -1;
}

static int net_tun_exit(struct slip_tun_t *t)
{
    if(t->fd >= 0) {
        close(t->fd);
        return 0;
    }
    return -1;
}


#else /* !USE_TUN_SUPPORT */

//tun stub
static uint8_t slip_tun_init(void)
{
    ERROR_PRINTF("tap is not supported in this build\n");
    return 1;
}

static void slip_tun_exit(void)
{
}

static uint8_t slip_tun_readable(void) 
{
    return 0;
}

static uint8_t slip_tun_read(void)
{
    return 0;
}

static uint8_t slip_tun_writeable(void)
{
    return 1;
}

static uint8_t slip_tun_write(uint8_t ch)
{
    return 0;
}
#endif /* USE_TUN_SUPPORT */


const static struct charwr_interface tun_interface = {
    .init = slip_tun_init,
    .exit = slip_tun_exit,
    .readable = slip_tun_readable,
    .read = slip_tun_read,
    .writeable = slip_tun_writeable,
    .write = slip_tun_write,
};

int slip_tun_register(struct uart_register *uart)
{
    uart_8250_register(uart, &tun_interface);
    return 0;
}

/*****************************END OF FILE***************************/
