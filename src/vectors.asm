CPU 8086
BITS 16

%include "InterruptHandlers.inc"

pic1_reg0   equ 20h
pic1_reg1   equ 21h

interrupt_table:
    dw defaultHandler   ; INT 00 - Divide by zero
    dw defaultHandler   ; INT 01 - Single step
    dw defaultHandler   ; INT 02 - Non-maskable interrupt
    dw int_ignore       ; INT 03 - Debugger breakpoint
    dw int_ignore       ; INT 04 - Integer overflow (into)
    dw int_ignore       ; INT 05 - BIOS Print Screen
    dw int_ignore       ; INT 06
    dw int_ignore       ; INT 07
    dw int_ignore       ; INT 08 - IRQ0 - Timer Channel 0
    dw int_ignore       ; INT 09 - IRQ1 - Keyboard
    dw defaultHandler   ; INT 0A - IRQ2
    dw defaultHandler   ; INT 0B - IRQ3
    dw defaultHandler   ; INT 0C - IRQ4
    dw defaultHandler   ; INT 0D - IRQ5
    dw defaultHandler   ; INT 0E - IRQ6 - Floppy
    dw defaultHandler   ; INT 0F - IRQ7
    dw int_10           ; INT 10 - BIOS Video Services
    dw defaultHandler   ; INT 11 - BIOS Get Equipment List
    dw defaultHandler   ; INT 12 - BIOS Get Memory Size
    dw defaultHandler   ; INT 13 - BIOS Floppy Disk Services
    dw defaultHandler   ; INT 14 - BIOS Serial Communications
    dw defaultHandler   ; INT 15 - BIOS Misc. System Services
    dw defaultHandler   ; INT 16 - BIOS Keyboard Services
    dw defaultHandler   ; INT 17 - BIOS Parallel Printer svc.
    dw defaultHandler   ; INT 18 - BIOS Start ROM BASIC
    dw defaultHandler   ; INT 19 - BIOS Boot the OS
    dw defaultHandler   ; INT 1A - BIOS Time Services
    dw defaultHandler   ; INT 1B - DOS Keyboard Break
    dw defaultHandler   ; INT 1C - User Timer Tick
    dw int_1D           ; INT 1D - Video Parameters Table
    dw defaultHandler   ; INT 1E - Floppy Parameters Table
VECTORS_COUNT: equ ($-interrupt_table) / 2

SECTION .text

global setupVectors
setupVectors:
    push ds
    push es
    push cs
    pop ds
    xor di, di
    mov es, di
    mov si, interrupt_table
    mov cx, VECTORS_COUNT
    mov ax, cs
.loop:
    movsw
    stosw
    loop .loop
    pop es
    pop ds
    ret

defaultHandler:
    iret

int_ignore:
    push ax
    push ds
    mov al, 0Bh         ; PIC OCW3 - read in-service register
    out pic1_reg0, al
    nop
    in  al, pic1_reg0   ; get IRQ number
    mov ah, al
    or  al, al
    jnz .1
    mov ah, 0FFh
    jmp .2
.1:
    in  al, pic1_reg1   ; clear the interrupt
    or  al, ah
    out pic1_reg1, al
    mov al, 20h         ; end of interrupt
    out pic1_reg0, al   ; signal end of interrupt
.2:
    pop ds
    pop ax
    iret
