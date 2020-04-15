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



#ifdef TUN_SUPPORT
#include <kfifo.h>

#include <pthread.h>
#ifdef USE_PRCTL_SET_THREAD_NAME
#include <sys/prctl.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <errno.h>
#include <linux/if_tun.h>

//struct ifreq ifr;
#include <linux/if.h>


static int tun_task_init(void);
static int tun_task_exit(void);

#define BUF_SIZE      (1800)
#define FIFO_SIZE     (2048)
static struct __kfifo send_fifo;
static uint8_t send_fifo_buffer[FIFO_SIZE];
static struct __kfifo recv_fifo;
static uint8_t recv_fifo_buffer[FIFO_SIZE];

#ifdef TUN_MUTEX_MODE
static pthread_mutex_t send_mut;
static pthread_mutex_t recv_mut;
#endif


/***********************extern*******************************/
static uint8_t slip_tun_init(void)
{
    __kfifo_init(&send_fifo, send_fifo_buffer, FIFO_SIZE, 1);
    __kfifo_init(&recv_fifo, recv_fifo_buffer, FIFO_SIZE, 1);
#ifdef TUN_MUTEX_MODE
    pthread_mutex_init(&send_mut, NULL);
    pthread_mutex_init(&recv_mut, NULL);
#endif

    if(tun_task_init() < 0) {
        return 0;
    }
    return 1;
}

static void slip_tun_exit(void)
{
    tun_task_exit();
}

/* Non-blockable */
static uint8_t slip_tun_readable(void)
{
    return (send_fifo.in != send_fifo.out);
}

/* Non-blockable */
static uint8_t slip_tun_read(void)
{
    uint8_t ch;
    __kfifo_out(&send_fifo, &ch, 1);
#ifdef TUN_MUTEX_MODE
    pthread_mutex_unlock(&send_mut);
#endif
    return ch;
}

/* Non-blockable */
static uint8_t slip_tun_writeable(void)
{
    if(kfifo_unused(&recv_fifo))
        return 1;
    else
        return 0;
}

/* Non-blockable */
static uint8_t slip_tun_write(uint8_t ch)
{
    __kfifo_in(&recv_fifo, &ch, 1);
#ifdef TUN_MUTEX_MODE
    pthread_mutex_unlock(&recv_mut);
#endif
    return 0;
}


/***********************extern end*******************************/


/* Blockable */
static inline void send_char(int ch, uint8_t *is_run)
{
    while(__kfifo_in(&send_fifo, &ch, 1) == 0 && *is_run) {
#ifdef TUN_MUTEX_MODE
        pthread_mutex_lock(&send_mut);
#else
        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 20,
        };
        select(0, NULL, NULL, NULL, &timeout);
#endif
    }
}

/* Blockable */
static inline void send_char_do(uint8_t *is_run)
{

}

/* Blockable */
static inline int recv_char(uint8_t *is_run)
{
    uint8_t ch;
    while(__kfifo_out(&recv_fifo, &ch, 1) == 0 && *is_run) {
#ifdef TUN_MUTEX_MODE
        pthread_mutex_lock(&recv_mut);
#else
        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 20,
        };
        select(0, NULL, NULL, NULL, &timeout);
#endif
    }
    return ch;
}




static int tun_alloc(int flags);
static void send_packet(unsigned char *p, int len, uint8_t *is_run);
static int recv_packet(unsigned char *p, int len, uint8_t *is_run);


struct slip_tun_status {
    int fd;
    uint8_t is_tun_out;
    uint8_t is_run;
    char *thread_name;
    pthread_t thread_id;
    unsigned char buf[BUF_SIZE];
};

struct slip_tun {
    int fd;
    struct slip_tun_status tun_out;
    struct slip_tun_status slip_out;
};


static void *tun_task_proc(void *base)
{
    struct slip_tun_status *s = (struct slip_tun_status *)base;
    int total_len;
#ifdef USE_PRCTL_SET_THREAD_NAME
    prctl(PR_SET_NAME, s->thread_name);
#endif
    DEBUG_PRINTF("%s task enter success %d!\n", s->thread_name, s->fd);
    while(s->is_run) {
        if(s->is_tun_out) {
            struct timeval timeout = {
                .tv_sec = 0,
                .tv_usec = 500000,
            };
            fd_set readset;
            FD_ZERO(&readset);
            FD_SET(s->fd, &readset);

            int r = select(s->fd + 1, &readset, NULL, NULL, &timeout);
            if (r < 0) {
                if(errno == EINTR)
                    continue;
                ERROR_PRINTF("%s select error\n", s->thread_name);
                break;
            }
            if (FD_ISSET(s->fd, &readset)) {
                int total_len = read(s->fd, s->buf, BUF_SIZE);
                if (total_len < 0) {
                    ERROR_PRINTF("%s Reading from interface error\n", s->thread_name);
                    break;
                }
                
                send_packet(s->buf, total_len, &s->is_run);
            }
        } else {
            //slip out
            int wlen;
            total_len = recv_packet(s->buf, BUF_SIZE, &s->is_run);
            do {
                wlen = write(s->fd, s->buf, total_len);
            } while(wlen < 0 && wlen == EINTR);
        }
    }

    s->is_run = 0;
    DEBUG_PRINTF("%s task exit!\n", s->thread_name);
    return NULL;
}


