/*
 * PHANTOM Kernel Payload - Entry Point
 *
 * This code executes inside ntoskrnl.exe's address space during
 * early kernel initialization (called from our winload trampoline
 * before KiSystemStartup gets control).
 *
 * CONSTRAINTS:
 *   - Position-independent code (PIC) — no relocations
 *   - No CRT — all functions resolved manually from ntoskrnl exports
 *   - No stack cookies — compiled with -fno-stack-protector
 *   - Must be re-entrant safe (IRQL considerations)
 *   - All strings as byte arrays (no .rdata references)
 */

#include "ntdefs.h"

/* embedded cheat DLL blob (generated at build time) */
#include "cheat_payload_blob.h"

/* from monitor.c */
extern void MonitorStart(PAYLOAD_CONTEXT *Ctx);
extern int  MonitorCheckForTarget(PAYLOAD_CONTEXT *Ctx);

/* forward declarations */
static PVOID  ResolveKernelApi(PVOID NtoskrnlBase, const char *FuncName);
static void   InstallSyscallHook(PAYLOAD_CONTEXT *Ctx);
static void   HidePayloadMemory(PAYLOAD_CONTEXT *Ctx);
static void   PatchDseFlag(PVOID NtoskrnlBase, UINTN NtoskrnlSize);

/* payload context — lives at the start of our runtime memory region */
static PAYLOAD_CONTEXT *gPayloadCtx = NULL;

/* ─── debug logging ─── */
static volatile uint32_t sDbgTick = 0;

static void DbgLog(PAYLOAD_CONTEXT *Ctx, unsigned char Level, const char *Msg)
{
    if (!Ctx) return;
    PHANTOM_STATUS *S = &Ctx->DbgStatus;

    unsigned int idx = S->LogHead % DBG_LOG_MAX_ENTRIES;
    DBG_LOG_ENTRY *e = &S->Log[idx];

    e->Level = Level;
    e->Tick = sDbgTick++;

    int i = 0;
    while (Msg[i] && i < DBG_LOG_MSG_LEN - 1) {
        e->Msg[i] = Msg[i];
        i++;
    }
    e->Msg[i] = 0;

    S->LogHead = (S->LogHead + 1) % DBG_LOG_MAX_ENTRIES;
    if (S->LogCount < DBG_LOG_MAX_ENTRIES) S->LogCount++;
}

static void DbgSyncStatus(PAYLOAD_CONTEXT *Ctx)
{
    PHANTOM_STATUS *S = &Ctx->DbgStatus;
    S->Magic          = PHANTOM_DBG_MAGIC;
    S->Version        = PHANTOM_DBG_VERSION;
    S->NtoskrnlBase   = (unsigned long long)Ctx->NtoskrnlBase;
    S->NtoskrnlSize   = (unsigned long long)Ctx->NtoskrnlSize;
    S->PayloadBase    = (unsigned long long)Ctx->PayloadBase;
    S->PayloadSize    = (unsigned long long)Ctx->PayloadSize;
    S->SsdtBase       = (unsigned long long)Ctx->SsdtBase;
    S->CodeCaveAddr   = (unsigned long long)Ctx->CodeCaveAddr;
    S->PfnDatabase    = (unsigned long long)Ctx->PfnDatabase;
    S->ApiResolved    = Ctx->ApiResolved;
    S->ApiFailed      = Ctx->ApiFailed;
    S->TargetPid      = (unsigned long long)Ctx->TargetPid;
    S->MappedBase     = (unsigned long long)Ctx->MappedBase;
    S->CheatDllSize   = (unsigned int)Ctx->CheatDllSize;
}

/*
 * Kernel payload entry point.
 * Called from the winload trampoline with:
 *   rcx = ntoskrnl base address
 *   rdx = ntoskrnl size
 *   r8  = payload physical base address
 *   r9  = payload allocation size (bytes)
 */
