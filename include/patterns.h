#ifndef _PATTERNS_H_
#define _PATTERNS_H_

/*
 * Signature patterns for locating key functions in the Windows boot chain.
 * These patterns target Windows 10 22H2 / Windows 11 23H2+ builds.
 *
 * Pattern format: hex bytes, '?' = wildcard nibble
 * Each pattern identifies a specific function or code sequence
 * that we need to hook or patch during boot.
 */

/* ─── bootmgfw.efi patterns ─── */

/*
 * ImgArchStartBootApplication - the function bootmgfw calls to transfer
 * control to winload.efi. We hook this to intercept winload before it runs.
 *
 * Pattern matches the function prologue:
 *   sub rsp, ??h
 *   mov [rsp+??h], rbx
 *   ... load parameters ...
 *   call ImgArchStartBootApplication
 */
#define PAT_IMGARCH_START_BOOT_APP \
    "48 8B C4 48 89 58 20 44 89 40 18 48 89 50 10 48 89 48 08 55 56 57 41 54 41 55 41 56 41 57 48 8D 68"

/*
 * Alternative pattern for newer Windows builds (24H2+)
 */
#define PAT_IMGARCH_START_BOOT_APP_V2 \
    "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 41 54 41 55 41 56 41 57 48 83 EC ?? 33 FF"

/* ─── winload.efi patterns ─── */

/*
 * OslFwpKernelSetupPhase1 - called during kernel setup, gives us access
 * to the loader block before ntoskrnl gets control.
 *
 * We scan for the call to BlImgAllocateImageBuffer that allocates
 * the kernel image buffer, which precedes the entry point call.
 */
#define PAT_OSL_FWP_KERNEL_SETUP \
    "48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 41 8B F0 48 8B DA 48 8B F9 E8"

/*
 * OslArchTransferToKernel - the final trampoline from winload to ntoskrnl.
 * This is where we install our last-stage hook right before kernel entry.
 */
#define PAT_OSL_ARCH_TRANSFER \
    "48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 48 89 78 20 41 56 48 83 EC ?? 48 8D"

/*
 * Alternative: scan for the `jmp rax` that transfers to KiSystemStartup
 */
#define PAT_TRANSFER_JMP_RAX \
    "FF E0"

/* ─── ntoskrnl.exe patterns ─── */

/*
 * KiSystemServiceUser - the syscall dispatch handler.
 * We locate this via LSTAR MSR value during boot transition.
 */
#define PAT_KI_SYSTEM_CALL \
    "0F 01 F8 65 48 89 24 25 10 00 00 00 65 48 8B 24 25 A8 01 00 00"

/*
 * MmPfnDatabase - pointer to the PFN database for hiding physical pages.
 * Pattern scans for the MOV instruction that loads the PFN database base.
 */
#define PAT_MM_PFN_DATABASE \
    "48 8B 05 ?? ?? ?? ?? 48 89 43 18 48 03 C3"

/*
 * Alternative PFN database pattern (Win11 24H2)
 */
#define PAT_MM_PFN_DATABASE_V2 \
    "48 8B 0D ?? ?? ?? ?? 4C 8D 04 C1 4D 8B 00"

/*
 * PsLoadedModuleList - head of the loaded driver list.
 * We don't appear here because we never register as a driver.
 */
#define PAT_PS_LOADED_MODULE_LIST \
    "48 8B 15 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 0D"

/*
 * ci.dll CiValidateImageHeader / CiValidateImageData
 * DSE (Driver Signature Enforcement) check.
 * We patch this to always return STATUS_SUCCESS.
 */
#define PAT_CI_VALIDATE \
    "48 89 5C 24 ?? 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 ?? ?? ?? ?? B8 ?? ?? ?? ?? E8"

/*
 * SepInitializeCodeIntegrity - called during boot, sets g_CiEnabled.
 * Patching the flag skips all CI checks.
 */
#define PAT_SEP_INIT_CI \
    "40 53 48 83 EC ?? 48 8B 05 ?? ?? ?? ?? C6 05"

/*
 * PatchGuard (KPP) initialization patterns.
 * KiFilterFiberContext - one of the PatchGuard init routines.
 * We NOP the call to prevent PG from arming its timers.
 */
#define PAT_KI_FILTER_FIBER_CONTEXT \
    "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 41 56 41 57 48 83 EC ?? 65 48 8B 04 25 ?? ?? ?? ?? 33 ED"

/*
 * ExpLicenseWatchInitWorker - another PatchGuard init vector.
 */
#define PAT_EXP_LICENSE_WATCH \
    "48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 33 FF 48 8D 35"

/* ─── FiveM / CFX patterns ─── */

/*
 * FiveM process name for identification
 */
#define FIVEM_PROCESS_NAME      "FiveM"
#define FIVEM_GTAProcess        "GTA5.exe"
#define FIVEM_CITIZEN           "CitizenFX"

/* ─── Pattern matching helpers ─── */

typedef struct {
    UINT8   Bytes[BK_MAX_PATTERN_LEN];
    UINT8   Mask[BK_MAX_PATTERN_LEN];
    UINTN   Length;
    CHAR8  *Name;
} BK_PATTERN;

static inline BOOLEAN BkParsePatternStr(const CHAR8 *Str, UINT8 *OutBytes, UINT8 *OutMask, UINTN *OutLen)
{
    UINTN idx = 0;
    while (*Str && idx < BK_MAX_PATTERN_LEN) {
        while (*Str == ' ') Str++;
        if (!*Str) break;

        if (*Str == '?') {
            OutBytes[idx] = 0x00;
            OutMask[idx]  = 0x00;
            Str++;
            if (*Str == '?') Str++;
        } else {
            UINT8 hi = 0, lo = 0;
            if (*Str >= '0' && *Str <= '9') hi = *Str - '0';
            else if (*Str >= 'A' && *Str <= 'F') hi = *Str - 'A' + 10;
            else if (*Str >= 'a' && *Str <= 'f') hi = *Str - 'a' + 10;
            Str++;
            if (*Str >= '0' && *Str <= '9') lo = *Str - '0';
            else if (*Str >= 'A' && *Str <= 'F') lo = *Str - 'A' + 10;
            else if (*Str >= 'a' && *Str <= 'f') lo = *Str - 'a' + 10;
            Str++;
            OutBytes[idx] = (hi << 4) | lo;
            OutMask[idx]  = 0xFF;
        }
        idx++;
    }
    *OutLen = idx;
    return idx > 0;
}

#endif /* _PATTERNS_H_ */
