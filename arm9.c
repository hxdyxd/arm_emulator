#include <stdio.h>
#include <stdint.h>

#define MEM_SIZE  0x6000

#define SubBIT(code, a, b) ((code) >> (b)) & 


#define Read_PC  (R[15] + 4)

//Register file
static unsigned int R[16];

//flags
static int N,C,V,Z;

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


void fetch(void)
{
    instruction_word = read_word(MEM, R[15]);
    //printf("FETCH: Fetch instruction 0x%x from address 0x%x\n", instruction_word, R[15]);
    printf("[0x%04x]: ", R[15]);
    R[15] += 4;
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

void decode(void)
{
    unsigned char Rn = 0;
    unsigned char Rd = 0;
    unsigned char Rs = 0;
    unsigned char Rm = 0;
    unsigned short immediate = 0;
    //ldr/str
    unsigned char Lf = 0, Bf = 0, Uf = 0;
    signed int b_offset = 0;
    unsigned char opcode = 0;
    unsigned char shift = 0;
    unsigned char rotate_imm = 0;
    unsigned char shift_amount = 0;
    unsigned char cond = 0;
    unsigned char f = 0;
    unsigned char Bit22 = 0, Bit20 = 0, Bit7 = 0, Bit4 = 0;
    unsigned char Bit24_23 = 0;
    
    cond = (instruction_word&0xF0000000) >> 28;  /* bit[31:28] */
    f = (instruction_word&0x0E000000)>>25;  /* bit[27:25] */
    opcode = (instruction_word&0x1E00000) >> 21;  /* bit[24:21] */
    Bit24_23 = opcode >> 2;  /* bit[24:23] */
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
                    printf("msr0 \r\n");
                } else if(!Bit21 && !Bit7) {
                    code_type = code_is_mrs;
                    printf("mrs \r\n");
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
                    code_type = code_is_bx;
                    printf("bx \r\n");
                } else {
                    //Data processing register shift by register
                    code_type = code_is_dp1;
                    printf("dp1 %s dst register R%d, first operand R%d, Second operand R%d %s R%d\r\n", opcode_table[opcode], Rd, Rn, Rm, shift_table[shift], Rs);
                }
                break;
            case 1:
                //Multiplies, extra load/stores
                Bit22 = (instruction_word&0x400000) >> 22;  /* bit[22] */
                switch(shift) {
                case 0:
                    if(Bit24_23 == 0) {
                        code_type = code_is_mult;
                        printf("mult \r\n");
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
            
            printf("msr1 \r\n");
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
        printf("ldr0 %s%s dst register R%d, base address at R%d, Second address immediate #%s0x%x\r\n", Lf?"LDR":"STR", Bf?"B":"", Rd, Rn, Uf?"+":"-", immediate);
        
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
        printf("ldm \r\n");
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
        printf("B%s PC[0x%x] +  0x%08x = 0x%x\r\n", Lf?"L":"", Read_PC, b_offset, Read_PC + b_offset );
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
/*    if(code_is_ldrh1 | code_is_ldrsb1 | code_is_ldrsh1) {
        code_rm = 
    } else if(code_is_b) {
        code_rm = 
    } else if(code_is_ldm) {
        code_rm = 
    } else if(code_is_ldr0) {
        code_rm = instruction_word&0xfff;
    } else if(code_is_msr1 | code_is_dp2) {
        code_rm = instruction_word&0xf;
    } else if(code_is_multl & ) {
        code_rm = instruction_word&0xf;
    }*/
}


// it is used to set the reset values
//reset all registers and memory content to 0
void reset_proc(void)
{
    int i;
    for(i=0;i<15;i++){
        R[i]=0;
    }
    for(i=0x2000; i<0x4000; i++){
        MEM[i]=0;
    }
    R[13]=0x3000;
}

int main()
{
    int i = 0x2000/4;
    
    load_program_memory("hello.bin");
    reset_proc();
    
    while(i--) {
        fetch();
        decode();
    }
    
    return 0;
}


