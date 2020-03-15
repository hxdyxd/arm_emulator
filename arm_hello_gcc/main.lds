/*
 * main.lds of arm_emulator
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

ENTRY(_Reset)

_Min_Heap_Size = 0x1200;      /* required amount of heap  */
_Min_Stack_Size = 0x1600; /* required amount of stack */

/* Specify the memory areas */
MEMORY
{
   FLASH (RX)     : ORIGIN = 0x00000000, LENGTH = 0x10000
   RAM (RWX)      : ORIGIN = 0x00010000, LENGTH = 0x8000
}

SECTIONS
{
   . =0x0;

   .text :
   {
      startup.o (INTERRUPT_VECTOR)
      *(.text)           /* .text sections (code) */
      *(.text*)          /* .text* sections (code) */
      . = ALIGN(4);
      _end_text = .;
   } >FLASH

   .rodata :
   {
      . = ALIGN(4);
      *(.rodata)         /* .rodata sections (constants, strings, etc.) */
      *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
      . = ALIGN(4);
   } >FLASH

   _sidata = LOADADDR(.data);
   .data :
   {
      . = ALIGN(4);
      _start_data = .;
      *(.data*)
      . = ALIGN(4);
      _end_data = .;
   } >RAM AT >FLASH

   .bss :
   {
      . = ALIGN(4); 
      _start_bss = .;
      *(.bss*)
      *(COMMON)
      _end_bss = .;
   } >RAM

   .heap :
   {
      . = ALIGN(4);
       end = .;
       _end = .;
       . = . + _Min_Heap_Size;
   } >RAM

   .stack :
   {
      . = ALIGN(8);
      . = . + _Min_Stack_Size; /* 4kB of stack memory */
      stack_top = .;
   } >RAM

   .ARM.attributes 0 : { *(.ARM.attributes) }
}
