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
#include <string.h>

/*******HAL*******/
#include <time.h>
#include <conio.h>

#define  KBHIT()      kbhit()
#define  GETCH()      getch()
#define  PUTCHAR(c)   putchar(c)
#define  GET_TICK()   (clock()*1000/CLOCKS_PER_SEC)
/*******HAL END*******/


#define MEM_SIZE   (0x2000000)  //32M
#define UART_NUMBER    1



struct peripheral_t {
    uint8_t memory[MEM_SIZE];

    struct interrupt_register {
        uint32_t MSK; //Determine which interrupt source is masked.
        uint32_t PND; //Indicate the interrupt request status
    }intc;

    struct timer_register {
        uint32_t CNT; //Determine which interrupt source is masked.
        uint32_t EN; //Indicate the interrupt request status
    }tim;

    struct earlyuart_register {
        uint32_t FLAG;
        uint32_t OUT;
    }earlyuart;

    struct uart_register {
        uint32_t DLL; //Divisor Latch Low, 1
        uint32_t DLH; //Divisor Latch High, 2
        uint32_t IER; //Interrupt Enable Register, 1
        uint32_t IIR; //Interrupt Identity Register, 2
        uint32_t FCR; //FIFO Control Register
        uint32_t LCR; //Line Control Register, 3
        uint32_t MCR; //Modem Control Register, 4
        uint32_t LSR; //Line Status Register, 5
        uint32_t MSR; //Modem Status Registe, 6
        uint32_t SCR;
        uint32_t RBR;
    }uart[UART_NUMBER];
};

void memory_reset(void *base);
uint32_t memory_read(void *base, uint32_t address);
void memory_write(void *base, uint32_t address, uint32_t data, uint8_t mask);

void intc_reset(void *base);
uint32_t intc_read(void *base, uint32_t address);
void intc_write(void *base, uint32_t address, uint32_t data, uint8_t mask);
uint32_t user_event(struct peripheral_t *base, const uint32_t code_counter, const uint32_t kips_speed);

void tim_reset(void *base);
uint32_t tim_read(void *base, uint32_t address);
void tim_write(void *base, uint32_t address, uint32_t data, uint8_t mask);

void earlyuart_reset(void *base);
uint32_t earlyuart_read(void *base, uint32_t address);
void earlyuart_write(void *base, uint32_t address, uint32_t data, uint8_t mask);

void uart_8250_reset(void *base);
uint32_t uart_8250_read(void *base, uint32_t address);
void uart_8250_write(void *base, uint32_t address, uint32_t data, uint8_t mask);


#endif

/*****************************END OF FILE***************************/