void __attribute__((section(".text"))) PayloadEntry(
    PVOID NtoskrnlBase, UINTN NtoskrnlSize,
    PVOID PayloadBase, UINTN PayloadAllocSize)
{
    static PAYLOAD_CONTEXT Ctx;
    gPayloadCtx = &Ctx;

    /* zero the debug status */
    {
        uint8_t *p = (uint8_t *)&Ctx.DbgStatus;
        for (UINTN z = 0; z < sizeof(PHANTOM_STATUS); z++) p[z] = 0;
    }

    Ctx.Magic        = 0xDEADC0DE;
    Ctx.NtoskrnlBase = NtoskrnlBase;
    Ctx.NtoskrnlSize = NtoskrnlSize;
    Ctx.PayloadBase  = PayloadBase;
    Ctx.PayloadSize  = PayloadAllocSize;
    Ctx.Initialized  = 0;
    Ctx.Hidden       = 0;
    Ctx.ApiResolved  = 0;
    Ctx.ApiFailed    = 0;
    Ctx.SsdtBase     = NULL;
    Ctx.CodeCaveAddr = NULL;

    Ctx.DbgStatus.Stages |= STAGE_ENTRY_RAN;
    DbgLog(&Ctx, DBG_OK, "PayloadEntry: kernel payload entered");
    DbgLog(&Ctx, DBG_INFO, "PayloadEntry: resolving kernel APIs...");

    /* ─── resolve kernel APIs ─── */
    /* each string is a local array to keep PIC clean */

    char s_ExAllocatePool2[] = {'E','x','A','l','l','o','c','a','t','e','P','o','o','l','2',0};
    char s_MmCopyVirtualMemory[] = {'M','m','C','o','p','y','V','i','r','t','u','a','l','M','e','m','o','r','y',0};
    char s_MmGetVirtualForPhysical[] = {'M','m','G','e','t','V','i','r','t','u','a','l','F','o','r','P','h','y','s','i','c','a','l',0};
    char s_PsGetCurrentProcess[] = {'P','s','G','e','t','C','u','r','r','e','n','t','P','r','o','c','e','s','s',0};
    char s_PsLookupProcessByProcessId[] = {'P','s','L','o','o','k','u','p','P','r','o','c','e','s','s','B','y','P','r','o','c','e','s','s','I','d',0};
    char s_ObfDereferenceObject[] = {'O','b','f','D','e','r','e','f','e','r','e','n','c','e','O','b','j','e','c','t',0};
    char s_MmMapIoSpace[] = {'M','m','M','a','p','I','o','S','p','a','c','e',0};
    char s_MmUnmapIoSpace[] = {'M','m','U','n','m','a','p','I','o','S','p','a','c','e',0};

    Ctx.Api.ExAllocatePool2 = (fn_ExAllocatePool2)ResolveKernelApi(NtoskrnlBase, s_ExAllocatePool2);
    Ctx.Api.MmCopyVirtualMemory = (fn_MmCopyVirtualMemory)ResolveKernelApi(NtoskrnlBase, s_MmCopyVirtualMemory);
    Ctx.Api.MmGetVirtualForPhysical = (fn_MmGetVirtualForPhysical)ResolveKernelApi(NtoskrnlBase, s_MmGetVirtualForPhysical);
    Ctx.Api.PsGetCurrentProcess = (fn_PsGetCurrentProcess)ResolveKernelApi(NtoskrnlBase, s_PsGetCurrentProcess);
    Ctx.Api.PsLookupProcessByProcessId = (fn_PsLookupProcessByProcessId)ResolveKernelApi(NtoskrnlBase, s_PsLookupProcessByProcessId);
    Ctx.Api.ObfDereferenceObject = (fn_ObfDereferenceObject)ResolveKernelApi(NtoskrnlBase, s_ObfDereferenceObject);
    Ctx.Api.MmMapIoSpace = (fn_MmMapIoSpace)ResolveKernelApi(NtoskrnlBase, s_MmMapIoSpace);
    Ctx.Api.MmUnmapIoSpace = (fn_MmUnmapIoSpace)ResolveKernelApi(NtoskrnlBase, s_MmUnmapIoSpace);

    /* ─── resolve injector APIs ─── */
    char s_KeStackAttachProcess[] = {'K','e','S','t','a','c','k','A','t','t','a','c','h','P','r','o','c','e','s','s',0};
    char s_KeUnstackDetachProcess[] = {'K','e','U','n','s','t','a','c','k','D','e','t','a','c','h','P','r','o','c','e','s','s',0};
    char s_ZwAllocateVirtualMemory[] = {'Z','w','A','l','l','o','c','a','t','e','V','i','r','t','u','a','l','M','e','m','o','r','y',0};
    char s_ZwProtectVirtualMemory[] = {'Z','w','P','r','o','t','e','c','t','V','i','r','t','u','a','l','M','e','m','o','r','y',0};
    char s_PsGetProcessPeb[] = {'P','s','G','e','t','P','r','o','c','e','s','s','P','e','b',0};
    char s_PsCreateSystemThread[] = {'P','s','C','r','e','a','t','e','S','y','s','t','e','m','T','h','r','e','a','d',0};
    char s_ZwClose[] = {'Z','w','C','l','o','s','e',0};
    char s_ExAllocatePool[] = {'E','x','A','l','l','o','c','a','t','e','P','o','o','l',0};
    char s_ExFreePool[] = {'E','x','F','r','e','e','P','o','o','l',0};
    char s_KeDelayExecutionThread[] = {'K','e','D','e','l','a','y','E','x','e','c','u','t','i','o','n','T','h','r','e','a','d',0};
    char s_ZwCreateThreadEx[] = {'Z','w','C','r','e','a','t','e','T','h','r','e','a','d','E','x',0};
    char s_KeInitializeApc[] = {'K','e','I','n','i','t','i','a','l','i','z','e','A','p','c',0};
    char s_KeInsertQueueApc[] = {'K','e','I','n','s','e','r','t','Q','u','e','u','e','A','p','c',0};
    char s_PsGetNextProcessThread[] = {'P','s','G','e','t','N','e','x','t','P','r','o','c','e','s','s','T','h','r','e','a','d',0};

    Ctx.Api.KeStackAttachProcess = (fn_KeStackAttachProcess)ResolveKernelApi(NtoskrnlBase, s_KeStackAttachProcess);
    Ctx.Api.KeUnstackDetachProcess = (fn_KeUnstackDetachProcess)ResolveKernelApi(NtoskrnlBase, s_KeUnstackDetachProcess);
    Ctx.Api.ZwAllocateVirtualMemory = (fn_ZwAllocateVirtualMemory)ResolveKernelApi(NtoskrnlBase, s_ZwAllocateVirtualMemory);
    Ctx.Api.ZwProtectVirtualMemory = (fn_ZwProtectVirtualMemory)ResolveKernelApi(NtoskrnlBase, s_ZwProtectVirtualMemory);
    Ctx.Api.PsGetProcessPeb = (fn_PsGetProcessPeb)ResolveKernelApi(NtoskrnlBase, s_PsGetProcessPeb);
    Ctx.Api.PsCreateSystemThread = (fn_PsCreateSystemThread)ResolveKernelApi(NtoskrnlBase, s_PsCreateSystemThread);
    Ctx.Api.ZwClose = (fn_ZwClose)ResolveKernelApi(NtoskrnlBase, s_ZwClose);
    Ctx.Api.ExAllocatePool = (fn_ExAllocatePool)ResolveKernelApi(NtoskrnlBase, s_ExAllocatePool);
    Ctx.Api.ExFreePool = (fn_ExFreePool)ResolveKernelApi(NtoskrnlBase, s_ExFreePool);
    Ctx.Api.KeDelayExecutionThread = (fn_KeDelayExecutionThread)ResolveKernelApi(NtoskrnlBase, s_KeDelayExecutionThread);
    Ctx.Api.ZwCreateThreadEx = (fn_ZwCreateThreadEx)ResolveKernelApi(NtoskrnlBase, s_ZwCreateThreadEx);
    Ctx.Api.KeInitializeApc = (fn_KeInitializeApc)ResolveKernelApi(NtoskrnlBase, s_KeInitializeApc);
    Ctx.Api.KeInsertQueueApc = (fn_KeInsertQueueApc)ResolveKernelApi(NtoskrnlBase, s_KeInsertQueueApc);
    Ctx.Api.PsGetNextProcessThread = (fn_PsGetNextProcessThread)ResolveKernelApi(NtoskrnlBase, s_PsGetNextProcessThread);

    /* count resolved APIs */
    {
        PVOID *apis = (PVOID *)&Ctx.Api;
        UINTN total = sizeof(KERNEL_API) / sizeof(PVOID);
        Ctx.ApiResolved = 0;
        Ctx.ApiFailed = 0;
        for (UINTN a = 0; a < total; a++) {
            if (apis[a]) Ctx.ApiResolved++;
            else Ctx.ApiFailed++;
        }
    }

    if (Ctx.ApiResolved > 0) {
        Ctx.DbgStatus.Stages |= STAGE_API_RESOLVED;
        DbgLog(&Ctx, DBG_OK, "API resolve: core APIs found");
    }
    if (Ctx.ApiFailed > 0)
        DbgLog(&Ctx, DBG_WARN, "API resolve: some APIs not found (non-fatal)");

    /* verify critical APIs resolved */
    if (!Ctx.Api.MmCopyVirtualMemory || !Ctx.Api.PsLookupProcessByProcessId) {
        DbgLog(&Ctx, DBG_ERR, "FATAL: critical APIs not resolved (MmCopy/PsLookup)");
        DbgSyncStatus(&Ctx);
        return;
    }

    /* ─── patch the DSE global flag ─── */
    DbgLog(&Ctx, DBG_INFO, "DSE: patching g_CiEnabled...");
    PatchDseFlag(NtoskrnlBase, NtoskrnlSize);
    Ctx.DbgStatus.Stages |= STAGE_DSE_PATCHED;
    DbgLog(&Ctx, DBG_OK, "DSE: g_CiEnabled patched to 0");

    /* ─── install syscall hook for communication ─── */
    DbgLog(&Ctx, DBG_INFO, "SSDT: installing syscall hook...");
    InstallSyscallHook(&Ctx);
    if (Ctx.SsdtBase) {
        Ctx.DbgStatus.Stages |= STAGE_SSDT_HOOKED;
        DbgLog(&Ctx, DBG_OK, "SSDT: hook installed via code cave");
    } else {
        DbgLog(&Ctx, DBG_ERR, "SSDT: hook installation FAILED");
    }

    /* ─── hide our memory from OS ─── */
    DbgLog(&Ctx, DBG_INFO, "PFN: hiding payload memory...");
    HidePayloadMemory(&Ctx);
    if (Ctx.Hidden) {
        Ctx.DbgStatus.Stages |= STAGE_MEMORY_HIDDEN;
        DbgLog(&Ctx, DBG_OK, "PFN: payload pages marked Bad");
    } else {
        DbgLog(&Ctx, DBG_WARN, "PFN: memory hiding failed (non-fatal)");
    }

    /* ─── initialize cheat DLL injector ─── */
    Ctx.CheatDllBlob = (uint8_t *)gCheatPayloadBlob;
    Ctx.CheatDllSize = sizeof(gCheatPayloadBlob);
    MonitorStart(&Ctx);

    Ctx.DbgStatus.Stages |= STAGE_MONITOR_STARTED;
    DbgLog(&Ctx, DBG_OK, "Monitor: started, waiting for FiveM process");

    Ctx.Initialized = 1;
    DbgSyncStatus(&Ctx);
    DbgLog(&Ctx, DBG_OK, "PHANTOM: fully initialized, payload active");
}

