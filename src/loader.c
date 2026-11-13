#include "loader.h"
#include "uart.h"
#include "io.h"
#include <stddef.h>

LoaderState g_loader;

static uint8_t g_sync_step = 0;

/* CRC-16-CCITT (poly 0x1021, init 0xFFFF) */
static uint16_t crc16_update(uint16_t crc, uint8_t data)
{
    crc ^= (uint16_t)data << 8;
    for (uint8_t i = 0; i < 8; i++)
    {
        if (crc & 0x8000)
            crc = (crc << 1) ^ 0x1021;
        else
            crc <<= 1;
    }
    return crc;
}

static inline void record_rx_byte(uint8_t b)
{
    g_loader.last_rx[g_loader.rx_bytes_total & (LOADER_RX_HISTORY_LEN - 1)] = b;
    g_loader.rx_bytes_total++;
    if (g_loader.last_rx_count < LOADER_RX_HISTORY_LEN)
        g_loader.last_rx_count++;
}

void loader_init(void)
{
    g_loader.last_seg = 0x1000;
    g_loader.last_off = 0x0000;
    g_loader.last_size = 0;
    g_loader.last_crc = 0;
    g_loader.rx_bytes_total = 0;
    g_loader.last_rx_count = 0;
    for (uint8_t i = 0; i < LOADER_RX_HISTORY_LEN; i++)
        g_loader.last_rx[i] = 0;
    g_loader.payload_ready = false;
    g_loader.status_msg = "Waiting for serial command (55 AA 'R' 'L')...";
    g_sync_step = 0;
}

static inline bool uart_getc_timeout(uint8_t* out_b)
{
    uint16_t timeout = 0xFFFF;
    while (--timeout)
    {
        if (uart_rx_ready())
        {
            *out_b = inb(UART_REG_RBR_THR_DLL);
            return true;
        }
    }
    return false;
}

static bool uart_get_u16_le_timeout(uint16_t* out_val)
{
    uint8_t lo, hi;
    if (!uart_getc_timeout(&lo)) return false;
    if (!uart_getc_timeout(&hi)) return false;
    *out_val = (uint16_t)lo | ((uint16_t)hi << 8);
    return true;
}

/* Fast assembly receiver for 115200 baud even when 16550 FIFO is disabled.
 * Includes a 0xFFFF-poll per-byte timeout in BX so a dropped byte never hangs the CPU. */
static bool uart_recv_block_far(uint16_t seg, uint16_t off, uint16_t size)
{
    while (size > 0)
    {
        /* Max bytes before 64KB offset wrap */
        uint16_t rem_in_seg = (uint16_t)(0x0000 - off);
        uint16_t chunk = (rem_in_seg == 0 || size < rem_in_seg) ? size : rem_in_seg;
        uint16_t di = off;
        uint16_t cx = chunk;

        __asm__ __volatile__(
            "push %%es\n\t"
            "movw %2, %%es\n\t"
            "cld\n\t"
            "1:\n\t"
            "movw $0xFFFF, %%bx\n\t"
            "movw %3, %%dx\n\t"
            "2:\n\t"
            "inb %%dx, %%al\n\t"
            "testb $0x01, %%al\n\t"
            "jnz 3f\n\t"
            "decw %%bx\n\t"
            "jnz 2b\n\t"
            "jmp 4f\n\t"
            "3:\n\t"
            "movw %4, %%dx\n\t"
            "inb %%dx, %%al\n\t"
            "stosb\n\t"
            "loop 1b\n\t"
            "4:\n\t"
            "pop %%es"
            : "+D"(di), "+c"(cx)
            : "r"(seg), "i"(UART_REG_LSR), "i"(UART_REG_RBR_THR_DLL)
            : "ax", "bx", "dx", "memory", "cc"
        );

        uint16_t received = chunk - cx;
        g_loader.rx_bytes_total += received;
        if (cx != 0)
        {
            return false;
        }

        size -= chunk;
        off += chunk;
        if (off == 0x0000)
        {
            seg += 0x1000;
        }
    }
    return true;
}

