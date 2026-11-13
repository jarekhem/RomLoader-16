#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Serial Protocol Constants
 * Frame format:
 *   [0x55] [0xAA] ['R'] ['L'] [CMD] ...
 */
#define LOADER_MAGIC_0 0x55
#define LOADER_MAGIC_1 0xAA
#define LOADER_MAGIC_2 'R'
#define LOADER_MAGIC_3 'L'

#define LOADER_CMD_PING          0x01
#define LOADER_CMD_LOAD          0x02
#define LOADER_CMD_EXEC          0x03
#define LOADER_CMD_LOAD_AND_EXEC 0x04

#define LOADER_ACK 0x06
#define LOADER_NAK 0x15

#define LOADER_RX_HISTORY_LEN 8

typedef struct
{
    uint16_t last_seg;
    uint16_t last_off;
    uint16_t last_size;
    uint16_t last_crc;
    uint16_t rx_bytes_total;
    uint8_t last_rx[LOADER_RX_HISTORY_LEN];
    uint8_t last_rx_count;
    bool payload_ready;
    const char* status_msg;
} LoaderState;

extern LoaderState g_loader;

void loader_init(void);
bool loader_poll_serial(void);
void loader_execute_last(void);

/* Assembly routine in boot.asm */
extern void __attribute__((noreturn)) loader_jump_far(uint16_t seg, uint16_t off);