/*
 * Resolve a kernel export by walking ntoskrnl's export table.
 * Pure position-independent implementation — no library calls.
 */
static PVOID ResolveKernelApi(PVOID NtoskrnlBase, const char *FuncName)
{
    uint8_t *Base = (uint8_t *)NtoskrnlBase;

    /* parse DOS header */
    if (*(uint16_t *)Base != 0x5A4D)
        return NULL;

    uint32_t PeOffset = *(uint32_t *)(Base + 0x3C);
    uint8_t *PeHdr = Base + PeOffset;

    if (*(uint32_t *)PeHdr != 0x00004550)
        return NULL;

    /* PE optional header starts after file header (24 bytes) */
    uint8_t *OptHdr = PeHdr + 4 + 20;

    /* export directory RVA is at offset 112 in optional header (64-bit) */
    uint32_t ExportRva  = *(uint32_t *)(OptHdr + 112);
    uint32_t ExportSize = *(uint32_t *)(OptHdr + 116);

    if (!ExportRva)
        return NULL;

    uint8_t *ExportDir = Base + ExportRva;

    uint32_t NumFunctions   = *(uint32_t *)(ExportDir + 20);
    uint32_t NumNames       = *(uint32_t *)(ExportDir + 24);
    uint32_t AddrOfFuncs    = *(uint32_t *)(ExportDir + 28);
    uint32_t AddrOfNames    = *(uint32_t *)(ExportDir + 32);
    uint32_t AddrOfOrdinals = *(uint32_t *)(ExportDir + 36);

    uint32_t *NameTable     = (uint32_t *)(Base + AddrOfNames);
    uint16_t *OrdinalTable  = (uint16_t *)(Base + AddrOfOrdinals);
    uint32_t *FuncTable     = (uint32_t *)(Base + AddrOfFuncs);

    for (uint32_t i = 0; i < NumNames; i++) {
        char *Name = (char *)(Base + NameTable[i]);

        /* manual strcmp */
        const char *a = FuncName;
        const char *b = Name;
        int match = 1;

        while (*a && *b) {
            if (*a != *b) { match = 0; break; }
            a++; b++;
        }
        if (*a || *b) match = 0;

        if (match) {
            uint16_t Ordinal = OrdinalTable[i];
            uint32_t FuncRva = FuncTable[Ordinal];

            /* check for forwarded export */
            if (FuncRva >= ExportRva && FuncRva < ExportRva + ExportSize)
                return NULL;

            return Base + FuncRva;
        }
    }

    return NULL;
}

