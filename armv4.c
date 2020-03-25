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
#include <armv4.h>

const char *string_register_mode[7] = {
    "USER",
    "FIQ",
    "IRQ",
    "SVC",
    "Undef",
    "Abort",
    "Mon",
};
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
uint8_t global_debug_flag = 0;

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
        ERROR("cpu mode is unknown %x\r\n", cpsr_m(cpu));
    }
    return cpu_mode;
}


#define register_read_user_mode(cpu,id)  cpu->reg[0][id]
#define register_write_user_mode(cpu,id,v)  cpu->reg[0][id] = v

/*
 * register_read
 * author:hxdyxd
 */
uint32_t register_read(struct armv4_cpu_t *cpu, uint8_t id)
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
        PRINTF("<%x>", cpsr_m(cpu));
        return cpu->reg[get_cpu_mode_code(cpu)][id];
    }
}

/*
 * register_write
 * author:hxdyxd
 */
void register_write(struct armv4_cpu_t *cpu, uint8_t id, uint32_t val)
{
    //Register file
    if(id < 8 || id == 15) {
        cpu->reg[CPU_MODE_USER][id] = val;
    } else if(cpsr_m(cpu) == CPSR_M_USR || cpsr_m(cpu) == CPSR_M_SYS) {
        cpu->reg[CPU_MODE_USER][id] = val;
    } else if(cpsr_m(cpu) != CPSR_M_FIQ && id < 13) {
        cpu->reg[CPU_MODE_USER][id] = val;
    } else {
        PRINTF("<%x>", cpsr_m(cpu));
        cpu->reg[get_cpu_mode_code(cpu)][id] = val;
    }
}

/*
 * exception_out
 * author:hxdyxd
 */
static void exception_out(struct armv4_cpu_t *cpu)
{
    //return;
    uint8_t cur_mode  = get_cpu_mode_code(cpu);
    WARN("PC = 0x%x , code = %d\r\n", register_read(cpu, 15), cpu->code_counter);
    WARN("cpsr = 0x%x, %s\n", cpsr(cpu), string_register_mode[cur_mode]);
    WARN("User mode register:");
    for(int i=0; i<16; i++) {
        if(i % 4 == 0) {
            WARN("\n");
        }
        WARN("R%2d = 0x%08x, ", i, cpu->reg[CPU_MODE_USER][i]);
    }
    WARN("\r\nPrivileged mode register\r\n");
    for(int i=1; i<7; i++) {
        WARN("R%2d = 0x%08x, ", 13, cpu->reg[i][13]);
        WARN("R%2d = 0x%08x, ", 14, cpu->reg[i][14]);
        WARN("%s \n", string_register_mode[i]);
    }
    ERROR("stop\n");
}


/*************cp15************/

#define   cp15_ctl(mmu)            (mmu)->reg[1]
#define   cp15_ttb(mmu)            (mmu)->reg[2]
#define   cp15_domain(mmu)         (mmu)->reg[3]
/* Fault Status Register, [7:4]domain  [3:0]status */
#define   cp15_fsr(mmu)            (mmu)->reg[5]
/* Fault Address Register */
#define   cp15_far(mmu)            (mmu)->reg[6]



#define   cp15_read(mmu,r)      (mmu)->reg[r]
#define   cp15_write(mmu,r,d)   (mmu)->reg[r] = d
#define   cp15_reset(mmu)      memset((mmu)->reg, 0, sizeof((mmu)->reg))


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

/*
 *  return 1, mmu fault
 */
#define mmu_check_status(mmu)   (mmu)->mmu_fault

/*
 *  uint8_t wr:                write: 1, read: 0
 *  uint8_t privileged:   privileged: 1, user: 0
 * author:hxdyxd
 */