static bool handle_load_payload(bool auto_exec)
{
    /* Disable interrupts during high-speed 115200 baud transfer so timer IRQ0
     * cannot cause UART RBR overrun when 16550 FIFO is disabled. */
    __asm__ __volatile__("cli");

    uint16_t seg = 0, off = 0, size = 0, expected_crc = 0;
    bool hdr_ok = uart_get_u16_le_timeout(&seg) &&
                  uart_get_u16_le_timeout(&off) &&
                  uart_get_u16_le_timeout(&size) &&
                  uart_get_u16_le_timeout(&expected_crc);

    if (!hdr_ok)
    {
        __asm__ __volatile__("sti");
        g_loader.status_msg = "Header RX timeout! Transfer aborted.";
        uart_putc(LOADER_NAK);
        return true;
    }

    g_loader.rx_bytes_total += 8;
    g_loader.last_seg = seg;
    g_loader.last_off = off;
    g_loader.last_size = size;
    g_loader.last_crc = expected_crc;
    g_loader.payload_ready = false;
    g_loader.status_msg = "Receiving payload...";

    /* Acknowledge header and immediately enter tight assembly receive loop */
    uart_putc(LOADER_ACK);
    bool data_ok = uart_recv_block_far(seg, off, size);

    __asm__ __volatile__("sti");

    if (!data_ok)
    {
        g_loader.status_msg = "Payload RX timeout! Byte lost.";
        uart_putc(LOADER_NAK);
        return true;
    }

    /* Compute CRC-16 over received RAM buffer */
    uint16_t crc = 0xFFFF;
    uint16_t cur_seg = seg;
    uint16_t cur_off = off;
    for (uint16_t i = 0; i < size; i++)
    {
        uint8_t b = far_peek_b(cur_seg, cur_off);
        crc = crc16_update(crc, b);

        cur_off++;
        if (cur_off == 0x0000)
        {
            cur_seg += 0x1000;
        }
    }

    if (crc != expected_crc)
    {
        g_loader.status_msg = "CRC ERROR! Transfer rejected.";
        uart_putc(LOADER_NAK);
        return true;
    }

    g_loader.payload_ready = true;
    g_loader.status_msg = "Payload loaded & verified (CRC OK).";
    uart_putc(LOADER_ACK);

    if (auto_exec)
    {
        uart_wait_tx_idle();
        loader_execute_last();
    }
    return true;
}

static bool process_rx_byte(uint8_t b)
{
    record_rx_byte(b);

    switch (g_sync_step)
    {
    case 0:
        if (b == LOADER_MAGIC_0) g_sync_step = 1;
        return false;
    case 1:
        g_sync_step = (b == LOADER_MAGIC_1) ? 2 : (b == LOADER_MAGIC_0 ? 1 : 0);
        return false;
    case 2:
        g_sync_step = (b == LOADER_MAGIC_2) ? 3 : (b == LOADER_MAGIC_0 ? 1 : 0);
        return false;
    case 3:
        g_sync_step = (b == LOADER_MAGIC_3) ? 4 : (b == LOADER_MAGIC_0 ? 1 : 0);
        return false;
    case 4:
        g_sync_step = 0;
        if (b == LOADER_CMD_PING)
        {
            uart_putc('O');
            uart_putc('K');
            g_loader.status_msg = "PING received -> replied OK.";
            return true;
        }
        else if (b == LOADER_CMD_LOAD)
        {
            return handle_load_payload(false);
        }
        else if (b == LOADER_CMD_LOAD_AND_EXEC)
        {
            return handle_load_payload(true);
        }
        else if (b == LOADER_CMD_EXEC)
        {
            __asm__ __volatile__("cli");
            uint16_t seg = 0, off = 0;
            bool ok = uart_get_u16_le_timeout(&seg) && uart_get_u16_le_timeout(&off);
            __asm__ __volatile__("sti");
            if (!ok)
            {
                uart_putc(LOADER_NAK);
                return true;
            }
            g_loader.last_seg = seg;
            g_loader.last_off = off;
            g_loader.payload_ready = true;
            uart_putc(LOADER_ACK);
            uart_wait_tx_idle();
            loader_execute_last();
            return true;
        }
        else
        {
            uart_putc(LOADER_NAK);
            g_loader.status_msg = "Unknown command byte.";
            return true;
        }
    }

    g_sync_step = 0;
    return false;
}

bool loader_poll_serial(void)
{
    uint8_t b;
    if (!uart_try_getc(&b))
    {
        return false;
    }

    /* Stay in a tight polling loop while a multi-byte frame is arriving
     * so we never drop bytes at 115200 baud during slow VGA screen updates. */
    while (1)
    {
        if (process_rx_byte(b))
        {
            return true;
        }

        /* When mid-header (g_sync_step > 0), wait longer so a 1-2ms USB-Serial
         * frame boundary never triggers a 25ms VGA redraw in the middle of a command. */
        uint16_t idle_guard = (g_sync_step > 0) ? 0xFFFF : 4000;
        bool got_next = false;
        while (--idle_guard)
        {
            if (uart_try_getc(&b))
            {
                got_next = true;
                break;
            }
        }
        if (!got_next)
        {
            g_sync_step = 0;
            /* Burst finished; return true so UI updates RX counter & hex history */
            return true;
        }
    }
}

void loader_execute_last(void)
{
    if (!g_loader.payload_ready) return;
    loader_jump_far(g_loader.last_seg, g_loader.last_off);
}
