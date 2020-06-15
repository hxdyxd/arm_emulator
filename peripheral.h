/*
 * peripheral.h of arm_emulator
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
#ifndef _PERIPHERAL_H_
#define _PERIPHERAL_H_


#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include <config.h>


#ifndef MEM_SIZE
#define MEM_SIZE   (1 << 25)  //32M
#endif

#define UART_NUMBER    (2)


#ifdef FS_MMAP_MODE
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#endif

/* return: 0 false ,1 true */
struct charwr_interface {
    uint8_t ( *init)(void);
    void ( *exit)(void);
    uint8_t ( *readable)(void);
    uint8_t ( *read)(void);
    uint8_t ( *writeable)(void);
    uint8_t ( *write)(uint8_t ch);
};

struct peripheral_t {
    uint8_t *mem;

    struct fs_t {
        char *filename;
#ifdef FS_MMAP_MODE
        int fd;
        uint8_t *map;
        uint32_t len;
#else
        FILE *fp;
#endif
    }fs;

    struct interrupt_register {
        uint32_t MSK; //Determine which interrupt source is masked.
        uint32_t PND; //Indicate the interrupt request status
    }intc;

    struct timer_register {
        //predefined start
        uint8_t is_run;
        uint32_t privious_cnt;
        pthread_t thread_id;
        uint32_t interrupt_id;
        //predefined end

        uint32_t CNT; //Determine which interrupt source is masked.
        uint32_t EN; //Indicate the interrupt request status
        uint32_t PERIOD;
    }tim;

    struct uart_register {
        //predefined start
        int ( *interface_register_cb)(struct uart_register *uart);
        uint32_t interrupt_id;
        //predefined end
        const struct charwr_interface *interface;

        uint32_t DLL; //Divisor Latch Low, 1
        uint32_t DLH; //Divisor Latch High, 2
        uint32_t IER; //Interrupt Enable Register, 1
#define UART_IER_MSI        0x08 /* Enable Modem status interrupt */
#define UART_IER_RLSI       0x04 /* Enable receiver line status interrupt */
#define UART_IER_THRI       0x02 /* Enable Transmitter holding register int. */
#define UART_IER_RDI        0x01 /* Enable receiver data interrupt */
        uint32_t IIR; //Interrupt Identity Register, 2
#define UART_IIR_NO_INT     0x01 /* No interrupts pending */
#define UART_IIR_ID         0x0e /* Mask for the interrupt ID */
#define UART_IIR_MSI        0x00 /* Modem status interrupt */
#define UART_IIR_THRI       0x02 /* Transmitter holding register empty */
#define UART_IIR_RDI        0x04 /* Receiver data interrupt */
#define UART_IIR_RLSI       0x06 /* Receiver line status interrupt */
        uint32_t FCR; //FIFO Control Register
        uint32_t LCR; //Line Control Register, 3
#define UART_LCR_DLAB       0x80 /* Divisor latch access bit */
#define UART_LCR_SBC        0x40 /* Set break control */
#define UART_LCR_SPAR       0x20 /* Stick parity (?) */
#define UART_LCR_EPAR       0x10 /* Even parity select */
#define UART_LCR_PARITY     0x08 /* Parity Enable */
#define UART_LCR_STOP       0x04 /* Stop bits: 0=1 bit, 1=2 bits */
#define UART_LCR_WLEN5      0x00 /* Wordlength: 5 bits */
#define UART_LCR_WLEN6      0x01 /* Wordlength: 6 bits */
#define UART_LCR_WLEN7      0x02 /* Wordlength: 7 bits */
#define UART_LCR_WLEN8      0x03 /* Wordlength: 8 bits */
        uint32_t MCR; //Modem Control Register, 4
#define UART_MCR_CLKSEL     0x80 /* Divide clock by 4 (TI16C752, EFR[4]=1) */
#define UART_MCR_TCRTLR     0x40 /* Access TCR/TLR (TI16C752, EFR[4]=1) */
#define UART_MCR_XONANY     0x20 /* Enable Xon Any (TI16C752, EFR[4]=1) */
#define UART_MCR_AFE        0x20 /* Enable auto-RTS/CTS (TI16C550C/TI16C750) */
#define UART_MCR_LOOP       0x10 /* Enable loopback test mode */
#define UART_MCR_OUT2       0x08 /* Out2 complement */
#define UART_MCR_OUT1       0x04 /* Out1 complement */
#define UART_MCR_RTS        0x02 /* RTS complement */
#define UART_MCR_DTR        0x01 /* DTR complement */
        uint32_t LSR; //Line Status Register, 5
#define UART_LSR_FIFOE      0x80 /* Fifo error */
#define UART_LSR_TEMT       0x40 /* Transmitter empty */
#define UART_LSR_THRE       0x20 /* Transmit-hold-register empty */
#define UART_LSR_BI         0x10 /* Break interrupt indicator */
#define UART_LSR_FE         0x08 /* Frame error indicator */
#define UART_LSR_PE         0x04 /* Parity error indicator */
#define UART_LSR_OE         0x02 /* Overrun error indicator */
#define UART_LSR_DR         0x01 /* Receiver data ready */
        uint32_t MSR; //Modem Status Registe, 6
#define UART_MSR_DCD        0x80 /* Data Carrier Detect */
#define UART_MSR_RI         0x40 /* Ring Indicator */
#define UART_MSR_DSR        0x20 /* Data Set Ready */
#define UART_MSR_CTS        0x10 /* Clear to Send */
#define UART_MSR_DDCD       0x08 /* Delta DCD */
#define UART_MSR_TERI       0x04 /* Trailing edge ring indicator */
#define UART_MSR_DDSR       0x02 /* Delta DSR */
#define UART_MSR_DCTS       0x01 /* Delta CTS */
#define UART_MSR_ANY_DELTA  0x0F /* Any of the delta bits! */
        uint32_t SCR;
        uint32_t RBR;
    }uart[UART_NUMBER];
};

#define  register_set(r,b,v)  do{ if(v) {r |= 1 << (b);} else {r &= ~(1 << (b));} }while(0)

void memory_exit(int s, void *base);
uint32_t memory_reset(void *base);
uint32_t memory_read(void *base, uint32_t address);
void memory_write(void *base, uint32_t address, uint32_t data, uint8_t mask);

void fs_exit(int s, void *base);
uint32_t fs_reset(void *base);
uint32_t fs_read(void *base, uint32_t address);
void fs_write(void *base, uint32_t address, uint32_t data, uint8_t mask);

uint32_t intc_reset(void *base);
uint32_t intc_read(void *base, uint32_t address);
void intc_write(void *base, uint32_t address, uint32_t data, uint8_t mask);
#define EVENT_TYPE_DETECT   (0)
#define EVENT_TYPE_HAPPEN   (1)
uint32_t user_event(struct peripheral_t *base, uint8_t type);

void tim_exit(int s, void *base);
uint32_t tim_reset(void *base);
uint32_t tim_read(void *base, uint32_t address);
void tim_write(void *base, uint32_t address, uint32_t data, uint8_t mask);

void uart_8250_exit(int s, void *base);
void uart_8250_register(struct uart_register *uart, const struct charwr_interface *interface);
uint32_t uart_8250_reset(void *base);
uint32_t uart_8250_read(void *base, uint32_t address);
void uart_8250_write(void *base, uint32_t address, uint32_t data, uint8_t mask);

uint8_t uart_8250_rw_enable(void);
uint8_t uart_8250_rw_disable(void);


#endif

/*****************************END OF FILE***************************/