/*
 * Patch g_CiEnabled to disable Driver Signature Enforcement.
 * We search for SepInitializeCodeIntegrity which sets this flag.
 */
static void PatchDseFlag(PVOID NtoskrnlBase, UINTN NtoskrnlSize)
{
    uint8_t *Base = (uint8_t *)NtoskrnlBase;

    /*
     * Pattern: C6 05 XX XX XX XX 01  (mov byte [rip+disp32], 1)
     * This sets g_CiEnabled = TRUE
     * We change it to: C6 05 XX XX XX XX 00  (g_CiEnabled = FALSE)
     */
    for (UINTN i = 0; i < NtoskrnlSize - 7; i++) {
        if (Base[i]   == 0xC6 &&
            Base[i+1] == 0x05 &&
            Base[i+6] == 0x01) {
            /* verify the target address is in .data section */
            int32_t Disp = *(int32_t *)(Base + i + 2);
            uint8_t *Target = Base + i + 7 + Disp;

            /* basic bounds check */
            if (Target >= Base && Target < Base + NtoskrnlSize) {
                Base[i+6] = 0x00;
                break;
            }
        }
    }
}

/*
 * Install a syscall hook for kernel<->usermode communication.
 *
 * Strategy: hook an unused/rare syscall number by modifying
 * the SSDT (System Service Descriptor Table). We pick a syscall
 * that's documented but never called (e.g., NtQueryDebugFilterState
 * or NtAddBootEntry).
 *
 * The hooked syscall checks for our magic value in the first
 * parameter. If present, it processes the COMM_REQUEST. Otherwise,
 * it calls the original handler.
 */
