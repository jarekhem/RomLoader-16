#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "io.h"

/* Default 16550 COM3 base port (0x03E8 on target V50/8086 hardware & Bochs com3) */
#ifndef UART_BASE_PORT
#define UART_BASE_PORT 0x03E8
#endif

/* 16550 / 8250 UART register offsets */
#define UART_REG_RBR_THR_DLL (UART_BASE_PORT + 0)
#define UART_REG_IER_DLM     (UART_BASE_PORT + 1)
#define UART_REG_IIR_FCR     (UART_BASE_PORT + 2)
#define UART_REG_LCR         (UART_BASE_PORT + 3)
#define UART_REG_MCR         (UART_BASE_PORT + 4)
#define UART_REG_LSR         (UART_BASE_PORT + 5)
#define UART_REG_MSR         (UART_BASE_PORT + 6)
#define UART_REG_SCR         (UART_BASE_PORT + 7)

#define UART_LSR_DR          0x01 /* Data Ready */
#define UART_LSR_THRE        0x20 /* Transmitter Holding Register Empty */
#define UART_LSR_TEMT        0x40 /* Transmitter Empty (Holding + Shift) */

/* Divisor for 115200 baud (1), 57600 (2), 38400 (3), 19200 (6), 9600 (12) */
#define UART_DIV_115200 1
#define UART_DIV_57600  2
#define UART_DIV_38400  3
#define UART_DIV_19200  6
#define UART_DIV_9600   12

typedef struct
{
    bool scratch_ok;        /* Scratchpad register (0x3EF) R/W 0x55 & 0xAA */
    bool divisor_ok;        /* DLAB=1 DLL/DLM R/W verification */
    bool loopback_tx_ok;    /* Internal loopback (MCR bit 4) THRE ready */
    bool loopback_rx_ok;    /* Internal loopback (MCR bit 4) 4-byte pattern TX->RX match */
    bool modem_loop_ok;     /* Internal loopback MCR RTS/DTR -> MSR CTS/DSR match */
    bool fifo_present;      /* IIR bits 7:6 == 11b (16550A FIFO) */
    bool fifo_enabled;      /* Whether FIFO is currently active */
    bool passed;            /* Overall COM3 controller self-test status */
    uint8_t mcr_setting;    /* Active MCR value in normal mode */
    uint8_t scratch_read;   /* Last value read from SCR */
    uint8_t loopback_read;  /* Last byte read in loopback test */
    uint8_t iir_val;        /* IIR register snapshot */
    uint8_t lsr_val;        /* LSR register snapshot */
    uint8_t msr_val;        /* MSR register snapshot */
} UartSelfTestResult;

extern UartSelfTestResult g_uart_test;

void uart_init(uint16_t divisor);
bool uart_selftest(uint16_t divisor);
void uart_wait_tx_idle(void);
void uart_toggle_fifo(void);
void uart_cycle_mcr(void);
void uart_read_regs(uint8_t* out_lsr, uint8_t* out_msr, uint8_t* out_iir, uint8_t* out_scr);
void uart_puts(const char* str);

static inline bool uart_rx_ready(void)
{
    return (inb(UART_REG_LSR) & UART_LSR_DR) != 0;
}

static inline bool uart_try_getc(uint8_t* out_byte)
{
    if (uart_rx_ready())
    {
        *out_byte = inb(UART_REG_RBR_THR_DLL);
        return true;
    }
    return false;
}

static inline uint8_t uart_getc(void)
{
    while (!uart_rx_ready()) {}
    return inb(UART_REG_RBR_THR_DLL);
}

static inline void uart_putc(uint8_t byte)
{
    while ((inb(UART_REG_LSR) & UART_LSR_THRE) == 0) {}
    outb(UART_REG_RBR_THR_DLL, byte);
}

static inline uint16_t uart_get_u16_le(void)
{
    uint16_t lo = uart_getc();
    uint16_t hi = uart_getc();
    return (hi << 8) | lo;
}
