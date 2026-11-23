/* C library functions UnixLib lacks (RISC OS builds only).
 * RISC OS port of Mupen64Plus. Copyright (C) 2026 David Sharp. GPL-2.0. */
#include <ctype.h>
#include <stddef.h>
#include <string.h>

char *strcasestr(const char *haystack, const char *needle)
{
    size_t n = strlen(needle);
    for (; *haystack; haystack++)
        if (strncasecmp(haystack, needle, n) == 0)
            return (char *) haystack;
    return n == 0 ? (char *) haystack : NULL;
}
