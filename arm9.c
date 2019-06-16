#include <stdio.h>
#include <stdint.h>

#define MEM_SIZE  0x6000

#define  IS_SET(v, bit) ( ((v)>>(bit))&1 )


static unsigned int spsr[7] = {0, };
#define  cpsr    spsr[0]


#define  cpsr_n_set(v)  do{ if(v) {cpsr |= 1 << 31;} else {cpsr &= ~(1 << 31);} }while(0)
#define  cpsr_z_set(v)  do{ if(v) {cpsr |= 1 << 30;} else {cpsr &= ~(1 << 30);} }while(0)
#define  cpsr_c_set(v)  do{ if(v) {cpsr |= 1 << 29;} else {cpsr &= ~(1 << 29);} }while(0)
#define  cpsr_v_set(v)  do{ if(v) {cpsr |= 1 << 28;} else {cpsr &= ~(1 << 28);} }while(0)

#define  cpsr_n  IS_SET(cpsr, 31)
#define  cpsr_z  IS_SET(cpsr, 30)
#define  cpsr_c  IS_SET(cpsr, 29)
#define  cpsr_v  IS_SET(cpsr, 28)
#define  cpsr_q  IS_SET(cpsr, 27)
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


//register function
static unsigned int Register[7][16];
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
        printf("cpu mode is unknown %x\r\n", cpsr_m);
    }
    return cpu_mode;
}

uint32_t register_read(uint8_t id)
{
    //Register file
    if(id == 15) {
        return Register[CPU_MODE_USER][id] + 4;
    } else if(id < 8) {
        return Register[CPU_MODE_USER][id];
    } else if(cpsr_m == CPSR_M_USR || cpsr_m == CPSR_M_SYS) {
        return Register[CPU_MODE_USER][id];
    } else if(cpsr_m != CPSR_M_FIQ && id < 13) {
        return Register[CPU_MODE_USER][id];
    } else {
        printf("<%x>", cpsr_m);
        return Register[get_cpu_mode_code()][id];
    }
}

void register_write(uint8_t id, uint32_t val)
{
    //Register file
    if(id < 8 || id == 15) {
        Register[CPU_MODE_USER][id] = val;
    } else if(cpsr_m == CPSR_M_USR || cpsr_m == CPSR_M_SYS) {
        Register[CPU_MODE_USER][id] = val;
    } else if(cpsr_m != CPSR_M_FIQ && id < 13) {
        Register[CPU_MODE_USER][id] = val;
    } else {
        printf("<%x>", cpsr_m);
        Register[get_cpu_mode_code()][id] = val;
    }
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
        exit(1);
    }
    address = 0;
    printf("load mem addr = %d \r\n", address);
    while(!feof(fp)) {
        fread(&instruction, 4, 1, fp);
        write_word(MEM, address, instruction);
        address = address + 4;
    }
    printf("load mem addr = %d \r\n", address);
    fclose(fp);
}

int read_word(char *mem, unsigned int address)
{
    int *data;
    if(address >= MEM_SIZE) {
        printf("mem overflow error\r\n");
        exit(1);
    }
    data =  (int*) (mem + address);
    return *data;
}

void write_word(char *mem, unsigned int address, unsigned int data)
{
    int *data_p;
    data_p = (int*) (mem + address);
    *data_p = data;
}

void write_byte(char *mem, unsigned int address, unsigned char data)
{
    char *data_p;
    data_p = (char *) (mem + address);
    *data_p = data;
}

