#include "kbd.h"
#include "io.h"
#include <assert.h>

#define KBC_DATA    0x60
#define KBC_CMD     0x64
#define KBC_STATUS  0x64

/* Native AT Scancode Set 2 make codes */
#define AT_MAKE_TAB     0x0D
#define AT_MAKE_ESC     0x76
#define AT_MAKE_ENTER   0x5A
#define AT_MAKE_UP      0x75
#define AT_MAKE_DOWN    0x72
#define AT_MAKE_LEFT    0x6B
#define AT_MAKE_RIGHT   0x74
#define AT_MAKE_SPACE   0x29
#define AT_MAKE_T       0x2C
#define AT_MAKE_F       0x2B
#define AT_MAKE_M       0x3A

static bool kbc_wait_write(void)
{
    uint16_t timeout = 0xFFFF;
    while ((inb(KBC_STATUS) & 0x02) && --timeout) io_wait();
    return timeout > 0;
}

static bool kbc_wait_read(void)
{
    uint32_t timeout = 0x7FFFF;
    while (!(inb(KBC_STATUS) & 0x01) && --timeout) io_wait();
    return timeout > 0;
}

static void kbc_flush(void)
{
    uint16_t timeout = 0x7FFF;
    while (--timeout)
    {
        if (inb(KBC_STATUS) & 0x01)
        {
            inb(KBC_DATA);
        }
        io_wait();
    }
}

void kbd_init(void)
{
    kbc_flush();

    /* Step 1: KBC Self-Test */
    kbc_wait_write();
    outb(KBC_CMD, 0xAA);
    if (!kbc_wait_read() || inb(KBC_DATA) != 0x55)
        assert(false && "KBC Error");

    /* Step 2: Keyboard Interface Test */
    kbc_wait_write();
    outb(KBC_CMD, 0xAB);
    if (!kbc_wait_read() || inb(KBC_DATA) != 0x00)
        assert(false && "CLK or DATA Line error");

    /* Step 3: Enable Keyboard Interface in 8042 */
    kbc_wait_write();
    outb(KBC_CMD, 0xAE);
    kbc_flush();

    /* Step 4: Reset Keyboard (BAT) */
    kbc_wait_write();
    outb(KBC_DATA, 0xFF);

    if (!kbc_wait_read() || inb(KBC_DATA) != 0xFA)
        assert(false && "KB RESET failed");

    if (!kbc_wait_read() || inb(KBC_DATA) != 0xAA)
        assert(false && "KB TEST failed");

    /* Step 5: Configure Command Byte - Disable Set 2->Set 1 translation & IRQ1 */
    kbc_wait_write();
    outb(KBC_CMD, 0x20);
    if (!kbc_wait_read())
        assert(false && "KBC CONFIG failed");

    uint8_t cfg = inb(KBC_DATA);
    cfg &= ~0x40; /* Bit 6 = 0: Keep native AT Scancode Set 2 */
    cfg &= ~0x01; /* Bit 0 = 0: Disable IRQ1 (polling mode) */

    kbc_wait_write();
    outb(KBC_CMD, 0x60);
    kbc_wait_write();
    outb(KBC_DATA, cfg);

    kbc_flush();
}

bool kbd_get_scancode(uint8_t* out_scancode)
{
    assert(out_scancode);

    if (inb(KBC_STATUS) & 0x01)
    {
        *out_scancode = inb(KBC_DATA);
        return true;
    }
    return false;
}

bool kbd_get_key(uint16_t* out_key)
{
    static bool expect_break = false;
    uint8_t scancode;

    if (kbd_get_scancode(&scancode))
    {
        if (scancode == 0xE0)
            return false;

        if (scancode == 0xF0)
        {
            expect_break = true;
            return false;
        }

        if (expect_break)
        {
            expect_break = false;
            return false;
        }

        switch (scancode)
        {
        case AT_MAKE_UP:    *out_key = KEY_UP;    return true;
        case AT_MAKE_DOWN:  *out_key = KEY_DOWN;  return true;
        case AT_MAKE_LEFT:  *out_key = KEY_LEFT;  return true;
        case AT_MAKE_RIGHT: *out_key = KEY_RIGHT; return true;
        case AT_MAKE_ENTER: *out_key = KEY_ENTER; return true;
        case AT_MAKE_ESC:   *out_key = KEY_ESC;   return true;
        case AT_MAKE_SPACE: *out_key = KEY_SPACE; return true;
        case AT_MAKE_TAB:   *out_key = KEY_TAB;   return true;
        case AT_MAKE_T:     *out_key = KEY_T;     return true;
        case AT_MAKE_F:     *out_key = KEY_F;     return true;
        case AT_MAKE_M:     *out_key = KEY_M;     return true;
        }
    }
    return false;
}