static inline int mmu_check_access_permissions(struct mmu_t *mmu, uint8_t ap,
 uint8_t privileged, uint8_t wr)
{
    switch(ap) {
    case 0:
        if(!cp15_ctl_s(mmu) && cp15_ctl_r(mmu) && !wr)
            return 0;
        if(cp15_ctl_s(mmu) && !cp15_ctl_r(mmu) && !wr && privileged)
            return 0;
        if(cp15_ctl_s(mmu) && cp15_ctl_r(mmu)) {
            WARN("mmu_check_access_permissions fault\r\n");
            ERROR("MMU stop\n");
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
 * author:hxdyxd
 */
static inline uint32_t mmu_transfer(struct armv4_cpu_t *cpu, uint32_t vaddr,
 uint8_t privileged, uint8_t wr)
{
    struct mmu_t *mmu = &cpu->mmu;
    mmu->mmu_fault = 0;
    if(!cp15_ctl_m(mmu)) {
        return vaddr;
    }
    if(cp15_ctl_a(mmu)) {
        //Check address alignment
        
    }
    //section page table, store size 16KB
    uint32_t page_table_entry = read_word_without_mmu(cpu,
     (cp15_ttb(mmu)&0xFFFFC000) | ((vaddr&0xFFF00000) >> 18));
    uint8_t first_level_descriptor = page_table_entry&3;
    if(first_level_descriptor == 0) {
        //Section translation fault
        cp15_fsr(mmu) = 0x5;
        cp15_far(mmu) = vaddr;
        mmu->mmu_fault = 1;
        PRINTF("Section translation fault va = 0x%x\r\n", vaddr);
        //exception_out();
    } else if(first_level_descriptor == 2) {
        //section entry
        uint8_t domain = (page_table_entry>>5)&0xf; //Domain field
        uint8_t domainval = domain << 1;
        domainval = (cp15_domain(mmu) >> domainval)&0x3;
        if(domainval == 0) {
            //Section domain fault
            cp15_fsr(mmu) = (domain << 4) | 0x9;
            cp15_far(mmu) = vaddr;
            mmu->mmu_fault = 1;
            WARN("Section domain fault\r\n");
            //getchar();
        } else if(domainval == 1) {
            //Client, Check access permissions
            uint8_t ap = (page_table_entry>>10)&0x3;
            if(mmu_check_access_permissions(mmu, ap, privileged, wr) == 0) {
                return (page_table_entry&0xFFF00000)|(vaddr&0x000FFFFF);
            } else {
                //Section permission fault
                cp15_fsr(mmu) = (domain << 4) | 0xd;
                cp15_far(mmu) = vaddr;
                mmu->mmu_fault = 1;
                WARN("Section permission fault\r\n");
                //getchar();
            }
        } else if(domainval == 3) {
            //Manager
            return (page_table_entry&0xFFF00000)|(vaddr&0x000FFFFF);
        } else {
            WARN("mmu fault\r\n");
            exception_out(cpu);
        }
        
    } else {
        //page table
        uint8_t domain = (page_table_entry>>5)&0xf; //Domain field
        uint8_t domainval =  domain << 1;
        domainval = (cp15_domain(mmu) >> domainval)&0x3;
        if(first_level_descriptor == 1) {
            //coarse page table, store size 1KB
            page_table_entry = read_word_without_mmu(cpu,
             (page_table_entry&0xFFFFFC00)|((vaddr&0x000FF000) >> 10));
        } else if(first_level_descriptor == 3) {
            //fine page table, store size 4KB
            page_table_entry = read_word_without_mmu(cpu,
             (page_table_entry&0xFFFFF000)|((vaddr&0x000FFC00) >> 8));
        } else {
            WARN("mmu fault\r\n");
            exception_out(cpu);
        }
        uint8_t second_level_descriptor = page_table_entry&3;
        if(second_level_descriptor == 0) {
            //Page translation fault
            cp15_fsr(mmu) = (domain << 4) | 0x7;
            cp15_far(mmu) = vaddr;
            mmu->mmu_fault = 1;
            PRINTF("Page translation fault va = 0x%x %d %d pte = 0x%x\r\n",
             vaddr, privileged, wr, page_table_entry);
            //exception_out(cpu);
        } else {
            if(domainval == 0) {
                //Page domain fault
                cp15_fsr(mmu) = (domain << 4) | 0xb;
                cp15_far(mmu) = vaddr;
                mmu->mmu_fault = 1;
                WARN("Page domain fault\r\n");
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
                    if(mmu_check_access_permissions(mmu, ap, privileged, wr) == 0) {
                        return (page_table_entry&0xFFFF0000)|(vaddr&0x0000FFFF);
                    } else {
                        //Sub-page permission fault
                        cp15_fsr(mmu) = (domain << 4) | 0xf;
                        cp15_far(mmu) = vaddr;
                        mmu->mmu_fault = 1;
                        PRINTF("Large Sub-page permission fault va = 0x%x %d %d %d\r\n",
                         vaddr, ap, privileged, wr);
                        //exception_out(cpu);
                    }
                }
                case 2:
                {
                    //small page, 4KB
                    uint8_t subpage = (vaddr >> 10)&3;
                    subpage <<= 1;
                    ap = (page_table_entry >> (subpage+4))&0x3;
                    if(mmu_check_access_permissions(mmu, ap, privileged, wr) == 0) {
                        return (page_table_entry&0xFFFFF000)|(vaddr&0x00000FFF);
                    } else {
                        //Sub-page permission fault
                        cp15_fsr(mmu) = (domain << 4) | 0xf;
                        cp15_far(mmu) = vaddr;
                        mmu->mmu_fault = 1;
                        PRINTF("Small Sub-page permission fault va = 0x%x %d %d %d\r\n",
                         vaddr, ap, privileged, wr);
                        //exception_out(cpu);
                    }
                }
                case 3:
                    //tiny page, 1KB
                    ap = (page_table_entry>>4)&0x3; //ap0
                    if(mmu_check_access_permissions(mmu, ap, privileged, wr) == 0) {
                        return (page_table_entry&0xFFFFFC00)|(vaddr&0x000003FF);
                    } else {
                        //Sub-page permission fault
                        cp15_fsr(mmu) = (domain << 4) | 0xf;
                        cp15_far(mmu) = vaddr;
                        mmu->mmu_fault = 1;
                        PRINTF("Tiny Sub-page permission fault va = 0x%x %d %d %d\r\n",
                         vaddr, ap, privileged, wr);
                        //exception_out(cpu);
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
                exception_out(cpu);
            }
        }
    }
    return MMU_EXCEPTION_ADDR;
}

/****cp15 end***********************************************************/

/*
 * read_mem
 * author:hxdyxd
 */
uint32_t read_mem(struct armv4_cpu_t *cpu, uint8_t privileged, uint32_t address,
 uint8_t mmu, uint8_t mask)
{
    if(address&mask) {
        //Check address alignment
        WARN("memory address alignment error 0x%x\n", address);
        exception_out(cpu);
    }
    
    if(mmu) {
        address = mmu_transfer(cpu, address, privileged, 0); //read
    }
    
    //1M Peripheral memory
    for(int i=0; i<cpu->peripheral.number; i++) {
        if(cpu->peripheral.link[i].read && (address&cpu->peripheral.link[i].mask) == cpu->peripheral.link[i].prefix) {
            uint32_t offset = address - cpu->peripheral.link[i].prefix;
            return cpu->peripheral.link[i].read(cpu->peripheral.link[i].reg_base, offset);
        }
    }

    if(address == 0x4001f030) {
        //code counter
        return cpu->code_counter;
    } else if(address == MMU_EXCEPTION_ADDR) {
        return 0xffffffff;
    }
    
    WARN("mem overflow error, read 0x%x\r\n", address);
    exception_out(cpu);
    return 0xffffffff;
}


/*
 * write_mem
 * author:hxdyxd
 */
void write_mem(struct armv4_cpu_t *cpu, uint8_t privileged, uint32_t address,
 uint32_t data,  uint8_t mask)
{
    if(address&mask) {
        //Check address alignment
        WARN("memory address alignment error 0x%x\n", address);
        exception_out(cpu);
    }
    
    address = mmu_transfer(cpu, address, privileged, 1); //write
    
    //4G Peripheral memory
    for(int i=0; i<cpu->peripheral.number; i++) {
        if(cpu->peripheral.link[i].write && (address&cpu->peripheral.link[i].mask) == cpu->peripheral.link[i].prefix) {
            uint32_t offset = address - cpu->peripheral.link[i].prefix;
            cpu->peripheral.link[i].write(cpu->peripheral.link[i].reg_base, offset, data, mask);
            return;
        }
    }
    if(address == 0x4001f030) {
        //ips
        cpu->code_counter = data;
        return;
    } else if(address == MMU_EXCEPTION_ADDR) {
        return;
    }

    WARN("mem error, write %d = 0x%0x\r\n", mask, address);
    exception_out(cpu);
}

/*
 * uint8_t type:
 * INT_EXCEPTION_UNDEF
 * INT_EXCEPTION_SWI
 * INT_EXCEPTION_PREABT
 * INT_EXCEPTION_DATAABT
 * INT_EXCEPTION_IRQ
 */
void interrupt_exception(struct armv4_cpu_t *cpu, uint8_t type)
{
    struct mmu_t *mmu = &cpu->mmu;
    uint32_t cpsr_int = cpsr(cpu);
    uint32_t next_pc = register_read(cpu, 15) - 4;  //lr 
    PRINTF("[INT %d]cpsr(0x%x) save to spsr, next_pc(0x%x) save to r14_pri \r\n",
     type, cpsr_int, next_pc);
    switch(type) {
    case INT_EXCEPTION_UNDEF:
        //UNDEF
        PRINTF("UNDEF\r\n");
        register_write(cpu, 15, 0x4|(cp15_ctl_v(mmu)?0xffff0000:0));
        cpsr_i_set(cpu, 1);  //disable irq
        cpsr_t_set(cpu, 0);  //arm mode
        cpsr(cpu) &= ~0x1f;
        cpsr(cpu) |= CPSR_M_UND;
        cpu->spsr[CPU_MODE_Undef] = cpsr_int;  //write SPSR_Undef
        register_write(cpu, 14, next_pc);  //write R14_Undef
        break;
    case INT_EXCEPTION_SWI:
        //swi
        PRINTF("SVC\r\n");
        register_write(cpu, 15, 0x8|(cp15_ctl_v(mmu)?0xffff0000:0));
        cpsr_i_set(cpu, 1);  //disable irq
        cpsr_t_set(cpu, 0);  //arm mode
        cpsr(cpu) &= ~0x1f;
        cpsr(cpu) |= CPSR_M_SVC;
        cpu->spsr[CPU_MODE_SVC] = cpsr_int;  //write SPSR_svc
        register_write(cpu, 14, next_pc);  //write R14_svc
        break;
    case INT_EXCEPTION_PREABT:
        //preAbt
        PRINTF("PREABT\r\n");
        register_write(cpu, 15, 0xc|(cp15_ctl_v(mmu)?0xffff0000:0));
        cpsr_i_set(cpu, 1);  //disable irq
        cpsr_t_set(cpu, 0);  //arm mode
        cpsr(cpu) &= ~0x1f;
        cpsr(cpu) |= CPSR_M_ABT;
        cpu->spsr[CPU_MODE_Abort] = cpsr_int;  //write SPSR_abt
        register_write(cpu, 14, next_pc + 0);  //write R14_abt
        break;
    case INT_EXCEPTION_DATAABT:
        //dataAbt
        PRINTF("DATAABT\r\n");
        register_write(cpu, 15, 0x10|(cp15_ctl_v(mmu)?0xffff0000:0));
        cpsr_i_set(cpu, 1);  //disable irq
        cpsr_t_set(cpu, 0);  //arm mode
        cpsr(cpu) &= ~0x1f;
        cpsr(cpu) |= CPSR_M_ABT;
        cpu->spsr[CPU_MODE_Abort] = cpsr_int;  //write SPSR_abt
        register_write(cpu, 14, next_pc + 4);  //write R14_abt
        break;
    case INT_EXCEPTION_IRQ:
        //irq
        if(cpsr_i(cpu)) {
            return;  //irq disable
        }
        PRINTF("IRQ\r\n");
        register_write(cpu, 15, 0x18|(cp15_ctl_v(mmu)?0xffff0000:0));
        cpsr_i_set(cpu, 1);  //disable irq
        cpsr_t_set(cpu, 0);  //arm mode
        cpsr(cpu) &= ~0x1f;
        cpsr(cpu) |= CPSR_M_IRQ;
        cpu->spsr[CPU_MODE_IRQ] = cpsr_int;  //write SPSR_irq
        register_write(cpu, 14, next_pc + 4);  //write R14_irq
        break;
    default:
        WARN("unknow interrupt\r\n");
        exception_out(cpu);
    }
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


void decode(struct armv4_cpu_t *cpu)
{
    struct decoder_t *dec = &cpu->decoder;
    uint8_t cond_satisfy = 0;
    
    uint8_t cond = (dec->instruction_word >> 28)&0xf;  /* bit[31:28] */
    switch(cond) {
    case 0x0:
        cond_satisfy = (cpsr_z(cpu) == 1);
        break;
    case 0x1:
        cond_satisfy = (cpsr_z(cpu) == 0);
        break;
    case 0x2:
        cond_satisfy = (cpsr_c(cpu) == 1);
        break;
    case 0x3:
        cond_satisfy = (cpsr_c(cpu) == 0);
        break;
    case 0x4:
        cond_satisfy = (cpsr_n(cpu) == 1);
        break;
    case 0x5:
        cond_satisfy = (cpsr_n(cpu) == 0);
        break;
    case 0x6:
        cond_satisfy = (cpsr_v(cpu) == 1);
        break;
    case 0x7:
        cond_satisfy = (cpsr_v(cpu) == 0);
        break;
    case 0x8:
        cond_satisfy = ((cpsr_c(cpu) == 1) & (cpsr_z(cpu) == 0));
        break;
    case 0x9:
        cond_satisfy = ((cpsr_c(cpu) == 0) | (cpsr_z(cpu) == 1));
        break;
    case 0xa:
        cond_satisfy = (cpsr_n(cpu) == cpsr_v(cpu));
        break;
    case 0xb:
        cond_satisfy = (cpsr_n(cpu) != cpsr_v(cpu));
        break;
    case 0xc:
        cond_satisfy = ((cpsr_z(cpu) == 0) & (cpsr_n(cpu) == cpsr_v(cpu)));
        break;
    case 0xd:
        cond_satisfy = ((cpsr_z(cpu) == 1) | (cpsr_n(cpu) != cpsr_v(cpu)));
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
        PRINTF("cond_satisfy = %d skip...\r\n", cond_satisfy);
        return;
    }
    



    uint8_t Rn;
    uint8_t Rd;
    uint8_t Rs;
    uint8_t Rm;
    uint16_t immediate = 0;
    int32_t b_offset = 0;
    uint8_t opcode;
    uint8_t shift;
    uint8_t rotate_imm;
    uint8_t shift_amount;
    uint8_t Lf = 0, Bf = 0, Uf = 0, Pf = 0, Wf = 0;
    uint8_t Bit22, Bit20, Bit7, Bit4;
    uint8_t Bit24_23;


    uint8_t f = (dec->instruction_word >> 25)&0x7;  /* bit[27:25] */
    opcode = (dec->instruction_word >> 21)&0xf;  /* bit[24:21] */
    Bit24_23 = opcode >> 2;  /* bit[24:23] */
    Bit22 = IS_SET(dec->instruction_word, 22);  /* bit[22] */
    Bit20 = IS_SET(dec->instruction_word, 20);  /* bit[20] */
    Rn = (dec->instruction_word >> 16)&0xf;  /* bit[19:16] */
    Rd = (dec->instruction_word >> 12)&0xf;  /* bit[15:12] */
    Rs = (dec->instruction_word >> 8)&0xf;  /* bit[11:8] */
    rotate_imm = Rs;  /* bit[11:8] */
    shift_amount = (dec->instruction_word >> 7)&0x1f;  /* bit[11:7] */
    shift = (dec->instruction_word >> 5)&0x3;  /* bit[6:5] */
    Bit4 = IS_SET(dec->instruction_word, 4);  /* bit[4] */
    Rm = (dec->instruction_word&0xF);  /* bit[3:0] */

    uint8_t code_type = code_is_unknow;
    switch(f) {
    case 0:
        switch(Bit4) {
        case 0:
            if( (Bit24_23 == 2 ) && !Bit20 ) {
                //Miscellaneous instructions
                uint8_t Bit21 = (dec->instruction_word&0x200000) >> 21;  /* bit[21] */
                uint8_t Bit7 = (dec->instruction_word&0x80) >> 7;  /* bit[7] */
                if(Bit21 && !Bit7) {
                    code_type = code_is_msr0;
                    PRINTF("msr0 %sPSR_%d  R%d\r\n", Bit22?"S":"C", Rn, Rm);
                } else if(!Bit21 && !Bit7) {
                    code_type = code_is_mrs;
                    PRINTF("mrs R%d %s\r\n", Rd, Bit22?"SPSR":"CPSR");
                } else {
                    //ERROR("undefined bit7 error \r\n");
                }
            } else {
                //Data processing register shift by immediate
                code_type = code_is_dp0;
                PRINTF("dp0 %s dst register R%d, first operand R%d, Second operand R%d[0x%x] %s immediate #0x%x\r\n",
                 opcode_table[opcode], Rd, Rn, Rm, register_read(cpu, Rm), shift_table[shift], shift_amount);
            }
            break;
        case 1:
            Bit7 = (dec->instruction_word&0x80) >> 7;  /* bit[7] */
            switch(Bit7) {
            case 0:
                if( (Bit24_23 == 2 ) && !Bit20 ) {
                    //Miscellaneous instructions
                    if(!IS_SET(dec->instruction_word, 22) && IS_SET(dec->instruction_word, 21) && !IS_SET(dec->instruction_word, 6) ) {
                        //Branch/exchange
                        code_type = code_is_bx;
                        Lf = IS_SET(dec->instruction_word, 5); //L bit
                        PRINTF("bx B%sX R%d\r\n", Lf?"L":"", Rm);
                    } else if(IS_SET(dec->instruction_word, 6) && !IS_SET(dec->instruction_word, 5) ) {
                        //Enhanced DSP add/subtracts
                        //ERROR("Enhanced DSP add/subtracts R%d\r\n", Rm);
                    } else if(IS_SET(dec->instruction_word, 6) && IS_SET(dec->instruction_word, 5)) {
                        //Software breakpoint
                        //ERROR("Software breakpoint \r\n");
                    } else if(!IS_SET(dec->instruction_word, 6) && !IS_SET(dec->instruction_word, 5) && opcode == 0xb) {
                        //Count leading zero
                        //ERROR("Count leading zero \r\n");
                    } else {
                        //ERROR("Undefed Miscellaneous instructions\r\n");
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
                Lf = IS_SET(dec->instruction_word, 20); //L bit
                Bf = IS_SET(dec->instruction_word, 22); //B bit
                Uf = IS_SET(dec->instruction_word, 23);  //U bit
                Pf = (opcode >> 3) & 1; //P bit[24]
                Wf = opcode & 1;  //W bit[21]
                switch(shift) {
                case 0:
                    if(Bit24_23 == 0) {
                        code_type = code_is_mult;
                        if( IS_SET(dec->instruction_word, 21) ) {
                            PRINTF("mult MUA%s R%d <= R%d * R%d + R%d\r\n", Bit20?"S":"", Rn, Rm, Rs, Rd);
                        } else {
                            PRINTF("mult MUL%s R%d <= R%d * R%d\r\n", Bit20?"S":"", Rn, Rm, Rs);
                        }
                    } else if(Bit24_23 == 1) {
                        code_type = code_is_multl;
                        PRINTF("multl %sM%s [R%d L, R%d H] %s= R%d * R%d\r\n", Bit22?"S":"U",
                         IS_SET(dec->instruction_word, 21)?"LAL":"ULL", Rd, Rn, IS_SET(dec->instruction_word, 21)?"+":"", Rm, Rs);
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
                        code_type = code_is_unknow;
                        //ERROR("undefed LDRD\r\n");
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
                        code_type = code_is_unknow;
                        //ERROR("undefed STRD\r\n");
                    }
                    break;
                default:
                    ;
                    //ERROR("undefed shift\r\n");
                }
                break;
            default:
                ;
                //ERROR("undefed Bit7\r\n");
            }
            break;
        default:
            ;
            //ERROR("undefed Bit4\r\n");
        }
        break;
    case 1:
        //Data processing immediate and move immediate to status register
        if( (opcode == 0x9 || opcode == 0xb ) && !Bit20 ) {
            code_type = code_is_msr1;
            immediate = dec->instruction_word&0xff;
            PRINTF("msr1 MSR %sPSR_%d  immediate [#%d ROR %d]\r\n", Bit22?"S":"C", Rn, immediate, rotate_imm*2);
        } else if( (opcode == 0x8 || opcode == 0xa ) && !Bit20 ) {
            PRINTF("undefined instruction\r\n");
        } else {
            //Data processing immediate
            code_type = code_is_dp2;
            immediate = dec->instruction_word&0xff;
            PRINTF("dp2 %s dst register R%d, first operand R%d, Second operand immediate [#%d ROR %d]\r\n",
             opcode_table[opcode], Rd, Rn, immediate, rotate_imm*2);
        }
        break;
    case 2:
        //load/store immediate offset
        code_type = code_is_ldr0;
        immediate = dec->instruction_word&0xFFF;
        Lf = IS_SET(dec->instruction_word, 20); //L bit
        Bf = IS_SET(dec->instruction_word, 22); //B bit
        Uf = IS_SET(dec->instruction_word, 23); //U bit
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
            Lf = IS_SET(dec->instruction_word, 20); //L bit
            Bf = IS_SET(dec->instruction_word, 22); //B bit
            Uf = IS_SET(dec->instruction_word, 23); //U bit
            Pf = (opcode >> 3) & 1; //P bit[24]
            Wf = opcode & 1;  //W bit[21]
            if(Pf) {
                PRINTF("ldr1 %s%s dst register R%d, [base address at R%d, offset address at %s[R%d(0x%x) %s %d]] %s\r\n",
                 Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-",  Rm, register_read(cpu, Rm), shift_table[shift], shift_amount, Wf?"!":"");
            } else {
                PRINTF("ldr1 %s%s dst register R%d, [base address at R%d], offset address at %s[R%d %s %d] \r\n",
                 Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-",  Rm, shift_table[shift], shift_amount);
            }
            
        } else {
            //ERROR("undefined instruction\r\n");
        }
        break;
    case 4:
        //load/store multiple
        code_type = code_is_ldm;
        Lf = IS_SET(dec->instruction_word, 20); //L bit
        Uf = IS_SET(dec->instruction_word, 23);  //U bit
        Pf = opcode >> 3;  //P bit
        Wf = opcode & 1;
        PRINTF("ldm %s%s%s R%d%s \r\n", Lf?"LDM":"STM", Uf?"I":"D", Pf?"B":"A",  Rn, Wf?"!":"");
        Rn = (dec->instruction_word >> 16)&0xf;
        break;
    case 5:
        //branch
        code_type = code_is_b;
        if( (dec->instruction_word&0xFFFFFF) >> 23) {
            //-
            b_offset = (dec->instruction_word&0x7FFFFF)|0xFF800000;
        } else {
            //+
            b_offset = dec->instruction_word&0x7FFFFF;
        }
        b_offset <<= 2;
        Lf = IS_SET(dec->instruction_word, 24); //L bit
        PRINTF("B%s PC[0x%x] +  0x%08x = 0x%x\r\n",
         Lf?"L":"", register_read(cpu, 15), b_offset, register_read(cpu, 15) + b_offset );
        break;
    case 6:
        //Coprocessor load/store and double register transfers
        //ERROR("Coprocessor todo... \r\n");
        break;
    case 7:
        //software interrupt
        if(IS_SET(dec->instruction_word, 24)) {
            code_type = code_is_swi;
            PRINTF("swi \r\n");
        } else {
            if(Bit4) {
                //Coprocessor move to register
                code_type = code_is_mcr;
                PRINTF("mcr \r\n");
            } else {
                //ERROR("Coprocessor data processing todo... \r\n");
            }
        }
        break;
    default:
        ;
        //ERROR("unsupported code\r\n");
    }

    //return cond_satisfy;


    //----------------------------------------------------------------------------------------------------------
    //----------------------------------------------------------------------------------------------------------
    //execute
    uint32_t operand1 = register_read(cpu, Rn);
    uint32_t operand2 = register_read(cpu, Rm);
    uint32_t rot_num = register_read(cpu, Rs);
    uint8_t add_flag = 1;
    /*
     * 0: turn off shifter
     * 3: PD(is) On
     * 4: DP(rs) On
     * 5: DP(i) On
    */
    uint8_t shifter_flag = 0;
    uint32_t carry = 0;
    
    switch(code_type) {
    case code_is_dp2:
        //immediate
        operand2 = immediate;
        rot_num = rotate_imm<<1;
        shift = 3;  //use ROR Only
        shifter_flag = 5;  //DP(i) On
        break;
    case code_is_b:
        operand1 = b_offset;
        operand2 = register_read(cpu, 15);
        rot_num = 0;
        shifter_flag = 0;  //no need to use shift
        break;
    case code_is_ldr0:
    case code_is_ldrh1:
    case code_is_ldrsb1:
    case code_is_ldrsh1:
        operand2 = immediate;
        //no need to use shift
        shifter_flag = 0;
        break;
    case code_is_dp0:
    case code_is_ldr1:
        //immediate shift
        rot_num = shift_amount;
        shifter_flag = 3;  //PD(is) On
        break;
    case code_is_ldrsb0:
    case code_is_ldrsh0:
    case code_is_ldrh0:
        //no need to use shift
        shifter_flag = 0;
        break;
    case code_is_msr1:
        operand1 = 0;  //msr don't support
        operand2 = immediate;
        rot_num = rotate_imm<<1;
        shift = 3;
        shifter_flag = 5;  //DP(i) On
        break;
    case code_is_msr0:
        operand1 = 0;  //msr don't support
        //no need to use shift
        shifter_flag = 0;
        break;
    case code_is_dp1:
        //register shift
        //No need to use
        shifter_flag = 4;  //DP(rs) On
        break;
    case code_is_bx:
        operand1 = 0;
        rot_num = 0;
        //no need to use shift
        shifter_flag = 0;
        break;
    case code_is_ldm:
        operand2 = 0;  //increase After
        rot_num = 0;
        shifter_flag = 0;  //no need to use shift
        break;
    case code_is_mult:
        operand2 = operand2 * rot_num;
        shifter_flag = 0;  //no need to use shift
        break;
    case code_is_multl:
        do{
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
                multl_long += ( ((uint64_t)register_read(cpu, Rn) << 32) | register_read(cpu, Rd));
            }
            
            register_write(cpu, Rn, multl_long >> 32);
            register_write(cpu, Rd, multl_long&0xffffffff);
            PRINTF("write 0x%x to R%d, write 0x%x to R%d  = (0x%x) * (0x%x) \r\n",
             register_read(cpu, Rn), Rn, register_read(cpu, Rd), Rd, operand2, rot_num);
            
            if(Bit20) {
                //Update CPSR register
                uint8_t out_sign = IS_SET(multl_long, 63);
                
                cpsr_n_set(cpu, out_sign);
                cpsr_z_set(cpu, multl_long == 0);
                //cv unaffected
                PRINTF("update flag nzcv %d%d%d%d \r\n", cpsr_n(cpu), cpsr_z(cpu), cpsr_c(cpu), cpsr_v(cpu));
            }
            
            return;
        }while(0);
        break;
    case code_is_swi:
        cpu->decoder.event_id = EVENT_ID_SWI;
        return;
        break;
    case code_is_mrs:
        shifter_flag = 0;  //no need to use shift
        break;
    case code_is_mcr:
        do{
            shifter_flag = 0;  //no need to use shift
            uint8_t op2 = (dec->instruction_word >> 5)&0x7;
            //  cp_num       Rs
            //  register     Rd
            //  cp_register  Rn
            if(Bit20) {
                //mrc
                if(Rs == 15) {
                    uint32_t cp15_val;
                    if(Rn) {
                        cp15_val = cp15_read(&cpu->mmu, Rn);
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
                    register_write(cpu, Rd, cp15_val);
                    PRINTF("read cp%d_c%d[0x%x] op2:%d to R%d \r\n", Rs, Rn, cp15_val, op2, Rd);
                } else {
                    ERROR("read unsupported cp%d_c%d \r\n", Rs, Rn);
                }
            } else {
                //mcr
                uint32_t register_val = register_read(cpu, Rd);
                if(Rs == 15) {
                    cp15_write(&cpu->mmu, Rn, register_val);
                    PRINTF("write R%d[0x%x] to cp%d_c%d \r\n", Rd, register_val, Rs, Rn);
                } else {
                    ERROR("write unsupported cp%d_c%d \r\n", Rs, Rn);
                }
            }
            return;
        }while(0);
        break;
    case code_is_swp:
        do {
            /* op1, Rn
             * op2, Rm
             */
            uint32_t tmp = read_word(cpu, operand1);
            if(mmu_check_status(&cpu->mmu)) {
                cpu->decoder.event_id = EVENT_ID_DATAABT;
                return;
            }
            write_word(cpu, operand1, operand2);
            register_write(cpu, Rd, tmp);
            
            return;
        }while(0);
        break;
    default:
        cpu->decoder.event_id = EVENT_ID_UNDEF;
        return;
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
                    shifter_carry_out = cpsr_c(cpu);
                } else {
                    shifter_carry_out = IS_SET(operand2, 32 - rot_num);
                    operand2 = operand2 << rot_num;
                }
            } else if(shifter_flag == 4) {
                //DP(rs) On
                if((rot_num&0xff) == 0) {
                    shifter_carry_out = cpsr_c(cpu);
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
                    shifter_carry_out = cpsr_c(cpu);
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
                    shifter_carry_out = cpsr_c(cpu);
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
                     shifter_carry_out = cpsr_c(cpu);
                } else {
                    shifter_carry_out = IS_SET(operand2, 31);
                }   
            } else if(shifter_flag == 3) {
                //PD(is) ROR On
                if(rot_num == 0) {
                    //RRX
                    shifter_carry_out = operand2&1;
                    operand2 = (cpsr_c(cpu) << 31) | (operand2 >> 1);
                } else {
                    //ROR
                    shifter_carry_out = IS_SET(operand2, rot_num-1);
                    operand2 = (operand2 << (32 - rot_num) | operand2 >> rot_num);
                }
            } else if(shifter_flag == 4) {
                //DP(rs) ROR On
                if((rot_num&0xff) == 0) {
                    shifter_carry_out = cpsr_c(cpu);
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
    
    switch(code_type) {
    case code_is_dp0:
    case code_is_dp1:
    case code_is_dp2:
        switch(opcode) {
        case 0:
        case 8:
            //AND, TST
            operand1 &= operand2;
            operand2 = 0;
            break;
        case 1:
        case 9:
            //EOR, TEQ
            operand1 ^= operand2;
            operand2 = 0;
            break;
        case 2:
        case 10:
            //SUB, CMP
            add_flag = 0;
            break;
        case 3:
            //RSB
            {
                uint32_t tmp = operand1;
                operand1 = operand2;
                operand2 = tmp;
            }
            add_flag = 0;
            break;
        case 4:
        case 11:
            //ADD, CMN
            break;
        case 5:
            //ADC
            carry = cpsr_c(cpu);  //todo...
            break;
        case 6:
            //SBC
            carry = !cpsr_c(cpu);  //todo...
            add_flag = 0;
            break;
        case 7:
            //RSC
            carry = !cpsr_c(cpu);  //todo...
            uint32_t tmp = operand1;
            operand1 = operand2;
            operand2 = tmp;
            add_flag = 0;
            break;
        case 12:
            //ORR
            operand1 |= operand2;
            operand2 = 0;
            break;
        case 13:
            //MOV
            operand1 = 0;
            break;
        case 14:
            //BIC
            operand1 &= ~operand2;
            operand2 = 0;
            break;
        case 15:
            //MVN
            operand1 = 0;
            operand2 = ~operand2;
            break;
        default:
            break;
        }
        break;
    case code_is_ldr0:
    case code_is_ldr1:
    case code_is_ldrsb1:
    case code_is_ldrsh1:
    case code_is_ldrh1:
    case code_is_ldrsb0:
    case code_is_ldrsh0:
    case code_is_ldrh0:
        if(Uf)
            add_flag = 1;
        else
            add_flag = 0;
        break;
    case code_is_mult:
        switch(opcode) {
        case 1:
            operand1 = register_read(cpu, Rd);  //Rd and Rn swap, !!!
            break;
        case 0:
            operand1 = 0;
            break;
        }
        break;
    default:
        break;
    }
    
    /**************************alu*************************/
    
    uint32_t sum_middle = add_flag?
        ((operand1&0x7fffffff) + (operand2&0x7fffffff) + carry):
        ((operand1&0x7fffffff) - (operand2&0x7fffffff) - carry);
    uint32_t cy_high_bits =  add_flag?
        (IS_SET(operand1, 31) + IS_SET(operand2, 31) + IS_SET(sum_middle, 31)):
        (IS_SET(operand1, 31) - IS_SET(operand2, 31) - IS_SET(sum_middle, 31));
    uint32_t aluout = ((cy_high_bits&1) << 31) | (sum_middle&0x7fffffff);
    PRINTF("[ALU]op1: 0x%x, sf_op2: 0x%x, c: %d, out: 0x%x, ", operand1, operand2, carry, aluout);
    
    /**************************alu end*********************/

    switch(code_type) {
    case code_is_dp0:
    case code_is_dp1:
    case code_is_dp2:
        if(Bit24_23 == 2) {
            PRINTF("update flag only \r\n");
        } else {
            register_write(cpu, Rd, aluout);
            PRINTF("write register R%d = 0x%x\r\n", Rd, aluout);
        }
        if(Bit20) {
            //Update CPSR register
            uint8_t out_sign = IS_SET(aluout, 31);
            
            cpsr_n_set(cpu, out_sign);
            cpsr_z_set(cpu, aluout == 0);
            
            uint8_t bit_cy = (cy_high_bits>>1)&1;
            uint8_t bit_ov = (bit_cy^IS_SET(sum_middle, 31))&1;
            
            if(opcode == 11 || opcode == 4 || opcode == 5) {
                //CMN, ADD, ADC
                cpsr_c_set(cpu, bit_cy );
                cpsr_v_set(cpu, bit_ov );
            } else if(opcode == 10 || opcode == 2 || opcode == 6 || opcode == 3 || opcode == 7) {
                //CMP, SUB, SBC, RSB, RSC
                cpsr_c_set(cpu, !bit_cy );
                cpsr_v_set(cpu, bit_ov );
            } else if(shifter_flag) {
                //TST, TEQ, ORR, MOV, MVN
                cpsr_c_set(cpu, shifter_carry_out );
                //c shifter_carry_out
                //v unaffected
            }
            
            PRINTF("[DP] update flag nzcv %d%d%d%d \r\n", cpsr_n(cpu), cpsr_z(cpu), cpsr_c(cpu), cpsr_v(cpu));
            
            if(Rd == 15 && Bit24_23 != 2) {
                uint8_t cpu_mode = get_cpu_mode_code(cpu);
                cpsr(cpu) = cpu->spsr[cpu_mode];
                PRINTF("[DP] cpsr 0x%x copy from spsr %d \r\n", cpsr(cpu), cpu_mode);
            }
        }
        break;
    case code_is_bx:
    case code_is_b:
        if(Lf) {
            register_write(cpu, 14, register_read(cpu, 15) - 4);  //LR register
            PRINTF("write register R%d = 0x%0x, ", 14, register_read(cpu, 14));
        }
        
        if(aluout&3) {
            ERROR("thumb unsupport \r\n");
        }
        register_write(cpu, 15, aluout & 0xfffffffc);  //PC register
        PRINTF("write register R%d = 0x%x \r\n", 15, aluout);
        break;
    case code_is_ldr0:
    case code_is_ldr1:
    case code_is_ldrsb1:
    case code_is_ldrsh1:
    case code_is_ldrh1:
    case code_is_ldrsb0:
    case code_is_ldrsh0:
    case code_is_ldrh0:
    {
        uint32_t address = operand1;  //Rn
        uint32_t data;
        if(Pf) {
            //first add
            address = aluout;  //aluout
        }
        
        uint32_t tmpcpsr = cpsr(cpu);
        if(!Pf && Wf && (code_type == code_is_ldr0 || code_type == code_is_ldr1) ) {
            //LDRT as user mode
            cpsr(cpu) &= 0x1f;
            cpsr(cpu) |= CPSR_M_USR;
        }
        
        if(Lf) {
            //LDR
            if(code_type == code_is_ldrh1 || code_type == code_is_ldrh0) {
                //Halfword
                data = read_halfword(cpu, address);
            } else if(code_type == code_is_ldrsh1 || code_type == code_is_ldrsh0) {
                //Halfword Signed
                data = read_halfword(cpu, address);
                if(data&0x8000) {
                    data |= 0xffff0000;
                }
            } else if(code_type == code_is_ldrsb1 || code_type == code_is_ldrsb0) {
                //Byte Signed
                data = read_byte(cpu, address);
                if(data&0x80) {
                    data |= 0xffffff00;
                }
            } else if(Bf) {
                //Byte
                data = read_byte(cpu, address);
            } else {
                //Word
                data = read_word(cpu, address);
            }
            
            if(mmu_check_status(&cpu->mmu)) {
                if(!Pf && Wf && (code_type == code_is_ldr0 || code_type == code_is_ldr1) ) {
                    //LDRT restore cpsr
                    cpsr(cpu) = tmpcpsr;
                }
                cpu->decoder.event_id = EVENT_ID_DATAABT;
                return;
            }
            register_write(cpu, Rd, data);
            PRINTF("load data [0x%x]:0x%x to R%d \r\n", address, register_read(cpu, Rd), Rd);
        } else {
            //STR
            data = register_read(cpu, Rd);
            if(code_type == code_is_ldrh1 || code_type == code_is_ldrh0) {
                //Halfword
                write_halfword(cpu, address, data);
            } else if(code_type == code_is_ldrsh1 || code_type == code_is_ldrsh0) {
                //Halfword Signed ?
                write_halfword(cpu, address, data);
                printf("STRSH ? \r\n");
                exception_out(cpu);
            } else if(code_type == code_is_ldrsb1 || code_type == code_is_ldrsb0) {
                //Byte Signed ?
                write_byte(cpu, address, data);
                printf("STRSB ? \r\n");
                exception_out(cpu);
            } else if(Bf) {
                write_byte(cpu, address, data);
            } else {
                write_word(cpu, address, data);
            }
            if(mmu_check_status(&cpu->mmu)) {
                if(!Pf && Wf && (code_type == code_is_ldr0 || code_type == code_is_ldr1) ) {
                    //LDRT restore cpsr
                    cpsr(cpu) = tmpcpsr;
                }
                cpu->decoder.event_id = EVENT_ID_DATAABT;
                return;
            }
            PRINTF("store data [R%d]:0x%x to 0x%x \r\n", Rd, data, address);
        }
        if(!(!Wf && Pf)) {
            //Update base register
            register_write(cpu, Rn, aluout);
            PRINTF("[LDR]write register R%d = 0x%x\r\n", Rn, aluout);
        }
        
        if(!Pf && Wf && (code_type == code_is_ldr0 || code_type == code_is_ldr1) ) {
            //LDRT restore cpsr
            cpsr(cpu) = tmpcpsr;
        }
    }
        break;
    case code_is_ldm:
        if(Lf) { //bit [20]
            //LDM
            uint8_t ldm_type = 0;
            if(Bit22 && !IS_SET(dec->instruction_word, 15)) {
                //LDM(2)
                ldm_type = 1;
            }
            
            uint8_t pc_include_flag = 0;
            uint32_t address = operand1;
            for(int i=0; i<16; i++) {
                int n = (Uf)?i:(15-i);
                if(IS_SET(dec->instruction_word, n)) {
                    if(Pf) {
                        if(Uf) address += 4;
                        else address -= 4;
                    }
                    uint32_t data = read_word(cpu, address);
                    if(mmu_check_status(&cpu->mmu)) {
                        cpu->decoder.event_id = EVENT_ID_DATAABT;
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
                        register_write_user_mode(cpu, n, data);
                    } else {
                        //LDM(1)
                        register_write(cpu, n, data);
                    }
                    
                    PRINTF("[LDM] load data [0x%x]:0x%x to R%d \r\n", address, data, n);
                    if(!Pf) {
                        if(Uf) address += 4;
                        else address -= 4;
                    }
                }
            }
            
            if(Wf) {  //bit[21] W
                register_write(cpu, Rn, address);
                PRINTF("[LDM] write R%d = 0x%0x \r\n", Rn, address);
            }
            
            if(pc_include_flag) {
                //LDM(3) copy spsr to cpsr
                uint8_t cpu_mode = get_cpu_mode_code(cpu);
                cpsr(cpu) = cpu->spsr[cpu_mode];
                PRINTF("ldm(3) cpsr 0x%x copy from spsr %d\r\n", cpsr(cpu), cpu_mode);
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
                if(IS_SET(dec->instruction_word, n)) {
                    if(Pf) {
                        if(Uf) address += 4;
                        else address -= 4;
                    }
                    if(stm_type) {
                        //STM(2)
                        write_word(cpu, address, register_read_user_mode(cpu, n));
                    } else {
                        //STM(1)
                        write_word(cpu, address, register_read(cpu, n));
                    }
                    
                    PRINTF("[STM] store data [R%d]:0x%x to 0x%x \r\n", n, register_read(cpu, n), address);
                    if(mmu_check_status(&cpu->mmu)) {
                        cpu->decoder.event_id = EVENT_ID_DATAABT;
                        return;
                    }
                    
                    if(!Pf) {
                        if(Uf) address += 4;
                        else address -= 4;
                    }
                }
            }
            
            if(Wf) {  //bit[21] W
                register_write(cpu, Rn, address);
                PRINTF("[STM] write R%d = 0x%0x \r\n", Rn, address);
            }
        }
        break;
    case code_is_msr0:
    case code_is_msr1:
    {
        uint8_t cpu_mode;
        if(!Bit22) {
            //write cpsr
            cpu_mode = 0;
            if(cpsr_m(cpu) == CPSR_M_USR && (Rn&0x7) ) {
                printf("[MSR] cpu is user mode, flags field can write only\r\n");
                exception_out(cpu);
            }
        } else {
            //write spsr
            cpu_mode = get_cpu_mode_code(cpu);
            if(cpu_mode == 0) {
                printf("[MSR] user mode has not spsr\r\n");
                exception_out(cpu);
            }
        }
        if(IS_SET(Rn, 0) ) {
            cpu->spsr[cpu_mode] &= 0xffffff00;
            cpu->spsr[cpu_mode] |= aluout&0xff;
        }
        if(IS_SET(Rn, 1) ) {
            cpu->spsr[cpu_mode] &= 0xffff00ff;
            cpu->spsr[cpu_mode] |= aluout&0xff00;
        }
        if(IS_SET(Rn, 2) ) {
            cpu->spsr[cpu_mode] &= 0xff00ffff;
            cpu->spsr[cpu_mode] |= aluout&0xff0000;
        }
        if(IS_SET(Rn, 3) ) {
            cpu->spsr[cpu_mode] &= 0x00ffffff;
            cpu->spsr[cpu_mode] |= aluout&0xff000000;
        }
        PRINTF("write register spsr_%d = 0x%x\r\n", cpu_mode, cpu->spsr[cpu_mode]);
    }
        break;
    case code_is_mult:
        register_write(cpu, Rn, aluout);   //Rd and Rn swap, !!!
        PRINTF("[MULT] write register R%d = 0x%x\r\n", Rn, aluout);
        
        if(Bit20) {
            //Update CPSR register
            uint8_t out_sign = IS_SET(aluout, 31);
            
            cpsr_n_set(cpu, out_sign);
            cpsr_z_set(cpu, aluout == 0);
            //cv unaffected
            PRINTF("update flag nzcv %d%d%d%d \r\n", cpsr_n(cpu), cpsr_z(cpu), cpsr_c(cpu), cpsr_v(cpu));
        }
        break;
    case code_is_mrs:
        if(Bit22) {
            uint8_t mode = get_cpu_mode_code(cpu);
            register_write(cpu, Rd, cpu->spsr[mode]);
            PRINTF("write register R%d = [spsr]0x%x\r\n", Rd, cpu->spsr[mode]);
        } else {
            register_write(cpu, Rd, cpsr(cpu));
            PRINTF("write register R%d = [cpsr]0x%x\r\n", Rd, cpsr(cpu));
        }
        break;
    default:
        ERROR("unsupport code %d\r\n", code_type );
        break;
    }
}





void cpu_init(struct armv4_cpu_t *cpu)
{
    memset(cpu, 0, sizeof(struct armv4_cpu_t));
    cpsr(cpu) = CPSR_M_SVC;
    cpsr_i_set(cpu, 1);  //disable irq
    cpsr_f_set(cpu, 1);  //disable fiq
    cp15_reset(&cpu->mmu);
}

void fetch(struct armv4_cpu_t *cpu)
{
    uint32_t pc = register_read(cpu, 15) - 4; //current pc
    if(((pc&3) != 0)) {
        PRINTF("[FETCH] error, pc[0x%x] overflow \r\n", pc);
        exception_out(cpu);
    }
    
    cpu->decoder.instruction_word = read_word(cpu, pc);
    PRINTF("[0x%04x]: ", pc);
    register_write(cpu, 15, pc + 4 ); //current pc add 4
    if(mmu_check_status(&cpu->mmu)) {
        cpu->decoder.event_id = EVENT_ID_PREAABT;
    }
}


/*
 * exception_out
 * author:hxdyxd
 */
void peripheral_register(struct armv4_cpu_t *cpu, struct peripheral_link_t *link, int number)
{
    cpu->peripheral.link = link;
    cpu->peripheral.number = number;
    for(int i=0; i<cpu->peripheral.number; i++) {
        if(cpu->peripheral.link[i].reset) {
            if(cpu->peripheral.link[i].reset(cpu->peripheral.link[i].reg_base)) {
                WARN("[%d]%s register at 0x%08x, size %d\n",
                    i, cpu->peripheral.link[i].name, cpu->peripheral.link[i].prefix, (~cpu->peripheral.link[i].mask)+1);
            } else {
                cpu->peripheral.link[i].reset = NULL;
                cpu->peripheral.link[i].read = NULL;
                cpu->peripheral.link[i].write = NULL;
            }
        }
    }
}


/*****************************END OF FILE***************************/
