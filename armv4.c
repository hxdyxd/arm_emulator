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

#define  BSET(b)  (1<<(b))
#define  BMASK(v,m,r)  (((v)&(m)) == (r))


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


static inline uint32_t cp15_read(struct mmu_t *mmu, uint8_t op2, uint8_t CRn)
{
    if(!CRn && op2) {
        //Cache Type register
        return 0;
    }
    return mmu->reg[CRn];
}

static inline void cp15_write(struct mmu_t *mmu, uint8_t op2, uint8_t CRn, uint32_t val)
{
    mmu->reg[CRn] = val;
}

static inline void cp15_reset(struct mmu_t *mmu)
{
    memset(mmu->reg, 0, sizeof(mmu->reg));
    mmu->reg[0] = (0x41 << 24) | (0x0 << 20) | (0x2 << 16) | (0x920 << 4) | 0x5; //Main ID register
}


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
static inline uint32_t mmu_transfer(struct armv4_cpu_t *cpu, uint32_t vaddr, uint8_t mask,
 uint8_t privileged, uint8_t wr)
{
    struct mmu_t *mmu = &cpu->mmu;
    mmu->mmu_fault = 0;
    if(!cp15_ctl_m(mmu)) {
        return vaddr;
    }
    if(cp15_ctl_a(mmu) && (vaddr&mask)) {
        //Check address alignment
        cp15_fsr(mmu) = 0x1;
        cp15_far(mmu) = vaddr;
        mmu->mmu_fault = 1;
        PRINTF("address alignment fault va = 0x%x\n", vaddr);
        return 0xffffffff;
    }
    //section page table, store size 16KB
    uint32_t page_table_entry = read_word_without_mmu(cpu,
     (cp15_ttb(mmu)&0xFFFFC000) | ((vaddr&0xFFF00000) >> 18));

    uint8_t domain = (page_table_entry>>5)&0xf; //Domain field
    uint8_t domainval =  domain << 1;
    domainval = (cp15_domain(mmu) >> domainval)&0x3;

    uint8_t first_level_descriptor = page_table_entry&3;
    switch(first_level_descriptor) {
    case 0:
        //Section translation fault
        cp15_fsr(mmu) = 0x5;
        cp15_far(mmu) = vaddr;
        mmu->mmu_fault = 1;
        PRINTF("Section translation fault va = 0x%x\n", vaddr);
        break;
    case 2:
        //section entry
        switch(domainval) {
        case 0:
            //Section domain fault
            cp15_fsr(mmu) = (domain << 4) | 0x9;
            cp15_far(mmu) = vaddr;
            mmu->mmu_fault = 1;
            PRINTF("Section domain fault\n");
            break;
        case 1:
            do {
                //Client, Check access permissions
                uint8_t ap = (page_table_entry>>10)&0x3;
                if(mmu_check_access_permissions(mmu, ap, privileged, wr) == 0)
                    return (page_table_entry&0xFFF00000)|(vaddr&0x000FFFFF);
                //Section permission fault
                cp15_fsr(mmu) = (domain << 4) | 0xd;
                cp15_far(mmu) = vaddr;
                mmu->mmu_fault = 1;
                PRINTF("Section permission fault\n");
            }while(0);
            break;
        case 3:
            //Manager
            return (page_table_entry&0xFFF00000)|(vaddr&0x000FFFFF);
        default:
            WARN("mmu fault\n");
            exception_out(cpu);
            break;
        }
        break;
    default:
        //page table
        if(first_level_descriptor == 1) {
            //coarse page table, store size 1KB
            page_table_entry = read_word_without_mmu(cpu,
             (page_table_entry&0xFFFFFC00)|((vaddr&0x000FF000) >> 10));
        } else if(first_level_descriptor == 3) {
            //fine page table, store size 4KB
            page_table_entry = read_word_without_mmu(cpu,
             (page_table_entry&0xFFFFF000)|((vaddr&0x000FFC00) >> 8));
        }
        uint8_t second_level_descriptor = page_table_entry&3;
        if(second_level_descriptor == 0) {
            //Page translation fault
            cp15_fsr(mmu) = (domain << 4) | 0x7;
            cp15_far(mmu) = vaddr;
            mmu->mmu_fault = 1;
            PRINTF("Page translation fault va = 0x%x %d %d pte = 0x%x\n",
             vaddr, privileged, wr, page_table_entry);
        } else {
            switch(domainval) {
            case 0:
                //Page domain fault
                cp15_fsr(mmu) = (domain << 4) | 0xb;
                cp15_far(mmu) = vaddr;
                mmu->mmu_fault = 1;
                WARN("Page domain fault\n");
                break;
            case 1:
                //Client, Check access permissions
                switch(second_level_descriptor) {
                case 1:
                    do {
                        //large page, 64KB
                        uint8_t subpage = (vaddr >> 14)&3;
                        uint8_t ap = (page_table_entry >> ((subpage<<1)+4))&0x3;
                        if(mmu_check_access_permissions(mmu, ap, privileged, wr) == 0)
                            return (page_table_entry&0xFFFF0000)|(vaddr&0x0000FFFF);
                    }while(0);
                    break;
                case 2:
                    do {
                        //small page, 4KB
                        uint8_t subpage = (vaddr >> 10)&3;
                        uint8_t ap = (page_table_entry >> ((subpage<<1)+4))&0x3;
                        if(mmu_check_access_permissions(mmu, ap, privileged, wr) == 0)
                            return (page_table_entry&0xFFFFF000)|(vaddr&0x00000FFF);
                    }while(0);
                    break;
                case 3:
                    do {
                        //tiny page, 1KB
                        uint8_t ap = (page_table_entry>>4)&0x3; //ap0
                        if(mmu_check_access_permissions(mmu, ap, privileged, wr) == 0)
                            return (page_table_entry&0xFFFFFC00)|(vaddr&0x000003FF);
                    }while(0);
                    break;
                }
                //Sub-page permission fault
                cp15_fsr(mmu) = (domain << 4) | 0xf;
                cp15_far(mmu) = vaddr;
                mmu->mmu_fault = 1;
                PRINTF("Sub-page permission fault va = 0x%x %d %d\n", vaddr, privileged, wr);
                break;
            case 3:
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
                break;
            default:
                printf("mmu fault\n");
                exception_out(cpu);
                break;
            } /* end switch(domainval) */
        } /* end if(second_level_descriptor == 0) */
        break;
    }
    return 0xffffffff;
}

