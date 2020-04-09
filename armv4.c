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
#include <disassembly.h>


#define  BSET(b)  (1<<(b))
#define  BMASK(v,m,r)  (((v)&(m)) == (r))


static const char *string_register_mode[7] = {
    "USER",
    "FIQ",
    "IRQ",
    "SVC",
    "Undef",
    "Abort",
    "Mon",
};

uint8_t global_debug_flag = 0;

/*
 * reg_show
 * author:hxdyxd
 */
void reg_show(struct armv4_cpu_t *cpu)
{
    uint8_t cur_mode  = get_cpu_mode_code(cpu);
    WARN("PC = 0x%x , code = %u\r\n", cpu->reg[CPU_MODE_USER][15], cpu->code_counter);
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
}

/*
 * exception_out
 * author:hxdyxd
 */
static void exception_out(struct armv4_cpu_t *cpu)
{
    reg_show(cpu);
    ERROR("stop\n");
}


/*************cp15************/
static inline void tlb_invalidata(struct mmu_t *mmu, uint32_t vaddr,
 uint8_t CRm, uint8_t op2);
static inline int mmu_check_access_permissions(struct mmu_t *mmu, uint8_t ap,
 uint8_t privileged, uint8_t wr);


static inline uint32_t cp15_read(struct mmu_t *mmu, uint8_t CRn, uint8_t CRm,
 uint32_t op2)
{
    switch(CRn) {
    case 0:
        if(op2)
            return 0; //Cache Type register
        else
            return mmu->reg[CRn]; //Main ID register
    case 1:
    case 2:
    case 3:
    case 5:
    case 6:
        return mmu->reg[CRn];
    case 7:
        //Cache functions
        return 0;
    case 8:
        //TLB functions
        return 0;
    default:
        WARN("CP15: read unsupport CRn:%d CRm:%d op2%d\n", CRn, CRm, op2);
        return 0;
    }
}


//mcr p15, 0, Rd, CRn, CRm, op2
static inline void cp15_write(struct mmu_t *mmu, uint32_t Rd_val, uint8_t CRn,
 uint8_t CRm, uint32_t op2)
{
    switch(CRn) {
    case 0:
        break;
    case 1:
    case 2:
    case 3:
    case 5:
    case 6:
        mmu->reg[CRn] = Rd_val;
        break;
    case 7:
        //Cache functions
        break;
    case 8:
        //TLB functions
        tlb_invalidata(mmu, Rd_val, CRm, op2);
        break;
    default:
        WARN("CP15: write unsupport Rd_val:%08x CRn:%d CRm:%d op2%d\n",
         Rd_val, CRn, CRm, op2);
        break;
    }
}


static inline void cp15_reset(struct mmu_t *mmu)
{
    memset(mmu->reg, 0, sizeof(mmu->reg));
    memset(mmu->tlb[0], 0, sizeof(struct tlb_t)*TLB_SIZE*2);
    //Main ID register
    mmu->reg[0] = (0x41 << 24) | (0x0 << 20) | (0x2 << 16) | (0x920 << 4) | 0x5;
    tlb_set_base(mmu, TLB_I);
}


/*************tlb*****************/


void tlb_show(struct mmu_t *mmu)
{
    struct tlb_t *tlb = mmu->tlb[TLB_D];
    for(int i=0; i<TLB_SIZE; i++) {
        WARN("TLB_D: [%d] va:%08x pa:%08x type:%d\n",
         i, tlb[i].vaddr, tlb[i].paddr, tlb[i].type);
    }
    tlb =mmu->tlb[TLB_I];
    for(int i=0; i<TLB_SIZE; i++) {
        WARN("TLB_I: [%d] va:%08x pa:%08x type:%d\n",
         i, tlb[i].vaddr, tlb[i].paddr, tlb[i].type);
    }
    WARN("TLB: %d/%d = %.3f\n", mmu->tlb_hit, mmu->tlb_total,
     mmu->tlb_hit*1.0/mmu->tlb_total);
}


#define tlb_set_manager(m,v,p) _tlb_set(m,v,p,1,0)
#define tlb_set_client(m,v,p,ap) _tlb_set(m,v,p,0,ap)

