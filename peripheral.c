/* 2020 03 11 */
/* By hxdyxd */
#include <peripheral.h>

/******************************memory*****************************************/
void memory_reset(void *base)
{
    uint8_t *memory = base;
    memset(memory, 0, MEM_SIZE);
}

uint32_t memory_read(void *base, uint32_t address)
{
    uint8_t *memory = base;
    return *((int*)(memory + address));
}

void memory_write(void *base, uint32_t address, uint32_t data, uint8_t mask)
{
    uint8_t *memory = base;
    switch(mask) {
    case 3:
        {    
            int *data_p = (int *) (memory + address);
            *data_p = data;
        }
        break;
    case 1:
        {
            short *data_p = (short *) (memory + address);
            *data_p = data;
        }
        break;
    default:
        {
            char * data_p = (char *) (memory + address);
            *data_p = data;
        }
        break;
    }
}


/******************************memory*****************************************/

/******************************interrupt**************************************/


void intc_reset(void *base)
{
    struct interrupt_register *intc = base;
    intc->MSK = 0xFFFFFFFF;
    intc->PND = 0x00000000;
}

uint32_t intc_read(void *base, uint32_t address)
{
    struct interrupt_register *intc = base;
    switch(address) {
    case 0x0:
        return intc->MSK;
    case 0x4:
        return intc->PND;
    default:
        ;
    }
    return 0;
}

void intc_write(void *base, uint32_t address, uint32_t data, uint8_t mask)
{
    struct interrupt_register *intc = base;
    switch(address) {
    case 0x0:
        intc->MSK = data;
    case 0x4:
        intc->PND = data;
    default:
        ;
    }
}

#define  int_is_set(v, bit) ( ((v)>>(bit))&1 )

uint32_t interrupt_happen(struct interrupt_register *intc, uint32_t id)
{
    if( int_is_set(intc->MSK, id) || int_is_set(intc->PND, id) ) {
        //masked or not cleared
        return 0;
    }
    intc->PND |= 1 << id;
    return 1;
}

/******************************interrupt**************************************/

/*******************************timer*****************************************/


void tim_reset(void *base)
{
    struct timer_register *tim = base;
    tim->CNT = 0x00000000;
    tim->EN =  0x00000000;
}

uint32_t tim_read(void *base, uint32_t address)
{
    struct timer_register *tim = base;
    switch(address) {
    case 0x0:
        tim->CNT = GET_TICK();
        return tim->CNT;
    case 0x4:
        return tim->EN;
    default:
        ;
    }
    return 0;
}

void tim_write(void *base, uint32_t address, uint32_t data, uint8_t mask)
{
    struct timer_register *tim = base;
    switch(address) {
    case 0x0:
        tim->CNT = data;
    case 0x4:
        tim->EN = data;
    default:
        ;
    }
}

/*******************************timer*****************************************/


/*******************************earlyuart*****************************************/


void earlyuart_reset(void *base)
{
    struct earlyuart_register *earlyuart = base;
    earlyuart->FLAG = 0x00000001;
    earlyuart->OUT =  0x00000000;
}

uint32_t earlyuart_read(void *base, uint32_t address)
{
    struct earlyuart_register *earlyuart = base;
    switch(address) {
    case 0x0:
        return earlyuart->FLAG;
    case 0x4:
        return earlyuart->OUT;
    default:
        ;
    }
    return 0;
}

void earlyuart_write(void *base, uint32_t address, uint32_t data, uint8_t mask)
{
    struct earlyuart_register *earlyuart = base;
    switch(address) {
    case 0x0:
        earlyuart->FLAG = data;
    case 0x4:
        PUTCHAR(data);
    default:
        ;
    }
}


/*******************************earlyuart*****************************************/


/*******************************uart*****************************************/


void uart_8250_reset(void *base) 
{
    struct uart_register *uart = base;
    memset(uart, 0, sizeof(struct uart_register));
    uart->IIR = 0x01;
    uart->LSR = 0x60;
}

