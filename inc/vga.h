#pragma once

#include <stdbool.h>
#include <stdint.h>

#define VGA_COLS 80
#define VGA_ROWS 25

bool vga_init(void);
void vga_clear(uint8_t attr);
void vga_draw_char(uint8_t x, uint8_t y, char ch, uint8_t attr);
uint8_t vga_print(uint8_t x, uint8_t y, const char* str, uint8_t attr);
uint8_t vga_print_hex8(uint8_t x, uint8_t y, uint8_t val, uint8_t attr);
uint8_t vga_print_hex16(uint8_t x, uint8_t y, uint16_t val, uint8_t attr);
void vga_draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t bg_attr, uint8_t border_attr, const char* title);
void vga_set_cursor(uint8_t x, uint8_t y);
void vga_disable_cursor(void);