static inline void _tlb_set(struct mmu_t *mmu, uint32_t vaddr, uint32_t paddr,
 uint8_t is_manager, uint8_t ap)
{
    struct tlb_t *tlb = mmu->tlb_base;
    uint32_t index = (vaddr >> 10) % TLB_SIZE;
    tlb[index].vaddr = vaddr & 0xFFFFFC00;
    tlb[index].paddr = paddr & 0xFFFFFC00;
    tlb[index].is_manager = is_manager;
    tlb[index].ap = ap;
    tlb[index].type = 1;
}


static inline void tlb_invalidata(struct mmu_t *mmu, uint32_t vaddr,
 uint8_t CRm, uint8_t op2)
{
    if(op2) {
        uint32_t index = (vaddr >> 10) % TLB_SIZE;
        if(CRm & 1)
            mmu->tlb[TLB_I][index].type = 0;
        if(CRm & 2)
            mmu->tlb[TLB_D][index].type = 0;
    } else {
        if(CRm & 1)
            memset(mmu->tlb[TLB_I], 0, sizeof(struct tlb_t)*TLB_SIZE);
        if(CRm & 2)
            memset(mmu->tlb[TLB_D], 0, sizeof(struct tlb_t)*TLB_SIZE);
    }
}


static inline uint8_t tlb_get(struct mmu_t *mmu, uint32_t vaddr, uint32_t *paddr,
 uint8_t privileged, uint8_t wr)
{
    struct tlb_t *tlb = mmu->tlb_base;
    uint32_t index = (vaddr >> 10) % TLB_SIZE;
    if(!tlb[index].type || tlb[index].vaddr != (vaddr&0xFFFFFC00))
        return 0; //TLB miss
    *paddr = tlb[index].paddr | (vaddr & 0x000003FF);
    if(tlb[index].is_manager)
        return 1; //TLB hit
    if(mmu_check_access_permissions(mmu, tlb[index].ap, privileged, wr) == 0)
        return 1; //TLB hit
    return 0; //TLB miss
}


/*************tlb*****************/


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


static inline uint32_t mmu_page_table_walk(struct armv4_cpu_t *cpu,
 uint32_t vaddr, uint8_t privileged, uint8_t wr)
{
    struct mmu_t *mmu = &cpu->mmu;
    uint32_t paddr = 0xffffffff;
    //page table walk
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
        case 2:
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
                if(mmu_check_access_permissions(mmu, ap, privileged, wr) == 0) {
                    paddr = (page_table_entry&0xFFF00000)|(vaddr&0x000FFFFF);
                    tlb_set_client(mmu, vaddr, paddr, ap);
                    return paddr;
                }
                //Section permission fault
                cp15_fsr(mmu) = (domain << 4) | 0xd;
                cp15_far(mmu) = vaddr;
                mmu->mmu_fault = 1;
                PRINTF("Section permission fault\n");
            }while(0);
            break;
        case 3:
            //Manager
            paddr = (page_table_entry&0xFFF00000)|(vaddr&0x000FFFFF);
            tlb_set_manager(mmu, vaddr, paddr);
            return paddr;
        default:
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
            case 2:
                //Page domain fault
                cp15_fsr(mmu) = (domain << 4) | 0xb;
                cp15_far(mmu) = vaddr;
                mmu->mmu_fault = 1;
                PRINTF("Page domain fault\n");
                break;
            case 1:
                //Client, Check access permissions
                switch(second_level_descriptor) {
                case 1:
                    do {
                        //large page, 64KB
                        uint8_t subpage = (vaddr >> 14)&3;
                        uint8_t ap = (page_table_entry >> ((subpage<<1)+4))&0x3;
                        if(mmu_check_access_permissions(mmu, ap, privileged, wr) == 0) {
                            paddr = (page_table_entry&0xFFFF0000)|(vaddr&0x0000FFFF);
                            tlb_set_client(mmu, vaddr, paddr, ap);
                            return paddr;
                        }
                    }while(0);
                    break;
                case 2:
                    do {
                        //small page, 4KB
                        uint8_t subpage = (vaddr >> 10)&3;
                        uint8_t ap = (page_table_entry >> ((subpage<<1)+4))&0x3;
                        if(mmu_check_access_permissions(mmu, ap, privileged, wr) == 0) {
                            paddr = (page_table_entry&0xFFFFF000)|(vaddr&0x00000FFF);
                            tlb_set_client(mmu, vaddr, paddr, ap);
                            return paddr;
                        }
                    }while(0);
                    break;
                case 3:
                    do {
                        //tiny page, 1KB
                        uint8_t ap = (page_table_entry>>4)&0x3; //ap0
                        if(mmu_check_access_permissions(mmu, ap, privileged, wr) == 0) {
                            paddr = (page_table_entry&0xFFFFFC00)|(vaddr&0x000003FF);
                            tlb_set_client(mmu, vaddr, paddr, ap);
                            return paddr;
                        }
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
                    paddr = (page_table_entry&0xFFFF0000)|(vaddr&0x0000FFFF);
                    break;
                case 2:
                    //small page, 4KB
                    paddr = (page_table_entry&0xFFFFF000)|(vaddr&0x00000FFF);
                    break;
                case 3:
                    //tiny page, 1KB
                    paddr = (page_table_entry&0xFFFFFC00)|(vaddr&0x000003FF);
                    break;
                default:
                    break;
                }
                tlb_set_manager(mmu, vaddr, paddr);
                return paddr;
                break;
            default:
                break;
            } /* end switch(domainval) */
        } /* end if(second_level_descriptor == 0) */
        break;
    }
    return 0xffffffff;
}


