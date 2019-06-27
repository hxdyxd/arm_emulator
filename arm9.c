/* 2019 06 26 */
/* By hxdyxd */
#include <stdio.h>
#include <stdint.h>
#include <time.h>

/****************************************************/
//Memary size

#define CODE_SIZE   0x10000
#define RAM_SIZE    0x8000

#define MEM_SIZE  0x20000
/****************************************************/

#define DEBUG                  0
#define MEM_CODE_READONLY      0

//uint8_t abort_test = 0;

#define PRINTF(...)  do{ if(DEBUG){printf(__VA_ARGS__);} }while(0)

#define  IS_SET(v, bit) ( ((v)>>(bit))&1 )


unsigned int spsr[7] = {0, };
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
static unsigned char MEM[MEM_SIZE];

//intermediate datapath and control path signals
static unsigned int instruction_word;

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
unsigned int Register[7][16];
#define   CPU_MODE_USER      0
#define   CPU_MODE_FIQ       1
#define   CPU_MODE_IRQ       2
#define   CPU_MODE_SVC       3
#define   CPU_MODE_Undef     4
#define   CPU_MODE_Abort     5
#define   CPU_MODE_Mon       6

uint8_t get_cpu_mode_code(void)
{
    uint8_t cpu_mode = 0;
    switch(cpsr_m) {
    case CPSR_M_USR:
    case CPSR_M_SYS:
        cpu_mode = CPU_MODE_USER;
        break;
    case CPSR_M_FIQ:
        cpu_mode = CPU_MODE_FIQ;
        break;
    case CPSR_M_IRQ:
        cpu_mode = CPU_MODE_IRQ;
        break;
    case CPSR_M_SVC:
        cpu_mode = CPU_MODE_SVC;
        break;
    case CPSR_M_MON:
        cpu_mode = CPU_MODE_USER;
        break;
    case CPSR_M_ABT:
        cpu_mode = CPU_MODE_Abort;
        break;
    case CPSR_M_UND:
        cpu_mode = CPU_MODE_Mon;
        break;
    default:
        cpu_mode = 0;
        PRINTF("cpu mode is unknown %x\r\n", cpsr_m);
        getchar();
    }
    return cpu_mode;
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
    printf("PC = 0x%x , code = %d\r\n", register_read(15), code_counter);
    printf("cpsr = 0x%x\n", cpsr);
    for(int i=0; i<16; i++) {
        if(i % 4 == 0) {
            printf("\n");
        }
        printf("R%d = 0x%08x, \t", i, register_read(i));
    }
    printf("\n");
    getchar();
}


static int read_word(char *mem, unsigned int address)
{
    int *data;
    if(address >= MEM_SIZE) {
        printf("mem overflow error, read 0x%x\r\n", address);
        exception_out();
    }
    
    if(address == 0x18020) {
        //sys clock ms
        uint32_t clk1ms = (clock()*1000/CLOCKS_PER_SEC);
        return clk1ms;
    } else if(address == 0x18030) {
        //ips
        return code_counter;
    }
    
    data =  (int*) (mem + address);
    return *data;
}


static void write_word(char *mem, unsigned int address, unsigned int data)
{
    int *data_p;
    if(address >= MEM_SIZE || (address&3) != 0) {
        printf("mem error, write word 0x%0x\r\n", address);
        exception_out();
    }
    if(address == 0x18030) {
        //ips
        code_counter = data;
        return;
    }
    
    data_p = (int *) (mem + address);
    *data_p = data;
}


static void write_halfword(char *mem, unsigned int address, unsigned short data)
{
    short *data_p;
    if(address >= MEM_SIZE || (address&1) != 0 
#if MEM_CODE_READONLY
     || address < CODE_SIZE
#endif
    ) {
        printf("mem error, write halfword 0x%0x\r\n", address);
        exception_out();
    }
    data_p = (short *) (mem + address);
    *data_p = data;
}