static PVOID sOrigSyscallHandler = NULL;

static NTSTATUS HookedSyscallHandler(COMM_REQUEST *Request)
{
    /* check if this is our communication request */
    if (!Request)
        goto CallOriginal;

    /* probe the magic value carefully */
    /* in kernel mode, we can access any address, but we should
     * verify the pointer is in usermode range */
    if ((uint64_t)Request >= 0x7FFFFFFFFFFF)
        goto CallOriginal;

    if (Request->Magic != COMM_MAGIC)
        goto CallOriginal;

    PAYLOAD_CONTEXT *Ctx = gPayloadCtx;
    if (!Ctx || !Ctx->Initialized)
        goto CallOriginal;

    Request->Status = STATUS_SUCCESS;

    switch (Request->Command) {
    case COMM_CMD_READ: {
        /* read memory from target process */
        PVOID TargetProcess = NULL;
        NTSTATUS Status = Ctx->Api.PsLookupProcessByProcessId(
            Request->ProcessId, &TargetProcess
        );
        if (!NT_SUCCESS(Status)) {
            Request->Status = Status;
            break;
        }

        SIZE_T BytesCopied = 0;
        Status = Ctx->Api.MmCopyVirtualMemory(
            TargetProcess,              /* source process */
            Request->Address,           /* source address */
            Ctx->Api.PsGetCurrentProcess(), /* target = our process */
            Request->Buffer,            /* target buffer */
            Request->Size,
            0,                          /* KernelMode */
            &BytesCopied
        );

        Ctx->Api.ObfDereferenceObject(TargetProcess);
        Request->Status = Status;
        Request->Result = BytesCopied;
        break;
    }

    case COMM_CMD_WRITE: {
        /* write memory to target process */
        PVOID TargetProcess = NULL;
        NTSTATUS Status = Ctx->Api.PsLookupProcessByProcessId(
            Request->ProcessId, &TargetProcess
        );
        if (!NT_SUCCESS(Status)) {
            Request->Status = Status;
            break;
        }

        SIZE_T BytesCopied = 0;
        Status = Ctx->Api.MmCopyVirtualMemory(
            Ctx->Api.PsGetCurrentProcess(), /* source = our process */
            Request->Buffer,                /* source buffer */
            TargetProcess,                  /* target process */
            Request->Address,               /* target address */
            Request->Size,
            0,
            &BytesCopied
        );

        Ctx->Api.ObfDereferenceObject(TargetProcess);
        Request->Status = Status;
        Request->Result = BytesCopied;
        break;
    }

    case COMM_CMD_BASE: {
        /* get module base address of target process */
        PVOID TargetProcess = NULL;
        NTSTATUS Status = Ctx->Api.PsLookupProcessByProcessId(
            Request->ProcessId, &TargetProcess
        );
        if (!NT_SUCCESS(Status)) {
            Request->Status = Status;
            break;
        }

        /* read PEB.ImageBaseAddress from target process */
        uint8_t *Eprocess = (uint8_t *)TargetProcess;
        PVOID Peb = *(PVOID *)(Eprocess + EPROCESS_PEB_OFFSET);

        if (Peb) {
            /* PEB.ImageBaseAddress is at offset 0x10 */
            SIZE_T BytesRead = 0;
            PVOID ImageBase = NULL;

            Ctx->Api.MmCopyVirtualMemory(
                TargetProcess,
                (uint8_t *)Peb + 0x10,
                Ctx->Api.PsGetCurrentProcess(),
                &ImageBase,
                sizeof(PVOID),
                0,
                &BytesRead
            );

            Request->Result = (ULONG_PTR)ImageBase;
        }

        Ctx->Api.ObfDereferenceObject(TargetProcess);
        break;
    }

    case COMM_CMD_STATUS: {
        /* return debug status to usermode */
        if (!Request->Buffer || Request->Size < sizeof(PHANTOM_STATUS))  {
            Request->Status = STATUS_NOT_FOUND;
            break;
        }

        /* sync live counters */
        Ctx->DbgStatus.SyscallCount = Ctx->DbgStatus.SyscallCount; /* already tracked */
        if (Ctx->CheatInjected)
            Ctx->DbgStatus.Stages |= STAGE_CHEAT_INJECTED;
        if (Ctx->TargetPid)
            Ctx->DbgStatus.Stages |= STAGE_TARGET_FOUND;

        DbgSyncStatus(Ctx);

        /* copy status struct to usermode buffer */
        SIZE_T BytesCopied = 0;
        NTSTATUS st = Ctx->Api.MmCopyVirtualMemory(
            Ctx->Api.PsGetCurrentProcess(),
            &Ctx->DbgStatus,
            Ctx->Api.PsGetCurrentProcess(),
            Request->Buffer,
            sizeof(PHANTOM_STATUS),
            0,
            &BytesCopied
        );
        Request->Status = st;
        Request->Result = BytesCopied;
        break;
    }

    default:
        Request->Status = STATUS_NOT_FOUND;
        break;
    }

    return Request->Status;

CallOriginal:
    /* count syscalls for debug display */
    if (gPayloadCtx)
        gPayloadCtx->DbgStatus.SyscallCount++;

    /* piggyback on normal syscalls to scan for FiveM */
    if (gPayloadCtx && gPayloadCtx->Initialized) {
        int injected = MonitorCheckForTarget(gPayloadCtx);
        gPayloadCtx->DbgStatus.ScanCount++;

        if (injected) {
            gPayloadCtx->DbgStatus.Stages |= STAGE_TARGET_FOUND | STAGE_CHEAT_INJECTED;
            DbgLog(gPayloadCtx, DBG_OK, "INJECT: cheat DLL mapped into FiveM!");
            DbgSyncStatus(gPayloadCtx);
        }
    }

    if (sOrigSyscallHandler) {
        typedef NTSTATUS (*SYSCALL_FUNC)(PVOID);
        return ((SYSCALL_FUNC)sOrigSyscallHandler)(Request);
    }
    return STATUS_NOT_FOUND;
}

