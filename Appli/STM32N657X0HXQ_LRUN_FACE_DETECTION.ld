/*
 * Face detection memory map.
 * AXISRAM2 and AXISRAM3-6 are reserved by the generated NPU network.
 * The LCD framebuffers are placed in external XSPI1 PSRAM.
 */

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(AXISRAM1_S) + LENGTH(AXISRAM1_S); /* end of "AXISRAM1_S" Ram type memory */
_sstack = _estack - _Min_Stack_Size;

_Min_Heap_Size = 0x200; /* required amount of heap  */
_Min_Stack_Size = 0x1000; /* startup and exception stack */

/* Memories definition */
MEMORY
{
  AXISRAM1_S (xrw)      : ORIGIN = 0x34000400, LENGTH =  1023K
  FLASH (xrw)           : ORIGIN = 0x71380000, LENGTH =  2M
  PSRAM (xrw)           : ORIGIN = 0x91000000, LENGTH =  16M
}

/* Sections */
SECTIONS
{
  /* The startup code into "RAM" Ram type memory */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >AXISRAM1_S

  /* The program code and other data into "RAM" Ram type memory */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */

    KEEP (*(.init))
    KEEP (*(.fini))
    KEEP (*(.keep_me))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >AXISRAM1_S

  .gnu.sgstubs :
  {
    _sNSCVeneer = .;
    . = ALIGN(4);
    *(.gnu.sgstubs*)   /* Secure Gateway stubs */
    . = ALIGN(4);
  } >AXISRAM1_S
  _eNSCVeneer = .;

  /* Constant data into "RAM" Ram type memory */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >AXISRAM1_S

  .noncacheable (NOLOAD) :
  {
    . = ALIGN(32);
    __snoncacheable = .;
    __uncached_bss_start__ = .;
    KEEP(*(.noncacheable))
    KEEP(*(.uncached_bss))
    . = ALIGN(32);
    __uncached_bss_end__ = .;
    __enoncacheable = .;
  } >AXISRAM1_S

  .ARM.extab   : {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >AXISRAM1_S

  .ARM : {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >AXISRAM1_S

  .preinit_array     :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >AXISRAM1_S

  .init_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >AXISRAM1_S

  .fini_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >AXISRAM1_S

  /* Used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections into "RAM" Ram type memory */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */

  } >AXISRAM1_S

  /* Uninitialized data section into "RAM" Ram type memory */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >AXISRAM1_S

  /* User_heap_stack section, used to check that there is enough "RAM" Ram  type memory left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    /* [__libc_heap_start, __libc_heap_end) belongs to newlib's _sbrk (see
     * sysmem.c). _Min_Stack_Size below is only a sizing check (the real MSP
     * stack lives at the top of RAM, from _sstack to _estack); it is kept
     * here unallocated to any C symbol so it stays out of both allocators'
     * reach. _end/end are placed AFTER this whole reservation, since the
     * micro T-Kernel Imalloc region (sys_start.c) starts at _end -- keeping
     * _end here would make Imalloc overlap the newlib heap. */
    PROVIDE ( __libc_heap_start = . );
    . = . + _Min_Heap_Size;
    PROVIDE ( __libc_heap_end = . );
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
  } >AXISRAM1_S

  .flash_section :
  {
     . = ALIGN(32);
    *(.flash_blob)
    . = ALIGN(32);
  } >FLASH

  .psram_section (NOLOAD):
  {
     . = ALIGN(32);
    *(.psram_bss)
    . = ALIGN(32);
  } >PSRAM

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
