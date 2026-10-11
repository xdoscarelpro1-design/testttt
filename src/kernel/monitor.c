/*
 * PHANTOM Kernel Payload — Process Monitor
 *
 * Detects when FiveM launches by scanning the EPROCESS linked list.
 * No kernel callback registration (PsSetCreateProcessNotifyRoutine),
 * no timer objects, no system threads — zero forensic footprint.
 *
 * Instead, we piggyback on the SSDT hook: every N-th syscall from
 * any process triggers a quick scan of the process list. When FiveM
 * appears, the injector maps the cheat DLL into its address space
 * from the embedded blob. One scan, one injection, then silence.
 */

#include "ntdefs.h"

/* from injector.c */
extern PVOID    InjectorMapDll(PAYLOAD_CONTEXT *Ctx, PVOID TargetProcess,
                               uint8_t *DllBlob, UINTN DllSize);
extern NTSTATUS InjectorExecuteEntry(PAYLOAD_CONTEXT *Ctx, PVOID TargetProcess,
                                     PVOID MappedBase, uint64_t EntryRva);

static volatile uint32_t sCheckCounter = 0;

/* scan interval: check every 2048 syscalls (~every few seconds at normal load) */
#define MONITOR_SCAN_INTERVAL 2048

/* ─── helpers ─── */

static int mon_prefix_match(const char *str, const char *prefix)
{
    /* _case-insensitive prefix match against a 15-byte ImageFileName_ */
    while (*prefix) {
        char a = *str, b = *prefix;
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return 0;
        str++; prefix++;
    }
    return 1;
}

/*
 * Walk the EPROCESS doubly-linked list to find a process whose
 * ImageFileName starts with "fivem" (case-insensitive).
 *
 * The list is rooted at any EPROCESS (we start from System via
 * PsGetCurrentProcess). We follow ActiveProcessLinks until we
 * wrap back to the start or find our target.
 *
 * Returns EPROCESS pointer if found, NULL otherwise.
 * Caller must dereference with ObfDereferenceObject.
 */
static PVOID MonitorScanProcessList(PAYLOAD_CONTEXT *Ctx)
{
    /* _walking the linked list at ring-0, tiptoeing past PatchGuard_ */
    PVOID SystemProcess = Ctx->Api.PsGetCurrentProcess();
    if (!SystemProcess) return NULL;

    uint8_t *StartEproc = (uint8_t *)SystemProcess;
    LIST_ENTRY *StartLink = (LIST_ENTRY *)(StartEproc + EPROCESS_ACTIVEPROCESSLINKS_OFFSET);
    LIST_ENTRY *CurLink = StartLink->Flink;

    char fivem_prefix[] = {'f','i','v','e','m',0};

    for (int safety = 0; safety < 4096 && CurLink != StartLink; safety++) {
        uint8_t *Eproc = (uint8_t *)CurLink - EPROCESS_ACTIVEPROCESSLINKS_OFFSET;
        char *ImageName = (char *)(Eproc + EPROCESS_IMAGEFILENAME_OFFSET);

        if (mon_prefix_match(ImageName, fivem_prefix)) {
            HANDLE Pid = *(HANDLE *)(Eproc + EPROCESS_UNIQUEPROCESSID_OFFSET);
            if ((uint64_t)Pid > 4) {
                /* verify the process is alive by looking it up properly */
                PVOID Process = NULL;
                NTSTATUS st = Ctx->Api.PsLookupProcessByProcessId(Pid, &Process);
                if (NT_SUCCESS(st))
                    return Process; /* caller must deref */
            }
        }

        CurLink = CurLink->Flink;
    }

    return NULL;
}

/*
 * Check if the target process has finished initial DLL loading.
 * We read PEB.Ldr and verify InLoadOrderModuleList has entries.
 * This prevents injecting before ntdll/kernel32 are mapped.
 */
static int MonitorIsProcessReady(PAYLOAD_CONTEXT *Ctx, PVOID TargetProcess)
{
    PVOID Peb = Ctx->Api.PsGetProcessPeb(TargetProcess);
    if (!Peb) return 0;

    /* read PEB.Ldr (offset 0x18) from the target process */
    PVOID Ldr = NULL;
    SIZE_T BytesRead = 0;

    NTSTATUS st = Ctx->Api.MmCopyVirtualMemory(
        TargetProcess, (uint8_t *)Peb + 0x18,
        Ctx->Api.PsGetCurrentProcess(), &Ldr,
        sizeof(PVOID), 0, &BytesRead
    );

    if (!NT_SUCCESS(st) || !Ldr) return 0;

    /* read Ldr.Initialized (offset 0x04) */
    BOOLEAN Initialized = 0;
    st = Ctx->Api.MmCopyVirtualMemory(
        TargetProcess, (uint8_t *)Ldr + 4,
        Ctx->Api.PsGetCurrentProcess(), &Initialized,
        sizeof(BOOLEAN), 0, &BytesRead
    );

    if (!NT_SUCCESS(st)) return 0;

    return Initialized ? 1 : 0;
}

