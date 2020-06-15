/*
 * armv4.c of arm_emulator
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
#ifndef _ARMV4_H_
#define _ARMV4_H_

#include <stdint.h>
#include <string.h>
#include <stdio.h>

//exit
#include <stdlib.h>

#define TLB_SIZE     (0x40)


extern uint8_t global_debug_flag;
#define DEBUG                  (global_debug_flag)


#define  IS_SET(v, bit) ( ((v)>>(bit))&1 )
#define  SWAP_VAL(a,b) do{uint32_t tmp = a;a = b;b = tmp;}while(0)

struct tlb_t {
    uint32_t vaddr;
    uint32_t paddr;
    uint8_t  type;

    uint8_t is_manager;
    uint8_t ap;
};


struct armv4_cpu_t {
    uint32_t spsr[7];
#define  cpsr(cpu)    (cpu)->spsr[0]

#define  cpsr_n_set(cpu,v)  do{ if(v) {cpsr(cpu) |= 1 << 31;} else {cpsr(cpu) &= ~(1 << 31);} }while(0)
#define  cpsr_z_set(cpu,v)  do{ if(v) {cpsr(cpu) |= 1 << 30;} else {cpsr(cpu) &= ~(1 << 30);} }while(0)
#define  cpsr_c_set(cpu,v)  do{ if(v) {cpsr(cpu) |= 1 << 29;} else {cpsr(cpu) &= ~(1 << 29);} }while(0)
#define  cpsr_v_set(cpu,v)  do{ if(v) {cpsr(cpu) |= 1 << 28;} else {cpsr(cpu) &= ~(1 << 28);} }while(0)

#define  cpsr_i_set(cpu,v)  do{ if(v) {cpsr(cpu) |= 1 << 7;} else {cpsr(cpu) &= ~(1 << 7);} }while(0)
#define  cpsr_f_set(cpu,v)  do{ if(v) {cpsr(cpu) |= 1 << 6;} else {cpsr(cpu) &= ~(1 << 6);} }while(0)
#define  cpsr_t_set(cpu,v)  do{ if(v) {cpsr(cpu) |= 1 << 5;} else {cpsr(cpu) &= ~(1 << 5);} }while(0)

#define  cpsr_n(cpu)  IS_SET(cpsr(cpu), 31)
#define  cpsr_z(cpu)  IS_SET(cpsr(cpu), 30)
#define  cpsr_c(cpu)  IS_SET(cpsr(cpu), 29)
#define  cpsr_v(cpu)  IS_SET(cpsr(cpu), 28)
#define  cpsr_q(cpu)  IS_SET(cpsr(cpu), 27)
#define  cpsr_i(cpu)  IS_SET(cpsr(cpu), 7)
#define  cpsr_f(cpu)  IS_SET(cpsr(cpu), 6)
#define  cpsr_t(cpu)  IS_SET(cpsr(cpu), 5)
#define  cpsr_m(cpu)  (cpsr(cpu)&0x1f)

#define     CPSR_M_USR   0x10U
#define     CPSR_M_FIQ   0x11U
#define     CPSR_M_IRQ   0x12U
#define     CPSR_M_SVC   0x13U
#define     CPSR_M_MON   0x16U
#define     CPSR_M_ABT   0x17U
#define     CPSR_M_HYP   0x1AU
#define     CPSR_M_UND   0x1BU
#define     CPSR_M_SYS   0x1FU


    /* register */
    uint32_t reg[7][16];
#define   CPU_MODE_USER      0
#define   CPU_MODE_FIQ       1
#define   CPU_MODE_IRQ       2
#define   CPU_MODE_SVC       3
#define   CPU_MODE_Undef     4
#define   CPU_MODE_Abort     5
#define   CPU_MODE_Mon       6

    struct decoder_t {
        uint32_t instruction_word;
        uint8_t code_type;
        uint8_t event_id;
#define EVENT_ID_IDLE      (0)
#define EVENT_ID_UNDEF     (1)
#define EVENT_ID_SWI       (2)
#define EVENT_ID_DATAABT   (3)
#define EVENT_ID_PREAABT   (4)
#define EVENT_ID_WFI       (5)
    }decoder;

    struct mmu_t {
        uint32_t reg[16];
        uint8_t mmu_fault;
#define   cp15_ctl(mmu)            (mmu)->reg[1]
#define   cp15_ttb(mmu)            (mmu)->reg[2]
#define   cp15_domain(mmu)         (mmu)->reg[3]
/* Fault Status Register, [7:4]domain  [3:0]status */
#define   cp15_fsr(mmu)            (mmu)->reg[5]
/* Fault Address Register */
#define   cp15_far(mmu)            (mmu)->reg[6]


//mmu enable
#define  cp15_ctl_m(mmu)  IS_SET(cp15_ctl(mmu), 0)
//Alignment fault checking
#define  cp15_ctl_a(mmu)  IS_SET(cp15_ctl(mmu), 1)
//System protection bit
#define  cp15_ctl_s(mmu)  IS_SET(cp15_ctl(mmu), 8)
//ROM protection bit
#define  cp15_ctl_r(mmu)  IS_SET(cp15_ctl(mmu), 9)
//high vectors
#define  cp15_ctl_v(mmu)  IS_SET(cp15_ctl(mmu), 13)

        struct tlb_t tlb[2][TLB_SIZE];
        struct tlb_t *tlb_base;
        uint32_t tlb_hit;
        uint32_t tlb_total;
#define TLB_D   (0)
#define TLB_I   (1)

/*
 *s:TLB_D,TLB_I
 */
#define tlb_set_base(mmu,s)  (mmu)->tlb_base = (mmu)->tlb[s]
    }mmu;


    struct peripheral_extern_t {
        uint32_t number;

        struct peripheral_link_t {
            char *name;
            uint32_t mask;
            uint32_t prefix;
            void *reg_base;
            uint32_t (*reset)(void *base);
            void (*exit)(void *base);
            uint32_t (*read)(void *base, uint32_t address);
            void (*write)(void *base, uint32_t address, uint32_t data, uint8_t mask);
        }*link;
    }peripheral;

    uint32_t code_counter;
    uint32_t code_time;
};

