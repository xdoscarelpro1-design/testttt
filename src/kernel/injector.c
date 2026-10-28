/*
 * PHANTOM Kernel Payload — Ghost APC Injector (Ring-0 → Ring-3)
 *
 * Maps a PE DLL into target usermode process with zero forensic signature.
 * No RWX memory, no thread creation, no shellcode, no PE artifacts.
 *
 * Injection method:
 *   1. Attach to target process context
 *   2. Allocate RW region (not RWX — defeats VirtualQueryEx scanners)
 *   3. Copy PE sections, process relocations, resolve imports
 *   4. Destroy all PE metadata (headers, import names, reloc data, exports)
 *   5. Apply per-section permissions (.text→ER, .rdata→R, .data→RW)
 *   6. Queue user-mode APC to existing thread (no new thread creation)
 *   7. APC fires entry point directly — no shellcode stub needed
 *   8. Detach — the DLL is alive, invisible to every known SS checker
 *
 * What this defeats:
 *   - VirtualQueryEx RWX scan (g_DriverCheck)   → split RW/ER permissions
 *   - Thread enumeration                        → no ZwCreateThreadEx
 *   - Memory string scanner (Generic-Cheat-Inj) → metadata fully wiped
 *   - YARA PE signature rules                   → headers zeroed
 *   - Module list scan                          → not in PEB.Ldr
 *
 * PIC constraints: no CRT, no .rdata strings, manual everything.
 */

#include "ntdefs.h"

/* ─── PE structures (local defs, no windows.h) ─── */

#pragma pack(push, 1)

typedef struct {
    uint16_t e_magic;
    uint8_t  _pad[58];
    uint32_t e_lfanew;
} INJ_DOS_HEADER;

typedef struct {
    uint32_t VirtualAddress;
    uint32_t Size;
} INJ_DATA_DIR;

typedef struct {
    uint16_t Magic;
    uint8_t  _skip1[14];
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint8_t  _skip2[16];
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint8_t  _skip3[8];
    uint16_t DllCharacteristics;
    uint8_t  _skip4[40];
    uint32_t NumberOfRvaAndSizes;
} INJ_OPT_HEADER64;

typedef struct {
    uint32_t Signature;
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint8_t  _skip[12];
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} INJ_FILE_HEADER;

typedef struct {
    char     Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint8_t  _skip[12];
    uint32_t Characteristics;
} INJ_SECTION_HEADER;

typedef struct {
    uint32_t VirtualAddress;
    uint32_t SizeOfBlock;
} INJ_BASE_RELOC;

typedef struct {
    union {
        uint32_t Characteristics;
        uint32_t OriginalFirstThunk;
    };
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;
    uint32_t FirstThunk;
} INJ_IMPORT_DESC;

typedef struct {
    uint32_t ExportFlags;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t NameRva;
    uint32_t OrdinalBase;
    uint32_t NumberOfFunctions;
    uint32_t NumberOfNames;
    uint32_t AddressOfFunctions;
    uint32_t AddressOfNames;
    uint32_t AddressOfNameOrdinals;
} INJ_EXPORT_DIR;

#pragma pack(pop)

/* PE data directory indices */
#define DIR_EXPORT    0
#define DIR_IMPORT    1
#define DIR_BASERELOC 5
#define DIR_DEBUG     6

/* ─── helpers ─── */

static int inj_streq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return (*a == *b);
}

static int inj_streqi(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return (*a == *b);
}

static void inj_memcpy(void *dst, const void *src, UINTN len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (len--) *d++ = *s++;
}

static void inj_memset(void *dst, uint8_t val, UINTN len)
{
    uint8_t *d = (uint8_t *)dst;
    while (len--) *d++ = val;
}

/*
 * Resolve an export from a usermode module by walking its export table.
 * ModuleBase is a virtual address in the target process — we're attached.
 */
