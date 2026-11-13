; =====================================================================
; Minimal test payload loaded over UART into RAM at 1000:0000
; Displays a green banner on VGA text screen and returns to ROM on ESC
; =====================================================================
BITS 16
CPU 8086
ORG 0x0000

start:
    cli
    mov ax, cs
    mov ds, ax
    mov ax, 0xB800
    mov es, ax

    ; Clear screen with Green on Black (0x0A)
    xor di, di
    mov ax, 0x2F20          ; White on Green background
    mov cx, 80 * 25
    cld
    rep stosw

    ; Print message on row 10, col 12 and also echo it to UART COM3 (0x03E8)
    mov di, (10 * 80 + 12) * 2
    mov si, msg_hello
    mov ah, 0x2E            ; Yellow on Green
.print_loop:
    lodsb
    test al, al
    jz .uart_newline
    stosw
    mov bl, al
    mov dx, 0x03ED
.uart_wait:
    in  al, dx
    test al, 0x20
    jz  .uart_wait
    mov dx, 0x03E8
    mov al, bl
    out dx, al
    jmp .print_loop

.uart_newline:
    mov dx, 0x03ED
.wait_cr:
    in  al, dx
    test al, 0x20
    jz  .wait_cr
    mov dx, 0x03E8
    mov al, 13
    out dx, al
    mov dx, 0x03ED
.wait_lf:
    in  al, dx
    test al, 0x20
    jz  .wait_lf
    mov dx, 0x03E8
    mov al, 10
    out dx, al

.wait_esc:
    ; Check keyboard controller for ESC (AT Set 2 scancode 0x76)
    in  al, 0x64
    test al, 0x01
    jz  .check_uart
    in  al, 0x60
    cmp al, 0x76
    je  .reboot

.check_uart:
    ; Also reboot if a new loader sync byte (0x55) or ESC (0x1B) arrives on COM3
    mov dx, 0x03ED
    in  al, dx
    test al, 0x01
    jz  .wait_esc
    mov dx, 0x03E8
    in  al, dx
    cmp al, 0x55
    je  .reboot
    cmp al, 0x1B
    jne .wait_esc

.reboot:
    ; Jump back to ROM Reset Vector (0xFFFF:0x0000)
    jmp 0xFFFF:0x0000

msg_hello:
    db "=== PAYLOAD EXECUTING FROM RAM (1000:0000)! Press ESC to reboot ===", 0