/*
 * SSDT hook installation.
 *
 * The SSDT (KeServiceDescriptorTable) in ntoskrnl.exe contains
 * relative offsets to syscall handlers. We modify one entry
 * to point to our handler.
 *
 * Target syscall: NtQueryDebugFilterState (index varies by build)
 * This syscall is rarely called and safe to hook.
 */
static void InstallSyscallHook(PAYLOAD_CONTEXT *Ctx)
{
    uint8_t *Base = (uint8_t *)Ctx->NtoskrnlBase;

    /*
     * Locate KeServiceDescriptorTable.
     * Pattern: lea r??, [rip + disp32] followed by specific context
     * that references the SSDT.
     */

    /* find KiSystemServiceRepeat which references the SSDT */
    /* pattern: 4C 8D 15 XX XX XX XX (lea r10, [rip+disp32]) */
    uint8_t *SsdtRef = NULL;

    for (UINTN i = 0; i < Ctx->NtoskrnlSize - 7; i++) {
        if (Base[i]   == 0x4C &&
            Base[i+1] == 0x8D &&
            Base[i+2] == 0x15) {
            /* verify context: should be near syscall dispatch */
            /* check for 'cmp eax, [r10+10h]' nearby */
            for (UINTN j = 0; j < 0x20; j++) {
                if (Base[i+7+j] == 0x41 &&
                    Base[i+8+j] == 0x3B &&
                    Base[i+9+j] == 0x42) {
                    SsdtRef = Base + i;
                    goto FoundSsdt;
                }
            }
        }
    }

FoundSsdt:
    if (!SsdtRef)
        return;

    /* resolve the SSDT address */
    int32_t SsdtDisp = *(int32_t *)(SsdtRef + 3);
    uint8_t *Ssdt = SsdtRef + 7 + SsdtDisp;
    Ctx->SsdtBase = Ssdt;

    /* SSDT structure:
     *   uint32_t *ServiceTable;    // +0x00
     *   uint32_t *CounterTable;    // +0x08
     *   uint64_t  NumberOfServices; // +0x10
     *   uint8_t  *ArgumentTable;   // +0x18
     */
    uint32_t *ServiceTable = *(uint32_t **)Ssdt;
    uint64_t  NumServices  = *(uint64_t *)(Ssdt + 0x10);

    if (!ServiceTable || NumServices < 0x100)
        return;

    /* NtQueryDebugFilterState is typically syscall 0x117 (Win10/11) */
    /* the SSDT stores entries as: (function_rva - table_base) << 4 | arg_count */
    uint32_t TargetIndex = 0x117;
    if (TargetIndex >= NumServices)
        return;

    /* save original handler */
    int32_t OrigOffset = (int32_t)(ServiceTable[TargetIndex] >> 4);
    sOrigSyscallHandler = (uint8_t *)ServiceTable + OrigOffset;
    Ctx->SyscallOriginal = sOrigSyscallHandler;

    /*
     * SSDT entries are 32-bit relative offsets (shifted by 4), giving
     * a range of ±128MB from the service table. Our handler lives in
     * EFI Runtime memory which can be GBs away. We solve this by
     * writing a 12-byte absolute jmp stub in a code cave inside
     * ntoskrnl's .text section, then pointing the SSDT at the stub.
     */

    /* find .text section in ntoskrnl PE */
    uint32_t PeOff = *(uint32_t *)(Base + 0x3C);
    uint8_t *PeHdr = Base + PeOff;
    uint16_t NumSec = *(uint16_t *)(PeHdr + 4 + 2);
    uint16_t OptSz  = *(uint16_t *)(PeHdr + 4 + 16);
    uint8_t *SecArr = PeHdr + 4 + 20 + OptSz;

    uint8_t *Cave = NULL;
    for (uint16_t s = 0; s < NumSec; s++) {
        uint8_t *Sh = SecArr + s * 40;
        if (Sh[0]=='.' && Sh[1]=='t' && Sh[2]=='e' && Sh[3]=='x' && Sh[4]=='t') {
            uint32_t SecVA   = *(uint32_t *)(Sh + 12);
            uint32_t SecVSz  = *(uint32_t *)(Sh + 8);
            uint8_t *SecEnd  = Base + SecVA + SecVSz;

            int RunLen = 0;
            for (int off = 1; off < 0x2000 && off < (int)SecVSz; off++) {
                uint8_t b = SecEnd[-off];
                if (b == 0xCC || b == 0x00) {
                    RunLen++;
                    if (RunLen >= 14) {
                        Cave = SecEnd - off;
                        break;
                    }
                } else {
                    RunLen = 0;
                }
            }
            break;
        }
    }

    if (!Cave)
        return;

    Ctx->CodeCaveAddr = Cave;

    /* disable write protection */
    uint64_t Cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(Cr0));
    __asm__ volatile("mov %0, %%cr0" :: "r"(Cr0 & ~CR0_WP));

    /* write absolute jmp stub in the code cave: mov rax, imm64; jmp rax */
    Cave[0] = 0x48; Cave[1] = 0xB8;
    *(uint64_t *)(Cave + 2) = (uint64_t)HookedSyscallHandler;
    Cave[10] = 0xFF; Cave[11] = 0xE0;

    /* point SSDT entry at the cave stub (which is within ±128MB of SSDT) */
    int32_t NewOffset = (int32_t)((uint8_t *)Cave - (uint8_t *)ServiceTable);
    uint8_t ArgCount = ServiceTable[TargetIndex] & 0xF;
    ServiceTable[TargetIndex] = ((uint32_t)NewOffset << 4) | ArgCount;

    /* restore write protection */
    __asm__ volatile("mov %0, %%cr0" :: "r"(Cr0));
}

