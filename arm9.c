/* 2019 06 26 */
/* By hxdyxd */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <conio.h>

#define  KBHIT()      kbhit()
#define  GETCH()      getch()
#define  PUTCHAR(c)   putchar(c)
#define  GET_TICK()   (clock()*1000/CLOCKS_PER_SEC)


static inline uint32_t mmu_transfer(uint32_t vaddr, uint8_t privileged, uint8_t wr);
void interrupt_exception(uint8_t type);

/****************************************************/
//Memary size

#define MEM_SIZE   (0x2000000)  //32M
/****************************************************/

#define DEBUG                  0


uint8_t abort_test = 0;

#define PRINTF(...)  do{ if(DEBUG){printf(__VA_ARGS__);} }while(0)

#define  IS_SET(v, bit) ( ((v)>>(bit))&1 )


uint32_t spsr[7] = {0, };
#define  cpsr    spsr[0]


#define  cpsr_n_set(v)  do{ if(v) {cpsr |= 1 << 31;} else {cpsr &= ~(1 << 31);} }while(0)
#define  cpsr_z_set(v)  do{ if(v) {cpsr |= 1 << 30;} else {cpsr &= ~(1 << 30);} }while(0)
#define  cpsr_c_set(v)  do{ if(v) {cpsr |= 1 << 29;} else {cpsr &= ~(1 << 29);} }while(0)
#define  cpsr_v_set(v)  do{ if(v) {cpsr |= 1 << 28;} else {cpsr &= ~(1 << 28);} }while(0)

#define  cpsr_i_set(v)  do{ if(v) {cpsr |= 1 << 7;} else {cpsr &= ~(1 << 7);} }while(0)
#define  cpsr_f_set(v)  do{ if(v) {cpsr |= 1 << 6;} else {cpsr &= ~(1 << 6);} }while(0)
#define  cpsr_t_set(v)  do{ if(v) {cpsr |= 1 << 5;} else {cpsr &= ~(1 << 5);} }while(0)

#define  cpsr_n  IS_SET(cpsr, 31)
#define  cpsr_z  IS_SET(cpsr, 30)
#define  cpsr_c  IS_SET(cpsr, 29)
#define  cpsr_v  IS_SET(cpsr, 28)
#define  cpsr_q  IS_SET(cpsr, 27)
#define  cpsr_i  IS_SET(cpsr, 7)
#define  cpsr_f  IS_SET(cpsr, 6)
#define  cpsr_t  IS_SET(cpsr, 5)
#define  cpsr_m  (cpsr&0x1f)

#define 	CPSR_M_USR   0x10U
#define 	CPSR_M_FIQ   0x11U
#define 	CPSR_M_IRQ   0x12U
#define 	CPSR_M_SVC   0x13U
#define 	CPSR_M_MON   0x16U
#define 	CPSR_M_ABT   0x17U
#define 	CPSR_M_HYP   0x1AU
#define 	CPSR_M_UND   0x1BU
#define 	CPSR_M_SYS   0x1FU


//memory
static uint8_t MEM[MEM_SIZE];

//intermediate datapath and control path signals
static uint32_t instruction_word;

const char *opcode_table[16] = {
    "AND", "EOR", "SUB", "RSB", 
    "ADD", "ADC", "SBC", "RSC", 
    "TST", "TEQ", "CMP", "CMN", 
    "ORR", "MOV", "BIC", "MVN"
};

const char *shift_table[4] = {
    "LSL", //00
    "LSR", //01
    "ASR", //10
    "ROR", //11
};

uint32_t code_counter = 0;

//register function
uint32_t Register[7][16];
#define   CPU_MODE_USER      0
#define   CPU_MODE_FIQ       1
#define   CPU_MODE_IRQ       2
#define   CPU_MODE_SVC       3
#define   CPU_MODE_Undef     4
#define   CPU_MODE_Abort     5
#define   CPU_MODE_Mon       6

const char *string_register_mode[7] = {
    "USER",
    "FIQ",
    "IRQ",
    "SVC",
    "Undef",
    "Abort",
    "Mon",
};


uint8_t get_cpu_mode_code(void)
{
    uint8_t cpu_mode = 0;
    switch(cpsr_m) {
    case CPSR_M_USR:
    case CPSR_M_SYS:
        cpu_mode = CPU_MODE_USER; //0
        break;
    case CPSR_M_FIQ:
        cpu_mode = CPU_MODE_FIQ; //1
        break;
    case CPSR_M_IRQ:
        cpu_mode = CPU_MODE_IRQ; //2
        break;
    case CPSR_M_SVC:
        cpu_mode = CPU_MODE_SVC;  //3
        break;
    case CPSR_M_MON:
        cpu_mode = CPU_MODE_Mon;  //6
        break;
    case CPSR_M_ABT:
        cpu_mode = CPU_MODE_Abort;  //5
        break;
    case CPSR_M_UND:
        cpu_mode = CPU_MODE_Undef;  //4
        break;
    default:
        cpu_mode = 0;
        PRINTF("cpu mode is unknown %x\r\n", cpsr_m);
        getchar();
    }
    return cpu_mode;
}

static inline uint32_t register_read_user_mode(uint8_t id)
{
    return Register[0][id];
}

static inline void register_write_user_mode(uint8_t id, uint32_t val)
{
    Register[0][id] = val;
}


static inline uint32_t register_read(uint8_t id)
{
    //Register file
    if(id < 8) {
        return Register[CPU_MODE_USER][id];
    } else if(id == 15) {
        return Register[CPU_MODE_USER][id] + 4;
    } else if(cpsr_m == CPSR_M_USR || cpsr_m == CPSR_M_SYS) {
        return Register[CPU_MODE_USER][id];
    } else if(cpsr_m != CPSR_M_FIQ && id < 13) {
        return Register[CPU_MODE_USER][id];
    } else {
        PRINTF("<%x>", cpsr_m);
        return Register[get_cpu_mode_code()][id];
    }
}

static inline void register_write(uint8_t id, uint32_t val)
{
    //Register file
    if(id < 8 || id == 15) {
        Register[CPU_MODE_USER][id] = val;
    } else if(cpsr_m == CPSR_M_USR || cpsr_m == CPSR_M_SYS) {
        Register[CPU_MODE_USER][id] = val;
    } else if(cpsr_m != CPSR_M_FIQ && id < 13) {
        Register[CPU_MODE_USER][id] = val;
    } else {
        PRINTF("<%x>", cpsr_m);
        Register[get_cpu_mode_code()][id] = val;
    }
}


static inline void exception_out(void)
{
    //return;
    uint8_t cur_mode  = get_cpu_mode_code();
    printf("PC = 0x%x , code = %d\r\n", register_read(15), code_counter);
    printf("cpsr = 0x%x, %s\n", cpsr, string_register_mode[cur_mode]);
    printf("User mode register:");
    for(int i=0; i<16; i++) {
        if(i % 4 == 0) {
            printf("\n");
        }
        printf("R%2d = 0x%08x, ", i, Register[CPU_MODE_USER][i]);
    }
    printf("\r\nPrivileged mode register\r\n");
    for(int i=1; i<7; i++) {
        printf("R%2d = 0x%08x, ", 13, Register[i][13]);
        printf("R%2d = 0x%08x, ", 14, Register[i][14]);
        printf("%s \n", string_register_mode[i]);
    }
    getchar();
}

/*
 *  interrupt_exception
 */
#define  INT_EXCEPTION_RESET      (0)
#define  INT_EXCEPTION_UNDEF      (1)
#define  INT_EXCEPTION_FIQ        (2)
#define  INT_EXCEPTION_IRQ        (3)
#define  INT_EXCEPTION_PREABT     (4)
#define  INT_EXCEPTION_SWI        (5)
#define  INT_EXCEPTION_DATAABT    (6)


/******************************interrupt**************************************/
struct interrupt_register {
    uint32_t MSK; //Determine which interrupt source is masked.
    uint32_t PND; //Indicate the interrupt request status
};

struct interrupt_register INT;


#define   int_reset()      do{\
 INT.MSK = 0xFFFFFFFF;\
 INT.PND = 0x00000000;\
}while(0)

#define  int_is_set(v, bit) ( ((v)>>(bit))&1 )

static inline uint32_t interrupt_happen(uint32_t id)
{
    if( int_is_set(INT.MSK, id) || int_is_set(INT.PND, id) ) {
        //masked or not cleared
        return 0;
    }
    INT.PND |= 1 << id;
    interrupt_exception(INT_EXCEPTION_IRQ);
    return 1;
}

/******************************interrupt**************************************/

/*******************************timer*****************************************/

struct timer_register {
    uint32_t CNT; //Determine which interrupt source is masked.
    uint32_t EN; //Indicate the interrupt request status
};

struct timer_register TIM;

#define   tim_reset()      do{\
 TIM.CNT = 0x00000000;\
 TIM.EN =  0x00000000;\
}while(0)

/*******************************timer*****************************************/

/*******************************uart*****************************************/

