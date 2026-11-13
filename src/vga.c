#include "vga.h"
#include "io.h"
#include <assert.h>
#include <stdlib.h>

static uint16_t g_vga_segment = 0xB800;

static void vga_enable_16color_bg(void)
{
    __asm__ __volatile__(
        "mov $0x1003, %%ax\n\t"
        "mov $0x00, %%bl\n\t"
        "int $0x10\n\t"
        :
        :
        : "ax", "bx"
    );
}

void vga_disable_cursor(void)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void vga_set_cursor(uint8_t x, uint8_t y)
{
    if (x >= VGA_COLS) x = VGA_COLS - 1;
    if (y >= VGA_ROWS) y = VGA_ROWS - 1;

    uint16_t pos = (uint16_t)y * VGA_COLS + x;

    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x0E);
    outb(0x3D4, 0x0B);
    outb(0x3D5, 0x0F);

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

bool vga_init(void)
{
    g_vga_segment = 0xB800;
    vga_disable_cursor();
    vga_enable_16color_bg();
    return true;
}

void vga_draw_char(uint8_t x, uint8_t y, char ch, uint8_t attr)
{
    if (x >= VGA_COLS || y >= VGA_ROWS) return;

    uint16_t offset = ((uint16_t)y * VGA_COLS + x) * 2;
    uint16_t val = ((uint16_t)attr << 8) | (uint8_t)ch;

    __asm__ __volatile__(
        "push %%es\n\t"
        "movw %0, %%es\n\t"
        "movw %%ax, %%es:(%%bx)\n\t"
        "pop %%es"
        :
        : "r"(g_vga_segment), "a"(val), "b"(offset)
        : "memory"
    );
}

void vga_clear(uint8_t attr)
{
    uint16_t fill = ((uint16_t)attr << 8) | ' ';
    uint16_t count = VGA_COLS * VGA_ROWS;
    uint16_t di = 0;

    __asm__ __volatile__(
        "push %%es\n\t"
        "movw %2, %%es\n\t"
        "cld\n\t"
        "rep stosw\n\t"
        "pop %%es"
        : "+D"(di), "+c"(count)
        : "r"(g_vga_segment), "a"(fill)
        : "memory", "cc"
    );
}

uint8_t vga_print(uint8_t x, uint8_t y, const char* str, uint8_t attr)
{
    if (!str) return x;
    while (*str != '\0' && x < VGA_COLS)
    {
        vga_draw_char(x++, y, *str++, attr);
    }
    return x;
}

static char nibble_to_hex(uint8_t nib)
{
    nib &= 0x0F;
    return (nib < 10) ? ('0' + nib) : ('A' + (nib - 10));
}

uint8_t vga_print_hex8(uint8_t x, uint8_t y, uint8_t val, uint8_t attr)
{
    vga_draw_char(x++, y, nibble_to_hex(val >> 4), attr);
    vga_draw_char(x++, y, nibble_to_hex(val & 0x0F), attr);
    return x;
}

uint8_t vga_print_hex16(uint8_t x, uint8_t y, uint16_t val, uint8_t attr)
{
    x = vga_print_hex8(x, y, (uint8_t)(val >> 8), attr);
    x = vga_print_hex8(x, y, (uint8_t)(val & 0xFF), attr);
    return x;
}

void vga_draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t bg_attr, uint8_t border_attr, const char* title)
{
    for (uint8_t row = 0; row < h; row++)
    {
        for (uint8_t col = 0; col < w; col++)
        {
            char ch = ' ';
            uint8_t attr = bg_attr;

            if (row == 0)
            {
                attr = border_attr;
                if (col == 0) ch = (char)0xDA;
                else if (col == w - 1) ch = (char)0xBF;
                else ch = (char)0xC4;
            }
            else if (row == h - 1)
            {
                attr = border_attr;
                if (col == 0) ch = (char)0xC0;
                else if (col == w - 1) ch = (char)0xD9;
                else ch = (char)0xC4;
            }
            else if (col == 0 || col == w - 1)
            {
                attr = border_attr;
                ch = (char)0xB3;
            }

            vga_draw_char(x + col, y + row, ch, attr);
        }
    }

    if (title)
    {
        uint8_t tx = x + 2;
        vga_draw_char(tx++, y, ' ', border_attr);
        tx = vga_print(tx, y, title, border_attr);
        vga_draw_char(tx, y, ' ', border_attr);
    }
}

void __attribute__((noreturn)) __assert_func(const char* file, int line)
{
    uint8_t attr = 0x4F;
    vga_clear(attr);
    vga_print(2, 1, "=== ROM LOADER ASSERTION FAILED ===", attr);

    uint8_t x = 2;
    x = vga_print(x, 3, "File : ", attr);
    x = vga_print(x, 3, file ? file : "?", attr);
    x = vga_print(x, 3, " : ", attr);

    char line_buf[8];
    itoa(line, line_buf, 10);
    vga_print(x, 3, line_buf, attr);

    __asm__ __volatile__("xchg %bx, %bx\n\t");
    while (1) {}
}