void fetch(void)
{
    instruction_word = read_word(MEM, register_read(15) - 4);
    //printf("FETCH: Fetch instruction 0x%x from address 0x%x\n", instruction_word, R[15]);
    printf("[0x%04x]: ", register_read(15) - 4 );
    register_write(15, register_read(15) );
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
                    printf("msr0 MSR %sPSR_%d  R%d\r\n", Bit22?"S":"C", Rn, Rm);
                } else if(!Bit21 && !Bit7) {
                    code_type = code_is_mrs;
                    printf("mrs MRS R%d %s\r\n", Rd, Bit22?"SPSR":"CPSR");
                } else {
                    printf(" undefed bit7 error \r\n");
                }
            } else {
                //Data processing register shift by immediate
                code_type = code_is_dp0;
                printf("dp0 %s dst register R%d, first operand R%d, Second operand R%d %s immediate #0x%x\r\n", opcode_table[opcode], Rd, Rn,Rm, shift_table[shift], shift_amount);
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
                        printf("bx B%sX R%d\r\n", Lf?"L":"", Rm);
                    } else if(IS_SET(instruction_word, 6) && !IS_SET(instruction_word, 5) ) {
                        //Enhanced DSP add/subtracts
                        printf("Enhanced DSP add/subtracts R%d\r\n", Rm);
                    } else if(IS_SET(instruction_word, 6) && IS_SET(instruction_word, 5)) {
                        //Software breakpoint
                        printf("Software breakpoint \r\n");
                    } else if(!IS_SET(instruction_word, 6) && !IS_SET(instruction_word, 5) && opcode == 0xb) {
                        //Count leading zero
                        printf("Count leading zero \r\n");
                    } else {
                        printf("Undefed Miscellaneous instructions\r\n");
                    }
                } else {
                    //Data processing register shift by register
                    code_type = code_is_dp1;
                    printf("dp1 %s dst register R%d, first operand R%d, Second operand R%d %s R%d\r\n", opcode_table[opcode], Rd, Rn, Rm, shift_table[shift], Rs);
                }
                break;
            case 1:
                //Multiplies, extra load/stores
                switch(shift) {
                case 0:
                    if(Bit24_23 == 0) {
                        code_type = code_is_mult;
                        if( IS_SET(instruction_word, 21) ) {
                            printf("mult MUA%s R%d <= R%d * R%d + R%d\r\n", Bit20?"S":"", Rd, Rm, Rs, Rn);
                        } else {
                            printf("mult MUL%s R%d <= R%d * R%d\r\n", Bit20?"S":"", Rd, Rm, Rs);
                        }
                    } else if(Bit24_23 == 1) {
                        code_type = code_is_multl;
                        printf("multl \r\n");
                    } else if(Bit24_23 == 2) {
                        code_type = code_is_swp;
                        printf("swp \r\n");
                    }
                    break;
                case 1:
                    //code_is_ldrh
                    if(Bit22) {
                        code_type = code_is_ldrh1;
                        printf("ldrh1 \r\n");
                    } else {
                        code_type = code_is_ldrh0;
                        printf("ldrh0 \r\n");
                    }
                    break;
                case 2:
                    //code_is_ldrsb
                    if(Bit22) {
                        code_type = code_is_ldrsb1;
                        printf("ldrsb1 \r\n");
                    } else {
                        code_type = code_is_ldrsb0;
                        printf("ldrsb0 \r\n");
                    }
                    break;
                case 3:
                    //code_is_ldrsh
                    if(Bit22) {
                        code_type = code_is_ldrsh1;
                        printf("ldrsh1 \r\n");
                    } else {
                        code_type = code_is_ldrsh0;
                        printf("ldrsh0 \r\n");
                    }
                    break;
                default:
                    printf("undefed shift\r\n");
                }
                break;
            default:
                printf("undefed Bit7\r\n");
            }
            break;
        default:
            printf("undefed Bit4\r\n");
        }
        break;
    case 1:
        //Data processing immediate and move immediate to status register
        if( (opcode == 0x9 || opcode == 0xb ) && !Bit20 ) {
            code_type = code_is_msr1;
            immediate = instruction_word&0xff;
            printf("msr1 MSR %sPSR_%d  immediate [#%d ROR %d]\r\n", Bit22?"S":"C", Rn, immediate, rotate_imm*2);
        } else if( (opcode == 0x8 || opcode == 0xa ) && !Bit20 ) {
            printf("undefined instruction\r\n");
        } else {
            //Data processing immediate
            code_type = code_is_dp2;
            immediate = instruction_word&0xff;
            printf("dp2 %s dst register R%d, first operand R%d, Second operand immediate [#%d ROR %d]\r\n", opcode_table[opcode], Rd, Rn, immediate, rotate_imm*2);
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
            printf("ldr0 %s%s dst register R%d, [base address at R%d, Second address immediate #%s0x%x] %s\r\n", Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
        } else {
            printf("ldr0 %s%s dst register R%d, [base address at R%d], Second address immediate #%s0x%x %s\r\n", Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-", immediate, Wf?"!":"");
        }
        
        break;
    case 3:
        //load/store register offset
        if(!Bit4) {
            code_type = code_is_ldr1;
            Lf = (instruction_word&0x100000)>>20; //L bit
            Bf = (instruction_word&0x400000)>>22; //B bit
            Uf = (instruction_word&0x800000) >> 23;  //U bit
            immediate = (instruction_word&0x00000F80) >> 7;
            printf("ldr1 %s%s dst register R%d, base address at R%d, offset address at %sR%d \r\n", Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-",  Rm);
            
        } else {
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
        printf("ldm %s%s%s R%d%s \r\n", Lf?"LDM":"STM", Uf?"I":"D", Pf?"B":"A",  Rn, Wf?"!":"");
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
        printf("B%s PC[0x%x] +  0x%08x = 0x%x\r\n", Lf?"L":"", register_read(15), b_offset, register_read(15) + b_offset );
        break;
    case 6:
        //Coprocessor load/store and double register transfers
        printf("Coprocessor todo... \r\n");
        break;
    case 7:
        //software interrupt
        code_type = code_is_swi;
        printf("swi \r\n");
        break;
    default:
        printf("undefed\r\n");
    }
}

void execute(void)
{
    unsigned int operand1 = register_read(Rn);
    unsigned int operand2 = register_read(Rm);
    unsigned int rot_num = register_read(Rs);
    unsigned char cond_satisfy = 0;
    
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
        //printf("cond_satisfy = %d skip...\r\n", cond_satisfy);
        return;
    }
    
    if(code_type == code_is_ldr0) {
        if(Uf) {
            operand2 = immediate;
        } else {
            operand2 = ~immediate + 1;
        }
        rot_num = 0;
    } else if(code_type == code_is_msr1) {
        operand1 = 0;  //msr don't support
        operand2 = immediate;
        rot_num = rotate_imm<<1;
        shift = 3;
    } else if(code_type == code_is_dp2) {
        operand2 = immediate;
        rot_num = rotate_imm<<1;
        shift = 3;
    } else if(code_type == code_is_dp0) {
        rot_num = shift_amount;
    } else if(code_type == code_is_bx) {
        operand1 = 0;
        rot_num = 0;
    } else if(code_type == code_is_b) {
        operand1 = b_offset;
        operand2 = register_read(15);
        rot_num = 0;
    } else if(code_type == code_is_ldm) {
        operand2 = 0;  //increase After
        rot_num = 0;
    } else {
        printf("unknow code_type = %d", code_type);
        getchar();
    }
    
    /******************shift**********************/
    //operand2 << rot_num
    
    switch(shift) {
    case 0:
        //LSL
        operand2 = operand2 << rot_num;
        break;
    case 1:
        //LSR
        operand2 = operand2 >> rot_num;
        break;
    case 2:
        //ASR
        
        operand2 = operand2 >> rot_num;
        break;
    case 3:
        //ROR
        operand2 = (operand2 << (32 - rot_num) | operand2 >> rot_num);
        break;
    }
    
    
    /******************shift**********************/
    
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
            operand2 = ~operand2 + 1;
        } else if(opcode == 3) {
            //RSB
            operand1 = ~operand1 + 1;
        } else if(opcode == 4 || opcode == 11) {
            //ADD, CMN
        } else if(opcode == 5) {
            //ADC
            operand2 += cpsr_c;  //todo...
        } else if(opcode == 6) {
            //SBC
            operand2 = ~operand2 + 1;
        } else if(opcode == 7) {
            //RSC
            operand1 = ~operand1 + 1;
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
            //MNV
            operand1 = 0;
            operand2 = ~operand2;
        } 
    }
    
    unsigned int aluout = operand1 + operand2;
    printf("op1: 0x%x, sf_op2: 0x%x, out: 0x%x, ", operand1, operand2, aluout);

    if(code_type == code_is_ldr0) {
        uint32_t address = operand1;  //Rn
        if(Pf) {
            //first add
            address = aluout;  //aluout
        }
        if(Lf) {
            //LDR
            if(Bf) {
                //Byte
                register_write(Rd, read_word(MEM, address) & 0xff);
            } else {
                register_write(Rd, read_word(MEM, address));
            }
            printf("load data [0x%x]:0x%x to R%d \r\n", address, register_read(Rd), Rd);
        } else {
            //STR
            if(Bf) {
                write_byte(MEM, address, register_read(Rd) & 0xff);
            } else {
                write_word(MEM, address, register_read(Rd));
            }
            printf("store data [R%d]:0x%x to 0x%x \r\n", Rd, register_read(Rd) & (Bf?0xff:0xffffffff), address);
        }
        if(!(!Wf && Pf)) {
            //Update base register
            register_write(Rn, aluout);
            printf("[LDR]write register R%d = 0x%x\r\n", Rn, aluout);
        }
        
    } else if(code_type == code_is_msr1 || code_type == code_is_msr0) {
        if(!Bit22) {
            if(IS_SET(Rn, 0) )
                cpsr |= aluout&0xff;
            if(IS_SET(Rn, 1) )
                cpsr |= aluout&0xff00;
            if(IS_SET(Rn, 2) )
                cpsr |= aluout&0xff0000;
            if(IS_SET(Rn, 3) )
                cpsr |= aluout&0xff000000;
            printf("write register cpsr = 0x%x\r\n", cpsr);
        } else {
            printf("spsr not support todo...\r\n");
        }
    } else if(code_type == code_is_dp0 || code_type == code_is_dp2) {
        if(Bit24_23 == 2) {
            printf("update flag only \r\n");
        } else {
            register_write(Rd, aluout);
            printf("write register R%d = 0x%x\r\n", Rd, aluout);
        }
        if(Bit20) {
            //Update CPSR register
            uint8_t op1_sign = IS_SET(operand1, 31);
            uint8_t op2_sign = IS_SET(operand2, 31);
            uint8_t out_sign = IS_SET(aluout, 31);
            
            cpsr_n_set(out_sign);
            cpsr_z_set(aluout == 0);
            cpsr_c_set( (aluout < operand1) && (aluout < operand2) );
            cpsr_v_set( 0 );
            printf("update flag nzcv %d%d%d%d \r\n", cpsr_n, cpsr_z, cpsr_c, cpsr_v);
        }
    } else if(code_type == code_is_bx || code_type == code_is_b) {
        if(Lf) {
            register_write(14, register_read(15) - 4);  //LR register
            printf("write register R%d = 0x%0x, ", 14, register_read(14));
        }
        register_write(15, aluout & 0xfffffffe);  //PC register
        printf("write register R%d = 0x%x \r\n", 15, aluout);
    } else if(code_type == code_is_ldm) {
        if(Lf) { //bit [20]
            //LDM
            uint32_t address = operand1;
            for(int i=0; i<15; i++) {
                if(IS_SET(instruction_word, i)) {
                    if(Pf) {
                        if(Uf) address += 4;
                        else address -= 4;
                    }
                    register_write(i, read_word(MEM, address));
                    printf("[LDM] load data [0x%x]:0x%x to R%d \r\n", address, read_word(MEM, address), i);
                    if(!Pf) {
                        if(Uf) address += 4;
                        else address -= 4;
                    }
                }
            }
            
            if(IS_SET(instruction_word, 15)) {
                if(Pf) {
                    if(Uf) address += 4;
                    else address -= 4;
                }
                register_write(15, read_word(MEM, address) & 0xfffffffe);
                printf("[LDM] load data [0x%x]:0x%x to R%d \r\n", address, read_word(MEM, address), 15);
                if(!Pf) {
                    if(Uf) address += 4;
                    else address -= 4;
                }
            }
            
            if(Wf) {  //bit[21] W
                register_write(Rn, address);
                printf("[LDM] write R%d = 0x%0x \r\n", Rn, address);
            }
            
        } else {
            //STM
            printf("unknow code_type = %d", code_type);
            getchar();
        }
    }
    
}


// it is used to set the reset values
//reset all registers and memory content to 0
void reset_proc(void)
{
    int i;
    cpsr = 0xd3;
    for(i=0; i<16; i++) {
        register_write(i, 0);
    }
    for(i=0x2000; i<0x4000; i++) {
        MEM[i]=0;
    }
}

int main()
{
    int i = 0x40000;
    
    load_program_memory("./Hello/Obj/hello.bin");
    reset_proc();
    
    while(i--) {
        fetch();
        decode();
        execute();
    }
    
    return 0;
}