/*
 *  uint8_t wr:                write: 1, read: 0
 *  uint8_t privileged:   privileged: 1, user: 0
 * author:hxdyxd
 */
static inline uint32_t mmu_transfer(struct armv4_cpu_t *cpu, uint32_t vaddr,
 uint8_t mask, uint8_t privileged, uint8_t wr)
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
    uint32_t paddr = 0xffffffff;
    ++mmu->tlb_total;
    if(tlb_get(mmu, vaddr, &paddr, privileged, wr)) {
        ++mmu->tlb_hit;
        return paddr;
    }
    return mmu_page_table_walk(cpu, vaddr, privileged, wr);
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
        if(cpu->peripheral.link[i].read &&
         BMASK(address, cpu->peripheral.link[i].mask, cpu->peripheral.link[i].prefix)) {
            uint32_t offset = address - cpu->peripheral.link[i].prefix;
            return cpu->peripheral.link[i].read(cpu->peripheral.link[i].reg_base, offset);
        }
    }

    if(address == 0x4001f030) {
        //code counter
        return cpu->code_counter;
    }
    
    WARN("address error, read 0x%x\r\n", address);
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
        if(cpu->peripheral.link[i].write &&
         BMASK(address, cpu->peripheral.link[i].mask, cpu->peripheral.link[i].prefix)) {
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

    WARN("address error, write 0x%0x\r\n", address);
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
    uint32_t pc_val = register_read(cpu, 15) - 4; //current pc_val
    
    tlb_set_base(&cpu->mmu, TLB_I);
    cpu->decoder.instruction_word = read_word(cpu, pc_val);
    tlb_set_base(&cpu->mmu, TLB_D);
    PRINTF("%8x:\t%08x\t", pc_val, cpu->decoder.instruction_word);
    register_write(cpu, 15, pc_val + 4 ); //current pc_val add 4
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
                    i, cpu->peripheral.link[i].name,
                     cpu->peripheral.link[i].prefix,
                      (~cpu->peripheral.link[i].mask)+1);
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

static inline uint8_t shifter(struct armv4_cpu_t *cpu, uint32_t *op, const uint8_t rot_num,
 const uint8_t type, const uint8_t shifts_mode)
{
    uint8_t shifter_carry_out = 0;
    uint32_t operand2 = *op;
    switch(type) {
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
#define ALU_MODE_SUB     (0)

#define _adder(op1,op2,m)  ((m==ALU_MODE_ADD)?(op1)+(op2):(op1)-(op2))

static inline uint32_t adder(const uint32_t op1, const uint32_t op2, const uint8_t add_mode)
{
    PRINTF("[ALU]op1: 0x%x, sf_op2: 0x%x, out: 0x%x, ",
         op1, op2, _adder(op1, op2, add_mode));
    return _adder(op1, op2, add_mode);
}


static inline uint32_t adder_with_carry(const uint32_t op1, const uint32_t op2,
 const uint32_t carry, const uint8_t add_mode, uint8_t *bit_cy, uint8_t *bit_ov)
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
    
    PRINTF("[ALU]op1: 0x%x, sf_op2: 0x%x, c: %d, out: 0x%x, ",
     op1, op2, carry, aluout);
    return aluout;
}



//**************************************************
//******************decode**************************
//**************************************************


static inline uint32_t code_shifter(struct armv4_cpu_t *cpu, const union ins_t ins,
 uint8_t code_type, uint8_t *carry)
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
        shifter_carry_out = shifter(cpu, &operand2, rot_num, 3,
         SHIFTS_MODE_32BIT_IMMEDIATE); //use ROR Only
        break;
    case code_type_dp1:
        //register shift
        rot_num = register_read(cpu, Rs); //32bit
        shifter_carry_out = shifter(cpu, &operand2, rot_num, shift,
         SHIFTS_MODE_REGISTER);
        break;
    case code_type_dp0:
    case code_type_ldr1:
        //immediate shift
        rot_num = shift_amount; //5bit
        shifter_carry_out = shifter(cpu, &operand2, rot_num, shift,
         SHIFTS_MODE_IMMEDIATE);
        break;

    case code_type_ldr0:
        operand2 = immediate_ldr; //12bit
        break;
    case code_type_ldrh1:
    case code_type_ldrsb1:
    case code_type_ldrsh1:
        operand2 = immediate_extldr; //8bit
        break;
    case code_type_b:
        operand2 = immediate_b;
        break;
    }
    *carry = shifter_carry_out;
    return operand2;
}


