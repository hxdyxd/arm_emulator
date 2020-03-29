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
#include <string.h>
#include <slip_tun.h>


#ifdef TUN_SUPPORT
#include <kfifo.h>

#include <pthread.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>

//struct ifreq ifr;
#include <linux/if.h>


#define DEBUG_PRINTF     printf
#define ERROR_PRINTF     printf


int tun_out_task_start(void);

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


static inline unsigned int slip_kfifo_unused(struct __kfifo *fifo)
{
    return (fifo->mask + 1) - (fifo->in - fifo->out);
}

/***********************extern*******************************/
int slip_tun_init(void)
{
    __kfifo_init(&send_fifo, send_fifo_buffer, FIFO_SIZE, 1);
    __kfifo_init(&recv_fifo, recv_fifo_buffer, FIFO_SIZE, 1);
#ifdef TUN_MUTEX_MODE
    pthread_mutex_init(&send_mut, NULL);
    pthread_mutex_init(&recv_mut, NULL);
#endif

    if(tun_out_task_start() < 0) {
        return 0;
    }
    return 1;
}

/* Non-blockable */
int slip_tun_readable(void)
{
    return (send_fifo.in != send_fifo.out);
}

/* Non-blockable */
int slip_tun_read(void)
{
    uint8_t ch;
    __kfifo_out(&send_fifo, &ch, 1);
#ifdef TUN_MUTEX_MODE
    pthread_mutex_unlock(&send_mut);
#endif
    return ch;
}

/* Non-blockable */
int slip_tun_writeable(void)
{
    if(slip_kfifo_unused(&recv_fifo))
        return 1;
    else
        return 0;
}

/* Non-blockable */
int slip_tun_write(int ch)
{
    __kfifo_in(&recv_fifo, &ch, 1);
#ifdef TUN_MUTEX_MODE
    pthread_mutex_unlock(&recv_mut);
#endif
    return 0;
}

/***********************extern end*******************************/
//Slip
static volatile uint8_t slip_out_task_run_flag = 0;
//Tun
static volatile uint8_t tun_out_task_run_flag = 0;

/* Blockable */
void send_char(int ch)
{
    while(__kfifo_in(&send_fifo, &ch, 1) == 0 && tun_out_task_run_flag) {
#ifdef TUN_MUTEX_MODE
        pthread_mutex_lock(&send_mut);
#else
        usleep(20);
#endif
    }
}

/* Blockable */
void send_char_do(void)
{

}

/* Blockable */
int recv_char(void)
{
    uint8_t ch;
    while(__kfifo_out(&recv_fifo, &ch, 1) == 0 && slip_out_task_run_flag) {
#ifdef TUN_MUTEX_MODE
        pthread_mutex_lock(&recv_mut);
#else
        usleep(20);
#endif
    }
    return ch;
}



int tun_alloc(int flags);
void send_packet(unsigned char *p, int len);
int recv_packet(unsigned char *p, int len);
#if CHECK_SUM_ON
//Checksum
int if_api_calculate_checksum(void *buf, int len);
int if_api_check(void *buf, int len);
#endif

static pthread_t gs_slip_out_task_pthread_id;
static pthread_t gs_tun_out_task_pthread_id;

static unsigned char tun_out_buffer[BUF_SIZE];
static unsigned char slip_out_buffer[BUF_SIZE];

void *slip_out_task_proc(void *par)
{
    int tun_fd = *(int *)par;
    prctl(PR_SET_NAME,"slip_out_task");
    DEBUG_PRINTF("slip out task enter success %d!\n", tun_fd);
    while(slip_out_task_run_flag) {
        int total_len = recv_packet(slip_out_buffer, BUF_SIZE);

#if CHECK_SUM_ON
        int temp_len = total_len;
        total_len = if_api_check(slip_out_buffer, total_len);
        if(total_len < 0) {
            DEBUG_PRINTF("lost %d bytes\n", temp_len);
            continue;
        }
#endif

#if 0
        DEBUG_PRINTF("SLIP R=%d\n", total_len);
#endif

        total_len = write(tun_fd, slip_out_buffer, total_len);
    }
    DEBUG_PRINTF("slip out task exit!\n");
    return NULL;
}


int slip_out_task_start(int *tun_fd)
{
    slip_out_task_run_flag = 1;
    DEBUG_PRINTF("slip out task start!\n");

    return pthread_create(&gs_slip_out_task_pthread_id, 0, slip_out_task_proc, tun_fd);
}

int slip_out_task_stop(void)
{
    if (slip_out_task_run_flag == 1) {
        slip_out_task_run_flag = 0;
        pthread_join(gs_slip_out_task_pthread_id, 0);
    } else {
        ERROR_PRINTF("slip out task failed!\n");
        return -1;
    }
    return 0;
}