/*
 * Get the entry point RVA for the "PhantomEntry" export in the DLL blob.
 * Falls back to AddressOfEntryPoint (DllMain) if not found.
 */
static uint64_t MonitorGetEntryRva(uint8_t *DllBlob)
{
    if (*(uint16_t *)DllBlob != 0x5A4D) return 0;

    uint32_t PeOff = *(uint32_t *)(DllBlob + 0x3C);
    uint8_t *Pe = DllBlob + PeOff;

    /* default: DllMain via AddressOfEntryPoint */
    uint32_t DefaultEntry = *(uint32_t *)(Pe + 4 + 20 + 16); /* OptHdr.AddressOfEntryPoint */

    /* try to find PhantomEntry export */
    uint32_t ExpRva = *(uint32_t *)(Pe + 4 + 20 + 112); /* DataDir[0].VirtualAddress */
    if (!ExpRva) return DefaultEntry;

    uint8_t *Exp = DllBlob + ExpRva;
    uint32_t NumNames = *(uint32_t *)(Exp + 24);
    uint32_t *Names   = (uint32_t *)(DllBlob + *(uint32_t *)(Exp + 32));
    uint16_t *Ords    = (uint16_t *)(DllBlob + *(uint32_t *)(Exp + 36));
    uint32_t *Funcs   = (uint32_t *)(DllBlob + *(uint32_t *)(Exp + 28));

    char phantom_entry[] = {'P','h','a','n','t','o','m','E','n','t','r','y',0};

    for (uint32_t i = 0; i < NumNames; i++) {
        char *Name = (char *)(DllBlob + Names[i]);
        const char *a = phantom_entry;
        const char *b = Name;
        int match = 1;
        while (*a && *b) {
            if (*a != *b) { match = 0; break; }
            a++; b++;
        }
        if (*a || *b) match = 0;

        if (match)
            return Funcs[Ords[i]];
    }

    return DefaultEntry;
}

/*
 * Called from HookedSyscallHandler on every syscall.
 * On every MONITOR_SCAN_INTERVAL-th call, scans for FiveM.
 * When found and ready, injects the cheat DLL.
 *
 * Returns 1 if injection happened this call, 0 otherwise.
 */
int MonitorCheckForTarget(PAYLOAD_CONTEXT *Ctx)
{
    if (Ctx->CheatInjected) return 0;
    if (!Ctx->CheatDllBlob || !Ctx->CheatDllSize) return 0;

    /* throttle: only scan every N syscalls */
    sCheckCounter++;
    if ((sCheckCounter & (MONITOR_SCAN_INTERVAL - 1)) != 0)
        return 0;

    /* scan process list */
    PVOID TargetProcess = MonitorScanProcessList(Ctx);
    if (!TargetProcess) return 0;

    /* wait for the process to finish loading */
    if (!MonitorIsProcessReady(Ctx, TargetProcess)) {
        Ctx->Api.ObfDereferenceObject(TargetProcess);
        return 0;
    }

    /* inject */
    PVOID MappedBase = InjectorMapDll(Ctx, TargetProcess,
                                       Ctx->CheatDllBlob, Ctx->CheatDllSize);

    if (MappedBase) {
        uint64_t EntryRva = MonitorGetEntryRva(Ctx->CheatDllBlob);

        NTSTATUS st = InjectorExecuteEntry(Ctx, TargetProcess,
                                            MappedBase, EntryRva);

        if (NT_SUCCESS(st)) {
            Ctx->CheatInjected = 1;
            Ctx->MappedBase = MappedBase;
            Ctx->TargetPid = *(HANDLE *)(
                (uint8_t *)TargetProcess + EPROCESS_UNIQUEPROCESSID_OFFSET
            );
        }
    }

    Ctx->Api.ObfDereferenceObject(TargetProcess);
    return Ctx->CheatInjected ? 1 : 0;
}

/*
 * Initialize monitor state in the payload context.
 * Called once from PayloadEntry after all APIs are resolved.
 */
void MonitorStart(PAYLOAD_CONTEXT *Ctx)
{
    Ctx->CheatInjected = 0;
    Ctx->TargetPid = NULL;
    Ctx->MappedBase = NULL;
    sCheckCounter = 0;
}