static void code_dp(struct armv4_cpu_t *cpu, const union ins_t ins,
 uint32_t operand1, uint32_t operand2, uint8_t carry_out)
{
    uint8_t carry_in = 0;
    uint32_t aluout = 0;
    uint8_t bit_ov = cpsr_v(cpu);
    switch(opcode) {
    case 5:
        //ADC
        carry_in = cpsr_c(cpu);
    case 4:
    case 11:
        //ADD, CMN
        aluout = adder_with_carry(operand1, operand2, carry_in,
         ALU_MODE_ADD, &carry_out, &bit_ov);
        break;

    case 3:
        //RSB
        SWAP_VAL(operand1, operand2);
        aluout = adder_with_carry(operand1, operand2, carry_in,
         ALU_MODE_SUB, &carry_out, &bit_ov);
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
        aluout = adder_with_carry(operand1, operand2, carry_in,
         ALU_MODE_SUB, &carry_out, &bit_ov);
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
        
        PRINTF("[DP] update flag nzcv %d%d%d%d \r\n",
         cpsr_n(cpu), cpsr_z(cpu), cpsr_c(cpu), cpsr_v(cpu));
        
        if(Rd == 15 && Bit24_23 != 2) {
            uint8_t cpu_mode = get_cpu_mode_code(cpu);
            cpsr(cpu) = cpu->spsr[cpu_mode];
            PRINTF("[DP] cpsr 0x%x copy from spsr %d \r\n", cpsr(cpu), cpu_mode);
        }
    }
}


static void code_b(struct armv4_cpu_t *cpu, const union ins_t ins,
 uint32_t operand1, uint32_t operand2)
{
    uint32_t aluout = 0;
    uint8_t lf = 0;
    switch(cpu->decoder.code_type) {
    case code_type_b:
        aluout = adder(operand1, operand2, ALU_MODE_ADD);
        lf = Lf_b;
        break;
    case code_type_bx:
        aluout = operand2;
        lf = Lf_bx;
        break;
    }

    if(lf) {
        register_write(cpu, 14, register_read(cpu, 15) - 4);  //LR register
        PRINTF("write register R%d = 0x%0x, ", 14, register_read(cpu, 14));
    }
    
    if(aluout&3) {
        ERROR("thumb unsupport \r\n");
    }
    register_write(cpu, 15, aluout & 0xfffffffc);  //PC register
    PRINTF("write register R%d = 0x%x \r\n", 15, aluout);
}


