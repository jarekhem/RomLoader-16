#pragma once

#include <stdint.h>

#ifndef ROMLOADER_UART_BASE
#define ROMLOADER_UART_BASE 0x03E8
#endif

#define ROMLOADER_UART_RBR  (ROMLOADER_UART_BASE + 0)
#define ROMLOADER_UART_LSR  (ROMLOADER_UART_BASE + 5)
#define ROMLOADER_SYNC_BYTE 0x55

static inline void __attribute__((noreturn)) romloader_reboot(void)
{
    __asm__ __volatile__(
        "cli\n\t"
        "ljmp $0xFFFF, $0x0000\n\t"
    );
    __builtin_unreachable();
}

/* Poll COM3 (0x03E8) for RomLoader-16 sync byte (0x55).
 * Call this inside your main event/keyboard polling loop to enable hands-free
 * closed-loop re-uploading from the host PC without pressing reset/ESC. */
static inline void romloader_poll_autoreboot(void)
{
    uint8_t lsr;
    __asm__ __volatile__("inb %%dx, %%al" : "=Ral"(lsr) : "d"((uint16_t)ROMLOADER_UART_LSR));
    if (lsr & 0x01)
    {
        uint8_t b;
        __asm__ __volatile__("inb %%dx, %%al" : "=Ral"(b) : "d"((uint16_t)ROMLOADER_UART_RBR));
        if (b == ROMLOADER_SYNC_BYTE)
        {
            romloader_reboot();
        }
    }
}
