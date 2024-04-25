SECTIONS {
  . = 0x7C00;
  .mbr16 : { *(.mbr16) }
  . = 0x8000;
  .entry16 : { *(.entry16) }
  .entry32 : { *(.entry32) }
  .text : { *(.text) }
  .rodata : { *(.rodata*) }

  .init_array :
  {
    build/crti.o(.init_array)
    KEEP (*(SORT(EXCLUDE_FILE(build/crti.o build/crtn.o) .init_array)))
    KEEP (*(EXCLUDE_FILE(build/crti.o build/crtn.o) .init_array))
    build/crtn.o(.init_array)
  }

  .fini_array :
  {
    build/crti.o(.fini_array)
    KEEP (*(SORT(EXCLUDE_FILE(build/crti.o build/crtn.o) .fini_array.*)))
    KEEP (*(EXCLUDE_FILE(build/crti.o build/crtn.o) .fini_array))
    build/crtn.o(.fini_array)
  }

  .data : { 
    data_start = .; /* Define start of .data */ 
    *(.data) 
    data_end = .; /* Define end of .data */
    }

  .bss : { 
    bss_start = .; /* Define start of .bss */
    *(.bss) }
    bss_end = .; /* Define end of .bss */
}