static void code_ldr(struct armv4_cpu_t *cpu, const union ins_t ins,
 uint32_t operand1, uint32_t operand2)
{
    uint32_t aluout = adder(operand1, operand2, Uf ? ALU_MODE_ADD : ALU_MODE_SUB);
    uint8_t code_type = cpu->decoder.code_type;

    uint32_t address = operand1;  //memory address
    uint32_t data;
    if(Pf) {
        address = aluout;
    }
    
    uint32_t tmpcpsr = cpsr(cpu);
    uint8_t ldrt = 0;
    if(!Pf && Wf) {
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
            if(data & 0x8000) {
                data |= 0xffff0000;
            }
            break;
        case code_type_ldrsb1:
        case code_type_ldrsb0:
            //Byte Signed
            data = read_byte(cpu, address);
            if(data & 0x80) {
                data |= 0xffffff00;
            }
            break;
        default:
            data = Bf ? read_byte(cpu, address) : read_word(cpu, address);
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
        PRINTF("load data [0x%x]:0x%x to R%d \r\n", 
            address, register_read(cpu, Rd), Rd);
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


static void code_ldm(struct armv4_cpu_t *cpu, const union ins_t ins,
 uint32_t operand1)
{
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
                
                PRINTF("[STM] store data [R%d]:0x%x to 0x%x \r\n", n,
                 register_read(cpu, n), address);
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
}


#define  UserMask    0xF0000000
#define  PrivMask    0x000000FF
#define  StateMask   0x00000020

static void code_msr(struct armv4_cpu_t *cpu, const union ins_t ins,
 uint32_t operand2)
{
    if(cpu->decoder.code_type == code_type_mrs) {
        if(Bit22) {
            uint8_t cpu_mode = get_cpu_mode_code(cpu);
            register_write(cpu, Rd, cpu->spsr[cpu_mode]);
            PRINTF("write register R%d = [spsr_%s]0x%x\r\n",
             Rd, string_register_mode[cpu_mode], cpu->spsr[cpu_mode]);
        } else {
            register_write(cpu, Rd, cpsr(cpu));
            PRINTF("write register R%d = [cpsr]0x%x\r\n", Rd, cpsr(cpu));
        }
    } else {
        uint32_t aluout = operand2;
        uint32_t byte_mask = 0;
        if(Rn & 1)
            byte_mask |= 0xff; //c
        if(Rn & 2)
            byte_mask |= 0xff00;  //x
        if(Rn & 4)
            byte_mask |= 0xff0000;  //s
        if(Rn & 8)
            byte_mask |= 0xff000000;  //f

        if(Bit22) {
            //write spsr
            uint8_t cpu_mode = get_cpu_mode_code(cpu);
            if(cpu_mode != 0) {
                byte_mask &= UserMask | PrivMask | StateMask;
                cpu->spsr[cpu_mode] &= ~byte_mask;
                cpu->spsr[cpu_mode] |= aluout & byte_mask;
                PRINTF("write register spsr_%s = 0x%x\r\n",
                 string_register_mode[cpu_mode], cpu->spsr[cpu_mode]);
            }
        } else {
            //write cpsr
            if(cpsr_m(cpu) == CPSR_M_USR) {
                byte_mask &= UserMask;
            } else {
                byte_mask &= UserMask | PrivMask;
            }
            cpsr(cpu) &= ~byte_mask;
            cpsr(cpu) |= aluout & byte_mask;
            PRINTF("write register cpsr = 0x%x\r\n", cpsr(cpu));
        }
    }
}


static void code_mcr(struct armv4_cpu_t *cpu, const union ins_t ins)
{
    //  cp_num       Rs
    //  register     Rd
    //  cp_register  Rn
    if(ins.mcr.cp_num != 15)
        return;
    uint32_t Rd_val;
    if(Bit20) {
        //mrc
        Rd_val = cp15_read(&cpu->mmu, ins.mcr.CRn, ins.mcr.CRm, ins.mcr.opcode2);

        if(Rd == 15) {
            cpsr(cpu) &= ~0xF0000000;
            cpsr(cpu) |= Rd_val & 0xF0000000;
        } else {
            register_write(cpu, Rd, Rd_val);
            PRINTF("read cp%d_c%d[0x%x] op2:%d to R%d \r\n",
             Rs, ins.mcr.CRn, Rd_val, ins.mcr.opcode2, Rd);
        }
        
    } else {
        //mcr
        Rd_val = register_read(cpu, Rd);
        cp15_write(&cpu->mmu, Rd_val, ins.mcr.CRn, ins.mcr.CRm, ins.mcr.opcode2);

        PRINTF("write R%d[0x%x] to cp%d_c%d \r\n",
         Rd, Rd_val, ins.mcr.cp_num, ins.mcr.CRn);
    }
}


static void code_mult(struct armv4_cpu_t *cpu, const union ins_t ins,
 uint32_t operand1, uint32_t operand2)
{
    uint32_t aluout = adder(operand1, operand2, ALU_MODE_ADD);

    register_write(cpu, Rn, aluout);   //Rd and Rn swap, !!!
    PRINTF("[MULT] write register R%d = 0x%x\r\n", Rn, aluout);
    
    if(Bit20) {
        //Update CPSR register
        uint8_t out_sign = IS_SET(aluout, 31);
        
        cpsr_n_set(cpu, out_sign);
        cpsr_z_set(cpu, aluout == 0);
        //cv unaffected
        PRINTF("update flag nzcv %d%d%d%d \r\n",
         cpsr_n(cpu), cpsr_z(cpu), cpsr_c(cpu), cpsr_v(cpu));
    }
}


static void code_multl(struct armv4_cpu_t *cpu, const union ins_t ins,
 uint32_t operand1, uint32_t operand2)
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
    register_write(cpu, Rd, multl_long & 0xffffffff);
    PRINTF("write 0x%x to R%d, write 0x%x to R%d  = (0x%x) * (0x%x) \r\n",
    register_read(cpu, Rn), Rn, register_read(cpu, Rd), Rd, rm_num, rs_num);
    
    if(Bit20) {
        //Update CPSR register
        uint8_t out_sign = IS_SET(multl_long, 63);
        
        cpsr_n_set(cpu, out_sign);
        cpsr_z_set(cpu, multl_long == 0);
        //cv unaffecteds
        PRINTF("update flag nzcv %d%d%d%d \r\n",
         cpsr_n(cpu), cpsr_z(cpu), cpsr_c(cpu), cpsr_v(cpu));
    }
}


static void code_swp(struct armv4_cpu_t *cpu, const union ins_t ins,
 uint32_t operand1, uint32_t operand2)
{
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
}


static void code_clz(struct armv4_cpu_t *cpu, const union ins_t ins,
 uint32_t operand2)
{
    int r = 32;
    uint32_t x = operand2;

    if (!(x & 0xffff0000u)) {
        x <<= 16;
        r -= 16;
    }
    if (!(x & 0xff000000u)) {
        x <<= 8;
        r -= 8;
    }
    if (!(x & 0xf0000000u)) {
        x <<= 4;
        r -= 4;
    }
    if (!(x & 0xc0000000u)) {
        x <<= 2;
        r -= 2;
    }
    if (!(x & 0x80000000u)) {
        x <<= 1;
        r -= 1;
    }
    register_write(cpu, Rd, r);
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

    
    if(DEBUG) {
        char code_buff[AS_CODE_LEN];
        dec->code_type = code_disassembly(dec->instruction_word,
         register_read(cpu, 15) - 8, code_buff, AS_CODE_LEN);
        PRINTF("%s\n", code_buff);
    } else {
        dec->code_type = code_decoder(ins);
    }

    //------------------------------------------------------------------------
    //------------------------------------------------------------------------
    //execute
    uint8_t carry_out = cpsr_c(cpu);

    uint32_t operand1 = register_read(cpu, Rn); //19_16
    uint32_t operand2 = code_shifter(cpu, ins, dec->code_type, &carry_out);
    
    switch(dec->code_type) {
    case code_type_dp0:
    case code_type_dp1:
    case code_type_dp2:
        code_dp(cpu, ins, operand1, operand2, carry_out);
        break;
    case code_type_bx:
    case code_type_b:
        operand1 = register_read(cpu, 15);
        code_b(cpu, ins, operand1, operand2);
        break;
    case code_type_ldr0:
    case code_type_ldr1:
    case code_type_ldrsb1:
    case code_type_ldrsh1:
    case code_type_ldrh1:
    case code_type_ldrsb0:
    case code_type_ldrsh0:
    case code_type_ldrh0:
        code_ldr(cpu, ins, operand1, operand2);
        break;
    case code_type_ldm:
        code_ldm(cpu, ins, operand1);
        break;
    case code_type_mrs:
    case code_type_msr1:
    case code_type_msr0:
        code_msr(cpu, ins, operand2);
        break;
    case code_type_mcr:
        code_mcr(cpu, ins);
        break;
    case code_type_mult:
        //Rd and Rn swap, !!!
        operand1 = opcode ? register_read(cpu, Rd) : 0; //MLA
        operand2 *= register_read(cpu, Rs);
        code_mult(cpu, ins, operand1, operand2);
        break;
    case code_type_multl:
        code_multl(cpu, ins, operand1, operand2);
        break;
    case code_type_swp:
        code_swp(cpu, ins, operand1, operand2);
        break;
    case code_type_clz:
        code_clz(cpu, ins, operand2);
        break;
    case code_type_swi:
        cpu->decoder.event_id = EVENT_ID_SWI;
        break;
    default:
        cpu->decoder.event_id = EVENT_ID_UNDEF;
    }
}


/*****************************END OF FILE***************************/
