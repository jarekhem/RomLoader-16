#include "uart.h"

const uint16_t g_uart_port_bases[UART_PORT_COUNT] = {
    UART_COM1_BASE,
    UART_COM2_BASE,
    UART_COM3_BASE,
    UART_COM4_BASE
};

const char* const g_uart_port_names[UART_PORT_COUNT] = {
    "COM1",
    "COM2",
    "COM3",
    "COM4"
};

uint8_t g_uart_port_idx = UART_DEFAULT_PORT_IDX;
uint16_t g_uart_base = UART_COM3_BASE;

UartSelfTestResult g_uart_test;
UartSelfTestResult g_uart_results[UART_PORT_COUNT];

static uint16_t g_last_divisor = UART_DIV_115200;
static bool g_fifo_want_enable = true;
static uint8_t g_mcr_mode = 0x00; /* 0x00 (default like serial_monitor), 0x03 (DTR+RTS), 0x0B (DTR+RTS+OUT2) */

static void uart_delay_loops(uint16_t loops)
{
    while (loops--)
    {
        io_wait();
    }
}

void uart_wait_tx_idle(void)
{
    uint16_t timeout = 0x7FFF;
    while (((inb(UART_REG_LSR) & (UART_LSR_THRE | UART_LSR_TEMT)) != (UART_LSR_THRE | UART_LSR_TEMT)) && --timeout)
    {
        io_wait();
    }
}

static void uart_flush_rx(void)
{
    for (uint8_t i = 0; i < 32; i++)
    {
        (void)inb(UART_REG_LSR);
        io_wait();
        (void)inb(UART_REG_RBR_THR_DLL);
        io_wait();
        if (!(inb(UART_REG_LSR) & UART_LSR_DR))
            break;
    }
    (void)inb(UART_REG_IIR_FCR);
    io_wait();
    (void)inb(UART_REG_MSR);
    io_wait();
}