static void write_byte(char *mem, unsigned int address, unsigned char data)
{
    char *data_p;
    if(address >= MEM_SIZE
#if MEM_CODE_READONLY
     || address < CODE_SIZE
#endif
     ) {
        printf("mem error, write byte 0x%0x\r\n", address);
        exception_out();
    }
    if(address == 0x18004) putchar(data);
    data_p = (char *) (mem + address);
    *data_p = data;
}


//load_program_memory reads the input memory, and populates the instruction 
// memory
void load_program_memory(char *file_name)
{
    FILE *fp;
    unsigned int address, instruction;
    fp = fopen(file_name, "rb");
    if(fp == NULL) {
        printf("Error opening input mem file\n");
        exception_out();
    }
    address = 0;
    while(!feof(fp)) {
        fread(&instruction, 4, 1, fp);
        write_word(MEM, address, instruction);
        address = address + 4;
    }
    printf("load mem size = 0x%x \r\n", address);
    fclose(fp);
}


void interrupt_exception(uint8_t type)
{
    uint32_t cpsr_int = cpsr;
    uint32_t next_pc = register_read(15) - 4;  //lr 
    PRINTF("cpsr(0x%x) save to r14_s\r\n", cpsr_int);
    switch(type) {
    case CPSR_M_SVC:
        //swi
        printf("svc lr = 0x%x swi\r\n", next_pc);
        register_write(15, 0x8);
        cpsr_i_set(1);  //disable irq
        cpsr_t_set(0);  //arm mode
        cpsr &= 0x1f;
        cpsr |= CPSR_M_SVC;
        spsr[CPU_MODE_SVC] = cpsr_int;  //write SPSR_svc
        register_write(14, next_pc);  //write R14_svc
        break;
    case CPSR_M_IRQ:
        //irq
        if(cpsr_i) {
            return;  //irq disable
        }
        register_write(15, 0x18);
        cpsr_i_set(1);  //disable irq
        cpsr_t_set(0);  //arm mode
        cpsr &= 0x1f;
        cpsr |= CPSR_M_IRQ;
        spsr[CPU_MODE_IRQ] = cpsr_int;  //write SPSR_irq
        register_write(14, next_pc + 4);  //write R14_irq
        break;
    default:
        exception_out();
    }
}