struct uart_register {
    uint32_t DLL; //Divisor Latch Low, 1
    uint32_t DLH; //Divisor Latch High, 2
    uint32_t IER; //Interrupt Enable Register, 1
    uint32_t IIR; //Interrupt Identity Register, 2
    uint32_t FCR; //FIFO¿ØÖÆ¼Ä´æÆ÷, 2
    uint32_t LCR; //Line Control Register, 3
    uint32_t MCR; //Modem Control Register, 4
    uint32_t LSR; //Line Status Register, 5
    uint32_t MSR; //Modem Status Registe, 6
    uint32_t SCR;
    uint32_t RBR;
};

struct uart_register UART[1];

void uart_8250_reset(void) 
{
    memset(UART, 0, sizeof(UART));
    UART[0].IIR = 0x01;
    UART[0].LSR = 0x60;
}

#define UART_LCR_DLAB(uart)   ((uart)->LCR & 0x80)
#define  register_set(r,b,v)  do{ if(v) {r |= 1 << (b);} else {r &= ~(1 << (b));} }while(0)

uint8_t uart_8250_read(struct uart_register *uart, uint8_t address)
{
    PRINTF("uart read 0x%x\n", address);
    switch(address) {
    case 0x0:
        if(UART_LCR_DLAB(uart)) {
            //Divisor Latch Low
            return uart->DLL;
        } else {
            //Receive Buffer Register
            register_set(UART[0].LSR, 0, 0);
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
    default:
        PRINTF("uart read %x\n", address);
    }
    return 0;
}

void uart_8250_write(struct uart_register *uart, uint8_t address, uint8_t data)
{
    PRINTF("uart write 0x%x 0x%x\n", address, data);
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
            register_set(uart->MSR, 7, (uart->MCR&0x8) ); //¸¨ÖúÊä³ö2
            register_set(uart->MSR, 6, (uart->MCR&0x4) ); //¸¨ÖúÊä³ö1
            register_set(uart->MSR, 5, (uart->MCR&0x1) ); //¸¨ÖúDTR
            register_set(uart->MSR, 4, (uart->MCR&0x2) ); //¸¨ÖúRTS
            
            register_set(uart->MSR, 3, (uart->MCR&0x8) ); //¸¨ÖúÊä³ö2
            register_set(uart->MSR, 2, (uart->MCR&0x4) ); //¸¨ÖúÊä³ö1
            register_set(uart->MSR, 1, (uart->MCR&0x1) ); //¸¨ÖúDTR
            register_set(uart->MSR, 0, (uart->MCR&0x2) ); //¸¨ÖúRTS
            //printf("Loopback msr = 0x%x\n", uart->MSR);
        } else {
            uart->MSR = 0;
            register_set(uart->MSR, 5, 1);  //DSR×¼±¸¾ÍÐ÷
            register_set(uart->MSR, 4, 1);  //CTSÓÐÐ§
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
        PRINTF("uart write %x\n", address);
    }
}



/*******************************uart*****************************************/


/*  memory */
#define  MMU_EXCEPTION_ADDR      0x4001f050
#define  is_privileged            (cpsr_m != CPSR_M_USR)

#define  read_word_without_mmu(a)       read_mem(0,a,0,3)
#define  read_word(m,a)                 read_mem(is_privileged,a,1,3)
#define  read_halfword(m,a)             (read_mem(is_privileged,a,1,1) & 0xffff)
#define  read_byte(m,a)                 (read_mem(is_privileged,a,1,0) & 0xff)

#define  read_word_mode(m,a)                 read_mem(m,a,1,3)
#define  read_halfword_mode(m,a)             (read_mem(m,a,1,1) & 0xffff)
#define  read_byte_mode(m,a)                 (read_mem(m,a,1,0) & 0xff)


static inline uint32_t read_mem(uint8_t privileged, uint32_t address, uint8_t mmu, uint8_t mask)
{
    if(address&mask) {
        //Check address alignment
        printf("memory address alignment error 0x%x\n", address);
        exception_out();
    }
    
    if(mmu) {
        address = mmu_transfer(address, privileged, 0); //read
    }
    
    if( (address>>20) ==0x400) {
        //1M Peripheral memory
        if(address == 0x4001f040) {
            return INT.MSK;
        } else if(address == 0x4001f044) {
            return INT.PND;
        } else if(address == 0x4001f000) {
            return 1;
        } else if(address == 0x4001f020) {
            //sys clock ms
            return GET_TICK();
        } else if(address == 0x4001f024) {
            //timer
            return TIM.EN;
        } else if(address == 0x4001f030) {
            //code counter
            return code_counter;
        } else if(address == MMU_EXCEPTION_ADDR) {
            return 0xffffffff;
        } else if( (address&0xfffffc00) == 0x40020000) {
            //uart
            return uart_8250_read(&UART[0], (uint8_t)address);
        }
    }
    
    if(address >= MEM_SIZE) {
        printf("mem overflow error, read 0x%x\r\n", address);
        exception_out();
    }
    
    return *((int*)(MEM + address));
}

#define  write_word(m,a,d)         write_mem(is_privileged,a,d,3)
#define  write_halfword(m,a,d)     write_mem(is_privileged,a,(d)&0xffff,1)
#define  write_byte(m,a,d)         write_mem(is_privileged,a,(d)&0xff,0)

#define  write_word_mode(m,a,d)         write_mem(m,a,d,3)
#define  write_halfword_mode(m,a,d)     write_mem(m,a,(d)&0xffff,1)
#define  write_byte_mode(m,a,d)         write_mem(m,a,(d)&0xff,0)

static inline void write_mem(uint8_t privileged, uint32_t address, uint32_t data,  uint8_t mask)
{
    if(address&mask) {
        //Check address alignment
        printf("memory address alignment error 0x%x\n", address);
        exception_out();
    }
    
    address = mmu_transfer(address, privileged, 1); //write
    
    if( (address>>20) ==0x400) {
        //1M Peripheral memory
        if(address == 0x4001f004) {
            PUTCHAR(data);
            return;
        } else if(address == 0x4001f040) {
            INT.MSK = data;
            return;
        } else if(address == 0x4001f044) {
            INT.PND = data;
            return;
        } else  if(address == 0x4001f030) {
            //ips
            code_counter = data;
            return;
        } else if(address == 0x4001f024) {
            //timer
            TIM.EN = data;
            return;
        } else if(address == MMU_EXCEPTION_ADDR) {
            return;
        } else if( (address&0xfffffc00) == 0x40020000) {
            //uart
            uart_8250_write(&UART[0], (uint8_t)address, (uint8_t)data);
            return;
        }
    }
    
    if(address >= MEM_SIZE) {
        printf("mem error, write %d = 0x%0x\r\n", mask, address);
        exception_out();
    }
    
    if(mask == 3) {
        int *data_p = (int *) (MEM + address);
        *data_p = data;
    } else if(mask == 1) {
        short *data_p = (short *) (MEM + address);
        *data_p = data;
    } else {
        char * data_p = (char *) (MEM + address);
        *data_p = data;
    }
}
/*  memory */


/***cp15***************************************************************/

/*
 *  CP15
 */
static uint32_t Register_cp15[16] = {0,};

#define   cp15_ctl            Register_cp15[1]
#define   cp15_ttb            Register_cp15[2]
#define   cp15_domain         Register_cp15[3]
/* Fault Status Register, [7:4]domain  [3:0]status */
#define   cp15_fsr            Register_cp15[5]
/* Fault Address Register */
#define   cp15_far            Register_cp15[6]



#define   cp15_read(r)      Register_cp15[r]
#define   cp15_write(r,d)   Register_cp15[r] = d
#define   cp15_reset()      memset(Register_cp15, 0, sizeof(Register_cp15))


//mmu enable
#define  cp15_ctl_m  IS_SET(cp15_ctl, 0)
//Alignment fault checking
#define  cp15_ctl_a  IS_SET(cp15_ctl, 1)
//System protection bit
#define  cp15_ctl_s  IS_SET(cp15_ctl, 8)
//ROM protection bit
#define  cp15_ctl_r  IS_SET(cp15_ctl, 9)
//high vectors
#define  cp15_ctl_v  IS_SET(cp15_ctl, 13)

uint8_t mmu_fault = 0;

/*
 *  return 1, mmu exception
 */
static inline uint8_t mmu_check_status(void)
{
    return mmu_fault;
}


/*
 *  uint8_t wr:                write: 1, read: 0
 *  uint8_t privileged:   privileged: 1, user: 0
 */
static inline int mmu_check_access_permissions(uint8_t ap, uint8_t privileged, uint8_t wr)
{
    switch(ap) {
    case 0:
        if(!cp15_ctl_s && cp15_ctl_r && !wr)
            return 0;
        if(cp15_ctl_s && !cp15_ctl_r && !wr && privileged)
            return 0;
        if(cp15_ctl_s && cp15_ctl_r) {
            printf("mmu_check_access_permissions fault\r\n");
            exception_out();
        }
        break;
    case 1:
        if(privileged)
            return 0;
        break;
    case 2:
        if(privileged || !wr)
            return 0;
        break;
    case 3:
        return 0;
    }
    return -1; //No access
}


/*
 *  uint8_t wr:                write: 1, read: 0
 *  uint8_t privileged:   privileged: 1, user: 0
 */
