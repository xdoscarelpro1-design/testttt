/*
 * Raw minimal EFI test — zero dependencies on gnu-efi.
 * Compiled directly with mingw to native PE32+ EFI application.
 * No CRT, no objcopy, no linker script. Pure.
 */

typedef unsigned long long UINT64;
typedef unsigned int       UINT32;
typedef unsigned short     UINT16;
typedef unsigned char      UINT8;
typedef UINT64             UINTN;
typedef UINTN              EFI_STATUS;
typedef void              *EFI_HANDLE;

/* ─── EFI Table Header ─── */
typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

/* ─── Simple Text Output Protocol ─── */
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_TEXT_RESET)(void *This, UINT8 ExtVerify);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_TEXT_OUTPUT)(void *This, UINT16 *Str);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_TEXT_TEST)(void *This, UINT16 *Str);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_TEXT_QUERY)(void *This, UINTN Mode, UINTN *Col, UINTN *Row);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_TEXT_SETMODE)(void *This, UINTN Mode);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_TEXT_SETATTR)(void *This, UINTN Attr);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_TEXT_CLEAR)(void *This);

typedef struct {
    EFI_TEXT_RESET    Reset;
    EFI_TEXT_OUTPUT   OutputString;
    EFI_TEXT_TEST     TestString;
    EFI_TEXT_QUERY    QueryMode;
    EFI_TEXT_SETMODE  SetMode;
    EFI_TEXT_SETATTR  SetAttribute;
    EFI_TEXT_CLEAR    ClearScreen;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

/* ─── Simple Text Input Protocol ─── */
typedef struct {
    UINT16 ScanCode;
    UINT16 UnicodeChar;
} EFI_INPUT_KEY;

typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_INPUT_RESET)(void *This, UINT8 ExtVerify);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_INPUT_READ)(void *This, EFI_INPUT_KEY *Key);

typedef struct {
    EFI_INPUT_RESET Reset;
    EFI_INPUT_READ  ReadKeyStroke;
    void           *WaitForKey;
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

/* ─── Boot Services (only what we need) ─── */
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_WAIT_FOR_EVENT)(UINTN Num, void **Evt, UINTN *Idx);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_STALL)(UINTN Microseconds);

typedef struct {
    EFI_TABLE_HEADER  Hdr;
    void             *RaiseTPL;            /* 0x18 */
    void             *RestoreTPL;          /* 0x20 */
    void             *AllocatePages;       /* 0x28 */
    void             *FreePages;           /* 0x30 */
    void             *GetMemoryMap;        /* 0x38 */
    void             *AllocatePool;        /* 0x40 */
    void             *FreePool;            /* 0x48 */
    void             *CreateEvent;         /* 0x50 */
    void             *SetTimer;            /* 0x58 */
    EFI_WAIT_FOR_EVENT WaitForEvent;       /* 0x60 */
    void             *SignalEvent;         /* 0x68 */
    void             *CloseEvent;          /* 0x70 */
    void             *CheckEvent;          /* 0x78 */
    void             *InstallProtocol;     /* 0x80 */
    void             *ReinstallProtocol;   /* 0x88 */
    void             *UninstallProtocol;   /* 0x90 */
    void             *HandleProtocol;      /* 0x98 */
    void             *Reserved;            /* 0xA0 */
    void             *RegisterProtocol;    /* 0xA8 */
    void             *LocateHandle;        /* 0xB0 */
    void             *LocateDevicePath;    /* 0xB8 */
    void             *InstallConfig;       /* 0xC0 */
    void             *LoadImage;           /* 0xC8 */
    void             *StartImage;          /* 0xD0 */
    void             *Exit;               /* 0xD8 */
    void             *UnloadImage;         /* 0xE0 */
    void             *ExitBootServices;    /* 0xE8 */
    void             *GetNextMono;         /* 0xF0 */
    EFI_STALL         Stall;              /* 0xF8 */
} EFI_BOOT_SERVICES;

/* ─── Runtime Services (only ResetSystem) ─── */
typedef void (__attribute__((ms_abi)) *EFI_RESET_SYSTEM)(UINT32 ResetType, EFI_STATUS Status, UINTN DataSize, void *Data);

typedef struct {
    EFI_TABLE_HEADER  Hdr;
    void             *GetTime;
    void             *SetTime;
    void             *GetWakeupTime;
    void             *SetWakeupTime;
    void             *SetVirtualAddressMap;
    void             *ConvertPointer;
    void             *GetVariable;
    void             *GetNextVariableName;
    void             *SetVariable;
    void             *GetNextHighMonotonicCount;
    EFI_RESET_SYSTEM  ResetSystem;
} EFI_RUNTIME_SERVICES;

/* ─── System Table ─── */
typedef struct {
    EFI_TABLE_HEADER                  Hdr;
    UINT16                           *FirmwareVendor;
    UINT32                            FirmwareRevision;
    UINT32                            _pad;
    EFI_HANDLE                        ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL   *ConIn;
    EFI_HANDLE                        ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL   *ConOut;
    EFI_HANDLE                        StdErrHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL   *StdErr;
    EFI_RUNTIME_SERVICES             *RuntimeServices;
    EFI_BOOT_SERVICES                *BootServices;
} EFI_SYSTEM_TABLE;

/* ─── Entry point ─── */
EFI_STATUS __attribute__((ms_abi)) efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST)
{
    (void)ImageHandle;

    UINT16 msg1[] = {'P','H','A','N','T','O','M',' ','T','E','S','T','\r','\n',0};
    UINT16 msg2[] = {'E','F','I',' ','W','O','R','K','S','!','\r','\n',0};
    UINT16 msg3[] = {'P','r','e','s','s',' ','a','n','y',' ','k','e','y','.','.','.','\r','\n',0};

    /* clear screen */
    ST->ConOut->ClearScreen(ST->ConOut);

    /* print messages */
    ST->ConOut->OutputString(ST->ConOut, msg1);
    ST->ConOut->OutputString(ST->ConOut, msg2);
    ST->ConOut->OutputString(ST->ConOut, msg3);

    /* wait for keypress */
    EFI_INPUT_KEY Key;
    UINTN Idx;
    ST->BootServices->WaitForEvent(1, &ST->ConIn->WaitForKey, &Idx);
    ST->ConIn->ReadKeyStroke(ST->ConIn, &Key);

    /* reboot */
    ST->RuntimeServices->ResetSystem(0, 0, 0, (void *)0);

    while(1) {}
    return 0;
}
