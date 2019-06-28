/* 2019 06 26 */
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
    msr cpsr_c, #0xd2   @进入IRQ模式
    ldr sp, =stack_top  @设置栈指针R13_irq
    msr cpsr_c, #0xd3   @进入SVC模式
    ldr sp, =stack_top  @设置栈指针R13_svc
    sub sp, sp, #80
    msr cpsr_c, #0xd0   @进入USER模式
    ldr sp, =stack_top  @设置栈指针R13
    sub sp, sp, #80
    sub sp, sp, #80
    bl reset
    msr cpsr_c, #0x50   @进入USER模式,开中断
    swi 0xcaf4
    bl main
    b .

undefined_instruction:
    b .

software_interrupt:
    stmdb sp!,{r0-r12,lr}     @寄存器入栈
    ldr r0, [lr, #-4]         @计算swi指令地址
    bic r0, r0, #0xff000000   @提取swi中断号
    ldr lr,=swi_return        @设置中断处理程序返回地址
    ldr pc,=handle_swi        @跳到中断处理程序
swi_return:
    ldmia sp!,{r0-r12,pc}^    @中断异常返回，寄存器出栈和恢复cpsr寄存器
    b .

prefetch_abort:
    b .

data_abort:
    b .

not_used:
    b .

irq:
    sub lr, lr, #4          @修正返回地址R14_irq
    stmdb sp!,{r0-r12,lr}   @寄存器入栈
    ldr lr,=irq_return      @设置中断处理程序返回地址
    ldr pc,=handle_irq      @跳到中断处理程序
irq_return:
    ldmia sp!,{r0-r12,pc}^  @中断异常返回，寄存器出栈和恢复cpsr寄存器
    b .

fiq:
    b .