static inline uint32_t mmu_transfer(uint32_t vaddr, uint8_t privileged, uint8_t wr)
{
    mmu_fault = 0;
    if(!cp15_ctl_m) {
        return vaddr;
    }
    if(cp15_ctl_a) {
        //Check address alignment
        
    }
    //section page table, store size 16KB
    uint32_t page_table_entry = read_word_without_mmu( (cp15_ttb&0xFFFFC000)|((vaddr&0xFFF00000) >> 18));
    uint8_t first_level_descriptor = page_table_entry&3;
    if(first_level_descriptor == 0) {
        //Section translation fault
        cp15_fsr = 0x5;
        cp15_far = vaddr;
        mmu_fault = 1;
        PRINTF("Section translation fault va = 0x%x\r\n", vaddr);
        //exception_out();
    } else if(first_level_descriptor == 2) {
        //section entry
        uint8_t domain = (page_table_entry>>5)&0xf; //Domain field
        uint8_t domainval = domain << 1;
        domainval = (cp15_domain >> domainval)&0x3;
        if(domainval == 0) {
            //Section domain fault
            cp15_fsr = (domain << 4) | 0x9;
            cp15_far = vaddr;
            mmu_fault = 1;
            printf("Section domain fault\r\n");
            //getchar();
        } else if(domainval == 1) {
            //Client, Check access permissions
            uint8_t ap = (page_table_entry>>10)&0x3;
            if(mmu_check_access_permissions(ap, privileged, wr) == 0) {
                return (page_table_entry&0xFFF00000)|(vaddr&0x000FFFFF);
            } else {
                //Section permission fault
                cp15_fsr = (domain << 4) | 0xd;
                cp15_far = vaddr;
                mmu_fault = 1;
                printf("Section permission fault\r\n");
                //getchar();
            }
        } else if(domainval == 3) {
            //Manager
            return (page_table_entry&0xFFF00000)|(vaddr&0x000FFFFF);
        } else {
            printf("mmu fault\r\n");
            exception_out();
        }
        
    } else {
        //page table
        uint8_t domain = (page_table_entry>>5)&0xf; //Domain field
        uint8_t domainval =  domain << 1;
        domainval = (cp15_domain >> domainval)&0x3;
        if(first_level_descriptor == 1) {
            //coarse page table, store size 1KB
            page_table_entry = read_word_without_mmu( (page_table_entry&0xFFFFFC00)|((vaddr&0x000FF000) >> 10));
        } else if(first_level_descriptor == 3) {
            //fine page table, store size 4KB
            page_table_entry = read_word_without_mmu( (page_table_entry&0xFFFFF000)|((vaddr&0x000FFC00) >> 8));
        } else {
            printf("mmu fault\r\n");
            exception_out();
        }
        uint8_t second_level_descriptor = page_table_entry&3;
        if(second_level_descriptor == 0) {
            //Page translation fault
            cp15_fsr = (domain << 4) | 0x7;
            cp15_far = vaddr;
            mmu_fault = 1;
            PRINTF("Page translation fault va = 0x%x %d %d pte = 0x%x\r\n",
             vaddr, privileged, wr, page_table_entry);
            //exception_out();
        } else {
            if(domainval == 0) {
                //Page domain fault
                cp15_fsr = (domain << 4) | 0xb;
                cp15_far = vaddr;
                mmu_fault = 1;
                printf("Page domain fault\r\n");
                //getchar();
            } else if(domainval == 1) {
                //Client, Check access permissions
                uint8_t ap;
                switch(second_level_descriptor) {
                case 1:
                {
                    break;
                    //large page, 64KB
                    uint8_t subpage = (vaddr >> 14)&3;
                    subpage <<= 1;
                    ap = (page_table_entry >> (subpage+4))&0x3;
                    if(mmu_check_access_permissions(ap, privileged, wr) == 0) {
                        return (page_table_entry&0xFFFF0000)|(vaddr&0x0000FFFF);
                    } else {
                        //Sub-page permission fault
                        cp15_fsr = (domain << 4) | 0xf;
                        cp15_far = vaddr;
                        mmu_fault = 1;
                        PRINTF("Large Sub-page permission fault va = 0x%x %d %d %d\r\n",
                         vaddr, ap, privileged, wr);
                        //exception_out();
                    }
                }
                case 2:
                {
                    //small page, 4KB
                    uint8_t subpage = (vaddr >> 10)&3;
                    subpage <<= 1;
                    ap = (page_table_entry >> (subpage+4))&0x3;
                    if(mmu_check_access_permissions(ap, privileged, wr) == 0) {
                        return (page_table_entry&0xFFFFF000)|(vaddr&0x00000FFF);
                    } else {
                        //Sub-page permission fault
                        cp15_fsr = (domain << 4) | 0xf;
                        cp15_far = vaddr;
                        mmu_fault = 1;
                        PRINTF("Small Sub-page permission fault va = 0x%x %d %d %d\r\n",
                         vaddr, ap, privileged, wr);
                        //exception_out();
                    }
                }
                case 3:
                    //tiny page, 1KB
                    ap = (page_table_entry>>4)&0x3; //ap0
                    if(mmu_check_access_permissions(ap, privileged, wr) == 0) {
                        return (page_table_entry&0xFFFFFC00)|(vaddr&0x000003FF);
                    } else {
                        //Sub-page permission fault
                        cp15_fsr = (domain << 4) | 0xf;
                        cp15_far = vaddr;
                        mmu_fault = 1;
                        PRINTF("Tiny Sub-page permission fault va = 0x%x %d %d %d\r\n",
                         vaddr, ap, privileged, wr);
                        //exception_out();
                    }
                }
                
            } else if(domainval == 3) {
                //Manager
                switch(second_level_descriptor) {
                case 1:
                    //large page, 64KB
                    return (page_table_entry&0xFFFF0000)|(vaddr&0x0000FFFF);
                case 2:
                    //small page, 4KB
                    return (page_table_entry&0xFFFFF000)|(vaddr&0x00000FFF);
                case 3:
                    //tiny page, 1KB
                    return (page_table_entry&0xFFFFFC00)|(vaddr&0x000003FF);
                }
            } else {
                printf("mmu fault\r\n");
                exception_out();
            }
        }
    }
    return MMU_EXCEPTION_ADDR;
}

/****cp15 end***********************************************************/


//load_program_memory reads the input memory, and populates the instruction 
// memory
uint32_t load_program_memory(char *file_name, uint32_t start)
{
    FILE *fp;
    unsigned int address, instruction;
    fp = fopen(file_name, "rb");
    if(fp == NULL) {
        printf("Error opening input mem file\n");
        exception_out();
    }
    address = start;
    while(!feof(fp)) {
        fread(&instruction, 4, 1, fp);
        write_word(MEM, address, instruction);
        address = address + 4;
    }
    printf("load mem start 0x%x, size 0x%x \r\n", start, address - start - 4);
    fclose(fp);
    return address - start - 4;
}


void interrupt_exception(uint8_t type)
{
    uint32_t cpsr_int = cpsr;
    uint32_t next_pc = register_read(15) - 4;  //lr 
    PRINTF("[INT %d]cpsr(0x%x) save to spsr, next_pc(0x%x) save to r14_pri \r\n", type, cpsr_int, next_pc);
    switch(type) {
    case INT_EXCEPTION_IRQ:
        //irq
        if(cpsr_i) {
            return;  //irq disable
        }
        PRINTF("IRQ\r\n");
        register_write(15, 0x18|(cp15_ctl_v?0xffff0000:0));
        cpsr_i_set(1);  //disable irq
        cpsr_t_set(0);  //arm mode
        cpsr &= ~0x1f;
        cpsr |= CPSR_M_IRQ;
        spsr[CPU_MODE_IRQ] = cpsr_int;  //write SPSR_irq
        register_write(14, next_pc + 4);  //write R14_irq
        break;
    case INT_EXCEPTION_PREABT:
        //preAbt
        PRINTF("PREABT\r\n");
        register_write(15, 0xc|(cp15_ctl_v?0xffff0000:0));
        cpsr_i_set(1);  //disable irq
        cpsr_t_set(0);  //arm mode
        cpsr &= ~0x1f;
        cpsr |= CPSR_M_ABT;
        spsr[CPU_MODE_Abort] = cpsr_int;  //write SPSR_abt
        register_write(14, next_pc + 0);  //write R14_abt
        break;
    case INT_EXCEPTION_SWI:
        //swi
        PRINTF("SVC\r\n");
        register_write(15, 0x8|(cp15_ctl_v?0xffff0000:0));
        cpsr_i_set(1);  //disable irq
        cpsr_t_set(0);  //arm mode
        cpsr &= ~0x1f;
        cpsr |= CPSR_M_SVC;
        spsr[CPU_MODE_SVC] = cpsr_int;  //write SPSR_svc
        register_write(14, next_pc);  //write R14_svc
        break;
    case INT_EXCEPTION_DATAABT:
        //dataAbt
        PRINTF("DATAABT\r\n");
        register_write(15, 0x10|(cp15_ctl_v?0xffff0000:0));
        cpsr_i_set(1);  //disable irq
        cpsr_t_set(0);  //arm mode
        cpsr &= ~0x1f;
        cpsr |= CPSR_M_ABT;
        spsr[CPU_MODE_Abort] = cpsr_int;  //write SPSR_abt
        register_write(14, next_pc + 4);  //write R14_abt
        break;
    default:
        printf("unknow interrupt\r\n");
        exception_out();
    }
}


