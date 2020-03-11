/* 2020 03 11 */
/* By hxdyxd */

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


#define UART_NUMBER    1



struct peripheral_t {
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

void intc_reset(void *base);
uint32_t intc_read(void *base, uint8_t address);
void intc_write(void *base, uint8_t address, uint32_t data);

void tim_reset(void *base);
uint32_t tim_read(void *base, uint8_t address);
void tim_write(void *base, uint8_t address, uint32_t data);

void earlyuart_reset(void *base);
uint32_t earlyuart_read(void *base, uint8_t address);
void earlyuart_write(void *base, uint8_t address, uint32_t data);

void uart_8250_reset(void *base);
uint32_t uart_8250_read(void *base, uint8_t address);
void uart_8250_write(void *base, uint8_t address, uint32_t data);

uint32_t interrupt_happen(struct interrupt_register *intc, uint32_t id);
//uint32_t user_event(struct armv4_cpu_t *cpu, uint32_t kips_speed);

#endif

/*****************************END OF FILE***************************/
