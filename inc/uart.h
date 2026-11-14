#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "io.h"

#define UART_PORT_COUNT 4

#define UART_COM1_BASE  0x03F8
#define UART_COM2_BASE  0x02F8
#define UART_COM3_BASE  0x03E8
#define UART_COM4_BASE  0x02E8

/* Default 16550 COM3 base port (index 2 = 0x03E8 on target V50/8086 hardware & Bochs com3) */
#define UART_DEFAULT_PORT_IDX 2

#ifndef UART_BASE_PORT
#define UART_BASE_PORT UART_COM3_BASE
#endif

extern uint16_t g_uart_base;
extern uint8_t g_uart_port_idx;
extern const uint16_t g_uart_port_bases[UART_PORT_COUNT];
extern const char* const g_uart_port_names[UART_PORT_COUNT];

/* 16550 / 8250 UART register offsets (relative to active g_uart_base) */
#define UART_REG_RBR_THR_DLL (g_uart_base + 0)
#define UART_REG_IER_DLM     (g_uart_base + 1)
#define UART_REG_IIR_FCR     (g_uart_base + 2)
#define UART_REG_LCR         (g_uart_base + 3)
#define UART_REG_MCR         (g_uart_base + 4)
#define UART_REG_LSR         (g_uart_base + 5)
#define UART_REG_MSR         (g_uart_base + 6)
#define UART_REG_SCR         (g_uart_base + 7)

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
    bool scratch_ok;        /* Scratchpad register (+7) R/W 0x55 & 0xAA */
    bool divisor_ok;        /* DLAB=1 DLL/DLM R/W verification */
    bool loopback_tx_ok;    /* Internal loopback (MCR bit 4) THRE ready */
    bool loopback_rx_ok;    /* Internal loopback (MCR bit 4) 4-byte pattern TX->RX match */
    bool modem_loop_ok;     /* Internal loopback MCR RTS/DTR -> MSR CTS/DSR match */
    bool fifo_present;      /* IIR bits 7:6 == 11b (16550A FIFO) */
    bool fifo_enabled;      /* Whether FIFO is currently active */
    bool passed;            /* Overall controller self-test status */
    uint8_t mcr_setting;    /* Active MCR value in normal mode */
    uint8_t scratch_read;   /* Last value read from SCR */
    uint8_t loopback_read;  /* Last byte read in loopback test */
    uint8_t iir_val;        /* IIR register snapshot */
    uint8_t lsr_val;        /* LSR register snapshot */
    uint8_t msr_val;        /* MSR register snapshot */
} UartSelfTestResult;

extern UartSelfTestResult g_uart_test;
extern UartSelfTestResult g_uart_results[UART_PORT_COUNT];

void uart_init(uint16_t divisor);
bool uart_selftest(uint16_t divisor);
uint8_t uart_test_all(uint16_t divisor);
bool uart_select_port(uint8_t port_idx);
void uart_wait_tx_idle(void);
void uart_toggle_fifo(void);
void uart_cycle_mcr(void);
void uart_read_regs(uint8_t* out_lsr, uint8_t* out_msr, uint8_t* out_iir, uint8_t* out_scr);
void uart_puts(const char* str);

static inline bool uart_rx_ready(void)
{
    uint8_t lsr = inb(UART_REG_LSR);
    return (lsr != 0xFF) && ((lsr & UART_LSR_DR) != 0);
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
    uint16_t timeout = 0xFFFF;
    while (((inb(UART_REG_LSR) & UART_LSR_THRE) == 0) && --timeout) {}
    outb(UART_REG_RBR_THR_DLL, byte);
}

static inline uint16_t uart_get_u16_le(void)
{
    uint16_t lo = uart_getc();
    uint16_t hi = uart_getc();
    return (hi << 8) | lo;
}
