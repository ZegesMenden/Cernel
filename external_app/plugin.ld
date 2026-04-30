ENTRY(calculate_sqrt)

SECTIONS
{
  /* Link at VMA 0 so the loader can add its own base address. */
  . = 0;

  .text : ALIGN(4)
  {
    KEEP(*(.text.calculate_sqrt))
    *(.text*)
  }

  .rodata : ALIGN(4)
  {
    *(.rodata*)
  }

  .data : ALIGN(4)
  {
    *(.data*)
  }

  .bss : ALIGN(4)
  {
    *(.bss*)
    *(COMMON)
  }

  /DISCARD/ :
  {
    *(.eh_frame*)
    *(.comment)
    *(.note*)
  }
}