/*
 * Hide our payload's physical pages from the OS memory manager.
 *
 * Strategy:
 *   1. Locate MmPfnDatabase
 *   2. Find our physical pages in the PFN database
 *   3. Mark them as "Bad" — the OS will never touch bad pages
 *   4. This makes our memory invisible to MmCopyVirtualMemory scans,
 *      !poolused, driver verifier, and SS tools that scan physical RAM
 */
static void HidePayloadMemory(PAYLOAD_CONTEXT *Ctx)
{
    uint8_t *Base = (uint8_t *)Ctx->NtoskrnlBase;

    /* locate MmPfnDatabase */
    /* pattern: 48 8B 05 XX XX XX XX 48 89 43 18 (mov rax, [rip+disp]) */
    uint8_t *PfnRef = NULL;

    for (UINTN i = 0; i < Ctx->NtoskrnlSize - 11; i++) {
        if (Base[i]   == 0x48 &&
            Base[i+1] == 0x8B &&
            (Base[i+2] == 0x05 || Base[i+2] == 0x0D)) {
            /* check context bytes */
            if (Base[i+7] == 0x48 && Base[i+8] == 0x89) {
                PfnRef = Base + i;
                break;
            }
        }
    }

    if (!PfnRef)
        return;

    int32_t PfnDisp = *(int32_t *)(PfnRef + 3);
    uint64_t *PfnDbPtr = (uint64_t *)(PfnRef + 7 + PfnDisp);
    uint8_t *PfnDatabase = (uint8_t *)*PfnDbPtr;

    if (!PfnDatabase)
        return;

    Ctx->PfnDatabase = PfnDatabase;

    /* calculate the physical page frame numbers of our payload */
    /* our payload is at a known physical address from the EFI allocation */
    /* the PayloadBase field in our header contains the physical address */

    uint64_t PhysBase = (uint64_t)Ctx->PayloadBase;
    UINTN NumPages = (Ctx->PayloadSize + PAGE_SIZE - 1) / PAGE_SIZE;

    /* disable write protection */
    uint64_t Cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(Cr0));
    __asm__ volatile("mov %0, %%cr0" :: "r"(Cr0 & ~CR0_WP));

    for (UINTN p = 0; p < NumPages; p++) {
        uint64_t Pfn = (PhysBase / PAGE_SIZE) + p;
        uint8_t *PfnEntry = PfnDatabase + (Pfn * MMPFN_SIZE);

        /* set page state to Bad (0x06) */
        /* this prevents the memory manager from ever reclaiming or scanning it */
        uint8_t *Flags = PfnEntry + PFN_FLAGS_OFFSET;

        /* clear the state bits (bits 0-2) and set to Bad */
        *Flags = (*Flags & ~0x07) | PFN_STATE_BAD;

        /* zero the reference count to prevent any outstanding refs */
        *(uint16_t *)(PfnEntry + PFN_REFERENCECOUNT_OFFSET) = 0;
    }

    /* restore write protection */
    __asm__ volatile("mov %0, %%cr0" :: "r"(Cr0));

    Ctx->Hidden = 1;
}