#define UART_LCR_DLAB(uart)   ((uart)->LCR & 0x80)
#define  register_set(r,b,v)  do{ if(v) {r |= 1 << (b);} else {r &= ~(1 << (b));} }while(0)

uint32_t uart_8250_read(void *base, uint32_t address)
{
    struct uart_register *uart = base;
    //PRINTF("uart read 0x%x\n", address);
    switch(address) {
    case 0x0:
        if(UART_LCR_DLAB(uart)) {
            //Divisor Latch Low
            return uart->DLL;
        } else {
            //Receive Buffer Register
            register_set(uart->LSR, 0, 0);
            if( KBHIT() )
                uart->RBR = GETCH();
            return uart->RBR;
        }
        break;
    case 0x4:
        if(UART_LCR_DLAB(uart)) {
            //Divisor Latch High
            return uart->DLH;
        } else {
            //Interrupt Enable Register 
            return uart->IER;
        }
        break;
    case 0x8:
        //Interrupt Identity Register
        return uart->IIR;
    case 0xc:
        //Line Control Register
        return uart->LCR;
    case 0x10:
        //Modem Control Register
        return uart->MCR;
    case 0x14:
        //Line Status Register, read-only
        return uart->LSR;
    case 0x18:
        //Modem Status Registe, read-only
        return uart->MSR;
    case 0x1c:
        //Scratchpad Register
        return uart->SCR;
    case 0xf8:
        return 0;
    default:
        printf("uart read %x\n", address);
    }
    return 0;
}

void uart_8250_write(void *base, uint32_t address, uint32_t data, uint8_t mask)
{
    struct uart_register *uart = base;
    //PRINTF("uart write 0x%x 0x%x\n", address, data);
    switch(address) {
    case 0x0:
        if(UART_LCR_DLAB(uart)) {
            //Divisor Latch Low
            uart->DLL = data;
        } else {
            //Transmit Holding Register
            PUTCHAR(data);
        }
        break;
    case 0x4:
        if(UART_LCR_DLAB(uart)) {
            //Divisor Latch High
            uart->DLH = data;
        } else {
            //Interrupt Enable Register 
            uart->IER = data;
        }
        break;
    case 0x8:
        //FIFO Control Register
        uart->FCR = data;
        break;
    case 0xc:
        //Line Control Register
        uart->LCR = data;
        break;
    case 0x10:{
        //Modem Control Register
        uint8_t lastmcr = uart->MCR;
        uart->MCR = data;
        if( (uart->MCR&1) ^ (lastmcr&1) ) {
            //set DSR
            uart->MSR |=  (1<<1);
        }
        if( (uart->MCR&0x10) ) {
            //Loopback
            register_set(uart->MSR, 7, (uart->MCR&0x8) ); //Auxiliary output 2
            register_set(uart->MSR, 6, (uart->MCR&0x4) ); //Auxiliary output 1
            register_set(uart->MSR, 5, (uart->MCR&0x1) ); //Auxiliary DTR
            register_set(uart->MSR, 4, (uart->MCR&0x2) ); //Auxiliary RTS
            
            register_set(uart->MSR, 3, (uart->MCR&0x8) ); //Auxiliary output 2
            register_set(uart->MSR, 2, (uart->MCR&0x4) ); //Auxiliary output 1
            register_set(uart->MSR, 1, (uart->MCR&0x1) ); //Auxiliary DTR
            register_set(uart->MSR, 0, (uart->MCR&0x2) ); //Auxiliary RTS
            //printf("Loopback msr = 0x%x\n", uart->MSR);
        } else {
            uart->MSR = 0;
            register_set(uart->MSR, 5, 1);  //DSR准备就绪
            register_set(uart->MSR, 4, 1);  //CTS有效
        }
        
        break;
    }
    case 0x14:
    case 0x18:
        break;
    case 0x1c:
        //uart->SCR = data;
        break;
    default:
        printf("uart write %x\n", address);
    }
}


/*******************************uart*****************************************/
/*****************************END OF FILE***************************/
