ENTRY(_ram_start)

MEMORY {
    /* Single-segment (Tiny model) RAM payload image (CS = DS = ES = SS = load_seg) */
    RAM (rwx) : ORIGIN = 0x0000, LENGTH = 60K
}

SECTIONS {
    .text 0x0000 : {
        KEEP(*(.text.entry))
        *(.text.init)
        *(.text)
        *(.text.*)
    } > RAM

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } > RAM

    .data : {
        __data_start = .;
        *(.data)
        *(.data.*)
        __data_end = .;
    } > RAM

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}
