/*
 * disassembly.c of arm_emulator
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
#include <stdio.h>
#include <disassembly.h>


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
const char *code_cond_table[16] = {
    "EQ","NE","CS","CC",
    "MI","PL","VS","VC",
    "HI","LS","GE","LT",
    "GT","LE","","-",
};


uint8_t code_decoder(const union ins_t ins)
{
    uint8_t code_type = code_type_unknow;
    if(ins.dp_is.cond == 0xf)
        return code_type;
    switch(ins.dp_is.f) {
    case 0:
        switch(Bit4) {
        case 0:
            if( (ins.word & 0x1b0ffe0) == 0x120f000) {
                //Miscellaneous instructions
                code_type = code_type_msr0;
            } else if( (ins.word & 0x1bf0fff ) == 0x10f0000 ) {
                //Miscellaneous instructions
                code_type = code_type_mrs;
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
                    if( (ins.word & 0x1ffffc0) == 0x12fff00) {
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
                        code_type = code_type_clz;
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
                        if(!Bit22)
                            code_type = code_type_mult;
                    } else if(Bit24_23 == 1) {
                        code_type = code_type_multl;
                    } else if(Bit24_23 == 2 && !Bit20 && !Bit21 && !Rs) {
                        code_type = code_type_swp;
                    }
                    break;
                case 1:
                    //code_type_ldrh
                    if(Bit22) {
                        code_type = code_type_ldrh1;
                    } else if(!Rs) {
                        code_type = code_type_ldrh0;
                    }
                    break;
                case 2:
                    //code_type_ldrsb
                    if(Bit22) {
                        code_type = code_type_ldrsb1;
                    } else if(!Rs) {
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
                    } else if(!Rs) {
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
        if( (ins.word & 0x1b0f000) == 0x120f000) {
            //24_23,21_20,15_12
            code_type = code_type_msr1;
        } else if( (ins.word & 0x1b00000) == 0x1000000 ) {
            //24_23,21_20
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


uint8_t code_disassembly(const uint32_t code, char *buf, int len)
{
    const union ins_t ins = {
        .word = code,
    };
    uint8_t code_type = code_decoder(ins);
    switch(code_type) {
    case code_type_dp0:
        sprintf(buf, "%s%s%s\tR%d, R%d, R%d %s #%d",
             opcode_table[opcode], Bit20?"S":"", code_cond_table[ins.dp_is.cond], Rd,
              Rn, Rm, shift_table[shift], shift_amount);
        break;
    case code_type_dp1:
        sprintf(buf, "%s%s%s\tR%d, R%d, R%d %s R%d",
            opcode_table[opcode], Bit20?"S":"", code_cond_table[ins.dp_is.cond], Rd,
             Rn, Rm, shift_table[shift], Rs);
        break;
    case code_type_dp2:
        sprintf(buf, "%s%s%s\tR%d, R%d, #%d",
         opcode_table[opcode], Bit20?"S":"", code_cond_table[ins.dp_is.cond], Rd,
          Rn, ((immediate_i << (32 - rotate_imm*2)) | (immediate_i >> (rotate_imm*2))) );
        break;

    case code_type_bx:
        sprintf(buf, "B%sX%s\tR%d", Lf_bx?"L":"", code_cond_table[ins.dp_is.cond], Rm);
        break;
    case code_type_b:
        sprintf(buf, "B%s%s\tPC + 0x%08x", Lf_b?"L":"", code_cond_table[ins.dp_is.cond], immediate_b);
        break;

    case code_type_ldr0:
        if(Pf) {
            sprintf(buf, "%s%s%s\tR%d, [R%d, #%s%u]%s",
             Lf?"LDR":"STR", Bf?"B":"", code_cond_table[ins.dp_is.cond],
              Rd, Rn, Uf?"":"-", immediate_ldr, Wf?"!":"");
        } else {
            sprintf(buf, "%s%s%s%s\tR%d, [R%d], #%s%u",
             Lf?"LDR":"STR", Bf?"B":"", Wf?"T":"", code_cond_table[ins.dp_is.cond],
              Rd, Rn, Uf?"":"-", immediate_ldr);
        }
        break;
    case code_type_ldr1:
        if(Pf) {
            sprintf(buf, "%s%s%s\tR%d, [R%d, %sR%d %s #%d]%s",
             Lf?"LDR":"STR", Bf?"B":"", code_cond_table[ins.dp_is.cond],
              Rd, Rn, Uf?"":"-", Rm, shift_table[shift], shift_amount, Wf?"!":"");
        } else {
            sprintf(buf, "%s%s%s%s\tR%d, [R%d], %sR%d %s #%d",
             Lf?"LDR":"STR", Bf?"B":"", Wf?"T":"", code_cond_table[ins.dp_is.cond],
              Rd, Rn, Uf?"":"-", Rm, shift_table[shift], shift_amount);
        }
        break;
    case code_type_ldrh0:
        if(Pf) {
            sprintf(buf, "%sH%s\tR%d, [R%d, %sR%d]%s",
             Lf?"LDR":"STR", code_cond_table[ins.dp_is.cond], Rd, Rn, Uf?"":"-", Rm, Wf?"!":"");
        } else {
            sprintf(buf, "%sH%s%s\tR%d, [R%d], %sR%d",
             Lf?"LDR":"STR", Wf?"T":"", code_cond_table[ins.dp_is.cond], Rd, Rn, Uf?"":"-", Rm);
        }
        break;
    case code_type_ldrh1:
        if(Pf) {
            sprintf(buf, "%sH%s\tR%d, [R%d, #%s%d]%s",
             Lf?"LDR":"STR", code_cond_table[ins.dp_is.cond], Rd, Rn, Uf?"":"-", immediate_extldr, Wf?"!":"");
        } else {
            sprintf(buf, "%sH%s%s\tR%d, [R%d], #%s%d",
             Lf?"LDR":"STR", Wf?"T":"", code_cond_table[ins.dp_is.cond], Rd, Rn, Uf?"":"-", immediate_extldr);
        }
        break;
    case code_type_ldrsb0:
        if(Pf) {
            sprintf(buf, "LDRSB%s\tR%d, [R%d, %sR%d]%s",
             code_cond_table[ins.dp_is.cond], Rd, Rn, Uf?"":"-", Rm, Wf?"!":"");
        } else {
            sprintf(buf, "LDRSB%s%s\tR%d, [R%d], %sR%d",
             Wf?"T":"", code_cond_table[ins.dp_is.cond], Rd, Rn, Uf?"":"-", Rm);
        }
        break;
    case code_type_ldrsb1:
        if(Pf) {
            sprintf(buf, "LDRSB%s\tR%d, [R%d, #%s%d]%s",
             code_cond_table[ins.dp_is.cond], Rd, Rn, Uf?"":"-", immediate_extldr, Wf?"!":"");
        } else {
            sprintf(buf, "LDRSB%s%s\tR%d, [R%d], #%s%d",
             Wf?"T":"", code_cond_table[ins.dp_is.cond], Rd, Rn, Uf?"":"-", immediate_extldr);
        }
        break;
    case code_type_ldrsh0:
        if(Pf) {
            sprintf(buf, "LDRSH%s\tR%d, [R%d, %sR%d]%s",
             code_cond_table[ins.dp_is.cond], Rd, Rn, Uf?"":"-", Rm, Wf?"!":"");
        } else {
            sprintf(buf, "LDRSH%s%s\tR%d, [R%d], %sR%d",
             Wf?"T":"", code_cond_table[ins.dp_is.cond], Rd, Rn, Uf?"":"-", Rm);
        }
        break;
    case code_type_ldrsh1:
        if(Pf) {
            sprintf(buf, "LDRSH%s\tR%d, [R%d, #%s%d]%s",
             code_cond_table[ins.dp_is.cond], Rd, Rn, Uf?"":"-", immediate_extldr, Wf?"!":"");
        } else {
            sprintf(buf, "LDRSH%s%s\tR%d, [R%d], #%s%d",
             Wf?"T":"", code_cond_table[ins.dp_is.cond], Rd, Rn, Uf?"":"-", immediate_extldr);
        }
        break;

    case code_type_msr0:
        sprintf(buf, "MSR%s\t%sPSR_%s%s%s%s R%d",
         code_cond_table[ins.dp_is.cond], Bit22?"S":"C",
          Rn&8?"f":"", Rn&4?"s":"", Rn&2?"x":"", Rn&1?"c":"", Rm);
        break;
    case code_type_msr1:
        sprintf(buf, "MSR%s\t%sPSR_%s%s%s%s #0x%x",
         code_cond_table[ins.dp_is.cond], Bit22?"S":"C",
          Rn&8?"f":"", Rn&4?"s":"", Rn&2?"x":"", Rn&1?"c":"",
           ((immediate_i << (32 - rotate_imm*2)) | (immediate_i >> (rotate_imm*2))) );
        break;
    case code_type_mrs:
        sprintf(buf, "MRS%s\tR%d %s",
         code_cond_table[ins.dp_is.cond], Rd, Bit22?"SPSR":"CPSR");
        break;

    case code_type_mult:
        sprintf(buf, "%s%s%s\tR%d, R%d, R%d, R%d",
         Bit21?"MLA":"MUL", Bit20?"S":"", code_cond_table[ins.dp_is.cond], Rn, Rm, Rs, Rd);
        break;

    case code_type_multl:
        sprintf(buf, "%sM%s%s%s\tR%d, R%d, R%d, R%d",
         Bit22?"S":"U", Bit21?"LAL":"ULL", Bit20?"S":"", code_cond_table[ins.dp_is.cond], Rd, Rn, Rm, Rs);
        break;

    case code_type_swp:
        sprintf(buf, "SWP%s%s\tR%d R%d [R%d]",
         Bf?"B":"", code_cond_table[ins.dp_is.cond], Rd, Rm, Rn);
        break;

    case code_type_ldm:
    {
        char *pbuf = buf;
        pbuf += sprintf(pbuf, "%s%s%s%s\tR%d%s, {",
         Lf?"LDM":"STM", Uf?"I":"D", Pf?"B":"A",
          code_cond_table[ins.dp_is.cond],  Rn, Wf?"!":"");
        char *list_start = pbuf;
        for(int i=0; i<16; i++) {
            if( (1<<i) & ins.word) {
                if(pbuf != list_start) {
                    pbuf += sprintf(pbuf, " ,");
                }
                pbuf += sprintf(pbuf, "R%d", i);
            }
        }
        sprintf(pbuf, "}%s", Bit22?"^":"");
        break;
    }

    case code_type_swi:
        sprintf(buf, "SVC%s\t0x%08x", code_cond_table[ins.dp_is.cond], ins.swi.number);
        break;
    case code_type_mcr:
        sprintf(buf, "%s%s\tp%d, %d, R%d, c%d, c%d, %d",
         Bit20?"MRC":"MCR", code_cond_table[ins.dp_is.cond], ins.mcr.cp_num, ins.mcr.opcode1,
          Rd, ins.mcr.CRn, ins.mcr.CRm, ins.mcr.opcode2);
        break;
    default:
        sprintf(buf, "\t; undefined");
        break;
    }
    return code_type;
}
/*****************************END OF FILE***************************/
