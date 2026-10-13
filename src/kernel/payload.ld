/*
 * PHANTOM Kernel Payload — Linker Script
 *
 * Forces all sections into a single contiguous blob so that
 * objcopy -O binary produces a complete PIC payload with code,
 * read-only data (including the cheat DLL blob), and BSS.
 */

SECTIONS
{
    . = 0;

    .text : {
        *(.text*)
        *(.rdata*)
        *(.rodata*)
        *(.data*)

        /* BSS — zero-initialized, must come last */
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.eh_frame*)
        *(.comment*)
        *(.reloc*)
        *(.pdata*)
        *(.xdata*)
        *(.edata*)
        *(.idata*)
        *(.CRT*)
        *(.ctors*)
        *(.dtors*)
    }
}
