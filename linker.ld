ENTRY(_start)

PHDRS
{
    text   PT_LOAD FLAGS(5);   /* R-X */
    rodata PT_LOAD FLAGS(4);   /* R-- */
    data   PT_LOAD FLAGS(6);   /* RW- */
}

SECTIONS
{
    . = 1M;
    _kernel_start = .;

    _text_start = .;
    .text : {
        KEEP(*(.multiboot))        /* must be within the first 8 KiB of the file */
        *(.boot)
        *(.text .text.*)
    } :text
    . = ALIGN(4K);
    _text_end = .;

    _rodata_start = .;
    .rodata : { *(.rodata .rodata.*) } :rodata
    . = ALIGN(4K);
    _rodata_end = .;

    _data_start = .;
    .data : { *(.boot_data) *(.data .data.*) } :data
    .bss (NOLOAD) : { *(COMMON) *(.bss .bss.*) } :data
    . = ALIGN(4K);
    _kernel_end = .;

    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) *(.gnu.*) }
}
