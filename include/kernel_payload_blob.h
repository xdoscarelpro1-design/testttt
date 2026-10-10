#ifndef _KERNEL_PAYLOAD_BLOB_H_
#define _KERNEL_PAYLOAD_BLOB_H_

/*
 * Embedded kernel payload blob.
 *
 * This is a placeholder. In a real build, this file is auto-generated
 * by the build system from the compiled kernel payload binary:
 *
 *   1. Compile src/kernel/entry.c as position-independent code
 *   2. Extract the .text section as raw binary
 *   3. Convert to a C byte array
 *
 * Build command (example):
 *   x86_64-w64-mingw32-gcc -ffreestanding -fno-stack-protector \
 *       -fPIC -mno-red-zone -nostdlib -c src/kernel/entry.c \
 *       -o kernel_payload.o -I include
 *   objcopy -O binary -j .text kernel_payload.o kernel_payload.bin
 *   xxd -i kernel_payload.bin > include/kernel_payload_blob.h
 *
 * The payload MUST be:
 *   - Position-independent (no absolute addresses except stack vars)
 *   - No CRT dependencies (no printf, malloc, etc.)
 *   - No stack cookies (-fno-stack-protector)
 *   - No red zone (-mno-red-zone, required for kernel code)
 *   - All strings as stack-allocated char arrays
 *
 * Maximum size: 1MB (enforced by BkPrepareKernelPayload)
 */

static const unsigned char gKernelPayloadBlob[] = {
    /* PayloadEntry function prologue */
    0x55,                               /* push rbp            */
    0x48, 0x89, 0xE5,                   /* mov rbp, rsp        */
    0x48, 0x83, 0xEC, 0x70,             /* sub rsp, 0x70       */
    0x48, 0x89, 0x4D, 0xF0,             /* mov [rbp-10h], rcx  (NtoskrnlBase) */
    0x48, 0x89, 0x55, 0xE8,             /* mov [rbp-18h], rdx  (NtoskrnlSize) */

    /*
     * PLACEHOLDER STUB — returns immediately.
     *
     * Replace this entire array with the output of:
     *   xxd -i kernel_payload.bin
     *
     * after compiling the kernel payload sources.
     */

    0x48, 0x83, 0xC4, 0x70,             /* add rsp, 0x70       */
    0x5D,                               /* pop rbp             */
    0xC3,                               /* ret                 */

    /* padding to 4KB boundary */
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
};

#endif /* _KERNEL_PAYLOAD_BLOB_H_ */
