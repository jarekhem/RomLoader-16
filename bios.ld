ENTRY(_reset)

_Segment_Size = 64K;
_ROM_Size = 32K;
_ROM_Reserved = 4;
_RAM_Reserved = 0x500;

MEMORY {
    /* 32KB ROM image mapped at CS=0xF000 (offsets 0x8000..0xFFFF) */
    ROM (rx) : ORIGIN = _Segment_Size - _ROM_Size, LENGTH = 32K
    /* Loader RAM in segment 0x0000 after IVT (0x000..0x3FF) and BDA (0x400..0x4FF) */
    RAM (rw) : ORIGIN = 0x0000 + _RAM_Reserved, LENGTH = _Segment_Size - _RAM_Reserved
}

SECTIONS {
    .text : {
        *(.text.init)
        *(.text)
        *(.text.*)
    } > ROM

    .data : {
        __data_start = .;
        *(.data)
        *(.data.*)

        *(.rodata)
        *(.rodata.*)

        __data_end = .;
    } > RAM AT > ROM

    __rom_data_start = LOADADDR(.data);

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /* 8086 Reset Vector at F000:FFF0 (physical 0xFFFF0) */
    .reset 0xFFF0 : {
        KEEP(*(.reset_vector))
    } > ROM

    .reserved (_Segment_Size - _ROM_Reserved) : {
        KEEP(*(.reserved_pad))
    } > ROM
}