/*
 * get_cpu_mode_code
 * author:hxdyxd
 */
static uint8_t get_cpu_mode_code(struct armv4_cpu_t *cpu)
{
    uint8_t cpu_mode = 0;
    switch(cpsr_m(cpu)) {
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
        printf("cpu mode is unknown %x\r\n", cpsr_m(cpu));
    }
    return cpu_mode;
}

#define register_read_user_mode(cpu,id)  cpu->reg[0][id]
#define register_write_user_mode(cpu,id,v)  cpu->reg[0][id] = v

/*
 * register_read
 * author:hxdyxd
 */
static inline uint32_t register_read(struct armv4_cpu_t *cpu, const uint8_t id)
{
    //Register file
    if(id < 8) {
        return cpu->reg[CPU_MODE_USER][id];
    } else if(id == 15) {
        return cpu->reg[CPU_MODE_USER][id] + 4;
    } else if(cpsr_m(cpu) == CPSR_M_USR || cpsr_m(cpu) == CPSR_M_SYS) {
        return cpu->reg[CPU_MODE_USER][id];
    } else if(cpsr_m(cpu) != CPSR_M_FIQ && id < 13) {
        return cpu->reg[CPU_MODE_USER][id];
    } else {
        return cpu->reg[get_cpu_mode_code(cpu)][id];
    }
}

/*
 * register_write
 * author:hxdyxd
 */
static inline void register_write(struct armv4_cpu_t *cpu, const uint8_t id, const uint32_t val)
{
    //Register file
    if(id < 8 || id == 15) {
        cpu->reg[CPU_MODE_USER][id] = val;
    } else if(cpsr_m(cpu) == CPSR_M_USR || cpsr_m(cpu) == CPSR_M_SYS) {
        cpu->reg[CPU_MODE_USER][id] = val;
    } else if(cpsr_m(cpu) != CPSR_M_FIQ && id < 13) {
        cpu->reg[CPU_MODE_USER][id] = val;
    } else {
        cpu->reg[get_cpu_mode_code(cpu)][id] = val;
    }
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
void cpu_init(struct armv4_cpu_t *cpu);
void interrupt_exception(struct armv4_cpu_t *cpu, uint8_t type);
void fetch(struct armv4_cpu_t *cpu);
void decode(struct armv4_cpu_t *cpu);
void peripheral_register(struct armv4_cpu_t *cpu, struct peripheral_link_t *link, int number);

void reg_show(struct armv4_cpu_t *cpu);

uint32_t read_mem(struct armv4_cpu_t *cpu, uint8_t privileged, uint32_t address, uint8_t mmu, uint8_t mask);
void write_mem(struct armv4_cpu_t *cpu, uint8_t privileged, uint32_t address, uint32_t data,  uint8_t mask);

/*  memory */
#define  is_privileged(cpu)            (cpsr_m(cpu) != CPSR_M_USR)

#define  read_word_without_mmu(cpu,a)   read_mem(cpu,0,a,0,3)
#define  read_word(cpu,a)               read_mem(cpu,is_privileged(cpu),a,1,3)
#define  read_halfword(cpu,a)           (read_mem(cpu,is_privileged(cpu),a,1,1) & 0xffff)
#define  read_byte(cpu,a)               (read_mem(cpu,is_privileged(cpu),a,1,0) & 0xff)

#define  read_word_mode(cpu,m,a)        read_mem(cpu,m,a,1,3)
#define  read_halfword_mode(cpu,m,a)    (read_mem(cpu,m,a,1,1) & 0xffff)
#define  read_byte_mode(cpu,m,a)        (read_mem(cpu,m,a,1,0) & 0xff)


#define  write_word(cpu,a,d)            write_mem(cpu,is_privileged(cpu),a,d,3)
#define  write_halfword(cpu,a,d)        write_mem(cpu,is_privileged(cpu),a,(d)&0xffff,1)
#define  write_byte(cpu,a,d)            write_mem(cpu,is_privileged(cpu),a,(d)&0xff,0)

#define  write_word_mode(cpu,m,a,d)         write_mem(cpu,m,a,d,3)
#define  write_halfword_mode(cpu,m,a,d)     write_mem(cpu,m,a,(d)&0xffff,1)
#define  write_byte_mode(cpu,m,a,d)         write_mem(cpu,m,a,(d)&0xff,0)

/* debug */
void tlb_show(struct mmu_t *mmu);

#endif
/*****************************END OF FILE***************************/
