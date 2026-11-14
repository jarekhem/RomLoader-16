#pragma once

#include <stdint.h>

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ __volatile__("inb %%dx, %%al" : "=Ral"(ret) : "d"(port) : "memory");
    return ret;
}

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ __volatile__("outb %%al, %%dx" : : "Ral"(val), "d"(port) : "memory");
}

static inline void io_wait(void)
{
    __asm__ __volatile__("outb %%al, $0x80" : : "Ral"((uint8_t)0) : "memory");
}

static inline void far_poke_b(uint16_t seg, uint16_t off, uint8_t val)
{
    __asm__ __volatile__(
        "push %%es\n\t"
        "movw %0, %%es\n\t"
        "movb %%al, %%es:(%%bx)\n\t"
        "pop %%es"
        :
        : "r"(seg), "b"(off), "Ral"(val)
        : "memory"
    );
}

static inline uint8_t far_peek_b(uint16_t seg, uint16_t off)
{
    uint8_t val;
    __asm__ __volatile__(
        "push %%es\n\t"
        "movw %1, %%es\n\t"
        "movb %%es:(%%bx), %%al\n\t"
        "pop %%es"
        : "=Ral"(val)
        : "r"(seg), "b"(off)
        : "memory"
    );
    return val;
}
