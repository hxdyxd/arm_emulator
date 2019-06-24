ENTRY(_Reset)

_Min_Heap_Size = 0x1000;      /* required amount of heap  */
_Min_Stack_Size = 0x1000; /* required amount of stack */

/* Specify the memory areas */
MEMORY
{
   FLASH (RX)     : ORIGIN = 0x00000000, LENGTH = 0x10000
   RAM (RWX)      : ORIGIN = 0x00010000, LENGTH = 0x8000
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
   } >FLASH

   .data :
   {
      _start_data = .;
      *(.data*)
      . = ALIGN(4);
      _end_data = .;
   } >FLASH

   .bss :
   {
      _start_bss = .;
      *(.bss*)
      *(COMMON)
      . = ALIGN(4);
      _end_bss = .;
   } >FLASH

   .heap :
   {
       . = ALIGN(8);
       end = .;
       _end = .;
       . = . + _Min_Heap_Size;
       . = ALIGN(8);
   } >FLASH

   . = ALIGN(8);
   . = . + _Min_Stack_Size; /* 4kB of stack memory */
   stack_top = .;

   .ARM.attributes 0 : { *(.ARM.attributes) }
}
