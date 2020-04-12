/*
 * peripheral.c of arm_emulator
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
#include <peripheral.h>
#include <assert.h>

#define LOG_NAME   "peripheral"
#define DEBUG_PRINTF(...)     printf("\033[0;32m" LOG_NAME "\033[0m: " __VA_ARGS__)
#define ERROR_PRINTF(...)     printf("\033[1;31m" LOG_NAME "\033[0m: " __VA_ARGS__)



/******************************memory*****************************************/
uint32_t memory_reset(void *base)
{
    uint8_t **mem = base;
    *mem = (uint8_t *)malloc(MEM_SIZE);
    if(!*mem) {
        ERROR_PRINTF("memory alloc err\n");
        return 0;
    }
    return 1;
}


void memory_exit(int s, void *base)
{
    uint8_t **mem = base;
    free(*mem);
    *mem = NULL;
}


uint32_t memory_read(void *base, uint32_t address)
{
    uint8_t **mem = base;
    return *((int*)(*mem + address));
}


void memory_write(void *base, uint32_t address, uint32_t data, uint8_t mask)
{
    uint8_t **mem = base;
    switch(mask) {
    case 3:
        {    
            int *data_p = (int *) (*mem + address);
            *data_p = data;
        }
        break;
    case 1:
        {
            short *data_p = (short *) (*mem + address);
            *data_p = data;
        }
        break;
    default:
        {
            char * data_p = (char *) (*mem + address);
            *data_p = data;
        }
        break;
    }
}


/******************************memory*****************************************/

/******************************fs*****************************************/
void fs_exit(int s, void *base)
{
    struct fs_t *fs = base;
#ifdef FS_MMAP_MODE
    if(fs->fd) {
        munmap(fs->map, fs->len);
        close(fs->fd);
        DEBUG_PRINTF("fs Exit %s\n", fs->filename);
    }
#else
    if(fs->fp) {
        fclose(fs->fp);
        DEBUG_PRINTF("fs Exit %s\n", fs->filename);
    }
#endif
}


uint32_t fs_reset(void *base)
{
    struct fs_t *fs = base;
    uint32_t ret = 0;
    if(fs->filename) {
#ifdef FS_MMAP_MODE
        fs->fd = open(fs->filename, O_RDWR);
        if(fs->fd < 0) {
            ERROR_PRINTF("fs Open %s: err\n", fs->filename);
            return ret;
        }
        fs->len = lseek(fs->fd, 0L, SEEK_END);
        fs->map = mmap(0, fs->len, PROT_READ | PROT_WRITE, MAP_SHARED, fs->fd, 0);
        if(fs->map == MAP_FAILED) {
            ERROR_PRINTF("fs mmap %s: err\n", fs->filename);
            close(fs->fd);
            return ret;
        }
        ret = 1;
#else
        fs->fp = fopen(fs->filename, "rb+");
        if(fs->fp == NULL) {
            ERROR_PRINTF("fs Open %s: err\n", fs->filename);
            return ret;
        }
        ret = 1;
#endif
    }
    return ret;
}


uint32_t fs_read(void *base, uint32_t address)
{
    struct fs_t *fs = base;
#ifdef FS_MMAP_MODE
    if(fs->map && address < fs->len) {
        return memory_read(&fs->map, address);
    }
    return 0;
#else
    uint32_t data = 0;
    if(fs->fp) {
        fseek(fs->fp, address, SEEK_SET);
        fread(&data, 4, 1, fs->fp);
    }
    return data;
#endif
}