bool uart_selftest(uint16_t divisor)
{
    g_last_divisor = divisor;
    

    g_uart_test.scratch_ok = false;
    g_uart_test.divisor_ok = false;
    g_uart_test.loopback_tx_ok = false;
    g_uart_test.loopback_rx_ok = false;
    g_uart_test.modem_loop_ok = false;
    g_uart_test.fifo_present = false;
    g_uart_test.fifo_enabled = false;
    g_uart_test.passed = false;
    g_uart_test.mcr_setting = g_mcr_mode;
    g_uart_test.scratch_read = 0xFF;
    g_uart_test.loopback_read = 0xFF;

    uint8_t exp_dll = (uint8_t)(divisor & 0xFF);
    uint8_t exp_dlm = (uint8_t)(divisor >> 8);

    __asm__ __volatile__("cli");

    /* 1. Force DLAB=0 + 8N1 (0x03) first so IER is selected (not DLM),
     *    disable UART interrupts, and enter Internal Loopback immediately
     *    so external RX activity is isolated and TX stays in mark state. */
    outb(UART_REG_LCR, 0x03);
    io_wait();
    outb(UART_REG_IER_DLM, 0x00);
    io_wait();
    outb(UART_REG_MCR, 0x13); /* Loopback + RTS + DTR */
    io_wait();

    /* 2. Scratchpad Register Test (offset +7 = 0x03EF) */
    outb(UART_REG_SCR, 0x55);
    io_wait();
    uint8_t s1 = inb(UART_REG_SCR);
    io_wait();

    outb(UART_REG_SCR, 0xAA);
    io_wait();
    uint8_t s2 = inb(UART_REG_SCR);
    io_wait();

    g_uart_test.scratch_read = (s1 != 0x55) ? s1 : s2;
    g_uart_test.scratch_ok = (s1 == 0x55) && (s2 == 0xAA);

    /* 3. Divisor Latch Register Test (DLAB=1, keeping 8N1 bits = 0x83)
     *    IMPORTANT: Do NOT write huge values (like 0xA55A) to DLM:DLL on real
     *    16550 silicon, because the 16-bit baud down-counter would latch 42330
     *    and freeze BAUDOUT! Always keep LCR word-length bits at 11b (8-bit). */
    outb(UART_REG_LCR, 0x83);
    io_wait();
    outb(UART_REG_RBR_THR_DLL, exp_dll);
    io_wait();
    outb(UART_REG_IER_DLM, exp_dlm);
    io_wait();

    uint8_t dll_chk = inb(UART_REG_RBR_THR_DLL);
    io_wait();
    uint8_t dlm_chk = inb(UART_REG_IER_DLM);
    io_wait();

    outb(UART_REG_LCR, 0x03); /* 8 bits, no parity, 1 stop bit, DLAB=0 */
    io_wait();
    uint8_t lcr_chk = inb(UART_REG_LCR);
    io_wait();

    g_uart_test.divisor_ok = (dll_chk == exp_dll) && (dlm_chk == exp_dlm) && (lcr_chk == 0x03);

    /* Fast-fail if neither scratchpad nor divisor/LCR responds (unmapped I/O port) */
    if (!g_uart_test.scratch_ok && !g_uart_test.divisor_ok)
    {
        outb(UART_REG_MCR, g_mcr_mode);
        io_wait();
        g_uart_test.lsr_val = inb(UART_REG_LSR);
        g_uart_test.msr_val = inb(UART_REG_MSR);
        g_uart_test.iir_val = inb(UART_REG_IIR_FCR);
        __asm__ __volatile__("sti");
        g_uart_results[g_uart_port_idx] = g_uart_test;
        return false;
    }

    /* Allow baud generator and shift registers to finish any power-on frame */
    uart_delay_loops(1000);
    uart_wait_tx_idle();

    /* 4. FIFO Presence Check */
    outb(UART_REG_IIR_FCR, 0x07); /* Enable FIFO, clear RX/TX FIFOs, 1-byte trigger */
    io_wait();
    uint8_t iir = inb(UART_REG_IIR_FCR);
    io_wait();
    g_uart_test.iir_val = iir;
    g_uart_test.fifo_present = ((iir & 0xC0) == 0xC0);

    if (g_fifo_want_enable && g_uart_test.fifo_present)
    {
        outb(UART_REG_IIR_FCR, 0x07); /* 1-byte trigger FIFO */
        g_uart_test.fifo_enabled = true;
    }
    else
    {
        outb(UART_REG_IIR_FCR, 0x00); /* 16450/8250 byte mode */
        g_uart_test.fifo_enabled = false;
    }
    io_wait();

    /* 5. Internal Loopback Mode Test (Modem Status + TX/RX Data Path)
     *    Up to 3 internal attempts to flush any power-on shift register glitch. */
    outb(UART_REG_MCR, 0x13); /* Loopback + RTS + DTR */
    uart_delay_loops(64);
    uint8_t msr_hi = inb(UART_REG_MSR);
    io_wait();

    outb(UART_REG_MCR, 0x10); /* Loopback, RTS=0, DTR=0 */
    uart_delay_loops(64);
    uint8_t msr_lo = inb(UART_REG_MSR);
    io_wait();

    g_uart_test.modem_loop_ok = ((msr_hi & 0x30) == 0x30) && ((msr_lo & 0x30) == 0x00);

    static const uint8_t test_vec[4] = { 0x55, 0xAA, 'R', 'L' };
    bool tx_ok = false;
    bool rx_ok = false;

    for (uint8_t attempt = 0; attempt < 3; attempt++)
    {
        outb(UART_REG_LCR, 0x03);
        io_wait();
        outb(UART_REG_MCR, 0x13);
        uart_delay_loops(500);
        uart_wait_tx_idle();
        if (g_uart_test.fifo_enabled)
        {
            outb(UART_REG_IIR_FCR, 0x07);
            io_wait();
        }
        uart_flush_rx();

        tx_ok = true;
        rx_ok = true;

        for (uint8_t i = 0; i < 4; i++)
        {
            uint16_t timeout = 0xFFFF;
            while (((inb(UART_REG_LSR) & UART_LSR_THRE) == 0) && --timeout)
                io_wait();
            if (timeout == 0)
            {
                tx_ok = false;
                break;
            }

            outb(UART_REG_RBR_THR_DLL, test_vec[i]);

            timeout = 0xFFFF;
            while (((inb(UART_REG_LSR) & UART_LSR_DR) == 0) && --timeout)
                io_wait();
            if (timeout == 0)
            {
                rx_ok = false;
                break;
            }

            uint8_t got = inb(UART_REG_RBR_THR_DLL);
            g_uart_test.loopback_read = got;
            if (got != test_vec[i])
            {
                rx_ok = false;
                break;
            }
        }

        if (tx_ok && rx_ok)
            break;
    }

    g_uart_test.loopback_tx_ok = tx_ok;
    g_uart_test.loopback_rx_ok = rx_ok;

    /* 6. Wait for transmitter to empty, exit Loopback, and explicitly re-apply
     *    clean 115200 8N1 configuration (identical to serial_monitor) so normal
     *    serial communication always works even if self-test reported an error. */
    uart_wait_tx_idle();
    outb(UART_REG_MCR, g_mcr_mode);
    io_wait();

    outb(UART_REG_LCR, 0x83);
    io_wait();
    outb(UART_REG_RBR_THR_DLL, exp_dll);
    io_wait();
    outb(UART_REG_IER_DLM, exp_dlm);
    io_wait();
    outb(UART_REG_LCR, 0x03);
    io_wait();
    outb(UART_REG_IER_DLM, 0x00);
    io_wait();

    if (g_uart_test.fifo_enabled)
        outb(UART_REG_IIR_FCR, 0x07);
    else
        outb(UART_REG_IIR_FCR, 0x00);
    io_wait();

    uart_delay_loops(500);
    uart_flush_rx();

    g_uart_test.lsr_val = inb(UART_REG_LSR);
    g_uart_test.msr_val = inb(UART_REG_MSR);
    g_uart_test.iir_val = inb(UART_REG_IIR_FCR);

    __asm__ __volatile__("sti");

    g_uart_test.passed = g_uart_test.scratch_ok &&
                         g_uart_test.divisor_ok &&
                         g_uart_test.loopback_tx_ok &&
                         g_uart_test.loopback_rx_ok;

    g_uart_results[g_uart_port_idx] = g_uart_test;
    return g_uart_test.passed;
}

