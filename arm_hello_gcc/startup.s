.section INTERRUPT_VECTOR, "x"
.global _Reset
_Reset:
    b Reset_Handler /* Reset */
    b . /* Undefined */
    b . /* SWI */
    b . /* Prefetch Abort */
    b . /* Data Abort */
    b . /* reserved */
    b . /* IRQ */
    b . /* FIQ */

Reset_Handler:
    ldr sp, =stack_top
    bl main
    b .
