ENTRY(gm_plugin_entry)
PHDRS { flash PT_LOAD FLAGS(5); ram PT_LOAD FLAGS(6); }
SECTIONS
{
    . = 0;
    .text : ALIGN(4) { KEEP(*(.text.gm_plugin_entry)) *(.text*) . = ALIGN(4); } :flash
    .rodata : ALIGN(4) { *(.rodata*) *(.srodata*) . = ALIGN(4); } :flash
    __gm_flash_end = .;
    . = 0x10000000;
    .data : ALIGN(4) { *(.data*) *(.sdata*) *(.got*) . = ALIGN(4); } :ram
    .bss (NOLOAD) : ALIGN(4) { *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); } :ram
    __gm_ram_end = .;
    /DISCARD/ : { *(.eh_frame*) *(.gcc_except_table*) *(.init*) *(.fini*) }
}
ASSERT(gm_plugin_entry == 0, "gm_plugin_entry must be first")

ASSERT(__gm_flash_end <= 500 * 1024, "plugin Flash code/constants exceed 500 KiB")
ASSERT(__gm_ram_end - 0x10000000 < 100 * 1024, "plugin static RAM must be below 100 KiB (heap/stack excluded)")
