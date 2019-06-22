ENTRY(_Reset)

_Min_Heap_Size = 0x1000;      /* required amount of heap  */
_Min_Stack_Size = 0x1000; /* required amount of stack */

/* Specify the memory areas */
MEMORY
{
   RAM (xrw)      : ORIGIN = 0x00000000, LENGTH = 0x18000
}


SECTIONS
{
   . = 0x0;
   .text :
   {
      startup.o (INTERRUPT_VECTOR)
      *(.text*)
      *(.rodata*)  /* all .rodata* sections (constants, strings, etc.) */
      KEEP (*(.init))
      KEEP (*(.fini))
      _end_text = .;
   } > RAM

   .data :
   {
      _start_data = .;
      *(.data*)
      . = ALIGN(4);
      _end_data = .;
   } > RAM

   .bss :
   {
      _start_bss = .;
      *(.bss*)
      *(COMMON)
      . = ALIGN(4);
      _end_bss = .;
   } > RAM

   .heap :
   {
       . = ALIGN(8);
       end = .;
       _end = .;
       . = . + _Min_Heap_Size;
       . = ALIGN(8);
   } > RAM

   . = ALIGN(8);
   . = . + _Min_Stack_Size; /* 4kB of stack memory */
   stack_top = .;

   .ARM.attributes 0 : { *(.ARM.attributes) }
}
