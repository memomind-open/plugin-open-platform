ENTRY(gm_plugin_entry)

SECTIONS
{
    . = 0;

    .text : ALIGN(4)
    {
        KEEP(*(.text.gm_plugin_entry))
        *(.text*)
    }

    .rodata : ALIGN(4)
    {
        *(.rodata*)
        *(.srodata*)
    }

    .data : ALIGN(4)
    {
        *(.data*)
        *(.sdata*)
    }

    .got : ALIGN(4)
    {
        *(.got*)
    }

    __gm_image_end = .;

    .bss (NOLOAD) : ALIGN(4)
    {
        __gm_bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        __gm_bss_end = .;
    }

    __gm_memory_end = .;

    /DISCARD/ :
    {
        *(.eh_frame*)
        *(.gcc_except_table*)
        *(.init*)
        *(.fini*)
    }
}

ASSERT(gm_plugin_entry == 0, "gm_plugin_entry must be first")
ASSERT(SIZEOF(.got) == 0, "GOT is forbidden in GM PIC plugins")