void fs_write(void *base, uint32_t address, uint32_t data, uint8_t mask)
{
    struct fs_t *fs = base;
#ifdef FS_MMAP_MODE
    if(fs->map && address < fs->len) {
        memory_write(&fs->map, address, data, mask);
    }
#else
    if(!fs->fp) {
        return;
    }
    switch(mask) {
    case 3:
        {
            fseek(fs->fp, address, SEEK_SET);
            fwrite(&data, 4, 1, fs->fp);
        }
        break;
    case 1:
        {
            fseek(fs->fp, address, SEEK_SET);
            fwrite(&data, 2, 1, fs->fp);
        }
        break;
    default:
        {
            fseek(fs->fp, address, SEEK_SET);
            fwrite(&data, 1, 1, fs->fp);
        }
        break;
    }
#endif
}


/******************************fs*****************************************/



/******************************interrupt**************************************/


uint32_t intc_reset(void *base)
{
    struct interrupt_register *intc = base;
    intc->MSK = 0xFFFFFFFF;
    intc->PND = 0x00000000;
    return 1;
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
        break;
    }
    return 0;
}


void intc_write(void *base, uint32_t address, uint32_t data, uint8_t mask)
{
    struct interrupt_register *intc = base;
    switch(address) {
    case 0x0:
        intc->MSK = data;
        break;
    case 0x4:
        intc->PND = data;
        break;
    default:
        break;
    }
}

#define  int_is_set(v, bit) ( ((v)>>(bit))&1 )


static uint32_t interrupt_happen(struct interrupt_register *intc, uint32_t id)
{
    if( int_is_set(intc->MSK, id) || int_is_set(intc->PND, id) ) {
        //masked or not cleared
        return 0;
    }
    intc->PND |= 1 << id;
    return 1;
}


/*
 * user_event: interrupt request
 * author:hxdyxd
 */
uint32_t user_event(struct peripheral_t *base, const uint32_t code_counter)
{
    uint32_t event = 0;
    struct interrupt_register *intc = &base->intc;
    struct timer_register *tim = &base->tim;
    if(tim->EN &&  tim->CNT - tim->privious_cnt >= 10 ) {
        tim->privious_cnt = tim->CNT;
        event = interrupt_happen(intc, tim->interrupt_id);
        return event;
    } else {
        for(int i=0; i<UART_NUMBER; i++) {
            struct uart_register *uart = &base->uart[i];
            if(uart->IER&0xf) {
                //uart enable
                uart->IIR = UART_IIR_NO_INT; //no interrupt pending
                //UART
                if( (uart->IER & UART_IER_THRI) && uart->interface->writeable() ) {
                    //Bit1, Enable Transmit Holding Register Empty Interrupt. 
                    if((event = interrupt_happen(intc, uart->interrupt_id)) != 0) {
                        uart->IIR = UART_IIR_THRI; // THR empty interrupt pending
                    }
                    return event;
                } else  if( (uart->IER & UART_IER_RDI) && 
                     (code_counter & 0x7fff) == 0x4000 && uart->interface->readable() ) {
                    //Bit0, Enable Received Data Available Interrupt. 
                    if((event = interrupt_happen(intc, uart->interrupt_id)) != 0 ) {
                        uart->IIR = UART_IIR_RDI; //received data available interrupt pending
                    }
                    return event;
                }
            } /*end if*/
        } /*end for*/
    }
    return event;
}


/******************************interrupt**************************************/

/*******************************timer*****************************************/

static void *tim_proc(void *base)
{
    struct timer_register *tim = base;
#ifdef USE_PRCTL_SET_THREAD_NAME
    prctl(PR_SET_NAME,"timer_task");
#endif
    DEBUG_PRINTF("timer task enter success!\n");

    while(tim->is_run) {
        int r = poll(NULL, 0, tim->PERIOD);
        if(tim->EN && r == 0) {
            tim->CNT++;
        }
    }
    tim->is_run = 0;
    DEBUG_PRINTF("timer task exit!\n");
    return NULL;
}


void tim_exit(int s, void *base)
{
    struct timer_register *tim = base;
    if (tim->is_run) {
        tim->is_run = 0;
        pthread_join(tim->thread_id, 0);
    } else {
        ERROR_PRINTF("timer task exit err!\n");
    }
}


