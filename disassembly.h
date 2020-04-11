/*
 * disassembly.h of arm_emulator
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
#ifndef _DISASSEMBLY_H
#define _DISASSEMBLY_H

#include <stdint.h>

#define AS_CODE_LEN          (256)
#define AS_CODE_FORMAT       "%8x:\t%08x\t%s\n"


#define  code_type_swi      1
#define  code_type_b        2
#define  code_type_ldm      3
#define  code_type_ldr1     4
#define  code_type_ldr0     5
#define  code_type_dp2      6
#define  code_type_msr1     7
#define  code_type_ldrsh1   8
#define  code_type_ldrsh0   9
#define  code_type_ldrsb1  10
#define  code_type_ldrsb0  11
#define  code_type_ldrh1   12
#define  code_type_ldrh0   13
#define  code_type_swp     14
#define  code_type_multl   15
#define  code_type_mult    16
#define  code_type_dp1     17
#define  code_type_bx      18
#define  code_type_dp0     19
#define  code_type_msr0    20
#define  code_type_mrs     21
#define  code_type_mcr     22
#define  code_type_clz     23
#define  code_type_ldrd1   24
#define  code_type_ldrd0   25
#define  code_type_unknow  255

/* instruction struct */
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

#define NO_RD       (Bit24_23 == 2) //TST, TEQ, CMP, CMN
#define NO_RN       (Bit24_23 == 3 && Bit21) //MOV, MVN

#define immediate_i       (ins.dp_i.immediate)
#define immediate_ldr     (ins.ldr_i.immediate)
#define immediate_extldr  ((Rs << 4) | Rm)
#define immediate_b       (((ins.b.offset23) ? (ins.b.offset22_0 | 0xFF800000) : (ins.b.offset22_0)) << 2)

uint8_t code_decoder(const union ins_t ins);
uint8_t code_disassembly(const uint32_t code, const uint32_t pc, char *buf, int len);

#endif
/*****************************END OF FILE***************************/
