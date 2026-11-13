#include "vga.h"
#include "kbd.h"
#include "uart.h"
#include "loader.h"
#include <stdbool.h>
#include <stdint.h>

#define ATTR_BG         0x17 /* Light Grey on Blue */
#define ATTR_BORDER     0x1F /* Bright White on Blue */
#define ATTR_HEADER     0x70 /* Black on Light Grey */
#define ATTR_LABEL      0x1B /* Bright Cyan on Blue */
#define ATTR_VALUE      0x1E /* Yellow on Blue */
#define ATTR_STATUS_OK  0x1A /* Bright Green on Blue */
#define ATTR_STATUS_ERR 0x4F /* Bright White on Red */
#define ATTR_FOOTER     0x70 /* Black on Light Grey */

static uint8_t g_prev_lsr = 0xFF;
static uint8_t g_prev_msr = 0xFF;

static void clear_line_range(uint8_t x_start, uint8_t x_end, uint8_t y, uint8_t attr)
{
    for (uint8_t col = x_start; col < x_end; col++)
        vga_draw_char(col, y, ' ', attr);
}

static void draw_static_ui(void)
{
    vga_clear(ATTR_BG);

    /* Top Header Bar */
    for (uint8_t x = 0; x < VGA_COLS; x++)
        vga_draw_char(x, 0, ' ', ATTR_HEADER);
    vga_print(2, 0, "8086 / V50 Bare-Metal ROM Serial Bootloader v0.2", ATTR_HEADER);

    /* Main Panel Box */
    vga_draw_box(2, 2, 76, 20, ATTR_BG, ATTR_BORDER, "Serial Loader & COM3 Hardware Monitor");

    vga_print(5, 4,  "UART Port      :", ATTR_LABEL);
    vga_print(5, 6,  "COM3 Self-Test :", ATTR_LABEL);
    vga_print(5, 8,  "UART Registers :", ATTR_LABEL);
    vga_print(5, 10, "RX Activity    :", ATTR_LABEL);

    vga_print(5, 12, "Target Address :", ATTR_LABEL);
    vga_print(5, 14, "Payload Info   :", ATTR_LABEL);
    vga_print(5, 16, "Payload Ready  :", ATTR_LABEL);
    vga_print(5, 19, "Status         :", ATTR_LABEL);

    /* Bottom Footer Bar */
    for (uint8_t x = 0; x < VGA_COLS; x++)
        vga_draw_char(x, 24, ' ', ATTR_FOOTER);
    vga_print(2, 24, "[ENTER] Run  [SPACE] Beacon  [T] Test COM3  [F] Toggle FIFO  [M] Cycle MCR", ATTR_FOOTER);
}

static void update_uart_regs_ui(uint8_t lsr, uint8_t msr, uint8_t iir, uint8_t scr)
{
    uint8_t x = 23;
    x = vga_print(x, 8, "LSR=0x", ATTR_VALUE);
    x = vga_print_hex8(x, 8, lsr, ATTR_VALUE);
    x = vga_print(x, 8, "  MSR=0x", ATTR_VALUE);
    x = vga_print_hex8(x, 8, msr, ATTR_VALUE);
    x = vga_print(x, 8, "  IIR=0x", ATTR_VALUE);
    x = vga_print_hex8(x, 8, iir, ATTR_VALUE);
    x = vga_print(x, 8, "  SCR=0x", ATTR_VALUE);
    vga_print_hex8(x, 8, scr, ATTR_VALUE);
}