void *tun_out_task_proc(void *par)
{
    int tun_fd = *(int *)par;
    prctl(PR_SET_NAME,"tun_out_task");
    DEBUG_PRINTF("tun out task enter success %d!\n", tun_fd);
    while(tun_out_task_run_flag) {
        int total_len = read(tun_fd, tun_out_buffer, BUF_SIZE-2);
#if 0
        DEBUG_PRINTF("TUN R=%d\n", total_len);
#endif
        if (total_len < 0) {
            ERROR_PRINTF("Reading from interface error\n");
            tun_out_task_run_flag =  0;
            slip_out_task_stop();
            close(tun_fd);
            return NULL;
        }
        
#if CHECK_SUM_ON
        total_len = if_api_calculate_checksum(tun_out_buffer, total_len);
#endif

        send_packet(tun_out_buffer, total_len);
    }
    close(tun_fd);
    tun_out_task_run_flag = 0;
    slip_out_task_stop();
    DEBUG_PRINTF("tun out task exit!\n");
    return NULL;
}

static int tun_fd;
int tun_out_task_start(void)
{
    DEBUG_PRINTF("open TUN/TAP device\n" );
    /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
     *        IFF_TAP   - TAP device
     *        IFF_NO_PI - Do not provide packet information
     */
    tun_fd = tun_alloc(IFF_TAP | IFF_NO_PI);
    if (tun_fd < 0) {
        ERROR_PRINTF("allocating interface error\n");
        return -1;
    }
#if 1
    if(slip_out_task_start(&tun_fd) < 0) {
        ERROR_PRINTF("slip_out_task start error\n");
        close(tun_fd);
        return -1;
    }
#endif
    tun_out_task_run_flag = 1;
    DEBUG_PRINTF("tun out task start %d!\n", tun_fd);

    return pthread_create(&gs_tun_out_task_pthread_id, 0, tun_out_task_proc, &tun_fd);
}

int tun_out_task_stop(void)
{
    if (tun_out_task_run_flag == 1) {
        tun_out_task_run_flag = 0;
        pthread_join(gs_tun_out_task_pthread_id, 0);
    } else {
        ERROR_PRINTF("tun out task failed!\n");
        return -1;
    }
    return 0;
}



int tun_alloc(int flags)
{

    struct ifreq ifr;
    int fd, err;
    char *clonedev = "/dev/net/tun";

    if ((fd = open(clonedev, O_RDWR)) < 0) {
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = flags;

    if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
        close(fd);
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
void send_packet(unsigned char *p, int len)
{
    /* send an initial END character to flush out any data that may
     * have accumulated in the receiver due to line noise
     */
    send_char(END);

    /* for each byte in the packet, send the appropriate character
     * sequence
    */
    while(len--) {
        switch(*p) {
            /* if it's the same code as an END character, we send a
             * special two character code so as not to make the
             * receiver think we sent an END */
           case END:
                send_char(ESC);
                send_char(ESC_END);
                break;
    
           /* if it's the same code as an ESC character,
            * we send a special two character code so as not
            * to make the receiver think we sent an ESC
            */
           case ESC:
                send_char(ESC);
                send_char(ESC_ESC);
                break;
    
           /* otherwise, we just send the character
            */
           default:
                send_char(*p);
        }
        
        p++;
    }

   /* tell the receiver that we're done sending the packet */
    send_char(END);
    send_char_do();
}


/* RECV_PACKET: receives a packet into the buffer located at "p".
 *      If more than len bytes are received, the packet will
 *      be truncated.
 *      Returns the number of bytes stored in the buffer.
 */
int recv_packet(unsigned char *p, int len)
{
    unsigned char c;
    int received = 0;

    /* sit in a loop reading bytes until we put together
     * a whole packet.
     * Make sure not to copy them into the packet if we
     * run out of room.
     */
    while(slip_out_task_run_flag) {
       /* get a character to process */
        c = recv_char();

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
                c = recv_char();

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


#if CHECK_SUM_ON
int if_api_calculate_checksum(void *buf, int len)
{
    uint8_t *p = (uint8_t *)buf;
    uint16_t sum = 0;
    int l = len;
    while(l--)
        sum += *p++;
    sum = ~sum;

    *p++ = (sum >> 8) & 0x7f;
    *p = sum & 0x7f;
    return len + 2;
}

int if_api_check(void *buf, int len)
{
    uint8_t *p = (uint8_t *)buf;
    if(len < 2)
        return -1;
    len -= 2;
    uint8_t chechsum_low = p[len+1];
    uint8_t chechsum_high = p[len];
    p[len+1] = 0;
    p[len] = 0;
    if_api_calculate_checksum(p, len);

    if(chechsum_low != p[len+1] || chechsum_high != p[len])
        return -1;
    return len;
}
#endif /* CHECK_SUM_ON */

#else

int slip_tun_init(void)
{
    return 1;
}

int slip_tun_readable(void) 
{
    return 0;
}

int slip_tun_read(void)
{
    return 0;
}

int slip_tun_writeable(void)
{
    return 1;
}

int slip_tun_write(int ch)
{
    printf("%02x,", ch);
    return 0;
}
#endif /*TUN_SUPPORT*/

/*****************************END OF FILE***************************/
