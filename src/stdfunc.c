#include <stdbool.h>
#include <stddef.h>

char* itoa(int value, char* str, int base)
{
    int i = 0;
    unsigned int uvalue;
    bool is_negative = false;

    if (base < 2 || base > 36)
    {
        str[0] = '\0';
        return str;
    }

    if (value == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    if (value < 0 && base == 10)
    {
        is_negative = true;
        uvalue = (unsigned int)(-value);
    }
    else
    {
        uvalue = (unsigned int)value;
    }

    while (uvalue != 0)
    {
        int rem = uvalue % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
        uvalue = uvalue / base;
    }

    if (is_negative)
        str[i++] = '-';

    str[i] = '\0';

    int start = 0;
    int end = i - 1;
    while (start < end)
    {
        char temp = str[start];
        str[start++] = str[end];
        str[end--] = temp;
    }

    return str;
}

void* memset(void* ptr, int value, size_t num)
{
    for (size_t i = 0; i < num; i++)
        ((char*)ptr)[i] = (char)value;

    return ptr;
}

void* memcpy(void* dest, const void* src, size_t num)
{
    for (size_t i = 0; i < num; i++)
        ((char*)dest)[i] = ((char*)src)[i];

    return dest;
}

size_t strnlen(const char* s, size_t max_len)
{
    size_t len = 0;
    while (len < max_len && s[len] != '\0')
        len++;

    return len;
}