void fetch(void)
{
    uint32_t pc = register_read(15) - 4; //current pc
    if(((pc&3) != 0)) {
        PRINTF("[FETCH] error, pc[0x%x] overflow \r\n", pc);
        exception_out();
    }
    
    instruction_word = read_word(MEM, pc);
    PRINTF("[0x%04x]: ", pc);
    register_write(15, pc + 4 ); //current pc add 4
}

#define  code_is_swi      1
#define  code_is_b        2
#define  code_is_ldm      3
#define  code_is_ldr1     4
#define  code_is_ldr0     5
#define  code_is_dp2      6
#define  code_is_msr1     7
#define  code_is_ldrsh1   8
#define  code_is_ldrsh0   9
#define  code_is_ldrsb1  10
#define  code_is_ldrsb0  11
#define  code_is_ldrh1   12
#define  code_is_ldrh0   13
#define  code_is_swp     14
#define  code_is_multl   15
#define  code_is_mult    16
#define  code_is_dp1     17
#define  code_is_bx      18
#define  code_is_dp0     19
#define  code_is_msr0    20
#define  code_is_mrs     21
#define  code_is_mcr     22
#define  code_is_unknow  255


static unsigned char code_type = 0;

static unsigned char Rn = 0;
static unsigned char Rd = 0;
static unsigned char Rs = 0;
static unsigned char Rm = 0;
static unsigned short immediate = 0;
static signed int b_offset = 0;
static unsigned char opcode = 0;
static unsigned char shift = 0;
static unsigned char rotate_imm = 0;
static unsigned char shift_amount = 0;
static unsigned char cond = 0;
static unsigned char Lf = 0, Bf = 0, Uf = 0, Pf = 0, Wf = 0;
static unsigned char Bit22 = 0, Bit20 = 0, Bit7 = 0, Bit4 = 0;
static unsigned char Bit24_23 = 0;


