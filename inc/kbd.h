#pragma once

#include <stdbool.h>
#include <stdint.h>

#define KEY_NONE  0x0000
#define KEY_ESC   0x011B
#define KEY_ENTER 0x1C0D
#define KEY_UP    0x4800
#define KEY_DOWN  0x5000
#define KEY_LEFT  0x4B00
#define KEY_RIGHT 0x4D00
#define KEY_SPACE 0x3920
#define KEY_TAB   0x0F09
#define KEY_T     0x1474
#define KEY_F     0x2166
#define KEY_M     0x326D
#define KEY_F1    0x3B00
#define KEY_F2    0x3C00
#define KEY_F3    0x3D00
#define KEY_F4    0x3E00

void kbd_init(void);
bool kbd_get_scancode(uint8_t* out_scancode);
bool kbd_get_key(uint16_t* out_key);
