/*
 * slip.c of arm_emulator
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
#include <slip.h>
#include <sys/time.h>
#include <sys/select.h>

/* Blockable */
static inline void send_char(struct __kfifo *fifo, int ch, uint8_t *is_run)
{
    while(__kfifo_in(fifo, &ch, 1) == 0 && *is_run) {
        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 20,
        };
        select(0, NULL, NULL, NULL, &timeout);
    }
}


#define FIFO_OUT_CHAR_PEEK(f,c,l)  __kfifo_out_peek_one(f,c,l)



/* SLIP special character codes */
#define END             0300    /* indicates end of packet */
#define ESC             0333    /* indicates byte stuffing */
#define ESC_END         0334    /* ESC ESC_END means END data byte */
#define ESC_ESC         0335    /* ESC ESC_ESC means ESC data byte */


void slip_send_packet(struct __kfifo *fifo, uint8_t *buf, int len, uint8_t *is_run)
{
    /* send an initial END character to flush out any data that may
     * have accumulated in the receiver due to line noise
     */
    send_char(fifo, END, is_run);

    /* for each byte in the packet, send the appropriate character
     * sequence
    */
    while(len--) {
        switch(*buf) {
        /* if it's the same code as an END character, we send a
         * special two character code so as not to make the
         * receiver think we sent an END */
        case END:
            send_char(fifo, ESC, is_run);
            send_char(fifo, ESC_END, is_run);
            break;

        /* if it's the same code as an ESC character,
        * we send a special two character code so as not
        * to make the receiver think we sent an ESC
        */
        case ESC:
            send_char(fifo, ESC, is_run);
            send_char(fifo, ESC_ESC, is_run);
            break;

        /* otherwise, we just send the character
        */
        default:
            send_char(fifo, *buf, is_run);
        }

        buf++;
    }

    /* tell the receiver that we're done sending the packet */
    send_char(fifo, END, is_run);
}

int slip_recv_poll(struct __kfifo *fifo, uint8_t *buf, int len)
{
    uint8_t ch;
    int fifo_out = 0;
    int received = 0;
    while(FIFO_OUT_CHAR_PEEK(fifo, &ch, fifo_out+1) == fifo_out+1) {
        fifo_out++;
        switch(ch) {
        case END:
            if(received) {
                fifo->out += fifo_out;
                return received;
            } else {
                break;
            }
        case ESC:
            if(FIFO_OUT_CHAR_PEEK(fifo, &ch, fifo_out+1) == fifo_out+1) {
                fifo_out++;
                switch(ch) {
                case ESC_END:
                    ch = END;
                    break;
                case ESC_ESC:
                    ch = ESC;
                    break;
                }
            } else {
                return 0;
            }
        default:
            if(received < len) {
                buf[received++] = ch;
            } else {
                //long packet
                fifo->out += fifo_out;
                return received;
            }
        }
    }
    return 0;
}

/*****************************END OF FILE***************************/