static int tun_task_start(struct slip_tun_status *s)
{
    s->is_run = 1;
    return pthread_create(&s->thread_id, 0, tun_task_proc, s);
}


static int tun_task_stop(struct slip_tun_status *s)
{
    if (s->is_run == 1) {
        s->is_run = 0;
        pthread_join(s->thread_id, 0);
    } else {
        ERROR_PRINTF("%s stop failed!\n", s->thread_name);
        return -1;
    }
    return 0;
}


static struct slip_tun *tun = NULL;

static int tun_task_init(void)
{
    tun = (struct slip_tun *)malloc(sizeof(struct slip_tun));
    if(!tun) {
        ERROR_PRINTF("allocating err\n");
        goto err0;
    }

    /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
     *        IFF_TAP   - TAP device
     *        IFF_NO_PI - Do not provide packet information
     */
    tun->fd = tun_alloc(IFF_TUN | IFF_NO_PI);
    if (tun->fd < 0)
        goto err1;

    tun->tun_out.fd = tun->fd;
    tun->tun_out.is_tun_out = 1;
    tun->tun_out.thread_name = "tun_out";
    if(tun_task_start(&tun->tun_out) < 0)
        goto err2;

    tun->slip_out.fd = tun->fd;
    tun->slip_out.is_tun_out = 0;
    tun->slip_out.thread_name = "slip_out";
    if(tun_task_start(&tun->slip_out) < 0)
        goto err2;

    return 0;

err2:
    close(tun->fd);
err1:
    free(tun);
    tun = NULL;
err0:
    return -1;
}


static int tun_task_exit(void)
{
    if(tun && tun->fd > 0) {
        tun_task_stop(&tun->slip_out);
        tun_task_stop(&tun->tun_out);
        close(tun->fd);
        free(tun);
        return 0;
    }
    return -1;
}


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






/* SLIP special character codes */
#define END             0300    /* indicates end of packet */
#define ESC             0333    /* indicates byte stuffing */
#define ESC_END         0334    /* ESC ESC_END means END data byte */
#define ESC_ESC         0335    /* ESC ESC_ESC means ESC data byte */

/* SEND_PACKET: sends a packet of length "len", starting at
 * location "p".
 */
static void send_packet(unsigned char *p, int len, uint8_t *is_run)
{
    /* send an initial END character to flush out any data that may
     * have accumulated in the receiver due to line noise
     */
    send_char(END, is_run);

    /* for each byte in the packet, send the appropriate character
     * sequence
    */
    while(len--) {
        switch(*p) {
            /* if it's the same code as an END character, we send a
             * special two character code so as not to make the
             * receiver think we sent an END */
           case END:
                send_char(ESC, is_run);
                send_char(ESC_END, is_run);
                break;
    
           /* if it's the same code as an ESC character,
            * we send a special two character code so as not
            * to make the receiver think we sent an ESC
            */
           case ESC:
                send_char(ESC, is_run);
                send_char(ESC_ESC, is_run);
                break;
    
           /* otherwise, we just send the character
            */
           default:
                send_char(*p, is_run);
        }
        
        p++;
    }

   /* tell the receiver that we're done sending the packet */
    send_char(END, is_run);
    send_char_do(is_run);
}


/* RECV_PACKET: receives a packet into the buffer located at "p".
 *      If more than len bytes are received, the packet will
 *      be truncated.
 *      Returns the number of bytes stored in the buffer.
 */
static int recv_packet(unsigned char *p, int len, uint8_t *is_run)
{
    unsigned char c;
    int received = 0;

    /* sit in a loop reading bytes until we put together
     * a whole packet.
     * Make sure not to copy them into the packet if we
     * run out of room.
     */
    while(*is_run) {
       /* get a character to process */
        c = recv_char(is_run);

       /* handle bytestuffing if necessary */
        switch(c) {

           /* if it's an END character then we're done with
            * the packet
            */
           case END:
               /* a minor optimization: if there is no
                * data in the packet, ignore it. This is
                * meant to avoid bothering IP with all
                * the empty packets generated by the
                * duplicate END characters which are in
                * turn sent to try to detect line noise.
                */
               if(received)
                    return received;
               else
                    break;

           /* if it's the same code as an ESC character, wait
            * and get another character and then figure out
            * what to store in the packet based on that.
            */
           case ESC:
                c = recv_char(is_run);

               /* if "c" is not one of these two, then we
                * have a protocol violation.  The best bet
                * seems to be to leave the byte alone and
                * just stuff it into the packet
                */
                switch(c) {
                    case ESC_END:
                        c = END;
                        break;
                    case ESC_ESC:
                        c = ESC;
                        break;
                }

           /* here we fall into the default handler and let
            * it store the character for us
            */
           default:
                if(received < len)
                    p[received++] = c;
                else
                    return received;
        }
    }
    return received;
}


#else

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
#endif /*TUN_SUPPORT*/


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