static void update_status_ui(void)
{
    /* Row 4: Port & active FIFO / MCR config */
    clear_line_range(23, 76, 4, ATTR_BG);
    uint8_t x = 23;
    x = vga_print(x, 4, "0x03E8 / COM3 (115200 8N1)  FIFO:", ATTR_VALUE);
    x = vga_print(x, 4, g_uart_test.fifo_enabled ? "ON " : "OFF",
                  g_uart_test.fifo_enabled ? ATTR_STATUS_OK : ATTR_VALUE);
    x = vga_print(x, 4, "  MCR:0x", ATTR_VALUE);
    vga_print_hex8(x, 4, g_uart_test.mcr_setting, ATTR_VALUE);

    /* Row 6: COM3 Controller Self-Test details */
    clear_line_range(23, 76, 6, ATTR_BG);
    x = 23;
    if (g_uart_test.passed)
    {
        x = vga_print(x, 6, "PASS ", ATTR_STATUS_OK);
    }
    else
    {
        x = vga_print(x, 6, "FAIL ", ATTR_STATUS_ERR);
    }

    x = vga_print(x, 6, "[SCR:", ATTR_VALUE);
    if (g_uart_test.scratch_ok)
    {
        x = vga_print(x, 6, "OK", ATTR_STATUS_OK);
    }
    else
    {
        x = vga_print_hex8(x, 6, g_uart_test.scratch_read, ATTR_STATUS_ERR);
    }

    x = vga_print(x, 6, " DLAB:", ATTR_VALUE);
    x = vga_print(x, 6, g_uart_test.divisor_ok ? "OK" : "ERR",
                  g_uart_test.divisor_ok ? ATTR_STATUS_OK : ATTR_STATUS_ERR);

    x = vga_print(x, 6, " FIFO:", ATTR_VALUE);
    x = vga_print(x, 6, g_uart_test.fifo_present ? "16550A" : "NONE",
                  g_uart_test.fifo_present ? ATTR_STATUS_OK : ATTR_VALUE);

    x = vga_print(x, 6, " LOOP:", ATTR_VALUE);
    if (g_uart_test.loopback_tx_ok && g_uart_test.loopback_rx_ok)
    {
        x = vga_print(x, 6, "OK", ATTR_STATUS_OK);
    }
    else if (!g_uart_test.loopback_tx_ok)
    {
        x = vga_print(x, 6, "TX!", ATTR_STATUS_ERR);
    }
    else
    {
        x = vga_print_hex8(x, 6, g_uart_test.loopback_read, ATTR_STATUS_ERR);
    }

    x = vga_print(x, 6, " MSR:", ATTR_VALUE);
    x = vga_print(x, 6, g_uart_test.modem_loop_ok ? "OK" : "ERR",
                  g_uart_test.modem_loop_ok ? ATTR_STATUS_OK : ATTR_STATUS_ERR);
    vga_print(x, 6, "]", ATTR_VALUE);

    /* Row 8: Live UART registers */
    uint8_t lsr, msr, iir, scr;
    uart_read_regs(&lsr, &msr, &iir, &scr);
    g_prev_lsr = lsr;
    g_prev_msr = msr;
    update_uart_regs_ui(lsr, msr, iir, scr);

    /* Row 10: RX byte counter and last 8 received hex bytes */
    clear_line_range(23, 76, 10, ATTR_BG);
    x = 23;
    x = vga_print(x, 10, "Cnt=0x", ATTR_VALUE);
    x = vga_print_hex16(x, 10, g_loader.rx_bytes_total, ATTR_VALUE);
    x = vga_print(x, 10, " | Last: ", ATTR_VALUE);
    for (uint8_t i = 0; i < LOADER_RX_HISTORY_LEN; i++)
    {
        if (i < g_loader.last_rx_count)
        {
            uint8_t ring_idx = (uint8_t)((g_loader.rx_bytes_total - g_loader.last_rx_count + i) & (LOADER_RX_HISTORY_LEN - 1));
            x = vga_print_hex8(x, 10, g_loader.last_rx[ring_idx], ATTR_STATUS_OK);
        }
        else
        {
            x = vga_print(x, 10, "--", ATTR_LABEL);
        }
        if (i < LOADER_RX_HISTORY_LEN - 1)
            vga_draw_char(x++, 10, ' ', ATTR_BG);
    }

    /* Row 12: Target Seg:Off */
    x = 23;
    x = vga_print_hex16(x, 12, g_loader.last_seg, ATTR_VALUE);
    vga_draw_char(x++, 12, ':', ATTR_VALUE);
    vga_print_hex16(x, 12, g_loader.last_off, ATTR_VALUE);

    /* Row 14: Payload Size & CRC16 */
    x = 23;
    x = vga_print(x, 14, "Size=0x", ATTR_VALUE);
    x = vga_print_hex16(x, 14, g_loader.last_size, ATTR_VALUE);
    x = vga_print(x, 14, " bytes   CRC16=0x", ATTR_VALUE);
    vga_print_hex16(x, 14, g_loader.last_crc, ATTR_VALUE);

    /* Row 16: Payload Ready */
    vga_print(23, 16, g_loader.payload_ready ? "YES (Press ENTER to run)" : "NO                      ",
              g_loader.payload_ready ? ATTR_STATUS_OK : ATTR_VALUE);

    /* Row 19: Status Line */
    clear_line_range(23, 76, 19, ATTR_BG);
    vga_print(23, 19, g_loader.status_msg, ATTR_STATUS_OK);
}

void app_main(void)
{
    vga_init();
    kbd_init();
    uart_init(UART_DIV_115200);
    loader_init();

    if (!g_uart_test.passed)
    {
        g_loader.status_msg = "COM3 self-test FAILED! Check 0x03E8 hardware.";
    }

    draw_static_ui();
    update_status_ui();

    /* Send startup banner over serial */
    uart_puts("\r\n[RomLoader-16] COM3 (0x03E8) Ready.\r\n");

    uint16_t reg_poll_div = 0;

    while (1)
    {
        if (loader_poll_serial())
        {
            update_status_ui();
        }
        else if (++reg_poll_div == 0)
        {
            /* Periodically refresh live LSR/MSR display if status lines change */
            uint8_t lsr, msr, iir, scr;
            uart_read_regs(&lsr, &msr, &iir, &scr);
            if (lsr != g_prev_lsr || msr != g_prev_msr)
            {
                g_prev_lsr = lsr;
                g_prev_msr = msr;
                update_uart_regs_ui(lsr, msr, iir, scr);
            }
        }

        uint16_t key;
        if (kbd_get_key(&key))
        {
            if (key == KEY_ENTER && g_loader.payload_ready)
            {
                g_loader.status_msg = "Jumping to payload...";
                update_status_ui();
                loader_execute_last();
            }
            else if (key == KEY_SPACE)
            {
                uart_puts("[RomLoader-16] PING beacon\r\n");
                g_loader.status_msg = "Sent UART beacon on COM3 (0x03E8).";
                update_status_ui();
            }
            else if (key == KEY_T)
            {
                bool ok = uart_selftest(UART_DIV_115200);
                g_loader.status_msg = ok ? "COM3 self-test PASSED." : "COM3 self-test FAILED!";
                update_status_ui();
            }
            else if (key == KEY_F)
            {
                uart_toggle_fifo();
                g_loader.status_msg = g_uart_test.fifo_enabled
                    ? "FIFO enabled (16550A 1-byte trigger)."
                    : "FIFO disabled (16450 byte mode).";
                update_status_ui();
            }
            else if (key == KEY_M)
            {
                uart_cycle_mcr();
                g_loader.status_msg = "Cycled UART MCR register.";
                update_status_ui();
            }
        }
    }
}
