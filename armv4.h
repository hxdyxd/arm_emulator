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

extern uint8_t global_debug_flag;
#define DEBUG                  (global_debug_flag)

#define PRINTF(...)  do{ if(DEBUG){printf(__VA_ARGS__);} }while(0)
#define WARN(...)  do{ if(1){printf(__VA_ARGS__);} }while(0)
#define ERROR(...)  do{ if(1){printf(__VA_ARGS__);printf("Press any key to exit...\n");getchar();exit(-1);} }while(0)

#define  IS_SET(v, bit) ( ((v)>>(bit))&1 )

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
        uint8_t swi_flag;
    }decoder;

    struct mmu_t {
        uint32_t reg[16];
        uint8_t mmu_fault;
    }mmu;
#define  MMU_EXCEPTION_ADDR      0x4001f050

    struct peripheral_extern_t {
        uint32_t number;

        struct peripheral_link_t {
            char *name;
            uint32_t mask;
            uint32_t prefix;
            void *reg_base;
            void (*reset)(void *base);
            uint32_t (*read)(void *base, uint32_t address);
            void (*write)(void *base, uint32_t address, uint32_t data, uint8_t mask);
        }*link;
    }peripheral;

    uint32_t code_counter;
};


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

uint32_t register_read(struct armv4_cpu_t *cpu, uint8_t id);
void register_write(struct armv4_cpu_t *cpu, uint8_t id, uint32_t val);


/*
 *  return 1, mmu exception
 */
#define mmu_check_status(mmu)   (mmu)->mmu_fault

uint32_t read_mem(struct armv4_cpu_t *cpu, uint8_t privileged, uint32_t address, uint8_t mmu, uint8_t mask);
void write_mem(struct armv4_cpu_t *cpu, uint8_t privileged, uint32_t address, uint32_t data,  uint8_t mask);

/*  memory */
#define  is_privileged(cpu)            (cpsr_m(cpu) != CPSR_M_USR)

#define  read_word_without_mmu(cpu,a)       read_mem(cpu,0,a,0,3)
#define  read_word(cpu,a)                 read_mem(cpu,is_privileged(cpu),a,1,3)
#define  read_halfword(cpu,a)             (read_mem(cpu,is_privileged(cpu),a,1,1) & 0xffff)
#define  read_byte(cpu,a)                 (read_mem(cpu,is_privileged(cpu),a,1,0) & 0xff)

#define  read_word_mode(cpu,m,a)                 read_mem(cpu,m,a,1,3)
#define  read_halfword_mode(cpu,m,a)             (read_mem(cpu,m,a,1,1) & 0xffff)
#define  read_byte_mode(cpu,m,a)                 (read_mem(cpu,m,a,1,0) & 0xff)


#define  write_word(cpu,a,d)         write_mem(cpu,is_privileged(cpu),a,d,3)
#define  write_halfword(cpu,a,d)     write_mem(cpu,is_privileged(cpu),a,(d)&0xffff,1)
#define  write_byte(cpu,a,d)         write_mem(cpu,is_privileged(cpu),a,(d)&0xff,0)

#define  write_word_mode(cpu,m,a,d)         write_mem(cpu,m,a,d,3)
#define  write_halfword_mode(cpu,m,a,d)     write_mem(cpu,m,a,(d)&0xffff,1)
#define  write_byte_mode(cpu,m,a,d)         write_mem(cpu,m,a,(d)&0xff,0)


#endif
/*****************************END OF FILE***************************/