void fetch(void)
{
    uint32_t pc = register_read(15) - 4; //current pc
    if(pc >= CODE_SIZE || ((pc&3) != 0)) {
        PRINTF("[FETCH] error, pc[0x%x] overflow \r\n", pc);
        exception_out();
    }
    
    instruction_word = read_word(MEM, register_read(15) - 4);
    PRINTF("[0x%04x]: ", register_read(15) - 4);
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
#define  code_is_unknow  255


unsigned char code_type = 0;

unsigned char Rn = 0;
unsigned char Rd = 0;
unsigned char Rs = 0;
unsigned char Rm = 0;
unsigned short immediate = 0;
signed int b_offset = 0;
unsigned char opcode = 0;
unsigned char shift = 0;
unsigned char rotate_imm = 0;
unsigned char shift_amount = 0;
unsigned char cond = 0;
unsigned char Lf = 0, Bf = 0, Uf = 0, Pf = 0, Wf = 0;
unsigned char Bit22 = 0, Bit20 = 0, Bit7 = 0, Bit4 = 0;
unsigned char Bit24_23 = 0;

void decode(void)
{
    
    //ldr/str
    unsigned char f = 0;
    
    cond = (instruction_word&0xF0000000) >> 28;  /* bit[31:28] */
    f = (instruction_word&0x0E000000)>>25;  /* bit[27:25] */
    opcode = (instruction_word&0x1E00000) >> 21;  /* bit[24:21] */
    Bit24_23 = opcode >> 2;  /* bit[24:23] */
    Bit22 = (instruction_word&0x400000) >> 22;  /* bit[22] */
    Bit20 = (instruction_word&0x100000) >> 20;  /* bit[20] */
    Rn = (instruction_word&0x000F0000) >> 16;  /* bit[19:16] */
    Rd = (instruction_word&0x0000F000) >> 12;  /* bit[15:12] */
    Rs = (instruction_word&0x00000F00) >> 8;  /* bit[11:8] */
    rotate_imm = Rs;  /* bit[11:8] */
    shift_amount = (instruction_word&0x00000F80) >> 7;  /* bit[11:7] */
    shift = (instruction_word&0x60) >> 5;  /* bit[6:5] */
    Bit4 = (instruction_word&0x10) >> 4;  /* bit[4] */
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
                PRINTF("dp0 %s dst register R%d, first operand R%d, Second operand R%d[0x%x] %s immediate #0x%x\r\n", opcode_table[opcode], Rd, Rn, Rm, register_read(Rm), shift_table[shift], shift_amount);
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
                    PRINTF("dp1 %s dst register R%d, first operand R%d, Second operand R%d %s R%d\r\n", opcode_table[opcode], Rd, Rn, Rm, shift_table[shift], Rs);
                }
                break;
            case 1:
                //Multiplies, extra load/stores
                Lf = (instruction_word&0x100000)>>20; //L bit
                Bf = (instruction_word&0x400000)>>22; //B bit
                Uf = (instruction_word&0x800000) >> 23;  //U bit
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
                            PRINTF("ldrh1 %sH R%d, [R%d, immediate %s#%d] %s\r\n", Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
                        } else {
                            PRINTF("ldrh1 %sH R%d, [R%d], immediate %s#%d %s\r\n", Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
                        }
                    } else {
                        code_type = code_is_ldrh0;
                        if(Pf) {
                            PRINTF("ldrh0 %sH R%d, [R%d, %s[R%d] ] %s\r\n", Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
                        } else {
                            PRINTF("ldrh0 %sH R%d, [R%d], %s[R%d] %s\r\n", Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
                        }
                    }
                    break;
                case 2:
                    //code_is_ldrsb
                    if(Bit22) {
                        code_type = code_is_ldrsb1;
                        immediate = (Rs << 4) | Rm;
                        if(Pf) {
                            PRINTF("ldrsb1 %sSB R%d, [R%d, immediate %s#%d] %s\r\n", Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
                        } else {
                            PRINTF("ldrsb1 %sSB R%d, [R%d], immediate %s#%d %s\r\n", Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
                        }
                    } else {
                        code_type = code_is_ldrsb0;
                        if(Pf) {
                            PRINTF("ldrsb0 %sSB R%d, [R%d, %s[R%d] ] %s\r\n", Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
                        } else {
                            PRINTF("ldrsb0 %sSB R%d, [R%d], %s[R%d] %s\r\n", Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
                        }
                    }
                    break;
                case 3:
                    //code_is_ldrsh
                    if(Bit22) {
                        code_type = code_is_ldrsh1;
                        immediate = (Rs << 4) | Rm;
                        if(Pf) {
                            PRINTF("ldrsh1 %sSH R%d, [R%d, immediate %s#%d] %s\r\n", Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
                        } else {
                            PRINTF("ldrsh1 %sSH R%d, [R%d], immediate %s#%d %s\r\n", Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
                        }
                    } else {
                        code_type = code_is_ldrsh0;
                        if(Pf) {
                            PRINTF("ldrsh0 %sSH R%d, [R%d, %s[R%d] ] %s\r\n", Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
                        } else {
                            PRINTF("ldrsh0 %sSH R%d, [R%d], %s[R%d] %s\r\n", Lf?"LDR":"STR", Rd, Rn, Uf?"+":"-", Rm, Wf?"!":"");
                        }
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
            PRINTF("dp2 %s dst register R%d, first operand R%d, Second operand immediate [#%d ROR %d]\r\n", opcode_table[opcode], Rd, Rn, immediate, rotate_imm*2);
        }
        break;
    case 2:
        //load/store immediate offset
        code_type = code_is_ldr0;
        immediate = instruction_word&0xFFF;
        Lf = (instruction_word&0x100000)>>20; //L bit
        Bf = (instruction_word&0x400000)>>22; //B bit
        Uf = (instruction_word&0x800000) >> 23;  //U bit
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
            Lf = (instruction_word&0x100000)>>20; //L bit
            Bf = (instruction_word&0x400000)>>22; //B bit
            Uf = (instruction_word&0x800000) >> 23;  //U bit
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
        Lf = (instruction_word&0x100000)>>20; //L bit
        Uf = (instruction_word&0x800000) >> 23;  //U bit
        Pf = opcode >> 3;  //P bit
        Wf = opcode & 1;
        PRINTF("ldm %s%s%s R%d%s \r\n", Lf?"LDM":"STM", Uf?"I":"D", Pf?"B":"A",  Rn, Wf?"!":"");
        Rn = (instruction_word&0x000F0000)>>16;
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
        Lf = (instruction_word&0x1000000)>>24; //L bit
        PRINTF("B%s PC[0x%x] +  0x%08x = 0x%x\r\n", Lf?"L":"", register_read(15), b_offset, register_read(15) + b_offset );
        break;
    case 6:
        //Coprocessor load/store and double register transfers
        code_type = code_is_unknow;
        printf("Coprocessor todo... \r\n");
        break;
    case 7:
        //software interrupt
        code_type = code_is_swi;
        PRINTF("swi \r\n");
        break;
    default:
        code_type = code_is_unknow;
        printf("undefed\r\n");
    }
}

void execute(void)
{
    unsigned int operand1 = register_read(Rn);
    unsigned int operand2 = register_read(Rm);
    unsigned int rot_num = register_read(Rs);
    unsigned char cond_satisfy = 0;
    unsigned char add_flag = 1;
    /*
     * 0: turn off shifter
     * 3: PD(is) On
     * 4: DP(rs) On
     * 5: DP(i) On
    */
    unsigned char shifter_flag = 0;
    unsigned int carry = 0;
    
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
        return;
    }
    
    if(code_type == code_is_ldrsb1 || code_type == code_is_ldrsh1 || code_type == code_is_ldrh1 || code_type == code_is_ldr0) {
        operand2 = immediate;
        //no need to use shift
        shifter_flag = 0;
    } else if(code_type == code_is_ldrsb0 || code_type == code_is_ldrsh0 || code_type == code_is_ldrh0) {
        //no need to use shift
        shifter_flag = 0;
    } else if(code_type == code_is_ldr1 || code_type == code_is_dp0) {
        //immediate shift
        rot_num = shift_amount;
        shifter_flag = 3;  //PD(is) On
    } else if(code_type == code_is_msr1) {
        operand1 = 0;  //msr don't support
        operand2 = immediate;
        rot_num = rotate_imm<<1;
        shift = 3;
        shifter_flag = 5;  //DP(i) On
    } else if(code_type == code_is_dp2) {
        //immediate
        operand2 = immediate;
        rot_num = rotate_imm<<1;
        shift = 3;  //use ROR Only
        shifter_flag = 5;  //DP(i) On
    } else if(code_type == code_is_dp1) {
        //register shift
        //No need to use
        shifter_flag = 4;  //DP(rs) On
    } else if(code_type == code_is_bx) {
        operand1 = 0;
        rot_num = 0;
        //no need to use shift
        shifter_flag = 0;
    } else if(code_type == code_is_b) {
        operand1 = b_offset;
        operand2 = register_read(15);
        rot_num = 0;
        shifter_flag = 0;  //no need to use shift
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
        interrupt_exception(CPSR_M_SVC);
        return;
    } else if(code_type == code_is_mrs) {
        shifter_flag = 0;  //no need to use shift
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
    
    uint32_t sum_middle = add_flag?((operand1&0x7fffffff) + (operand2&0x7fffffff) + carry):((operand1&0x7fffffff) - (operand2&0x7fffffff) - carry);
    uint32_t cy_high_bits =  add_flag?(IS_SET(operand1, 31) + IS_SET(operand2, 31) + IS_SET(sum_middle, 31)):(IS_SET(operand1, 31) - IS_SET(operand2, 31) - IS_SET(sum_middle, 31));
    uint32_t aluout = ((cy_high_bits&1) << 31) | (sum_middle&0x7fffffff);
    PRINTF("op1: 0x%x, sf_op2: 0x%x, c: %d, out: 0x%x, ", operand1, operand2, carry, aluout);

    if(code_type == code_is_ldr0 || code_type == code_is_ldr1 ||
       code_type == code_is_ldrsb1 || code_type == code_is_ldrsh1 || code_type == code_is_ldrh1 ||
       code_type == code_is_ldrsb0 || code_type == code_is_ldrsh0 || code_type == code_is_ldrh0 )
    {
        uint32_t address = operand1;  //Rn
        if(Pf) {
            //first add
            address = aluout;  //aluout
        }
        if(Lf) {
            //LDR
            if(code_type == code_is_ldrh1 || code_type == code_is_ldrh0) {
                //Halfword
                register_write(Rd, read_word(MEM, address) & 0xffff);
            } else if(code_type == code_is_ldrsh1 || code_type == code_is_ldrsh0) {
                //Halfword Signed
                uint32_t data = read_word(MEM, address) & 0xffff;
                if(data&0x8000) {
                    data |= 0xffff0000;
                }
                register_write(Rd, data);
            } else if(code_type == code_is_ldrsb1 || code_type == code_is_ldrsb0) {
                //Byte Signed
                uint32_t data = read_word(MEM, address) & 0xff;
                if(data&0x80) {
                    data |= 0xffffff00;
                }
                register_write(Rd, data);
            } else if(Bf) {
                //Byte
                register_write(Rd, read_word(MEM, address) & 0xff);
            } else {
                //Word
                if((address&0x3) != 0) {
                    printf("ldr unsupported address = 0x%x\n", address);
                    getchar();
                }
                register_write(Rd, read_word(MEM, address));
            }
            PRINTF("load data [0x%x]:0x%x to R%d \r\n", address, register_read(Rd), Rd);
        } else {
            //STR
            if(code_type == code_is_ldrh1 || code_type == code_is_ldrh0) {
                //Halfword
                write_halfword(MEM, address, register_read(Rd) & 0xffff);
                PRINTF("store data [R%d]:0x%x to 0x%x \r\n", Rd,  register_read(Rd) & 0xffff, address);
            } else if(code_type == code_is_ldrsh1 || code_type == code_is_ldrsh0) {
                //Halfword Signed ?
                uint16_t data = register_read(Rd) & 0xffff;
                write_halfword(MEM, address, data);
                PRINTF("store data [R%d]:0x%x to 0x%x \r\n", Rd,  data, address);
                printf("STRSH ? \r\n");
                exception_out();
            } else if(code_type == code_is_ldrsb1 || code_type == code_is_ldrsb0) {
                //Byte Signed ?
                uint8_t data = register_read(Rd) & 0xff;
                write_byte(MEM, address, data);
                PRINTF("store data [R%d]:0x%x to 0x%x \r\n", Rd,  data, address);
                printf("STRSB ? \r\n");
                exception_out();
            } else if(Bf) {
                write_byte(MEM, address, register_read(Rd) & 0xff);
                PRINTF("store data [R%d]:0x%x to 0x%x \r\n", Rd,  register_read(Rd) & 0xff, address);
            } else {
                write_word(MEM, address, register_read(Rd));
                PRINTF("store data [R%d]:0x%x to 0x%x \r\n", Rd, register_read(Rd), address);
            }
        }
        if(!(!Wf && Pf)) {
            //Update base register
            register_write(Rn, aluout);
            PRINTF("[LDR]write register R%d = 0x%x\r\n", Rn, aluout);
        }
        
        if(!Pf && Wf) {
            printf("UNSUPPORT LDRT\r\n");
            getchar();
        }
        
    } else if(code_type == code_is_dp0 || code_type == code_is_dp1 ||  code_type == code_is_dp2) {
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
            
            PRINTF("update flag nzcv %d%d%d%d \r\n", cpsr_n, cpsr_z, cpsr_c, cpsr_v);
            
            if(Rd == 15 && Bit24_23 != 2) {
                uint8_t cpu_mode = get_cpu_mode_code();
                cpsr = spsr[cpu_mode];
                PRINTF("cpsr 0x%x copy from spsr %d \r\n", cpsr, cpu_mode);
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
    } else if(code_type == code_is_ldm) {
        if(Bit22) {
            printf("[LDM] ldm bit22 set, error\r\n");
            getchar();
        }
        
        if(Lf) { //bit [20]
            //LDM
            uint32_t address = operand1;
            for(int i=0; i<16; i++) {
                int n = (Uf)?i:(15-i);
                if(IS_SET(instruction_word, n)) {
                    if(Pf) {
                        if(Uf) address += 4;
                        else address -= 4;
                    }
                    uint32_t data = read_word(MEM, address);
                    if(i == 15) {
                        data &= 0xfffffffc;
                    }
                    register_write(n, data);
                    PRINTF("[LDM] load data [0x%x]:0x%x to R%d \r\n", address, read_word(MEM, address), n);
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
            
        } else {
            //STM
            uint32_t address = operand1;
            for(int i=0; i<16; i++) {
                int n = (Uf)?i:(15-i);
                if(IS_SET(instruction_word, n)) {
                    if(Pf) {
                        if(Uf) address += 4;
                        else address -= 4;
                    }
                    write_word(MEM, address, register_read(n));
                    PRINTF("[STM] store data [R%d]:0x%x to 0x%x \r\n", n, register_read(n), address);
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
        if(!Bit22) {
            if(IS_SET(Rn, 0) ) {
                cpsr &= 0xffffff00;
                cpsr |= aluout&0xff;
            } else if(IS_SET(Rn, 1) ) {
                cpsr &= 0xffff00ff;
                cpsr |= aluout&0xff00;
            } else if(IS_SET(Rn, 2) ) {
                cpsr &= 0xff00ffff;
                cpsr |= aluout&0xff0000;
            } else if(IS_SET(Rn, 3) ) {
                cpsr &= 0x00ffffff;
                cpsr |= aluout&0xff000000;
            }
            PRINTF("write register cpsr = 0x%x\r\n", cpsr);
        } else {
            printf("spsr not support todo...\r\n");
        }
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
    for(i=0; i<16; i++) {
        register_write(i, 0);
    }
    for(i = CODE_SIZE; i < CODE_SIZE + RAM_SIZE; i++) {
        MEM[i] = 0;
    }
}


int main()
{
    uint32_t last_counter = 0;
    
    //load_program_memory("./Hello/Obj/hello.bin");
    load_program_memory("./arm_hello_gcc/hello.bin");
    reset_proc();
    
    while(1) {
        fetch();
        decode();
        execute();
        
        uint32_t clk =  (clock()*1000/CLOCKS_PER_SEC);
        
        //per millisecond timer irq test
        if(code_counter%15000 == 0) {
            interrupt_exception(CPSR_M_IRQ);
        }
        
        
#if DEBUG
        if(getchar() == 'c') {
            exception_out();
        }
#endif
#if 0
        if(abort_test) {
            exception_out();
            abort_test = 0;
        }
#endif
       
        code_counter++;
    }
    
    printf("\r\n---------------test code -----------\r\n");
    
    return 0;
}

