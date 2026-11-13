BITS 16
CPU 8086

; =====================================================================
; RESET VECTOR (Mapped to physical 0xFFFF0)
; =====================================================================
SECTION .reset_vector progbits alloc exec
global _reset
_reset:
    jmp 0xF000:start16

SECTION .reserved_pad progbits alloc
    dw 0xFFFF
    dw 0xFFFF

; =====================================================================
; INITIALIZATION CODE (Mapped to physical 0xF0000)
; =====================================================================
SECTION .text.init progbits alloc exec
global start16
global systemInit_done
global loader_jump_far

extern app_main
extern systemInit_start
extern systemInit_error
extern setupVectors

; Linker script symbols
extern __data_start
extern __data_end
extern __rom_data_start
extern __bss_start
extern __bss_end

start16:
    cli
    cld
    jmp systemInit_start

systemInit_done:
    ; 1. Setup Stack and Data Segments in RAM (Segment 0x0000)
    xor ax, ax
    mov ss, ax
    mov sp, 0xFFFE
    mov ds, ax
    mov es, ax

    call setupVectors

    ; 2. Execute VGA Option ROM (0xC000:0003)
    call 0xC000:0x0003

    ; 3. Restore Data Segments (VGA ROM might have changed them)
    xor ax, ax
    mov ds, ax
    mov es, ax

    ; 4. Copy initialized data (.data) from ROM to RAM
    mov cx, __data_end
    sub cx, __data_start
    jz .skip_data
    mov si, __rom_data_start
    mov di, __data_start
.copy_data:
    cs lodsb
    stosb
    loop .copy_data
.skip_data:

    ; 5. Clear BSS section in RAM
    mov cx, __bss_end
    sub cx, __bss_start
    jz .skip_bss
    mov di, __bss_start
    xor ax, ax
    rep stosb
.skip_bss:
    sti
    ; 6. Call the C ROM Loader entry point
    call app_main

    ; 7. Infinite Halt Loop
.halt:
    hlt
    jmp .halt

; =====================================================================
; void loader_jump_far(uint16_t seg, uint16_t off)
; Transfers control to a loaded binary payload at seg:off
; =====================================================================
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
