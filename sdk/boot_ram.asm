BITS 16
CPU 8086

SECTION .text.entry progbits alloc exec
global _ram_start
extern app_main
extern __bss_start
extern __bss_end

_ram_start:
    cli
    cld

    ; Ensure all segment registers match CS (Tiny model in target RAM segment)
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0xFFFE

    ; Clear .bss section in target RAM segment
    mov cx, __bss_end
    sub cx, __bss_start
    jz .skip_bss
    mov di, __bss_start
    xor ax, ax
    rep stosb
.skip_bss:
    sti

    ; Call C application entry point
    call app_main

    ; If app_main returns (e.g. on ESC), warm-reboot back into RomLoader-16
    cli
    jmp 0xFFFF:0x0000

global loader_jump_far
loader_jump_far:
    cli
    cld
    mov bp, sp
    mov ax, [bp+2]      ; seg
    mov bx, [bp+4]      ; off
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0xFFFE
    push ax             ; target CS
    push bx             ; target IP
    retf