/****cp15 end***********************************************************/

/*
 * read_mem
 * author:hxdyxd
 */
uint32_t read_mem(struct armv4_cpu_t *cpu, uint8_t privileged, uint32_t address,
 uint8_t mmu, uint8_t mask)
{
    if(mmu) {
        address = mmu_transfer(cpu, address, mask, privileged, 0); //read
        if(mmu_check_status(&cpu->mmu))
            return 0xffffffff;
    }
    
    //1M Peripheral memory
    for(int i=0; i<cpu->peripheral.number; i++) {
        if(cpu->peripheral.link[i].read && BMASK(address, cpu->peripheral.link[i].mask, cpu->peripheral.link[i].prefix)) {
            uint32_t offset = address - cpu->peripheral.link[i].prefix;
            return cpu->peripheral.link[i].read(cpu->peripheral.link[i].reg_base, offset);
        }
    }

    if(address == 0x4001f030) {
        //code counter
        return cpu->code_counter;
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
    address = mmu_transfer(cpu, address, mask, privileged, 1); //write
    if(mmu_check_status(&cpu->mmu))
        return;
    
    //4G Peripheral memory
    for(int i=0; i<cpu->peripheral.number; i++) {
        if(cpu->peripheral.link[i].write && BMASK(address, cpu->peripheral.link[i].mask, cpu->peripheral.link[i].prefix)) {
            uint32_t offset = address - cpu->peripheral.link[i].prefix;
            cpu->peripheral.link[i].write(cpu->peripheral.link[i].reg_base, offset, data, mask);
            return;
        }
    }
    if(address == 0x4001f030) {
        //ips
        cpu->code_counter = data;
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


#define SHIFTS_MODE_IMMEDIATE         (1)
#define SHIFTS_MODE_REGISTER          (2)
#define SHIFTS_MODE_32BIT_IMMEDIATE   (3)

static uint8_t shifter(struct armv4_cpu_t *cpu, uint32_t *op, const uint8_t rot_num,
 const uint8_t shift, const uint8_t shifts_mode)
{
    uint8_t shifter_carry_out = 0;
    uint32_t operand2 = *op;
    switch(shift) {
    case 0:
        //LSL
        //5.1.4, 5.1.5, 5.1.6
        if(rot_num == 0) {
            shifter_carry_out = cpsr_c(cpu);
        } else if(rot_num < 32) {
            shifter_carry_out = IS_SET(operand2, 32 - rot_num);
            operand2 = operand2 << rot_num;
        } else if(rot_num == 32) {
            shifter_carry_out = operand2&1;
            operand2 = 0;
        } else /* operand2 > 32 */ {
            shifter_carry_out = 0;
            operand2 = 0;
        }

        break;
    case 1:
        //LSR
        switch(shifts_mode) {
        case SHIFTS_MODE_IMMEDIATE:
            //PD(is) On, 5.1.7
            if(rot_num == 0) {
                shifter_carry_out = IS_SET(operand2, 31);
                operand2 = 0;
            } else {
                shifter_carry_out = IS_SET(operand2, rot_num-1);
                operand2 = operand2 >> rot_num;
            }
            break;
        case SHIFTS_MODE_REGISTER:
            //DP(rs) On, 5.1.8
            if(rot_num == 0) {
                shifter_carry_out = cpsr_c(cpu);
            } else if(rot_num < 32) {
                shifter_carry_out = IS_SET(operand2, rot_num-1);
                operand2 = operand2 >> rot_num;
            } else if(rot_num == 32) {
                //If n is 32 or more, then all the bits in the result are cleared to 0.
                shifter_carry_out = IS_SET(operand2, 31);
                operand2 = 0;
            } else /* operand2 > 32 */ {
                shifter_carry_out = 0;
                operand2 = 0;
            }
            break;
        }

        break;
    case 2:
        //ASR
        switch(shifts_mode) {
        case SHIFTS_MODE_IMMEDIATE:
            //PD(is) On, 5.1.9
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
            break;
        case SHIFTS_MODE_REGISTER:
            //DP(rs) On, 5.1.10
            if(rot_num == 0) {
                shifter_carry_out = cpsr_c(cpu);
            } else if(rot_num < 32) {
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
            break;
        }
        
        break;
    case 3:
        //ROR, RRX
        switch(shifts_mode) {
        case SHIFTS_MODE_32BIT_IMMEDIATE:
            //DP(i), 5.1.3
            operand2 = (operand2 << (32 - rot_num) | operand2 >> rot_num);
            if(rot_num == 0) {
                shifter_carry_out = cpsr_c(cpu);
            } else {
                shifter_carry_out = IS_SET(operand2, 31);
            }
            break;
        case SHIFTS_MODE_IMMEDIATE:
            //PD(is) ROR On
            if(rot_num == 0) {
                //RRX, 5.1.13
                shifter_carry_out = operand2&1;
                operand2 = (cpsr_c(cpu) << 31) | (operand2 >> 1);
            } else {
                //ROR, 5.1.11
                shifter_carry_out = IS_SET(operand2, rot_num-1);
                operand2 = (operand2 << (32 - rot_num) | operand2 >> rot_num);
            }
            break;
        case SHIFTS_MODE_REGISTER:
            //DP(rs) ROR On
            //ROR, 5.1.12
            if(rot_num == 0) {
                shifter_carry_out = cpsr_c(cpu);
            } else if((rot_num&0x1f) == 0) {
                shifter_carry_out = IS_SET(operand2, 31);
            } else /* rot_num > 0 */ {
                uint8_t rot_num_5bit = rot_num & 0x1f; //5bit
                shifter_carry_out = IS_SET(operand2, rot_num_5bit-1);
                operand2 = (operand2 << (32 - rot_num_5bit) | operand2 >> rot_num_5bit);
            }
            break;
        }
        
        break;
    }
    *op = operand2;
    return shifter_carry_out;
}



#define ALU_MODE_ADD     (1)
#define ALU_MODE_SUB     (2)

static uint32_t adder(const uint32_t op1, const uint32_t op2, const uint32_t carry,
 const uint8_t add_mode, uint8_t *bit_cy, uint8_t *bit_ov)
{
    uint32_t sum_middle = 0, cy_high_bits = 0;
    uint32_t aluout = 0;
    if(add_mode == ALU_MODE_ADD) {
        sum_middle = (op1&0x7fffffff) + (op2&0x7fffffff) + carry;
        cy_high_bits =  IS_SET(op1, 31) + IS_SET(op2, 31) + IS_SET(sum_middle, 31);

        aluout = (cy_high_bits << 31) | (sum_middle&0x7fffffff);
        *bit_cy = (cy_high_bits >> 1)&1;
        *bit_ov = (*bit_cy ^ IS_SET(sum_middle, 31))&1;
    } else if(add_mode == ALU_MODE_SUB) {
        sum_middle = (op1&0x7fffffff) - (op2&0x7fffffff) - carry;
        cy_high_bits =  IS_SET(op1, 31) - IS_SET(op2, 31) - IS_SET(sum_middle, 31);

        aluout = (cy_high_bits << 31) | (sum_middle&0x7fffffff);
        *bit_cy = (cy_high_bits >> 1)&1;
        *bit_ov = (*bit_cy ^ IS_SET(sum_middle, 31))&1;
        *bit_cy = !(*bit_cy);
    }
    
    PRINTF("[ALU]op1: 0x%x, sf_op2: 0x%x, c: %d, out: 0x%x, ", op1, op2, carry, aluout);
    return aluout;
}


//**************************************************
//******************decode**************************
//**************************************************

struct ins_dp_immediate_shift {
    uint32_t Rm:4;
    uint32_t bit4:1; //0
    uint32_t shift:2;
    uint32_t shift_amount:5;
    uint32_t Rd:4;
    uint32_t Rn:4;
    uint32_t S:1;
    uint32_t opcode:4;
    uint32_t f:3; //0
    uint32_t cond:4;
};

struct ins_dp_register_shift {
    uint32_t Rm:4;
    uint32_t bit4:1; //1
    uint32_t shift:2;
    uint32_t bit7:1; //0
    uint32_t Rs:4;
    uint32_t Rd:4;
    uint32_t Rn:4;
    uint32_t S:1;
    uint32_t opcode:4;
    uint32_t f:3; //0
    uint32_t cond:4;
};

struct ins_dp_immediate {
    uint32_t immediate:8;
    uint32_t rotate:4;
    uint32_t Rd:4;
    uint32_t Rn:4;
    uint32_t S:1;
    uint32_t opcode:4;
    uint32_t f:3; //1
    uint32_t cond:4;
};

struct ins_ldr_immediate_offset {
    uint32_t immediate:12;
    uint32_t Rd:4;
    uint32_t Rn:4;
    uint32_t L:1;
    uint32_t W:1;
    uint32_t B:1;
    uint32_t U:1;
    uint32_t P:1;
    uint32_t f:3; //2
    uint32_t cond:4;
};

struct ins_ldr_register_offset {
    uint32_t Rm:4;
    uint32_t bit4:1;
    uint32_t shift:2;
    uint32_t shift_amount:5;
    uint32_t Rd:4;
    uint32_t Rn:4;
    uint32_t L:1;
    uint32_t W:1;
    uint32_t B:1;
    uint32_t U:1;
    uint32_t P:1;
    uint32_t f:3; //3
    uint32_t cond:4;
};

struct ins_ldm {
    uint32_t list:16;
    uint32_t Rn:4;
    uint32_t L:1;
    uint32_t W:1;
    uint32_t S:1;
    uint32_t U:1;
    uint32_t P:1;
    uint32_t f:3; //4
    uint32_t cond:4;
};

struct ins_b {
    uint32_t offset22_0:23;
    uint32_t offset23:1;
    uint32_t L:1;
    uint32_t f:3; //5
    uint32_t cond:4;
};

struct ins_swi {
    uint32_t number:24;
    uint32_t bit24:1;
    uint32_t f:3; //5
    uint32_t cond:4;
};

struct ins_mcr {
    uint32_t CRm:4;
    uint32_t bit4:1; //1
    uint32_t opcode2:3;
    uint32_t cp_num:4;
    uint32_t Rd:4;
    uint32_t CRn:4;
    uint32_t bit20:1; //0
    uint32_t opcode1:4;
    uint32_t f:3; //7
    uint32_t cond:4;
};

struct ins_bit {
    uint32_t bit3_0:4;
    uint32_t bit4:1;
    uint32_t bit5:1;
    uint32_t bit6:1;
    uint32_t bit7:1;
    uint32_t bit19_8:12;
    uint32_t bit20:1;
    uint32_t bit21:1;
    uint32_t bit22:1;
    uint32_t bit24_23:2;
    uint32_t bit31_25:7;
};


union ins_t {
    uint32_t word;
    struct ins_dp_immediate_shift    dp_is;
    struct ins_dp_register_shift     dp_rs;
    struct ins_dp_immediate          dp_i;
    struct ins_ldr_immediate_offset  ldr_i;
    struct ins_ldr_register_offset   ldr_r;
    struct ins_ldm                   ldm;
    struct ins_b                     b;
    struct ins_swi                   swi;
    struct ins_mcr                   mcr;
    struct ins_bit                   _bit;
};

#define opcode        (ins.dp_is.opcode)
#define Bit4          (ins._bit.bit4)
#define Bit5          (ins._bit.bit5)
#define Bit6          (ins._bit.bit6)
#define Bit7          (ins._bit.bit7)
#define Bit20         (ins._bit.bit20)
#define Bit21         (ins._bit.bit21)
#define Bit22         (ins._bit.bit22)
#define Bit24_23      (ins._bit.bit24_23)
#define Rm            (ins.dp_rs.Rm)
#define Rn            (ins.dp_rs.Rn)
#define Rd            (ins.dp_rs.Rd)
#define Rs            (ins.dp_rs.Rs)
#define shift_amount  (ins.dp_is.shift_amount)
#define shift         (ins.dp_is.shift)
#define rotate_imm    (ins.dp_i.rotate)
#define Pf            (ins.ldr_r.P) //ldr,ldm
#define Uf            (ins.ldr_r.U) //ldr,ldm
#define Bf            (ins.ldr_r.B) //ldr,swp
#define Wf            (ins.ldr_r.W) //ldr,ldm
#define Lf            (ins.ldr_r.L) //ldr,ldm
#define Lf_b          (ins.b.L) //b
#define Lf_bx         (Bit5)  //bx

#define immediate_i       (ins.dp_i.immediate)
#define immediate_ldr     (ins.ldr_i.immediate)
#define immediate_extldr  ((Rs << 4) | Rm)
#define immediate_b       (((ins.b.offset23)?(ins.b.offset22_0|0xFF800000):(ins.b.offset22_0))<<2)


static inline uint8_t code_decoder(const union ins_t ins)
{
    uint8_t code_type = code_type_unknow;
    switch(ins.dp_is.f) {
    case 0:
        switch(Bit4) {
        case 0:
            if( (Bit24_23 == 2 ) && !Bit20 ) {
                //Miscellaneous instructions
                if(Bit21 && !Bit7) {
                    code_type = code_type_msr0;
                } else if(!Bit21 && !Bit7) {
                    code_type = code_type_mrs;
                } else {
                    //ERROR("undefined bit7 error \r\n");
                }
            } else {
                //Data processing register shift by immediate
                code_type = code_type_dp0;
            }
            break;
        case 1:
            switch(Bit7) {
            case 0:
                if( (Bit24_23 == 2 ) && !Bit20 ) {
                    //Miscellaneous instructions
                    if(!Bit22 && Bit21 && !Bit6 ) {
                        //Branch/exchange
                        code_type = code_type_bx;
                    } else if(Bit6 && !Bit5 ) {
                        //Enhanced DSP add/subtracts
                        //ERROR("Enhanced DSP add/subtracts R%d\r\n", Rm);
                    } else if(Bit6 && Bit5) {
                        //Software breakpoint
                        //ERROR("Software breakpoint \r\n");
                    } else if(!Bit6 && !Bit5 && opcode == 0xb) {
                        //Count leading zero
                        //ERROR("Count leading zero \r\n");
                    } else {
                        //ERROR("Undefed Miscellaneous instructions\r\n");
                    }
                } else {
                    //Data processing register shift by register
                    code_type = code_type_dp1;
                }
                break;
            case 1:
                //Multiplies, extra load/storesss
                switch(shift) {
                case 0:
                    if(Bit24_23 == 0) {
                        code_type = code_type_mult;
                    } else if(Bit24_23 == 1) {
                        code_type = code_type_multl;
                    } else if(Bit24_23 == 2 && !Bit20 && !Bit21) {
                        code_type = code_type_swp;
                    }
                    break;
                case 1:
                    //code_type_ldrh
                    if(Bit22) {
                        code_type = code_type_ldrh1;
                    } else {
                        code_type = code_type_ldrh0;
                    }
                    break;
                case 2:
                    //code_type_ldrsb
                    if(Bit22) {
                        code_type = code_type_ldrsb1;
                    } else {
                        code_type = code_type_ldrsb0;
                    }
                    if(!Lf) {
                        code_type = code_type_unknow;
                        //ERROR("undefed LDRD\r\n");
                    }
                    break;
                case 3:
                    //code_type_ldrsh
                    if(Bit22) {
                        code_type = code_type_ldrsh1;
                    } else {
                        code_type = code_type_ldrsh0;
                    }
                    if(!Lf) {
                        code_type = code_type_unknow;
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
            code_type = code_type_msr1;
        } else if( (opcode == 0x8 || opcode == 0xa ) && !Bit20 ) {
            //PRINTF("undefined instruction\r\n");
        } else {
            //Data processing immediate
            code_type = code_type_dp2;
        }
        break;
    case 2:
        //load/store immediate offset
        code_type = code_type_ldr0;
        break;
    case 3:
        //load/store register offset
        if(!ins.ldr_r.bit4) {
            code_type = code_type_ldr1;
        } else {
            //ERROR("undefined instruction\r\n");
        }
        break;
    case 4:
        //load/store multiple
        code_type = code_type_ldm;
        break;
    case 5:
        //branch
        code_type = code_type_b;
        break;
    case 6:
        //Coprocessor load/store and double register transfers
        //ERROR("Coprocessor todo... \r\n");
        break;
    case 7:
        //software interrupt
        if(ins.swi.bit24) {
            code_type = code_type_swi;
        } else {
            if(Bit4) {
                //Coprocessor move to register
                code_type = code_type_mcr;
            } else {
                //ERROR("Coprocessor data processing todo... \r\n");
            }
        }
        break;
    }
    return code_type;
}


static void code_debug(const union ins_t ins, const uint8_t code_type)
{
    switch(code_type) {
    case code_type_msr0:
        PRINTF("msr0 %sPSR_%d  R%d\r\n", Bit22?"S":"C", Rn, Rm);
        break;
    case code_type_mrs:
        PRINTF("mrs R%d %s\r\n", Rd, Bit22?"SPSR":"CPSR");
        break;
    case code_type_dp0:
        PRINTF("dp0 %s dst register R%d, first operand R%d, Second operand R%d %s immediate #0x%x\r\n",
             opcode_table[opcode], Rd, Rn, Rm, shift_table[shift], shift_amount);
        break;
    case code_type_bx:
        PRINTF("bx B%sX R%d\r\n", Lf_bx?"L":"", Rm);
        break;
    case code_type_dp1:
        PRINTF("dp1 %s dst register R%d, first operand R%d, Second operand R%d %s R%d\r\n",
                 opcode_table[opcode], Rd, Rn, Rm, shift_table[shift], Rs);
        break;
    case code_type_mult:
        if( IS_SET(ins.word, 21) ) {
            PRINTF("mult MUA%s R%d <= R%d * R%d + R%d\r\n", Bit20?"S":"", Rn, Rm, Rs, Rd);
        } else {
            PRINTF("mult MUL%s R%d <= R%d * R%d\r\n", Bit20?"S":"", Rn, Rm, Rs);
        }
        break;
    case code_type_multl:
        PRINTF("multl %sM%s [R%d L, R%d H] %s= R%d * R%d\r\n", Bit22?"S":"U",
            IS_SET(ins.word, 21)?"LAL":"ULL", Rd, Rn, Bit21?"+":"", Rm, Rs);
        break;
    case code_type_swp:
        PRINTF("swp SWP%s\r\n", Bf?"B":"");
        break;
    case code_type_ldrh1:
        if(Pf) {
            PRINTF("ldrh1 %sH R%d, [R%d, immediate %s#%d] %s\r\n",
             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate_extldr, Wf?"!":"");
        } else {
            PRINTF("ldrh1 %sH R%d, [R%d], immediate %s#%d %s\r\n",
            Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate_extldr, Wf?"!":"");
        }
        break;
    case code_type_ldrh0:
        if(Pf) {
            PRINTF("ldrh0 %sH R%d, [R%d, %s[R%d] ] %s\r\n",
             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
        } else {
            PRINTF("ldrh0 %sH R%d, [R%d], %s[R%d] %s\r\n",
             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
        }
        break;
    case code_type_ldrsb1:
        if(Pf) {
            PRINTF("ldrsb1 %sSB R%d, [R%d, immediate %s#%d] %s\r\n",
             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate_extldr, Wf?"!":"");
        } else {
            PRINTF("ldrsb1 %sSB R%d, [R%d], immediate %s#%d %s\r\n",
             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate_extldr, Wf?"!":"");
        }
        break;
    case code_type_ldrsb0:
        if(Pf) {
            PRINTF("ldrsb0 %sSB R%d, [R%d, %s[R%d] ] %s\r\n",
             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
        } else {
            PRINTF("ldrsb0 %sSB R%d, [R%d], %s[R%d] %s\r\n",
             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
        }
        break;
    case code_type_ldrsh1:
        if(Pf) {
            PRINTF("ldrsh1 %sSH R%d, [R%d, immediate %s#%d] %s\r\n",
             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate_extldr, Wf?"!":"");
        } else {
            PRINTF("ldrsh1 %sSH R%d, [R%d], immediate %s#%d %s\r\n",
             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate_extldr, Wf?"!":"");
        }
        break;
    case code_type_ldrsh0:
        if(Pf) {
            PRINTF("ldrsh0 %sSH R%d, [R%d, %s[R%d] ] %s\r\n",
             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
        } else {
            PRINTF("ldrsh0 %sSH R%d, [R%d], %s[R%d] %s\r\n",
             Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
        }
        break;
    case code_type_msr1:
        PRINTF("msr1 MSR %sPSR_%d  immediate [#%d ROR %d]\r\n", Bit22?"S":"C", Rn, immediate_i, rotate_imm*2);
        break;
    case code_type_dp2:
        PRINTF("dp2 %s dst register R%d, first operand R%d, Second operand immediate [#%d ROR %d]\r\n",
         opcode_table[opcode], Rd, Rn, immediate_i, rotate_imm*2);
        break;
    case code_type_ldr0:
        if(Pf) {
            PRINTF("ldr0 %s%s dst register R%d, [base address at R%d, Second address immediate #%s0x%x] %s\r\n",
             Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-", immediate_ldr, Wf?"!":"");
        } else {
            PRINTF("ldr0 %s%s dst register R%d, [base address at R%d], Second address immediate #%s0x%x\r\n",
             Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-", immediate_ldr);
        }
        break;
    case code_type_ldr1:
        if(Pf) {
            PRINTF("ldr1 %s%s dst register R%d, [base address at R%d, offset address at %s[R%d %s %d]] %s\r\n",
             Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-",  Rm, shift_table[shift], shift_amount, Wf?"!":"");
        } else {
            PRINTF("ldr1 %s%s dst register R%d, [base address at R%d], offset address at %s[R%d %s %d] \r\n",
             Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-",  Rm, shift_table[shift], shift_amount);
        }
        break;
    case code_type_ldm:
        PRINTF("ldm %s%s%s R%d%s \r\n", Lf?"LDM":"STM", Uf?"I":"D", Pf?"B":"A",  Rn, Wf?"!":"");
        break;
    case code_type_b:
        PRINTF("B%s PC +  0x%08x\r\n", Lf_b?"L":"", immediate_b);
        break;
    case code_type_swi:
        PRINTF("swi \r\n");
    case code_type_mcr:
        PRINTF("mcr \r\n");
        break;
    }
}


static inline uint32_t code_shifter(struct armv4_cpu_t *cpu, const union ins_t ins, uint8_t code_type, uint8_t *carry)
{
    uint32_t operand2 = register_read(cpu, Rm); //3_0;
    uint32_t rot_num;
    uint8_t shifter_carry_out = 0;
    
    switch(code_type) {
    case code_type_msr1:
    case code_type_dp2:
        //immediate
        operand2 = immediate_i; //8bit
        rot_num = rotate_imm<<1;
        shifter_carry_out = shifter(cpu, &operand2, rot_num, 3, SHIFTS_MODE_32BIT_IMMEDIATE); //use ROR Only
        break;
    case code_type_dp1:
        //register shift
        rot_num = register_read(cpu, Rs); //32bit
        shifter_carry_out = shifter(cpu, &operand2, rot_num, shift, SHIFTS_MODE_REGISTER);
        break;
    case code_type_dp0:
    case code_type_ldr1:
        //immediate shift
        rot_num = shift_amount; //5bit
        shifter_carry_out = shifter(cpu, &operand2, rot_num, shift, SHIFTS_MODE_IMMEDIATE);
        break;

    case code_type_ldr0:
        operand2 = immediate_ldr; //12bit
        break;
    case code_type_ldrh1:
    case code_type_ldrsb1:
    case code_type_ldrsh1:
        operand2 = immediate_extldr; //8bit
        break;
    case code_type_ldrh0:
    case code_type_ldrsb0:
    case code_type_ldrsh0:
    case code_type_bx:
    case code_type_msr0:
    case code_type_multl:
        break;
    case code_type_mrs:
    case code_type_ldm:
    case code_type_swp:
    case code_type_mcr:
        break;
    case code_type_b:
        operand2 = immediate_b;
        break;
    case code_type_mult:
        operand2 *= register_read(cpu, Rs);
        break;

    case code_type_swi:
        cpu->decoder.event_id = EVENT_ID_SWI;
        break;
    default:
        cpu->decoder.event_id = EVENT_ID_UNDEF;
    }
    *carry = shifter_carry_out;
    return operand2;
}


static inline uint32_t code_alu(struct armv4_cpu_t *cpu, const union ins_t ins, uint8_t code_type,
 uint32_t operand1, uint32_t operand2, uint8_t *carry_out, uint8_t *bit_ov)
{
    uint8_t carry_in = 0;
    uint32_t aluout = 0;

    switch(code_type) {
    case code_type_dp0:
    case code_type_dp1:
    case code_type_dp2:
        switch(opcode) {
        case 5:
            //ADC
            carry_in = cpsr_c(cpu);
        case 4:
        case 11:
            //ADD, CMN
            aluout = adder(operand1, operand2, carry_in, ALU_MODE_ADD, carry_out, bit_ov);
            break;

        case 3:
            //RSB
            SWAP_VAL(operand1, operand2);
            aluout = adder(operand1, operand2, carry_in, ALU_MODE_SUB, carry_out, bit_ov);
            break;
        case 7:
            //RSC
            SWAP_VAL(operand1, operand2);
        case 6:
            //SBC
            carry_in = !cpsr_c(cpu);
        case 2:
        case 10:
            //SUB, CMP
            aluout = adder(operand1, operand2, carry_in, ALU_MODE_SUB, carry_out, bit_ov);
            break;

        case 0:
        case 8:
            //AND, TST
            aluout = operand1 & operand2;
            break;
        case 1:
        case 9:
            //EOR, TEQ
            aluout = operand1 ^ operand2;
            break;
        case 12:
            //ORR
            aluout = operand1 | operand2;
            break;
        case 13:
            //MOV
            aluout = operand2;
            break;
        case 14:
            //BIC
            aluout = operand1 & (~operand2);
            break;
        case 15:
            //MVN
            aluout = ~operand2;
            break;
        }
        break;
    case code_type_ldr0:
    case code_type_ldr1:
    case code_type_ldrsb1:
    case code_type_ldrsh1:
    case code_type_ldrh1:
    case code_type_ldrsb0:
    case code_type_ldrsh0:
    case code_type_ldrh0:
        aluout = adder(operand1, operand2, carry_in, Uf?ALU_MODE_ADD:ALU_MODE_SUB, carry_out, bit_ov);
        break;
    case code_type_b:
        operand1 = register_read(cpu, 15);
        aluout = adder(operand1, operand2, carry_in, ALU_MODE_ADD, carry_out, bit_ov);
        break;
    case code_type_mult:
        //Rd and Rn swap, !!!
        operand1 = opcode?register_read(cpu, Rd):0; //MLA
        aluout = adder(operand1, operand2, carry_in, ALU_MODE_ADD, carry_out, bit_ov);
        break;

    case code_type_bx:
    case code_type_msr0:
    case code_type_msr1:
        aluout = operand2;
        break;
    case code_type_ldm:
    case code_type_mrs:
    case code_type_swp:
    case code_type_swi:
    case code_type_mcr:
    case code_type_multl:
        break;
    }
    return aluout;
}


void decode(struct armv4_cpu_t *cpu)
{
    struct decoder_t *dec = &cpu->decoder;
    uint8_t cond_satisfy = 0;
    const union ins_t ins = {
        .word = dec->instruction_word,
    };
    
    switch(ins.dp_is.cond) {
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

    uint8_t code_type = code_decoder(ins);
    if(DEBUG)
        code_debug(ins, code_type);

    //----------------------------------------------------------------------------------------------------------
    //----------------------------------------------------------------------------------------------------------
    //execute
    uint8_t carry_out = cpsr_c(cpu);
    uint8_t bit_ov = cpsr_v(cpu);
    uint32_t operand1 = register_read(cpu, Rn); //19_16
    uint32_t operand2 = code_shifter(cpu, ins, code_type, &carry_out);
    uint32_t aluout = code_alu(cpu, ins, code_type, operand1, operand2, &carry_out, &bit_ov);


    switch(code_type) {
    case code_type_dp0:
    case code_type_dp1:
    case code_type_dp2:
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
            cpsr_c_set(cpu, carry_out );
            cpsr_v_set(cpu, bit_ov );
            
            PRINTF("[DP] update flag nzcv %d%d%d%d \r\n", cpsr_n(cpu), cpsr_z(cpu), cpsr_c(cpu), cpsr_v(cpu));
            
            if(Rd == 15 && Bit24_23 != 2) {
                uint8_t cpu_mode = get_cpu_mode_code(cpu);
                cpsr(cpu) = cpu->spsr[cpu_mode];
                PRINTF("[DP] cpsr 0x%x copy from spsr %d \r\n", cpsr(cpu), cpu_mode);
            }
        }
        break;
    case code_type_bx:
    case code_type_b:
        if((code_type == code_type_bx)?Lf_bx:Lf_b) {
            register_write(cpu, 14, register_read(cpu, 15) - 4);  //LR register
            PRINTF("write register R%d = 0x%0x, ", 14, register_read(cpu, 14));
        }
        
        if(aluout&3) {
            ERROR("thumb unsupport \r\n");
        }
        register_write(cpu, 15, aluout & 0xfffffffc);  //PC register
        PRINTF("write register R%d = 0x%x \r\n", 15, aluout);
        break;
    case code_type_swp:
        /* op1, Rn
         * op2, Rm
         */
        if(Bf) {
            //SWPB 4.1.52 todo...
            ERROR("swpb\n");
        } else {
            //SWP
            uint32_t aluout = read_word(cpu, operand1);
            if(mmu_check_status(&cpu->mmu)) {
                cpu->decoder.event_id = EVENT_ID_DATAABT;
                return;
            }
            write_word(cpu, operand1, operand2);
            register_write(cpu, Rd, aluout);
        }
        break;
    case code_type_ldr0:
    case code_type_ldr1:
    case code_type_ldrsb1:
    case code_type_ldrsh1:
    case code_type_ldrh1:
    case code_type_ldrsb0:
    case code_type_ldrsh0:
    case code_type_ldrh0:
    {
        uint32_t address = operand1;  //memory address
        uint32_t data;
        if(Pf) {
            address = aluout;
        }
        
        uint32_t tmpcpsr = cpsr(cpu);
        uint8_t ldrt = 0;
        if(!Pf && Wf && (code_type == code_type_ldr0 || code_type == code_type_ldr1) ) {
            //LDRT as user mode
            cpsr(cpu) &= 0x1f;
            cpsr(cpu) |= CPSR_M_USR;
            ldrt = 1;
        }
        
        if(Lf) {
            //LDR
            switch(code_type) {
            case code_type_ldrh1:
            case code_type_ldrh0:
                //Halfword
                data = read_halfword(cpu, address);
                break;
            case code_type_ldrsh1:
            case code_type_ldrsh0:
                //Halfword Signed
                data = read_halfword(cpu, address);
                if(data&0x8000) {
                    data |= 0xffff0000;
                }
                break;
            case code_type_ldrsb1:
            case code_type_ldrsb0:
                //Byte Signed
                data = read_byte(cpu, address);
                if(data&0x80) {
                    data |= 0xffffff00;
                }
                break;
            default:
                data = Bf?read_byte(cpu, address):read_word(cpu, address);
            }
            
            if(mmu_check_status(&cpu->mmu)) {
                if(ldrt) {
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
            switch(code_type) {
            case code_type_ldrh1:
            case code_type_ldrh0:
                //Halfword
                write_halfword(cpu, address, data);
                break;
            case code_type_ldrsh1:
            case code_type_ldrsh0:
                //Halfword Signed ?
                write_halfword(cpu, address, data);
                printf("STRSH ? \r\n");
                exception_out(cpu);
                break;
            case code_type_ldrsb1:
            case code_type_ldrsb0:
                //Byte Signed ?
                write_byte(cpu, address, data);
                printf("STRSB ? \r\n");
                exception_out(cpu);
                break;
            default:
                if(Bf) {
                    write_byte(cpu, address, data);
                } else {
                    write_word(cpu, address, data);
                }
            }

            if(mmu_check_status(&cpu->mmu)) {
                if(ldrt) {
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
        
        if(ldrt) {
            //LDRT restore cpsr
            cpsr(cpu) = tmpcpsr;
        }
    }
        break;
    case code_type_ldm:
        if(Lf) { //bit [20]
            //LDM
            uint8_t ldm_type = 0;
            if(Bit22 && !IS_SET(ins.word, 15)) {
                //LDM(2)
                ldm_type = 1;
            }
            
            uint8_t pc_include_flag = 0;
            uint32_t address = operand1;
            for(int i=0; i<16; i++) {
                int n = (Uf)?i:(15-i);
                if(IS_SET(ins.word, n)) {
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
                if(IS_SET(ins.word, n)) {
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
    case code_type_msr0:
    case code_type_msr1:
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
    case code_type_mult:
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
    case code_type_multl:
    {
        uint64_t multl_long = 0;
        uint8_t negative = 0;
        uint32_t rs_num = operand2;
        uint32_t rm_num = register_read(cpu, Rs);
        if(Bit22) {
            //Signed
            if(IS_SET(rm_num, 31)) {
                rm_num = ~rm_num + 1;
                negative++;
            }
            
            if(IS_SET(rs_num, 31)) {
                rs_num = ~rs_num + 1;
                negative++;
            }
        }
        
        multl_long = (uint64_t)rm_num * rs_num;
        if( negative == 1 ) {
            //Set negative sign
            multl_long = ~(multl_long - 1);
        }
        
        if(opcode&1) {
            multl_long += ( ((uint64_t)operand1 << 32) | register_read(cpu, Rd));
        }
        
        register_write(cpu, Rn, multl_long >> 32);
        register_write(cpu, Rd, multl_long&0xffffffff);
        PRINTF("write 0x%x to R%d, write 0x%x to R%d  = (0x%x) * (0x%x) \r\n",
        register_read(cpu, Rn), Rn, register_read(cpu, Rd), Rd, rm_num, rs_num);
        
        if(Bit20) {
            //Update CPSR register
            uint8_t out_sign = IS_SET(multl_long, 63);
            
            cpsr_n_set(cpu, out_sign);
            cpsr_z_set(cpu, multl_long == 0);
            //cv unaffecteds
            PRINTF("update flag nzcv %d%d%d%d \r\n", cpsr_n(cpu), cpsr_z(cpu), cpsr_c(cpu), cpsr_v(cpu));
        }
    }
        break;
    case code_type_mrs:
        if(Bit22) {
            uint8_t mode = get_cpu_mode_code(cpu);
            register_write(cpu, Rd, cpu->spsr[mode]);
            PRINTF("write register R%d = [spsr]0x%x\r\n", Rd, cpu->spsr[mode]);
        } else {
            register_write(cpu, Rd, cpsr(cpu));
            PRINTF("write register R%d = [cpsr]0x%x\r\n", Rd, cpsr(cpu));
        }
        break;
    case code_type_mcr:
        //  cp_num       Rs
        //  register     Rd
        //  cp_register  Rn
        if(Bit20) {
            //mrc
            if(ins.mcr.cp_num == 15) {
                uint32_t cp15_val = cp15_read(&cpu->mmu, ins.mcr.opcode2, ins.mcr.CRn);
                register_write(cpu, Rd, cp15_val);
                PRINTF("read cp%d_c%d[0x%x] op2:%d to R%d \r\n", Rs, ins.mcr.CRn, cp15_val, ins.mcr.opcode2, Rd);
            } else {
                ERROR("read unsupported cp%d_c%d \r\n", Rs, ins.mcr.CRn);
            }
        } else {
            //mcr
            uint32_t register_val = register_read(cpu, Rd);
            if(ins.mcr.cp_num == 15) {
                cp15_write(&cpu->mmu, ins.mcr.opcode2, ins.mcr.CRn, register_val);
                PRINTF("write R%d[0x%x] to cp%d_c%d \r\n", Rd, register_val, ins.mcr.cp_num, ins.mcr.CRn);
            } else {
                ERROR("write unsupported cp%d_c%d \r\n", Rs, ins.mcr.CRn);
            }
        }
        break;
    case code_type_swi:
        break;
    }
}



/*****************************END OF FILE***************************/
