OUTPUT_FORMAT(binary)
ENTRY(kstart)

KERNEL_VO  = 0xFFFFFFFF80000000;
KERNEL_VMA = 0xFFFFFFFF80100000;
KERNEL_LMA = 0x100000;

SECTIONS
{
    . = KERNEL_VMA;

    .text.boot : AT(KERNEL_LMA)
    {
        KEEP(*(kstart))
        *(.text.boot)
    }

    .text ALIGN(0x100) : AT(ADDR(.text) - KERNEL_VO)
    {
        *(.text)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VO)
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VO)
    {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VO)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /*
    .stack ALIGN(4K) : AT(ADDR(.stack) - KERNEL_VO)
    {
        *(.stack.*)
        . = ALIGN(16);
        __stack_start = .;
        . = . + 0x4000;
        . = ALIGN(16);
        __stack_end = .;
    }
    */

    .stack ALIGN(4K) : AT(ADDR(.stack) - KERNEL_VO)
    {
        *(.stack.*)
        . = ALIGN(16);
        __stack_start = .;  /* nome mais claro */
        . += 0x4000;
        . = ALIGN(16);
        __stack_end = .;     /* stack cresce para baixo */
    }

    .heap ALIGN(4K) : AT(ADDR(.heap) - KERNEL_VO)
    {
        *(.heap.*)
        . = ALIGN(16);
        __heap_start = .;
        . = . + 0x10000;
        __heap_end = .;
    }

    /* exporting for kernel.c visibility */
    _kernel_text_boot_start = ADDR(.text.boot);
    _kernel_text_boot_end   = ADDR(.text);

    _kernel_text_start      = ADDR(.text);
    _kernel_text_end        = ADDR(.rodata);

    _kernel_rodata_start    = ADDR(.rodata);
    _kernel_rodata_end      = ADDR(.data);

    _kernel_data_start      = ADDR(.data);
    _kernel_data_end        = ADDR(.bss);

    _kernel_bss_start       = ADDR(.bss);
    _kernel_bss_end         = ADDR(.stack);

    _kernel_stack_start     = __stack_start;
    _kernel_stack_end       = __stack_end;

    _kernel_heap_start      = __heap_start;
    _kernel_heap_end        = __heap_end;

    _kernel_vma             = KERNEL_VMA;
    _kernel_lma             = KERNEL_LMA;
    _kernel_vo              = KERNEL_VMA - KERNEL_LMA;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
