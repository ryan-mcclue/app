/* TODO(Ryan): Understand kiB to hex and thinking in hex */

ENTRY(my_start_symbol)
/* If want to have smaller binary size, will have to manually create elf headers */
__linux_mmap_min_addr = 0x10000;

SECTIONS 
{
  . = __linux_mmap_min_addr;
  .my_section : 
  { 
    *(.my_section*) 
  }
  .rodata : 
  {
    *(.rodata*) 
  }
}