static PVOID InjResolveExport(uint8_t *ModuleBase, const char *FuncName)
{
    if (*(uint16_t *)ModuleBase != 0x5A4D)
        return NULL;

    uint32_t PeOff = *(uint32_t *)(ModuleBase + 0x3C);
    uint8_t *Pe = ModuleBase + PeOff;

    if (*(uint32_t *)Pe != 0x00004550)
        return NULL;

    INJ_FILE_HEADER *Fh = (INJ_FILE_HEADER *)Pe;
    INJ_DATA_DIR *Dirs = (INJ_DATA_DIR *)((uint8_t *)(Pe + 4 + 20) + 112);

    uint32_t ExpRva  = Dirs[DIR_EXPORT].VirtualAddress;
    uint32_t ExpSize = Dirs[DIR_EXPORT].Size;
    if (!ExpRva) return NULL;

    INJ_EXPORT_DIR *Exp = (INJ_EXPORT_DIR *)(ModuleBase + ExpRva);

    uint32_t *Names    = (uint32_t *)(ModuleBase + Exp->AddressOfNames);
    uint16_t *Ordinals = (uint16_t *)(ModuleBase + Exp->AddressOfNameOrdinals);
    uint32_t *Funcs    = (uint32_t *)(ModuleBase + Exp->AddressOfFunctions);

    for (uint32_t i = 0; i < Exp->NumberOfNames; i++) {
        char *Name = (char *)(ModuleBase + Names[i]);
        if (inj_streq(FuncName, Name)) {
            uint32_t Rva = Funcs[Ordinals[i]];
            if (Rva >= ExpRva && Rva < ExpRva + ExpSize)
                return NULL;
            return ModuleBase + Rva;
        }
    }
    return NULL;
}

/*
 * Find a loaded module in the target process by walking PEB→Ldr.
 * Must be called while attached to the target process context.
 */
static PVOID InjFindModule(PVOID Peb, const char *ModuleName)
{
    if (!Peb) return NULL;

    PEB_LDR_DATA_UM *Ldr = *(PEB_LDR_DATA_UM **)((uint8_t *)Peb + 0x18);
    if (!Ldr) return NULL;

    LIST_ENTRY *Head = &Ldr->InLoadOrderModuleList;
    LIST_ENTRY *Cur  = Head->Flink;

    for (int safety = 0; safety < 256 && Cur != Head; safety++) {
        LDR_DATA_TABLE_ENTRY_UM *Entry = (LDR_DATA_TABLE_ENTRY_UM *)Cur;

        if (Entry->BaseDllName.Buffer && Entry->BaseDllName.Length > 0) {
            WCHAR *WName = Entry->BaseDllName.Buffer;
            USHORT Len   = Entry->BaseDllName.Length / sizeof(WCHAR);

            const char *n = ModuleName;
            int match = 1;
            for (USHORT j = 0; j < Len && *n; j++, n++) {
                WCHAR wc = WName[j];
                char  nc = *n;
                if (wc >= 'A' && wc <= 'Z') wc += 32;
                if (nc >= 'A' && nc <= 'Z') nc += 32;
                if ((char)wc != nc) { match = 0; break; }
            }
            if (match && *n == '\0')
                return Entry->DllBase;
        }

        Cur = Cur->Flink;
    }
    return NULL;
}

/*
 * Process base relocations for a manually mapped PE.
 * Delta = actual_base - preferred_base.
 */
static void InjProcessRelocs(uint8_t *MappedBase, uint8_t *RawPe)
{
    uint32_t PeOff = *(uint32_t *)(RawPe + 0x3C);
    INJ_OPT_HEADER64 *Oh = (INJ_OPT_HEADER64 *)(RawPe + PeOff + 4 + 20);
    INJ_DATA_DIR *Dirs = (INJ_DATA_DIR *)((uint8_t *)(RawPe + PeOff + 4 + 20) + 112);

    int64_t Delta = (int64_t)((uint64_t)MappedBase - Oh->ImageBase);
    if (Delta == 0) return;

    uint32_t RelocRva  = Dirs[DIR_BASERELOC].VirtualAddress;
    uint32_t RelocSize = Dirs[DIR_BASERELOC].Size;
    if (!RelocRva || !RelocSize) return;

    uint8_t *RelocBase = MappedBase + RelocRva;
    uint8_t *RelocEnd  = RelocBase + RelocSize;

    while (RelocBase < RelocEnd) {
        INJ_BASE_RELOC *Block = (INJ_BASE_RELOC *)RelocBase;
        if (!Block->SizeOfBlock) break;

        uint32_t NumEntries = (Block->SizeOfBlock - 8) / 2;
        uint16_t *Entries = (uint16_t *)(RelocBase + 8);

        for (uint32_t i = 0; i < NumEntries; i++) {
            uint16_t Type   = Entries[i] >> 12;
            uint16_t Offset = Entries[i] & 0xFFF;
            uint8_t *Target = MappedBase + Block->VirtualAddress + Offset;

            if (Type == IMAGE_REL_BASED_DIR64)
                *(uint64_t *)Target += Delta;
            else if (Type == IMAGE_REL_BASED_HIGHLOW)
                *(uint32_t *)Target += (uint32_t)Delta;
        }

        RelocBase += Block->SizeOfBlock;
    }
}