uint8_t uart_test_all(uint16_t divisor)
{
    uint8_t saved_idx = g_uart_port_idx;
    uint8_t mask = 0;

    for (uint8_t i = 0; i < UART_PORT_COUNT; i++)
    {
        g_uart_port_idx = i;
        g_uart_base = g_uart_port_bases[i];
        if (uart_selftest(divisor))
        {
            mask |= (uint8_t)(1U << i);
        }
    }

    g_uart_port_idx = saved_idx;
    g_uart_base = g_uart_port_bases[saved_idx];
    g_uart_test = g_uart_results[saved_idx];
    return mask;
}

bool uart_select_port(uint8_t port_idx)
{
    if (port_idx >= UART_PORT_COUNT)
        port_idx = UART_DEFAULT_PORT_IDX;

    g_uart_port_idx = port_idx;
    g_uart_base = g_uart_port_bases[port_idx];
    return uart_selftest(g_last_divisor);
}

void uart_init(uint16_t divisor)
{
    g_uart_port_idx = UART_DEFAULT_PORT_IDX;
    g_uart_base = g_uart_port_bases[UART_DEFAULT_PORT_IDX];

    uint8_t mask = uart_test_all(divisor);
    if (!g_uart_results[UART_DEFAULT_PORT_IDX].passed && mask != 0)
    {
        for (uint8_t i = 0; i < UART_PORT_COUNT; i++)
        {
            if (g_uart_results[i].passed)
            {
                g_uart_port_idx = i;
                g_uart_base = g_uart_port_bases[i];
                g_uart_test = g_uart_results[i];
                break;
            }
        }
    }
}

void uart_toggle_fifo(void)
{
    g_fifo_want_enable = !g_fifo_want_enable;
    uart_selftest(g_last_divisor);
}

void uart_cycle_mcr(void)
{
    if (g_mcr_mode == 0x00)
        g_mcr_mode = 0x03; /* DTR + RTS */
    else if (g_mcr_mode == 0x03)
        g_mcr_mode = 0x0B; /* DTR + RTS + OUT2 */
    else
        g_mcr_mode = 0x00; /* All modem outputs deasserted (like serial_monitor) */

    g_uart_test.mcr_setting = g_mcr_mode;
    g_uart_results[g_uart_port_idx].mcr_setting = g_mcr_mode;
    outb(UART_REG_MCR, g_mcr_mode);
    io_wait();
    uart_flush_rx();
}

void uart_read_regs(uint8_t* out_lsr, uint8_t* out_msr, uint8_t* out_iir, uint8_t* out_scr)
{
    if (out_lsr) *out_lsr = inb(UART_REG_LSR);
    if (out_msr) *out_msr = inb(UART_REG_MSR);
    if (out_iir) *out_iir = inb(UART_REG_IIR_FCR);
    if (out_scr) *out_scr = inb(UART_REG_SCR);
}

void uart_puts(const char* str)
{
    if (!str) return;
    while (*str)
    {
        uart_putc((uint8_t)*str++);
    }
}
