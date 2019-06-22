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
    .word software_interrupt
_prefetch_abort:
    .word prefetch_abort
_data_abort:
    .word data_abort
_not_used:
    .word not_used
_irq:
    .word irq
_fiq:
    .word fiq

Reset_Handler:
    msr CPSR_c, #0x10
    ldr sp, =stack_top
    bl main
    b .

undefined_instruction:
    b .

software_interrupt:
    b .

prefetch_abort:
    b .

data_abort:
    b .

not_used:
    b .

irq:
    b .

fiq:
    b .
