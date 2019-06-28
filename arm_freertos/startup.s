/* 2019 06 29 */
/* By hxdyxd */
.section INTERRUPT_VECTOR, "x"

/*
 * Exception vector table
 */
.global _Reset
_Reset:
    b Reset_Handler
    ldr pc, _undefined_instruction
    ldr pc, _software_interrupt
    ldr pc, _prefetch_abort
    ldr pc, _data_abort
    ldr pc, _not_used
    ldr pc, _irq
    ldr pc, _fiq

_undefined_instruction:
    .word undefined_instruction
_software_interrupt:
    .word vPortYieldProcessor
_prefetch_abort:
    .word prefetch_abort
_data_abort:
    .word data_abort
_not_used:
    .word not_used
_irq:
    .word vTickISR
_fiq:
    .word fiq

    /* Stack Sizes */
    .set  UND_STACK_SIZE, 0x00000004
    .set  ABT_STACK_SIZE, 0x00000004
    .set  FIQ_STACK_SIZE, 0x00000004
    .set  IRQ_STACK_SIZE, 0X00000400
    .set  SVC_STACK_SIZE, 0X00000400

    /* Standard definitions of Mode bits and Interrupt (I & F) flags in PSRs */
    .set  MODE_USR, 0x10            /* User Mode */
    .set  MODE_FIQ, 0x11            /* FIQ Mode */
    .set  MODE_IRQ, 0x12            /* IRQ Mode */
    .set  MODE_SVC, 0x13            /* Supervisor Mode */
    .set  MODE_ABT, 0x17            /* Abort Mode */
    .set  MODE_UND, 0x1B            /* Undefined Mode */
    .set  MODE_SYS, 0x1F            /* System Mode */

    .equ  I_BIT, 0x80               /* when I bit is set, IRQ is disabled */
    .equ  F_BIT, 0x40               /* when F bit is set, FIQ is disabled */


Reset_Handler:

    /* Setup a stack for each mode - note that this only sets up a usable stack
    for system/user, SWI and IRQ modes.   Also each mode is setup with
    interrupts initially disabled. */
    ldr r0, =stack_top
    msr CPSR_c, #MODE_UND|I_BIT|F_BIT /* Undefined Instruction Mode */
    mov   sp, r0
    sub   r0, r0, #UND_STACK_SIZE
    msr   CPSR_c, #MODE_ABT|I_BIT|F_BIT /* Abort Mode */
    mov   sp, r0
    sub   r0, r0, #ABT_STACK_SIZE
    msr   CPSR_c, #MODE_FIQ|I_BIT|F_BIT /* FIQ Mode */
    mov   sp, r0
    sub   r0, r0, #FIQ_STACK_SIZE
    msr   CPSR_c, #MODE_IRQ|I_BIT|F_BIT /* IRQ Mode */
    mov   sp, r0
    sub   r0, r0, #IRQ_STACK_SIZE
    msr   CPSR_c, #MODE_SVC|I_BIT|F_BIT /* Supervisor Mode */
    mov   sp, r0
    sub   r0, r0, #SVC_STACK_SIZE
    msr   CPSR_c, #MODE_SYS|I_BIT|F_BIT /* System Mode */
    mov   sp, r0

    /* We want to start in supervisor mode.  Operation will switch to system
    mode when the first task starts. */
    msr   CPSR_c, #MODE_SVC|I_BIT|F_BIT

    /* Clear BSS. */
    /* Initialise data. */
    bl      reset

    mov     r0, #0          /* no arguments  */
    mov     r1, #0          /* no argv either */
    bl      main
    b .


undefined_instruction:
    b .

prefetch_abort:
    b .

data_abort:
    b .

not_used:
    b .

fiq:
    b .