/*
 * Resolve all imports for the mapped PE.
 */
static int InjResolveImports(uint8_t *MappedBase, uint8_t *RawPe, PVOID Peb)
{
    uint32_t PeOff = *(uint32_t *)(RawPe + 0x3C);
    INJ_DATA_DIR *Dirs = (INJ_DATA_DIR *)((uint8_t *)(RawPe + PeOff + 4 + 20) + 112);

    uint32_t ImpRva = Dirs[DIR_IMPORT].VirtualAddress;
    if (!ImpRva) return 1;

    INJ_IMPORT_DESC *Imp = (INJ_IMPORT_DESC *)(MappedBase + ImpRva);

    while (Imp->Name) {
        char *DllName = (char *)(MappedBase + Imp->Name);

        PVOID ModBase = InjFindModule(Peb, DllName);
        if (!ModBase)
            return 0;

        uint64_t *Thunk = (uint64_t *)(MappedBase + Imp->FirstThunk);
        uint64_t *OrigThunk = Imp->OriginalFirstThunk ?
            (uint64_t *)(MappedBase + Imp->OriginalFirstThunk) : Thunk;

        while (*OrigThunk) {
            if (*OrigThunk & (1ULL << 63)) {
                uint16_t Ordinal = (uint16_t)(*OrigThunk & 0xFFFF);
                uint8_t *Mod = (uint8_t *)ModBase;
                uint32_t ModPeOff = *(uint32_t *)(Mod + 0x3C);
                INJ_DATA_DIR *ModDirs = (INJ_DATA_DIR *)((Mod + ModPeOff + 4 + 20) + 112);
                uint32_t ModExpRva = ModDirs[DIR_EXPORT].VirtualAddress;
                if (ModExpRva) {
                    INJ_EXPORT_DIR *ModExp = (INJ_EXPORT_DIR *)(Mod + ModExpRva);
                    uint32_t *ModFuncs = (uint32_t *)(Mod + ModExp->AddressOfFunctions);
                    uint16_t Idx = Ordinal - (uint16_t)ModExp->OrdinalBase;
                    if (Idx < ModExp->NumberOfFunctions)
                        *Thunk = (uint64_t)(Mod + ModFuncs[Idx]);
                }
            } else {
                char *FuncName = (char *)(MappedBase + (uint32_t)*OrigThunk + 2);
                PVOID Addr = InjResolveExport((uint8_t *)ModBase, FuncName);
                if (Addr)
                    *Thunk = (uint64_t)Addr;
            }
            Thunk++;
            OrigThunk++;
        }

        Imp++;
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════
 *  NEW: Aggressive PE metadata destruction
 *
 *  After imports are resolved and relocations processed, every PE
 *  artifact is dead weight that only serves forensic scanners.
 *  We nuke it all: headers, import names, hint/name entries,
 *  relocation data, export tables, debug directory, PDB paths.
 *
 *  The IAT (FirstThunk resolved addresses) is preserved — that's
 *  what the running code actually calls through.
 * ═══════════════════════════════════════════════════════════════ */
static void InjWipeMetadata(uint8_t *Mapped, uint8_t *RawPe, uint32_t HeaderSize)
{
    /* _erasing the birth certificate while the patient is still on the table_ */
    uint32_t PeOff = *(uint32_t *)(RawPe + 0x3C);
    INJ_DATA_DIR *Dirs = (INJ_DATA_DIR *)(RawPe + PeOff + 4 + 20 + 112);

    /* ── wipe import DLL names, hint/name entries, and ILT ── */
    uint32_t ImpRva = Dirs[DIR_IMPORT].VirtualAddress;
    if (ImpRva) {
        INJ_IMPORT_DESC *Imp = (INJ_IMPORT_DESC *)(Mapped + ImpRva);
        while (Imp->Name) {
            /* zero the DLL name string (e.g. "kernel32.dll") */
            char *DllStr = (char *)(Mapped + Imp->Name);
            while (*DllStr) { *DllStr = 0; DllStr++; }

            /* zero the ILT (OriginalFirstThunk) hint/name structures */
            if (Imp->OriginalFirstThunk) {
                uint64_t *Ilt = (uint64_t *)(Mapped + Imp->OriginalFirstThunk);
                while (*Ilt) {
                    if (!(*Ilt & (1ULL << 63))) {
                        /* import by name: zero hint (2 bytes) + function name string */
                        uint8_t *HintName = Mapped + (uint32_t)*Ilt;
                        *(uint16_t *)HintName = 0;
                        char *Fn = (char *)(HintName + 2);
                        while (*Fn) { *Fn = 0; Fn++; }
                    }
                    *Ilt = 0;
                    Ilt++;
                }
            }

            /* peek at next before destroying current descriptor */
            uint32_t NextName = (Imp + 1)->Name;
            inj_memset(Imp, 0, sizeof(INJ_IMPORT_DESC));
            Imp++;
            if (!NextName) break;
        }
    }

    /* ── wipe relocation data (entire .reloc contents) ── */
    uint32_t RelocRva = Dirs[DIR_BASERELOC].VirtualAddress;
    uint32_t RelocSz  = Dirs[DIR_BASERELOC].Size;
    if (RelocRva && RelocSz)
        inj_memset(Mapped + RelocRva, 0, RelocSz);

    /* ── wipe export directory (function names, ordinals, name RVAs) ── */
    uint32_t ExpRva = Dirs[DIR_EXPORT].VirtualAddress;
    uint32_t ExpSz  = Dirs[DIR_EXPORT].Size;
    if (ExpRva && ExpSz)
        inj_memset(Mapped + ExpRva, 0, ExpSz);

    /* ── wipe debug directory (PDB paths, build IDs) ── */
    uint32_t DbgRva = Dirs[DIR_DEBUG].VirtualAddress;
    uint32_t DbgSz  = Dirs[DIR_DEBUG].Size;
    if (DbgRva && DbgSz)
        inj_memset(Mapped + DbgRva, 0, DbgSz);

    /* ── wipe PE headers last (DOS + PE + Optional + Section table) ── */
    inj_memset(Mapped, 0, HeaderSize);
}

/* ═══════════════════════════════════════════════════════════════
 *  NEW: Per-section permission enforcement
 *
 *  Instead of one giant RWX blob, each section gets the minimum
 *  permissions it needs. This makes us look like a legitimate
 *  loaded DLL to VirtualQueryEx scanners:
 *    .text  → PAGE_EXECUTE_READ  (code, no write)
 *    .rdata → PAGE_READONLY      (const data)
 *    .data  → PAGE_READWRITE     (mutable data)
 *    .reloc → PAGE_READONLY      (zeroed, read-only tomb)
 *
 *  The g_DriverCheck VirtualQueryEx scan specifically hunts for
 *  RWX regions. Split permissions dodge it completely.
 * ═══════════════════════════════════════════════════════════════ */
static void InjApplySectionPerms(PAYLOAD_CONTEXT *Ctx, uint8_t *Mapped,
                                  uint8_t *RawPe)
{
    uint32_t PeOff = *(uint32_t *)(RawPe + 0x3C);
    INJ_FILE_HEADER *Fh = (INJ_FILE_HEADER *)(RawPe + PeOff);
    INJ_SECTION_HEADER *Sec = (INJ_SECTION_HEADER *)(
        RawPe + PeOff + 4 + 20 + Fh->SizeOfOptionalHeader
    );

    for (uint16_t i = 0; i < Fh->NumberOfSections; i++) {
        if (Sec[i].VirtualSize == 0) continue;

        uint32_t Chars = Sec[i].Characteristics;
        ULONG Prot;

        if (Chars & IMAGE_SCN_MEM_EXECUTE)
            Prot = PAGE_EXECUTE_READ;
        else if (Chars & IMAGE_SCN_MEM_WRITE)
            Prot = PAGE_READWRITE;
        else
            Prot = PAGE_READONLY;

        PVOID SecBase = Mapped + Sec[i].VirtualAddress;
        SIZE_T SecSize = (Sec[i].VirtualSize + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        if (SecSize == 0) SecSize = PAGE_SIZE;
        ULONG OldProt;

        Ctx->Api.ZwProtectVirtualMemory(
            (HANDLE)-1, &SecBase, &SecSize, Prot, &OldProt
        );
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  Map a PE DLL blob into the target process from kernel.
 *
 *  Returns the mapped base address on success, NULL on failure.
 *  The caller must be at PASSIVE_LEVEL.
 *
 *  Key difference from old method: allocates as RW (not RWX),
 *  then hardens permissions per-section after all fixups.
 * ═══════════════════════════════════════════════════════════════ */
PVOID InjectorMapDll(PAYLOAD_CONTEXT *Ctx, PVOID TargetProcess,
                     uint8_t *DllBlob, UINTN DllSize)
{
    /* _the stack smells of fresh allocations and zero-day confidence_ */
    if (!DllBlob || DllSize < 0x100) return NULL;

    /* validate PE */
    INJ_DOS_HEADER *Dos = (INJ_DOS_HEADER *)DllBlob;
    if (Dos->e_magic != 0x5A4D) return NULL;

    uint32_t PeOff = Dos->e_lfanew;
    INJ_FILE_HEADER *Fh = (INJ_FILE_HEADER *)(DllBlob + PeOff);
    if (Fh->Signature != 0x00004550) return NULL;

    INJ_OPT_HEADER64 *Oh = (INJ_OPT_HEADER64 *)(DllBlob + PeOff + 4 + 20);
    uint32_t ImageSize  = Oh->SizeOfImage;
    uint32_t HeaderSize = Oh->SizeOfHeaders;

    /* attach to target process */
    KAPC_STATE ApcState;
    inj_memset(&ApcState, 0, sizeof(ApcState));
    Ctx->Api.KeStackAttachProcess(TargetProcess, &ApcState);

    /* allocate as PAGE_READWRITE — no execute permission yet */
    PVOID AllocBase = NULL;
    SIZE_T AllocSize = ImageSize;
    NTSTATUS Status = Ctx->Api.ZwAllocateVirtualMemory(
        (HANDLE)-1, &AllocBase, 0, &AllocSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE
    );

    if (!NT_SUCCESS(Status)) {
        Ctx->Api.KeUnstackDetachProcess(&ApcState);
        return NULL;
    }

    uint8_t *Mapped = (uint8_t *)AllocBase;

    /* copy PE headers */
    inj_memcpy(Mapped, DllBlob, HeaderSize);

    /* copy sections */
    INJ_SECTION_HEADER *Sec = (INJ_SECTION_HEADER *)(
        DllBlob + PeOff + 4 + 20 + Fh->SizeOfOptionalHeader
    );

    for (uint16_t i = 0; i < Fh->NumberOfSections; i++) {
        if (Sec[i].SizeOfRawData == 0) continue;
        inj_memcpy(
            Mapped + Sec[i].VirtualAddress,
            DllBlob + Sec[i].PointerToRawData,
            Sec[i].SizeOfRawData
        );
        if (Sec[i].VirtualSize > Sec[i].SizeOfRawData) {
            inj_memset(
                Mapped + Sec[i].VirtualAddress + Sec[i].SizeOfRawData,
                0,
                Sec[i].VirtualSize - Sec[i].SizeOfRawData
            );
        }
    }

    /* process relocations (needs write to .text — still RW) */
    InjProcessRelocs(Mapped, DllBlob);

    /* resolve imports from target's loaded modules */
    PVOID Peb = Ctx->Api.PsGetProcessPeb(TargetProcess);
    if (!InjResolveImports(Mapped, DllBlob, Peb)) {
        Ctx->Api.KeUnstackDetachProcess(&ApcState);
        return NULL;
    }

    /* destroy all PE metadata while pages are still writable */
    InjWipeMetadata(Mapped, DllBlob, HeaderSize);

    /* harden: set per-section permissions (.text→ER, .rdata→R, .data→RW) */
    InjApplySectionPerms(Ctx, Mapped, DllBlob);

    Ctx->Api.KeUnstackDetachProcess(&ApcState);
    return AllocBase;
}

/* ═══════════════════════════════════════════════════════════════
 *  Ghost APC execution — the core bypass
 *
 *  Instead of ZwCreateThreadEx (detectable by thread enumeration
 *  and thread-creation callbacks), we queue a user-mode APC to an
 *  existing FiveM thread. The APC fires the entry point directly
 *  when the thread next enters an alertable wait state.
 *
 *  No new thread. No shellcode page. No RWX trampoline.
 *  The entry point is called as the APC NormalRoutine:
 *    rcx = NormalContext    = MappedBase  (hModule for DllMain)
 *    rdx = SystemArgument1 = 1           (DLL_PROCESS_ATTACH)
 *    r8  = SystemArgument2 = NULL        (lpReserved)
 *
 *  This is argument-compatible with both DllMain and PhantomEntry.
 * ═══════════════════════════════════════════════════════════════ */

/* ExFreePool stash for the kernel routine callback */
static fn_ExFreePool sApcFreePool = NULL;

/*
 * APC kernel routine — fires at APC_LEVEL in kernel context when
 * the target thread transitions to user mode to deliver our APC.
 * Only job: free the KAPC allocation from NonPagedPool.
 */
static void __attribute__((used)) InjApcKernelRoutine(
    PVOID Apc, void **NormalRoutine, void **NormalContext,
    void **SystemArgument1, void **SystemArgument2)
{
    (void)NormalRoutine;
    (void)NormalContext;
    (void)SystemArgument1;
    (void)SystemArgument2;

    if (sApcFreePool && Apc)
        sApcFreePool(Apc);
}

/*
 * Execute the mapped DLL's entry point via APC injection.
 *
 * Finds an existing thread in the target process, queues a user-mode
 * APC with our entry point as the NormalRoutine. The APC fires at the
 * thread's next alertable wait — FiveM's message pump guarantees
 * delivery within milliseconds.
 */
NTSTATUS InjectorExecuteEntry(PAYLOAD_CONTEXT *Ctx, PVOID TargetProcess,
                              PVOID MappedBase, uint64_t EntryRva)
{
    /* _one APC and the ghost slips into the machine — no thread, no trace_ */

    /* stash ExFreePool for the APC kernel routine callback */
    sApcFreePool = Ctx->Api.ExFreePool;

    /* find a thread in the target process */
    PVOID Thread = Ctx->Api.PsGetNextProcessThread(TargetProcess, NULL);
    if (!Thread)
        return STATUS_NOT_FOUND;

    /* allocate KAPC from NonPagedPool — kernel memory, invisible to user-mode scanners */
    PVOID Apc = Ctx->Api.ExAllocatePool(POOL_NONPAGED, KAPC_ALLOC_SIZE);
    if (!Apc) {
        Ctx->Api.ObfDereferenceObject(Thread);
        return STATUS_NOT_FOUND;
    }

    inj_memset(Apc, 0, KAPC_ALLOC_SIZE);

    /* entry point in user-mode address space */
    PVOID EntryPoint = (uint8_t *)MappedBase + EntryRva;

    /* initialize user-mode APC:
     *   KernelRoutine  = InjApcKernelRoutine (frees KAPC on delivery)
     *   RundownRoutine = NULL (no cleanup on thread termination)
     *   NormalRoutine  = EntryPoint (our DllMain/PhantomEntry)
     *   ApcMode        = UserMode (execute in ring-3 context)
     *   NormalContext   = MappedBase (first argument: hModule)
     */
    Ctx->Api.KeInitializeApc(
        Apc,
        Thread,
        APC_ENV_ORIGINAL,
        (PVOID)InjApcKernelRoutine,
        NULL,
        EntryPoint,
        UMODE,
        MappedBase
    );

    /* queue the APC — thread executes our entry at next alertable wait
     *   SystemArgument1 = 1 (DLL_PROCESS_ATTACH for DllMain compatibility)
     *   SystemArgument2 = NULL (lpReserved)
     *   Increment       = 0 (no priority boost)
     */
    BOOLEAN Inserted = Ctx->Api.KeInsertQueueApc(
        Apc,
        (PVOID)1,
        NULL,
        0
    );

    /* release thread reference from PsGetNextProcessThread */
    Ctx->Api.ObfDereferenceObject(Thread);

    if (!Inserted) {
        Ctx->Api.ExFreePool(Apc);
        return STATUS_NOT_FOUND;
    }

    return STATUS_SUCCESS;
}