uint8_t decode(void)
{
    uint8_t cond_satisfy = 0;
    
    cond = (instruction_word >> 28)&0xf;  /* bit[31:28] */
    switch(cond) {
    case 0x0:
        cond_satisfy = (cpsr_z == 1);
        break;
    case 0x1:
        cond_satisfy = (cpsr_z == 0);
        break;
    case 0x2:
        cond_satisfy = (cpsr_c == 1);
        break;
    case 0x3:
        cond_satisfy = (cpsr_c == 0);
        break;
    case 0x4:
        cond_satisfy = (cpsr_n == 1);
        break;
    case 0x5:
        cond_satisfy = (cpsr_n == 0);
        break;
    case 0x6:
        cond_satisfy = (cpsr_v == 1);
        break;
    case 0x7:
        cond_satisfy = (cpsr_v == 0);
        break;
    case 0x8:
        cond_satisfy = ((cpsr_c == 1) & (cpsr_z == 0));
        break;
    case 0x9:
        cond_satisfy = ((cpsr_c == 0) | (cpsr_z == 1));
        break;
    case 0xa:
        cond_satisfy = (cpsr_n == cpsr_v);
        break;
    case 0xb:
        cond_satisfy = (cpsr_n != cpsr_v);
        break;
    case 0xc:
        cond_satisfy = ((cpsr_z == 0) & (cpsr_n == cpsr_v));
        break;
    case 0xd:
        cond_satisfy = ((cpsr_z == 1) | (cpsr_n != cpsr_v));
        break;
    case 0xe:
        cond_satisfy = 1;
        break;
    case 0xf:
        cond_satisfy = 0;
        break;
    default:
        ;
    }
    
    if(!cond_satisfy) {
        //PRINTF("cond_satisfy = %d skip...\r\n", cond_satisfy);
        return cond_satisfy;
    }
    
    unsigned char f = 0;
    f = (instruction_word >> 25)&0x7;  /* bit[27:25] */
    opcode = (instruction_word >> 21)&0xf;  /* bit[24:21] */
    Bit24_23 = opcode >> 2;  /* bit[24:23] */
    Bit22 = IS_SET(instruction_word, 22);  /* bit[22] */
    Bit20 = IS_SET(instruction_word, 20);  /* bit[20] */
    Rn = (instruction_word >> 16)&0xf;  /* bit[19:16] */
    Rd = (instruction_word >> 12)&0xf;  /* bit[15:12] */
    Rs = (instruction_word >> 8)&0xf;  /* bit[11:8] */
    rotate_imm = Rs;  /* bit[11:8] */
    shift_amount = (instruction_word >> 7)&0x1f;  /* bit[11:7] */
    shift = (instruction_word >> 5)&0x3;  /* bit[6:5] */
    Bit4 = IS_SET(instruction_word, 4);  /* bit[4] */
    Rm = (instruction_word&0xF);  /* bit[3:0] */
    code_type = code_is_unknow;
    
    switch(f) {
    case 0:
        switch(Bit4) {
        case 0:
            if( (Bit24_23 == 2 ) && !Bit20 ) {
                //Miscellaneous instructions
                unsigned char Bit21 = (instruction_word&0x200000) >> 21;  /* bit[21] */
                unsigned char Bit7 = (instruction_word&0x80) >> 7;  /* bit[7] */
                if(Bit21 && !Bit7) {
                    code_type = code_is_msr0;
                    PRINTF("msr0 MSR %sPSR_%d  R%d\r\n", Bit22?"S":"C", Rn, Rm);
                } else if(!Bit21 && !Bit7) {
                    code_type = code_is_mrs;
                    PRINTF("mrs MRS R%d %s\r\n", Rd, Bit22?"SPSR":"CPSR");
                } else {
                    code_type = code_is_unknow;
                    printf(" undefed bit7 error \r\n");
                }
            } else {
                //Data processing register shift by immediate
                code_type = code_is_dp0;
                PRINTF("dp0 %s dst register R%d, first operand R%d, Second operand R%d[0x%x] %s immediate #0x%x\r\n",
                 opcode_table[opcode], Rd, Rn, Rm, register_read(Rm), shift_table[shift], shift_amount);
            }
            break;
        case 1:
            Bit7 = (instruction_word&0x80) >> 7;  /* bit[7] */
            switch(Bit7) {
            case 0:
                if( (Bit24_23 == 2 ) && !Bit20 ) {
                    //Miscellaneous instructions
                    if(!IS_SET(instruction_word, 22) && IS_SET(instruction_word, 21) && !IS_SET(instruction_word, 6) ) {
                        //Branch/exchange
                        code_type = code_is_bx;
                        Lf = IS_SET(instruction_word, 5); //L bit
                        PRINTF("bx B%sX R%d\r\n", Lf?"L":"", Rm);
                    } else if(IS_SET(instruction_word, 6) && !IS_SET(instruction_word, 5) ) {
                        //Enhanced DSP add/subtracts
                        code_type = code_is_unknow;
                        printf("Enhanced DSP add/subtracts R%d\r\n", Rm);
                    } else if(IS_SET(instruction_word, 6) && IS_SET(instruction_word, 5)) {
                        //Software breakpoint
                        code_type = code_is_unknow;
                        printf("Software breakpoint \r\n");
                    } else if(!IS_SET(instruction_word, 6) && !IS_SET(instruction_word, 5) && opcode == 0xb) {
                        //Count leading zero
                        code_type = code_is_unknow;
                        printf("Count leading zero \r\n");
                    } else {
                        code_type = code_is_unknow;
                        printf("Undefed Miscellaneous instructions\r\n");
                    }
                } else {
                    //Data processing register shift by register
                    code_type = code_is_dp1;
                    PRINTF("dp1 %s dst register R%d, first operand R%d, Second operand R%d %s R%d\r\n",
                     opcode_table[opcode], Rd, Rn, Rm, shift_table[shift], Rs);
                }
                break;
            case 1:
                //Multiplies, extra load/stores
                Lf = IS_SET(instruction_word, 20); //L bit
                Bf = IS_SET(instruction_word, 22); //B bit
                Uf = IS_SET(instruction_word, 23);  //U bit
                Pf = (opcode >> 3) & 1; //P bit[24]
                Wf = opcode & 1;  //W bit[21]
                switch(shift) {
                case 0:
                    if(Bit24_23 == 0) {
                        code_type = code_is_mult;
                        if( IS_SET(instruction_word, 21) ) {
                            PRINTF("mult MUA%s R%d <= R%d * R%d + R%d\r\n", Bit20?"S":"", Rn, Rm, Rs, Rd);
                        } else {
                            PRINTF("mult MUL%s R%d <= R%d * R%d\r\n", Bit20?"S":"", Rn, Rm, Rs);
                        }
                    } else if(Bit24_23 == 1) {
                        code_type = code_is_multl;
                        PRINTF("multl %sM%s [R%d L, R%d H] %s= R%d * R%d\r\n", Bit22?"S":"U",
                         IS_SET(instruction_word, 21)?"LAL":"ULL", Rd, Rn, IS_SET(instruction_word, 21)?"+":"", Rm, Rs);
                    } else if(Bit24_23 == 2) {
                        code_type = code_is_swp;
                        PRINTF("swp \r\n");
                    }
                    break;
                case 1:
                    //code_is_ldrh
                    if(Bit22) {
                        code_type = code_is_ldrh1;
                        immediate = (Rs << 4) | Rm;
                        if(Pf) {
                            PRINTF("ldrh1 %sH R%d, [R%d, immediate %s#%d] %s\r\n",
                             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
                        } else {
                            PRINTF("ldrh1 %sH R%d, [R%d], immediate %s#%d %s\r\n",
                             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
                        }
                    } else {
                        code_type = code_is_ldrh0;
                        if(Pf) {
                            PRINTF("ldrh0 %sH R%d, [R%d, %s[R%d] ] %s\r\n",
                             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
                        } else {
                            PRINTF("ldrh0 %sH R%d, [R%d], %s[R%d] %s\r\n",
                             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
                        }
                    }
                    break;
                case 2:
                    //code_is_ldrsb
                    if(Bit22) {
                        code_type = code_is_ldrsb1;
                        immediate = (Rs << 4) | Rm;
                        if(Pf) {
                            PRINTF("ldrsb1 %sSB R%d, [R%d, immediate %s#%d] %s\r\n",
                             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
                        } else {
                            PRINTF("ldrsb1 %sSB R%d, [R%d], immediate %s#%d %s\r\n",
                             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
                        }
                    } else {
                        code_type = code_is_ldrsb0;
                        if(Pf) {
                            PRINTF("ldrsb0 %sSB R%d, [R%d, %s[R%d] ] %s\r\n",
                             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
                        } else {
                            PRINTF("ldrsb0 %sSB R%d, [R%d], %s[R%d] %s\r\n",
                             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
                        }
                    }
                    if(!Lf) {
                        printf("undefed LDRD\r\n");
                        getchar();
                    }
                    break;
                case 3:
                    //code_is_ldrsh
                    if(Bit22) {
                        code_type = code_is_ldrsh1;
                        immediate = (Rs << 4) | Rm;
                        if(Pf) {
                            PRINTF("ldrsh1 %sSH R%d, [R%d, immediate %s#%d] %s\r\n",
                             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
                        } else {
                            PRINTF("ldrsh1 %sSH R%d, [R%d], immediate %s#%d %s\r\n",
                             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
                        }
                    } else {
                        code_type = code_is_ldrsh0;
                        if(Pf) {
                            PRINTF("ldrsh0 %sSH R%d, [R%d, %s[R%d] ] %s\r\n",
                             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
                        } else {
                            PRINTF("ldrsh0 %sSH R%d, [R%d], %s[R%d] %s\r\n",
                             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
                        }
                    }
                    if(!Lf) {
                        printf("undefed STRD\r\n");
                        getchar();
                    }
                    break;
                default:
                    code_type = code_is_unknow;
                    printf("undefed shift\r\n");
                }
                break;
            default:
                code_type = code_is_unknow;
                printf("undefed Bit7\r\n");
            }
            break;
        default:
            code_type = code_is_unknow;
            printf("undefed Bit4\r\n");
        }
        break;
    case 1:
        //Data processing immediate and move immediate to status register
        if( (opcode == 0x9 || opcode == 0xb ) && !Bit20 ) {
            code_type = code_is_msr1;
            immediate = instruction_word&0xff;
            PRINTF("msr1 MSR %sPSR_%d  immediate [#%d ROR %d]\r\n", Bit22?"S":"C", Rn, immediate, rotate_imm*2);
        } else if( (opcode == 0x8 || opcode == 0xa ) && !Bit20 ) {
            PRINTF("undefined instruction\r\n");
        } else {
            //Data processing immediate
            code_type = code_is_dp2;
            immediate = instruction_word&0xff;
            PRINTF("dp2 %s dst register R%d, first operand R%d, Second operand immediate [#%d ROR %d]\r\n",
             opcode_table[opcode], Rd, Rn, immediate, rotate_imm*2);
        }
        break;
    case 2:
        //load/store immediate offset
        code_type = code_is_ldr0;
        immediate = instruction_word&0xFFF;
        Lf = IS_SET(instruction_word, 20); //L bit
        Bf = IS_SET(instruction_word, 22); //B bit
        Uf = IS_SET(instruction_word, 23); //U bit
        Pf = (opcode >> 3) & 1; //P bit[24]
        Wf = opcode & 1;  //W bit[21]
        if(Pf) {
            PRINTF("ldr0 %s%s dst register R%d, [base address at R%d, Second address immediate #%s0x%x] %s\r\n",
             Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
        } else {
            PRINTF("ldr0 %s%s dst register R%d, [base address at R%d], Second address immediate #%s0x%x\r\n",
             Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-", immediate);
        }
        
        break;
    case 3:
        //load/store register offset
        if(!Bit4) {
            code_type = code_is_ldr1;
            Lf = IS_SET(instruction_word, 20); //L bit
            Bf = IS_SET(instruction_word, 22); //B bit
            Uf = IS_SET(instruction_word, 23); //U bit
            Pf = (opcode >> 3) & 1; //P bit[24]
            Wf = opcode & 1;  //W bit[21]
            if(Pf) {
                PRINTF("ldr1 %s%s dst register R%d, [base address at R%d, offset address at %s[R%d(0x%x) %s %d]] %s\r\n",
                 Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-",  Rm, register_read(Rm), shift_table[shift], shift_amount, Wf?"!":"");
            } else {
                PRINTF("ldr1 %s%s dst register R%d, [base address at R%d], offset address at %s[R%d %s %d] \r\n",
                 Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-",  Rm, shift_table[shift], shift_amount);
            }
            
        } else {
            code_type = code_is_unknow;
            printf("undefined instruction\r\n");
        }
        break;
    case 4:
        //load/store multiple
        code_type = code_is_ldm;
        Lf = IS_SET(instruction_word, 20); //L bit
        Uf = IS_SET(instruction_word, 23);  //U bit
        Pf = opcode >> 3;  //P bit
        Wf = opcode & 1;
        PRINTF("ldm %s%s%s R%d%s \r\n", Lf?"LDM":"STM", Uf?"I":"D", Pf?"B":"A",  Rn, Wf?"!":"");
        Rn = (instruction_word >> 16)&0xf;
        break;
    case 5:
        //branch
        code_type = code_is_b;
        if( (instruction_word&0xFFFFFF) >> 23) {
            //-
            b_offset = (instruction_word&0x7FFFFF)|0xFF800000;
        } else {
            //+
            b_offset = instruction_word&0x7FFFFF;
        }
        b_offset <<= 2;
        Lf = IS_SET(instruction_word, 24); //L bit
        PRINTF("B%s PC[0x%x] +  0x%08x = 0x%x\r\n",
         Lf?"L":"", register_read(15), b_offset, register_read(15) + b_offset );
        break;
    case 6:
        //Coprocessor load/store and double register transfers
        code_type = code_is_unknow;
        printf("Coprocessor todo... \r\n");
        exception_out();
        break;
    case 7:
        //software interrupt
        if(IS_SET(instruction_word, 24)) {
            code_type = code_is_swi;
            PRINTF("swi \r\n");
        } else {
            if(Bit4) {
                //Coprocessor move to register
                code_type = code_is_mcr;
                PRINTF("mcr \r\n");
            } else {
                printf("Coprocessor data processing todo... \r\n");
                exception_out();
            }
        }
        break;
    default:
        code_type = code_is_unknow;
        printf("undefed\r\n");
        exception_out();
    }
    return cond_satisfy;
}


static uint8_t swi_flag = 0;

void execute(void)
{
    uint32_t operand1 = register_read(Rn);
    uint32_t operand2 = register_read(Rm);
    uint32_t rot_num = register_read(Rs);
    unsigned char add_flag = 1;
    /*
     * 0: turn off shifter
     * 3: PD(is) On
     * 4: DP(rs) On
     * 5: DP(i) On
    */
    unsigned char shifter_flag = 0;
    uint32_t carry = 0;
    
    if(code_type == code_is_dp2) {
        //immediate
        operand2 = immediate;
        rot_num = rotate_imm<<1;
        shift = 3;  //use ROR Only
        shifter_flag = 5;  //DP(i) On
    } else if(code_type == code_is_b) {
        operand1 = b_offset;
        operand2 = register_read(15);
        rot_num = 0;
        shifter_flag = 0;  //no need to use shift
    } else if(code_type == code_is_ldr0 || code_type == code_is_ldrh1 || code_type == code_is_ldrsb1 || code_type == code_is_ldrsh1) {
        operand2 = immediate;
        //no need to use shift
        shifter_flag = 0;
    } else if(code_type == code_is_dp0 || code_type == code_is_ldr1) {
        //immediate shift
        rot_num = shift_amount;
        shifter_flag = 3;  //PD(is) On
    } else if(code_type == code_is_ldrsb0 || code_type == code_is_ldrsh0 || code_type == code_is_ldrh0) {
        //no need to use shift
        shifter_flag = 0;
    } else if(code_type == code_is_msr1) {
        operand1 = 0;  //msr don't support
        operand2 = immediate;
        rot_num = rotate_imm<<1;
        shift = 3;
        shifter_flag = 5;  //DP(i) On
    } else if(code_type == code_is_msr0) {
        operand1 = 0;  //msr don't support
        //no need to use shift
        shifter_flag = 0;
    } else if(code_type == code_is_dp1) {
        //register shift
        //No need to use
        shifter_flag = 4;  //DP(rs) On
    } else if(code_type == code_is_bx) {
        operand1 = 0;
        rot_num = 0;
        //no need to use shift
        shifter_flag = 0;
    } else if(code_type == code_is_ldm) {
        operand2 = 0;  //increase After
        rot_num = 0;
        shifter_flag = 0;  //no need to use shift
    } else if(code_type == code_is_mult) {
        operand2 = operand2 * rot_num;
        shifter_flag = 0;  //no need to use shift
    } else if(code_type == code_is_multl) {
        uint64_t multl_long = 0;
        uint8_t negative = 0;
        if(Bit22) {
            //Signed
            if(IS_SET(operand2, 31)) {
                operand2 = ~operand2 + 1;
                negative++;
            }
            
            if(IS_SET(rot_num, 31)) {
                rot_num = ~rot_num + 1;
                negative++;
            }
        }
        
        multl_long = (uint64_t)operand2 * rot_num;
        if( negative == 1 ) {
            //Set negative sign
            multl_long = ~(multl_long - 1);
        }
        
        if(opcode&1) {
            multl_long += ( ((uint64_t)register_read(Rn) << 32) | register_read(Rd));
        }
        
        register_write(Rn, multl_long >> 32);
        register_write(Rd, multl_long&0xffffffff);
        PRINTF("write 0x%x to R%d, write 0x%x to R%d  = (0x%x) * (0x%x) \r\n", register_read(Rn), Rn, register_read(Rd), Rd, operand2, rot_num);
        
        if(Bit20) {
            //Update CPSR register
            uint8_t out_sign = IS_SET(multl_long, 63);
            
            cpsr_n_set(out_sign);
            cpsr_z_set(multl_long == 0);
            //cv unaffected
            PRINTF("update flag nzcv %d%d%d%d \r\n", cpsr_n, cpsr_z, cpsr_c, cpsr_v);
        }
        
        return;
    } else if(code_type == code_is_swi) {
        swi_flag = 1;
        return;
    } else if(code_type == code_is_mrs) {
        shifter_flag = 0;  //no need to use shift
    } else if(code_type == code_is_mcr) {
        shifter_flag = 0;  //no need to use shift
        uint8_t op2 = (instruction_word >> 5)&0x7;
        //  cp_num       Rs
        //  register     Rd
        //  cp_register  Rn
        if(Bit20) {
            //mrc
            if(Rs == 15) {
                uint32_t cp15_val;
                if(Rn) {
                    cp15_val = cp15_read(Rn);
                } else {
                    //register 0
                    if(op2) {
                        //Cache Type register
                        cp15_val = 0;
                        //printf("Cache Type register\r\n");
                    } else {
                        //Main ID register
                        cp15_val = (0x41 << 24) | (0x0 << 20) | (0x2 << 16) | (0x920 << 4) | 0x5;
                    }
                }
                register_write(Rd, cp15_val);
                PRINTF("read cp%d_c%d[0x%x] op2:%d to R%d \r\n", Rs, Rn, cp15_val, op2, Rd);
            } else {
                printf("read cp%d_c%d \r\n", Rs, Rn);
                getchar();
            }
        } else {
            //mcr
            uint32_t register_val = register_read(Rd);
            if(Rs == 15) {
                cp15_write(Rn, register_val);
                PRINTF("write R%d[0x%x] to cp%d_c%d \r\n", Rd, register_val, Rs, Rn);
            } else {
                printf("write to cp%d_c%d \r\n", Rs, Rn);
                getchar();
            }
        }
        return;
    } else if(code_type == code_is_swp) {
        if(operand1 & 3) {
            printf("swp error \r\n");
            getchar();
        }
        /* op1, Rn
         * op2, Rm
         */
        uint32_t tmp = read_word(MEM, operand1);
        if(mmu_check_status()) {
            return;
        }
        write_word(MEM, operand1, operand2);
        register_write(Rd, tmp);
        
        return;
    } else {
        printf("unknow code_type = %d", code_type);
        getchar();
    }
    
    /******************shift**********************/
    //operand2 << rot_num
    uint8_t shifter_carry_out = 0;
    if(shifter_flag) {
        switch(shift) {
        case 0:
            //LSL
            if(shifter_flag == 3) {
                //PD(is) On
                if(rot_num == 0) {
                    shifter_carry_out = cpsr_c;
                } else {
                    shifter_carry_out = IS_SET(operand2, 32 - rot_num);
                    operand2 = operand2 << rot_num;
                }
            } else if(shifter_flag == 4) {
                //DP(rs) On
                if((rot_num&0xff) == 0) {
                    shifter_carry_out = cpsr_c;
                } else if((rot_num&0xff) < 32) {
                    shifter_carry_out = IS_SET(operand2, 32 - rot_num);
                    operand2 = operand2 << rot_num;
                } else if((rot_num&0xff) == 32) {
                    shifter_carry_out = operand2&1;
                    operand2 = 0;
                } else /* operand2 > 32 */ {
                    shifter_carry_out = 0;
                    operand2 = 0;
                }
            }
            
            break;
        case 1:
            //LSR
            if(shifter_flag == 3) {
                //PD(is) On
                if(rot_num == 0) {
                    shifter_carry_out = IS_SET(operand2, 31);
                    operand2 = 0;
                } else {
                    shifter_carry_out = IS_SET(operand2, rot_num-1);
                    operand2 = operand2 >> rot_num;
                }
            } else if(shifter_flag == 4) {
                //DP(rs) On
                if((rot_num&0xff) == 0) {
                    shifter_carry_out = cpsr_c;
                } else if((rot_num&0xff) < 32) {
                    shifter_carry_out = IS_SET(operand2, rot_num-1);
                    operand2 = operand2 >> rot_num;
                } else if((rot_num&0xff) == 32) {
                    shifter_carry_out = IS_SET(operand2, 31);
                    operand2 = 0;
                } else /* operand2 > 32 */ {
                    shifter_carry_out = 0;
                    operand2 = 0;
                }           
            }

            break;
        case 2:
            //ASR
            if(shifter_flag == 3) {
                //PD(is) On
                if(rot_num == 0) {
                    shifter_carry_out = IS_SET(operand2, 31);
                    if(!IS_SET(operand2, 31)) {
                        operand2 = 0;
                    } else {
                        operand2 = 0xffffffff;
                    }
                } else {
                    shifter_carry_out = IS_SET(operand2, rot_num-1);
                    operand2 = (int32_t)operand2 >> rot_num;
                }
            } else if(shifter_flag == 4) {
                //DP(rs) On
                if((rot_num&0xff) == 0) {
                    shifter_carry_out = cpsr_c;
                } else if((rot_num&0xff) < 32) {
                    shifter_carry_out = IS_SET(operand2, rot_num-1);
                    operand2 = (int32_t)operand2 >> rot_num;
                } else /* operand2 >= 32 */ {
                    if(!IS_SET(operand2, 31)) {
                        shifter_carry_out = IS_SET(operand2, 31);
                        operand2 = 0;
                    } else {
                        shifter_carry_out = IS_SET(operand2, 31);
                        operand2 = 0xffffffff;
                    }
                }     
            }
            
            break;
        case 3:
            //ROR, RRX
            if(shifter_flag == 5) {
                //DP(i)
                operand2 = (operand2 << (32 - rot_num) | operand2 >> rot_num);
                if(rot_num == 0) {
                     shifter_carry_out = cpsr_c;
                } else {
                    shifter_carry_out = IS_SET(operand2, 31);
                }   
            } else if(shifter_flag == 3) {
                //PD(is) ROR On
                if(rot_num == 0) {
                    //RRX
                    shifter_carry_out = operand2&1;
                    operand2 = (cpsr_c << 31) | (operand2 >> 1);
                } else {
                    //ROR
                    shifter_carry_out = IS_SET(operand2, rot_num-1);
                    operand2 = (operand2 << (32 - rot_num) | operand2 >> rot_num);
                }
            } else if(shifter_flag == 4) {
                //DP(rs) ROR On
                if((rot_num&0xff) == 0) {
                    shifter_carry_out = cpsr_c;
                } else if((rot_num&0x1f) == 0) {
                    shifter_carry_out = IS_SET(operand2, 31);
                } else /* rot_num > 0 */ {
                    shifter_carry_out = IS_SET(operand2, rot_num-1);
                    operand2 = (operand2 << (32 - rot_num) | operand2 >> rot_num);
                }
            }
            
            break;
        }
    }
    
    
    /******************shift end*******************/
    
    if(code_type == code_is_dp0 || code_type == code_is_dp1 || code_type == code_is_dp2) {
        if(opcode == 0 || opcode == 8) {
            //AND, TST
            operand1 &= operand2;
            operand2 = 0;
        } else if(opcode == 1 || opcode == 9) {
            //EOR, TEQ
            operand1 ^= operand2;
            operand2 = 0;
        } else if(opcode == 2 || opcode == 10) {
            //SUB, CMP
            add_flag = 0;
        } else if(opcode == 3) {
            //RSB
            uint32_t tmp = operand1;
            operand1 = operand2;
            operand2 = tmp;
            add_flag = 0;
        } else if(opcode == 4 || opcode == 11) {
            //ADD, CMN
        } else if(opcode == 5) {
            //ADC
            carry = cpsr_c;  //todo...
        } else if(opcode == 6) {
            //SBC
            carry = !cpsr_c;  //todo...
            add_flag = 0;
        } else if(opcode == 7) {
            //RSC
            carry = !cpsr_c;  //todo...
            uint32_t tmp = operand1;
            operand1 = operand2;
            operand2 = tmp;
            add_flag = 0;
        } else if(opcode == 12) {
            //ORR
            operand1 |= operand2;
            operand2 = 0;
        } else if(opcode == 13) {
            //MOV
            operand1 = 0;
        } else if(opcode == 14) {
            //BIC
            operand1 &= ~operand2;
            operand2 = 0;
        } else if(opcode == 15) {
            //MVN
            operand1 = 0;
            operand2 = ~operand2;
        }
    } else if(code_type == code_is_ldr1 || code_type == code_is_ldr0 || 
            code_type == code_is_ldrsb1 || code_type == code_is_ldrsh1 || code_type == code_is_ldrh1 ||
            code_type == code_is_ldrsb0 || code_type == code_is_ldrsh0 || code_type == code_is_ldrh0 )
    {
        if(Uf) {
            add_flag = 1;
        } else {
            add_flag = 0;
        }
    } else if(code_type == code_is_mult) {
        if(opcode == 0) {
            operand1 = 0;
        } else if(opcode == 1) {
            operand1 = register_read(Rd);  //Rd and Rn swap, !!!
        }
    }
    
    /**************************alu*************************/
    
    uint32_t sum_middle = add_flag?((operand1&0x7fffffff) + (operand2&0x7fffffff) + carry):((operand1&0x7fffffff) - (operand2&0x7fffffff) - carry);
    uint32_t cy_high_bits =  add_flag?(IS_SET(operand1, 31) + IS_SET(operand2, 31) + IS_SET(sum_middle, 31)):(IS_SET(operand1, 31) - IS_SET(operand2, 31) - IS_SET(sum_middle, 31));
    uint32_t aluout = ((cy_high_bits&1) << 31) | (sum_middle&0x7fffffff);
    PRINTF("[ALU]op1: 0x%x, sf_op2: 0x%x, c: %d, out: 0x%x, ", operand1, operand2, carry, aluout);
    
    /**************************alu end*********************/

    if(code_type == code_is_dp0 || code_type == code_is_dp1 ||  code_type == code_is_dp2) {
        if(Bit24_23 == 2) {
            PRINTF("update flag only \r\n");
        } else {
            register_write(Rd, aluout);
            PRINTF("write register R%d = 0x%x\r\n", Rd, aluout);
        }
        if(Bit20) {
            //Update CPSR register
            uint8_t out_sign = IS_SET(aluout, 31);
            
            cpsr_n_set(out_sign);
            cpsr_z_set(aluout == 0);
            
            uint8_t bit_cy = (cy_high_bits>>1)&1;
            uint8_t bit_ov = (bit_cy^IS_SET(sum_middle, 31))&1;
            
            if(opcode == 11 || opcode == 4 || opcode == 5) {
                //CMN, ADD, ADC
                cpsr_c_set( bit_cy );
                cpsr_v_set( bit_ov );
            } else if(opcode == 10 || opcode == 2 || opcode == 6 || opcode == 3 || opcode == 7) {
                //CMP, SUB, SBC, RSB, RSC
                cpsr_c_set( !bit_cy );
                cpsr_v_set( bit_ov );
            } else if(shifter_flag) {
                //TST, TEQ, ORR, MOV, MVN
                cpsr_c_set( shifter_carry_out );
                //c shifter_carry_out
                //v unaffected
            }
            
            PRINTF("[DP] update flag nzcv %d%d%d%d \r\n", cpsr_n, cpsr_z, cpsr_c, cpsr_v);
            
            if(Rd == 15 && Bit24_23 != 2) {
                uint8_t cpu_mode = get_cpu_mode_code();
                cpsr = spsr[cpu_mode];
                PRINTF("[DP] cpsr 0x%x copy from spsr %d \r\n", cpsr, cpu_mode);
            }
        }
    } else if(code_type == code_is_bx || code_type == code_is_b) {
        if(Lf) {
            register_write(14, register_read(15) - 4);  //LR register
            PRINTF("write register R%d = 0x%0x, ", 14, register_read(14));
        }
        
        if(aluout&3) {
            printf("thumb unsupport \r\n");
            getchar();
        }
        register_write(15, aluout & 0xfffffffc);  //PC register
        PRINTF("write register R%d = 0x%x \r\n", 15, aluout);
    } else if(code_type == code_is_ldr0 || code_type == code_is_ldr1 ||
       code_type == code_is_ldrsb1 || code_type == code_is_ldrsh1 || code_type == code_is_ldrh1 ||
       code_type == code_is_ldrsb0 || code_type == code_is_ldrsh0 || code_type == code_is_ldrh0 )
    {
        uint32_t address = operand1;  //Rn
        uint32_t data;
        if(Pf) {
            //first add
            address = aluout;  //aluout
        }
        
        uint32_t tmpcpsr = cpsr;
        if(!Pf && Wf && (code_type == code_is_ldr0 || code_type == code_is_ldr1) ) {
            //LDRT as user mode
            cpsr &= 0x1f;
            cpsr |= CPSR_M_USR;
        }
        
        if(Lf) {
            //LDR
            if(code_type == code_is_ldrh1 || code_type == code_is_ldrh0) {
                //Halfword
                data = read_halfword(MEM, address);
            } else if(code_type == code_is_ldrsh1 || code_type == code_is_ldrsh0) {
                //Halfword Signed
                data = read_halfword(MEM, address);
                if(data&0x8000) {
                    data |= 0xffff0000;
                }
            } else if(code_type == code_is_ldrsb1 || code_type == code_is_ldrsb0) {
                //Byte Signed
                data = read_byte(MEM, address);
                if(data&0x80) {
                    data |= 0xffffff00;
                }
            } else if(Bf) {
                //Byte
                data = read_byte(MEM, address);
            } else {
                //Word
                if((address&0x3) != 0) {
                    printf("ldr unsupported address = 0x%x\n", address);
                    getchar();
                }
                data = read_word(MEM, address);
            }
            
            if(mmu_check_status()) {
                if(!Pf && Wf && (code_type == code_is_ldr0 || code_type == code_is_ldr1) ) {
                    //LDRT restore cpsr
                    cpsr = tmpcpsr;
                }
                return;
            }
            register_write(Rd, data);
            PRINTF("load data [0x%x]:0x%x to R%d \r\n", address, register_read(Rd), Rd);
        } else {
            //STR
            data = register_read(Rd);
            if(code_type == code_is_ldrh1 || code_type == code_is_ldrh0) {
                //Halfword
                write_halfword(MEM, address, data);
            } else if(code_type == code_is_ldrsh1 || code_type == code_is_ldrsh0) {
                //Halfword Signed ?
                write_halfword(MEM, address, data);
                printf("STRSH ? \r\n");
                exception_out();
            } else if(code_type == code_is_ldrsb1 || code_type == code_is_ldrsb0) {
                //Byte Signed ?
                write_byte(MEM, address, data);
                printf("STRSB ? \r\n");
                exception_out();
            } else if(Bf) {
                write_byte(MEM, address, data);
            } else {
                write_word(MEM, address, data);
            }
            if(mmu_check_status()) {
                if(!Pf && Wf && (code_type == code_is_ldr0 || code_type == code_is_ldr1) ) {
                    //LDRT restore cpsr
                    cpsr = tmpcpsr;
                }
                return;
            }
            PRINTF("store data [R%d]:0x%x to 0x%x \r\n", Rd, data, address);
        }
        if(!(!Wf && Pf)) {
            //Update base register
            register_write(Rn, aluout);
            PRINTF("[LDR]write register R%d = 0x%x\r\n", Rn, aluout);
        }
        
        if(!Pf && Wf && (code_type == code_is_ldr0 || code_type == code_is_ldr1) ) {
            //LDRT restore cpsr
            cpsr = tmpcpsr;
        }
        
    } else if(code_type == code_is_ldm) {
        
        if(Lf) { //bit [20]
            //LDM
            uint8_t ldm_type = 0;
            if(Bit22 && !IS_SET(instruction_word, 15)) {
                //LDM(2)
                ldm_type = 1;
            }
            
            uint8_t pc_include_flag = 0;
            uint32_t address = operand1;
            for(int i=0; i<16; i++) {
                int n = (Uf)?i:(15-i);
                if(IS_SET(instruction_word, n)) {
                    if(Pf) {
                        if(Uf) address += 4;
                        else address -= 4;
                    }
                    uint32_t data = read_word(MEM, address);
                    if(mmu_check_status()) {
                        return;
                    }
                    if(n == 15) {
                        data &= 0xfffffffc;
                        if(Bit22) {
                            pc_include_flag = 1;
                        }
                    }
                    if(ldm_type) {
                        //LDM(2)
                        register_write_user_mode(n, data);
                    } else {
                        //LDM(1)
                        register_write(n, data);
                    }
                    
                    PRINTF("[LDM] load data [0x%x]:0x%x to R%d \r\n", address, data, n);
                    if(!Pf) {
                        if(Uf) address += 4;
                        else address -= 4;
                    }
                }
            }
            
            if(Wf) {  //bit[21] W
                register_write(Rn, address);
                PRINTF("[LDM] write R%d = 0x%0x \r\n", Rn, address);
            }
            
            if(pc_include_flag) {
                //LDM(3) copy spsr to cpsr
                uint8_t cpu_mode = get_cpu_mode_code();
                cpsr = spsr[cpu_mode];
                PRINTF("ldm(3) cpsr 0x%x copy from spsr %d\r\n", cpsr, cpu_mode);
            }
            
        } else {
            uint8_t stm_type = 0;
            if(Bit22) {
                //STM(2)
                stm_type = 1;
            }
            //STM
            uint32_t address = operand1;
            for(int i=0; i<16; i++) {
                int n = (Uf)?i:(15-i);
                if(IS_SET(instruction_word, n)) {
                    if(Pf) {
                        if(Uf) address += 4;
                        else address -= 4;
                    }
                    if(stm_type) {
                        //STM(2)
                        write_word(MEM, address, register_read_user_mode(n));
                    } else {
                        //STM(1)
                        write_word(MEM, address, register_read(n));
                    }
                    
                    PRINTF("[STM] store data [R%d]:0x%x to 0x%x \r\n", n, register_read(n), address);
                    if(mmu_check_status()) {
                        return;
                    }
                    
                    if(!Pf) {
                        if(Uf) address += 4;
                        else address -= 4;
                    }
                }
            }
            
            if(Wf) {  //bit[21] W
                register_write(Rn, address);
                PRINTF("[STM] write R%d = 0x%0x \r\n", Rn, address);
            }
        }
    } else if(code_type == code_is_msr1 || code_type == code_is_msr0) {
        uint8_t cpu_mode;
        if(!Bit22) {
            //write cpsr
            cpu_mode = 0;
            if(cpsr_m == CPSR_M_USR && (Rn&0x7) ) {
                printf("[MSR] cpu is user mode, flags field can write only\r\n");
                exception_out();
            }
        } else {
            //write spsr
            cpu_mode = get_cpu_mode_code();
            if(cpu_mode == 0) {
                printf("[MSR] user mode has not spsr\r\n");
                exception_out();
            }
        }
        if(IS_SET(Rn, 0) ) {
            spsr[cpu_mode] &= 0xffffff00;
            spsr[cpu_mode] |= aluout&0xff;
        }
        if(IS_SET(Rn, 1) ) {
            spsr[cpu_mode] &= 0xffff00ff;
            spsr[cpu_mode] |= aluout&0xff00;
        }
        if(IS_SET(Rn, 2) ) {
            spsr[cpu_mode] &= 0xff00ffff;
            spsr[cpu_mode] |= aluout&0xff0000;
        }
        if(IS_SET(Rn, 3) ) {
            spsr[cpu_mode] &= 0x00ffffff;
            spsr[cpu_mode] |= aluout&0xff000000;
        }
        PRINTF("write register spsr_%d = 0x%x\r\n", cpu_mode, spsr[cpu_mode]);
        
    } else if(code_type == code_is_mult) {
        register_write(Rn, aluout);   //Rd and Rn swap, !!!
        PRINTF("[MULT] write register R%d = 0x%x\r\n", Rn, aluout);
        
        if(Bit20) {
            //Update CPSR register
            uint8_t out_sign = IS_SET(aluout, 31);
            
            cpsr_n_set(out_sign);
            cpsr_z_set(aluout == 0);
            //cv unaffected
            PRINTF("update flag nzcv %d%d%d%d \r\n", cpsr_n, cpsr_z, cpsr_c, cpsr_v);
        }
        
    } else if(code_type == code_is_mrs) {
        if(Bit22) {
            uint8_t mode = get_cpu_mode_code();
            register_write(Rd, spsr[mode]);
            PRINTF("write register R%d = [spsr]0x%x\r\n", Rd, spsr[mode]);
        } else {
            register_write(Rd, cpsr);
            PRINTF("write register R%d = [cpsr]0x%x\r\n", Rd, cpsr);
        }
    } else {
        printf("unsupport code %d\r\n", code_type );
    }
    
}


// it is used to set the reset values
//reset all registers and memory content to 0
void reset_proc(void)
{
    int i;
    cpsr = CPSR_M_SVC;
    cpsr_i_set(1);  //disable irq
    cpsr_f_set(1);  //disable fiq
    cp15_reset();
    int_reset();
    tim_reset();
    uart_8250_reset();
    for(i=0; i<16; i++) {
        register_write(i, 0);
    }
    code_counter = 0;
}

#if 0
uint32_t inc_counter[23];

void list_inc_counter(void)
{
    for(int i=0; i<23; i++) {
        printf("code %d counter %d\n", i, inc_counter[i]);
    }
}
#endif

int main(int argc, char **argv)
{
    char *path = "./arm_linux/zImage";
    char *dtb_path = "./arm_linux/arm-emulator.dtb";
    
    uint32_t kips_speed = 40000; //per 10ms
    if(argc > 1) {
        kips_speed = atoi(argv[1]);
    }
    printf("KIPS_SPEED = %d\n", kips_speed);
    
    load_program_memory(path, 0);
    load_program_memory(dtb_path, MEM_SIZE - 0x4000);
    reset_proc();
    
    register_write(1, 0xffffffff);         //set r1
    register_write(2, MEM_SIZE - 0x4000);  //set r2
    
    while(1) {
        code_counter++;
#if 0
        uint32_t pc = register_read(15) - 4;
        if(pc <= 0x10250 &&  pc >= 0x10210  &&  cp15_ctl_m) {
            abort_test = 1;
            printf("pc = 0x%x, debug\n", pc);
        }
#endif
        
        fetch();
        if(mmu_check_status()) {
            //check fetch instruction_word fault
            interrupt_exception(INT_EXCEPTION_PREABT);
            mmu_fault = 0;
#if 0
            printf("-------preAbt cp15_far %x-------\n", cp15_far);
            exception_out();
#endif
            continue;
        }
        
        if(!decode()) {
            continue;
        }
#if 0
        if(code_type < 23) {
            inc_counter[code_type]++;
        }
#endif
        
        execute();
        
        if(swi_flag) {
            swi_flag = 0;
#if 0
            uint32_t call_number = register_read(7);
            printf(" swi r7 = %d\n", call_number);
#endif
            interrupt_exception(INT_EXCEPTION_SWI);
        } else if(mmu_check_status()) {
            //check memory data fault
            interrupt_exception(INT_EXCEPTION_DATAABT);
            mmu_fault = 0;
#if 0
            printf("-------code is %d, cp15_far %x------\n", code_type, cp15_far);
            exception_out();
#endif
        } else if(TIM.EN && !cpsr_i && code_counter%kips_speed == 0 ) {
            //timer enable, int_controler enable, cpsr_irq not disable
            //per 100 millisecond timer irq
            static uint32_t tick_timer = 0;
            uint32_t new_tick_timer = GET_TICK();
            if(new_tick_timer - tick_timer >= 10) {
                tick_timer = new_tick_timer;
                interrupt_happen(0);
            }
        } else if( (UART[0].IER&0xf) && !cpsr_i  ) {
            //uart enable, cpsr_irq not disable
            //UART
            if( (UART[0].IER&0x2) ) {
                //Bit1, Enable Transmit Holding Register Empty Interrupt. 
                if(interrupt_happen(1)) {
                    UART[0].IIR = 0x2; // THR empty
                }
            } else  if( (UART[0].IER&0x1) && code_counter%kips_speed == (kips_speed/2) && KBHIT() ) {
                //Avoid calling kbhit functions frequently
                //Bit0, Enable Received Data Available Interrupt. 
                if( interrupt_happen(1) ) {
                    UART[0].IIR = 0x4; //received data available
                    register_set(UART[0].LSR, 0, 1);
                }
            } else {
                UART[0].IIR = 1; //no interrupt pending
            }
        }
        
#if DEBUG
        if(getchar() == 'c') {
            exception_out();
            getchar();
        }
#endif
#if 0
        if(abort_test) {
            exception_out();
            abort_test = 0;
        }
#endif
    }
    
    printf("\r\n---------------test code -----------\r\n");
    
    return 0;
}

/*****************************END OF FILE***************************/