uint32_t tim_reset(void *base)
{
    struct timer_register *tim = base;
    tim->CNT = 0x00000000;
    tim->EN =  0x00000000;
    tim->PERIOD = 1;
    DEBUG_PRINTF("timer interrupt id: %d\n", tim->interrupt_id);

    tim->is_run = 1;
    tim->privious_cnt = 0;
    if(pthread_create(&tim->thread_id, 0, tim_proc, tim) < 0) {
        ERROR_PRINTF("timer task err!\n");
        return 0;
    }
    return 1;
}


uint32_t tim_read(void *base, uint32_t address)
{
    struct timer_register *tim = base;
    switch(address) {
    case 0x0:
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
        if(!tim->EN)
            tim->CNT = data;
        break;
    case 0x4:
        tim->EN = data;
        break;
    default:
        ;
    }
}

/*******************************timer*****************************************/


/*******************************uart*****************************************/

uint8_t uart_8250_rw_enable(void)
{
    return 1;
}


uint8_t uart_8250_rw_disable(void)
{
    return 0;
}


void uart_8250_register(struct uart_register *uart,
 const struct charwr_interface *interface)
{
    uart->interface = interface;
}


uint32_t uart_8250_reset(void *base) 
{
    struct uart_register *uart = base;
    uart->IER = 0;
    uart->IIR = UART_IIR_NO_INT; //no interrupt pending
    uart->LSR = UART_LSR_TEMT | UART_LSR_THRE; //THR empty
    if(!uart->interface_register_cb) {
        ERROR_PRINTF("uart_8250 interface_register_cb = null\n");
        return 0;
    }
    if(uart->interface_register_cb(uart) < 0)
        return 0;
    if(!uart->interface->init)
        return 0;
    if(!uart->interface->init())
        return 0;

    DEBUG_PRINTF("uart_8250 interrupt id: %d\n", uart->interrupt_id);
    return 1;
}


void uart_8250_exit(int s, void *base)
{
    struct uart_register *uart = base;
    if(uart->interface->exit)
        uart->interface->exit();
}



uint32_t uart_8250_read(void *base, uint32_t address)
{
    struct uart_register *uart = base;
    //printf("uart read 0x%x\n", address);
    switch(address) {
    case 0x0:
        if( uart->LCR & UART_LCR_DLAB ) {
            //Divisor Latch Low
            return uart->DLL;
        } else {
            //Receive Buffer Register
            uart->RBR = uart->interface->read();
            return uart->RBR;
        }
        break;
    case 0x4:
        if( uart->LCR & UART_LCR_DLAB ) {
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
        register_set(uart->LSR, 0, uart->interface->readable()?1:0);
        register_set(uart->LSR, 6, uart->interface->writeable()?1:0);
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
        ERROR_PRINTF("uart_8250 read %x\n", address);
    }
    return 0;
}


void uart_8250_write(void *base, uint32_t address, uint32_t data, uint8_t mask)
{
    struct uart_register *uart = base;
    //PRINTF("uart write 0x%x 0x%x\n", address, data);
    switch(address) {
    case 0x0:
        if( uart->LCR & UART_LCR_DLAB ) {
            //Divisor Latch Low
            uart->DLL = data;
        } else {
            //Transmit Holding Register
            uart->interface->write(data);
        }
        break;
    case 0x4:
        if( uart->LCR & UART_LCR_DLAB ) {
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
        uart->MCR = data;

        if( uart->MCR & UART_MCR_LOOP ) {
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
            uart->MSR |= UART_MSR_DSR;
            uart->MSR |= UART_MSR_CTS;
            if( uart->MCR & UART_MCR_RTS ) {
                //printf("RTS = 1, Cannot receive data\n");
            }
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
        ERROR_PRINTF("uart_8250 write %x\n", address);
    }
}


/*******************************uart*****************************************/
/*****************************END OF FILE***************************/
