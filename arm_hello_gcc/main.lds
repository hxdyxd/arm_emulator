ENTRY(_Reset)

SECTIONS
{
   . = 0x0;
   .text :
   {
      startup.o (INTERRUPT_VECTOR)
      *(.text)
      *(.text*)
      KEEP (*(.init))
      KEEP (*(.fini))
      _end_text = .;
   }

   .data :
   {
      _start_data = .;
      *(.data)
      *(.data*)
      _end_data = .;
   }

   .bss :
   {
      _start_bss = .;
      *(.bss)
      *(.bss*)
      *(COMMON)
      _end_bss = .;
   }
   . = ALIGN(8);
   . = . + 0x1000; /* 4kB of stack memory */
   stack_top = .;
   _ebss = .;
}
