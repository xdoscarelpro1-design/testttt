/*
 * efi_defs.h - Self-contained UEFI definitions for x86_64-w64-mingw32
 *
 * Replaces gnu-efi <efi.h> and <efilib.h>. No external dependencies
 * except <stdint.h>. Designed for direct PE compilation with mingw:
 *   x86_64-w64-mingw32-gcc -ffreestanding -Wl,--subsystem,10
 *
 * EFIAPI is empty because mingw already uses the MS x64 ABI.
 */

#ifndef EFI_DEFS_H
#define EFI_DEFS_H

#include <stdint.h>

/* ================================================================
 * Section 1: Basic Types
 * ================================================================ */

typedef uint8_t   UINT8;
typedef uint16_t  UINT16;
typedef uint32_t  UINT32;
typedef uint64_t  UINT64;
typedef int16_t   INT16;
typedef int32_t   INT32;
typedef int64_t   INT64;
typedef uint64_t  UINTN;
typedef int64_t   INTN;

typedef unsigned char   CHAR8;
typedef unsigned short  CHAR16;
typedef UINT8           BOOLEAN;
typedef void            VOID;

typedef VOID*           EFI_HANDLE;
typedef VOID*           EFI_EVENT;
typedef UINT64          EFI_PHYSICAL_ADDRESS;
typedef UINTN           EFI_STATUS;

#ifndef TRUE
#define TRUE  ((BOOLEAN)1)
#endif
#ifndef FALSE
#define FALSE ((BOOLEAN)0)
#endif
#ifndef NULL
#define NULL  ((VOID*)0)
#endif

/* ================================================================
 * Section 2: Calling Convention and Annotation Macros
 * ================================================================ */

/* mingw targets MS x64 ABI by default -- EFIAPI is intentionally empty */
#define EFIAPI

#define IN
#define OUT
#define OPTIONAL
#define CONST const

/* ================================================================
 * Section 3: Status Codes
 * ================================================================ */

#define EFI_SUCCESS             ((EFI_STATUS)0)
#define EFI_LOAD_ERROR          ((EFI_STATUS)(0x8000000000000001ULL))
#define EFI_INVALID_PARAMETER   ((EFI_STATUS)(0x8000000000000002ULL))
#define EFI_NOT_FOUND           ((EFI_STATUS)(0x8000000000000005ULL))
#define EFI_OUT_OF_RESOURCES    ((EFI_STATUS)(0x8000000000000009ULL))
#define EFI_UNSUPPORTED         ((EFI_STATUS)(0x8000000000000003ULL))
#define EFI_BUFFER_TOO_SMALL    ((EFI_STATUS)(0x8000000000000005ULL))

#define EFI_ERROR(x)            (((INTN)(x)) < 0)

/* ================================================================
 * Section 4: EFI_GUID
 * ================================================================ */

typedef struct {
    UINT32  Data1;
    UINT16  Data2;
    UINT16  Data3;
    UINT8   Data4[8];
} EFI_GUID;

/* ================================================================
 * Section 5: Forward Declarations
 * ================================================================ */

typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL   EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct _EFI_BOOT_SERVICES               EFI_BOOT_SERVICES;
typedef struct _EFI_RUNTIME_SERVICES            EFI_RUNTIME_SERVICES;
typedef struct _EFI_DEVICE_PATH_PROTOCOL        EFI_DEVICE_PATH_PROTOCOL;
typedef struct _EFI_FILE_PROTOCOL               EFI_FILE_PROTOCOL;
typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
typedef struct _EFI_SYSTEM_TABLE                EFI_SYSTEM_TABLE;

/* ================================================================
 * Section 6: EFI_TABLE_HEADER
 * ================================================================ */

typedef struct {
    UINT64  Signature;
    UINT32  Revision;
    UINT32  HeaderSize;
    UINT32  CRC32;
    UINT32  Reserved;
} EFI_TABLE_HEADER;

/* ================================================================
 * Section 7: Console Definitions
 * ================================================================ */

/* Input key */
typedef struct {
    UINT16  ScanCode;
    CHAR16  UnicodeChar;
} EFI_INPUT_KEY;

/* Color attributes */
#define EFI_BLACK            0x00
#define EFI_LIGHTGREEN       0x0A
#define EFI_LIGHTCYAN        0x0B
#define EFI_YELLOW           0x0E
#define EFI_WHITE            0x0F
#define EFI_BACKGROUND_BLACK 0x00

#define CHAR_CARRIAGE_RETURN 0x000D

/* Simple Text Output Mode */
typedef struct {
    INT32    MaxMode;
    INT32    Mode;
    INT32    Attribute;
    INT32    CursorColumn;
    INT32    CursorRow;
    BOOLEAN  CursorVisible;
} SIMPLE_TEXT_OUTPUT_MODE;

/* --- Text Output function pointer types --- */

typedef EFI_STATUS (EFIAPI *EFI_TEXT_RESET)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
    IN BOOLEAN                          ExtendedVerification
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
    IN CHAR16                          *String
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_TEST_STRING)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
    IN CHAR16                          *String
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_QUERY_MODE)(
    IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    IN  UINTN                          ModeNumber,
    OUT UINTN                          *Columns,
    OUT UINTN                          *Rows
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_MODE)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
    IN UINTN                           ModeNumber
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_ATTRIBUTE)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
    IN UINTN                           Attribute
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_CLEAR_SCREEN)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_CURSOR_POSITION)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
    IN UINTN                           Column,
    IN UINTN                           Row
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_ENABLE_CURSOR)(
    IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
    IN BOOLEAN                         Visible
);

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_RESET                Reset;
    EFI_TEXT_STRING               OutputString;
    EFI_TEXT_TEST_STRING          TestString;
    EFI_TEXT_QUERY_MODE           QueryMode;
    EFI_TEXT_SET_MODE             SetMode;
    EFI_TEXT_SET_ATTRIBUTE        SetAttribute;
    EFI_TEXT_CLEAR_SCREEN         ClearScreen;
    EFI_TEXT_SET_CURSOR_POSITION  SetCursorPosition;
    EFI_TEXT_ENABLE_CURSOR        EnableCursor;
    SIMPLE_TEXT_OUTPUT_MODE       *Mode;
};

/* --- Text Input function pointer types --- */

typedef EFI_STATUS (EFIAPI *EFI_INPUT_RESET)(
    IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL   *This,
    IN BOOLEAN                          ExtendedVerification
);

typedef EFI_STATUS (EFIAPI *EFI_INPUT_READ_KEY)(
    IN  EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *This,
    OUT EFI_INPUT_KEY                   *Key
);

struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_INPUT_RESET     Reset;
    EFI_INPUT_READ_KEY  ReadKeyStroke;
    EFI_EVENT           WaitForKey;
};

/* ================================================================
 * Section 8: EFI_TIME
 * ================================================================ */

typedef struct {
    UINT16  Year;
    UINT8   Month;
    UINT8   Day;
    UINT8   Hour;
    UINT8   Minute;
    UINT8   Second;
    UINT8   Pad1;
    UINT32  Nanosecond;
    INT16   TimeZone;
    UINT8   Daylight;
    UINT8   Pad2;
} EFI_TIME;

/* ================================================================
 * Section 9: Memory Types, Allocation Types, Memory Descriptor
 * ================================================================ */

typedef UINT32 EFI_MEMORY_TYPE;
#define EfiReservedMemoryType   0
#define EfiLoaderCode           1
#define EfiLoaderData           2
#define EfiBootServicesCode     3
#define EfiBootServicesData     4
#define EfiRuntimeServicesCode  5
#define EfiRuntimeServicesData  6
#define EfiConventionalMemory   7
#define EfiMaxMemoryType       16

typedef UINT32 EFI_ALLOCATE_TYPE;
#define AllocateAnyPages    0
#define AllocateMaxAddress  1
#define AllocateAddress     2

typedef struct {
    UINT32                  Type;
    EFI_PHYSICAL_ADDRESS    PhysicalStart;
    EFI_PHYSICAL_ADDRESS    VirtualStart;
    UINT64                  NumberOfPages;
    UINT64                  Attribute;
} EFI_MEMORY_DESCRIPTOR;

/* Locate search type for LocateHandleBuffer */
#define AllHandles       0
#define ByRegisterNotify 1
#define ByProtocol       2

/* Reset type */
typedef UINT32 EFI_RESET_TYPE;
#define EfiResetCold 0
#define EfiResetWarm 1

/* ================================================================
 * Section 10: Boot Services Function Pointer Types
 * ================================================================ */

typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_PAGES)(
    IN     EFI_ALLOCATE_TYPE        Type,
    IN     EFI_MEMORY_TYPE          MemoryType,
    IN     UINTN                    Pages,
    IN OUT EFI_PHYSICAL_ADDRESS     *Memory
);

typedef EFI_STATUS (EFIAPI *EFI_FREE_PAGES)(
    IN EFI_PHYSICAL_ADDRESS         Memory,
    IN UINTN                        Pages
);

typedef EFI_STATUS (EFIAPI *EFI_GET_MEMORY_MAP)(
    IN OUT UINTN                    *MemoryMapSize,
    OUT    EFI_MEMORY_DESCRIPTOR    *MemoryMap,
    OUT    UINTN                    *MapKey,
    OUT    UINTN                    *DescriptorSize,
    OUT    UINT32                   *DescriptorVersion
);

typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_POOL)(
    IN  EFI_MEMORY_TYPE             PoolType,
    IN  UINTN                       Size,
    OUT VOID                        **Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FREE_POOL)(
    IN VOID                         *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_WAIT_FOR_EVENT)(
    IN  UINTN                       NumberOfEvents,
    IN  EFI_EVENT                   *Event,
    OUT UINTN                       *Index
);

typedef EFI_STATUS (EFIAPI *EFI_HANDLE_PROTOCOL)(
    IN  EFI_HANDLE                  Handle,
    IN  EFI_GUID                    *Protocol,
    OUT VOID                        **Interface
);

typedef EFI_STATUS (EFIAPI *EFI_IMAGE_LOAD)(
    IN  BOOLEAN                     BootPolicy,
    IN  EFI_HANDLE                  ParentImageHandle,
    IN  EFI_DEVICE_PATH_PROTOCOL    *DevicePath,
    IN  VOID                        *SourceBuffer   OPTIONAL,
    IN  UINTN                       SourceSize,
    OUT EFI_HANDLE                  *ImageHandle
);

typedef EFI_STATUS (EFIAPI *EFI_IMAGE_START)(
    IN  EFI_HANDLE                  ImageHandle,
    OUT UINTN                       *ExitDataSize,
    OUT CHAR16                      **ExitData      OPTIONAL
);

typedef EFI_STATUS (EFIAPI *EFI_EXIT_BOOT_SERVICES)(
    IN EFI_HANDLE                   ImageHandle,
    IN UINTN                        MapKey
);

typedef EFI_STATUS (EFIAPI *EFI_STALL)(
    IN UINTN                        Microseconds
);

typedef EFI_STATUS (EFIAPI *EFI_LOCATE_HANDLE_BUFFER)(
    IN     UINTN                    SearchType,
    IN     EFI_GUID                 *Protocol       OPTIONAL,
    IN     VOID                     *SearchKey       OPTIONAL,
    IN OUT UINTN                    *NoHandles,
    OUT    EFI_HANDLE               **Buffer
);

/* ================================================================
 * Section 11: EFI_BOOT_SERVICES
 *
 * 44 function pointer slots after EFI_TABLE_HEADER.
 * Each slot is 8 bytes (pointer on x86_64).
 * Offsets verified: Header=0x00(24B), first ptr=0x18, last=0x170.
 * ================================================================ */

struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER            Hdr;                        /* 0x00 */

    /* Task Priority Services */
    void                        *RaiseTPL;                  /* 0x18 */
    void                        *RestoreTPL;                /* 0x20 */

    /* Memory Services */
    EFI_ALLOCATE_PAGES          AllocatePages;              /* 0x28 */
    EFI_FREE_PAGES              FreePages;                  /* 0x30 */
    EFI_GET_MEMORY_MAP          GetMemoryMap;               /* 0x38 */
    EFI_ALLOCATE_POOL           AllocatePool;               /* 0x40 */
    EFI_FREE_POOL               FreePool;                   /* 0x48 */

    /* Event & Timer Services */
    void                        *CreateEvent;               /* 0x50 */
    void                        *SetTimer;                  /* 0x58 */
    EFI_WAIT_FOR_EVENT          WaitForEvent;               /* 0x60 */
    void                        *SignalEvent;               /* 0x68 */
    void                        *CloseEvent;                /* 0x70 */
    void                        *CheckEvent;                /* 0x78 */

    /* Protocol Handler Services */
    void                        *InstallProtocolInterface;  /* 0x80 */
    void                        *ReinstallProtocolInterface;/* 0x88 */
    void                        *UninstallProtocolInterface;/* 0x90 */
    EFI_HANDLE_PROTOCOL         HandleProtocol;             /* 0x98 */
    void                        *Reserved;                  /* 0xA0 */
    void                        *RegisterProtocolNotify;    /* 0xA8 */
    void                        *LocateHandle;              /* 0xB0 */
    void                        *LocateDevicePath;          /* 0xB8 */
    void                        *InstallConfigurationTable; /* 0xC0 */

    /* Image Services */
    EFI_IMAGE_LOAD              LoadImage;                  /* 0xC8 */
    EFI_IMAGE_START             StartImage;                 /* 0xD0 */
    void                        *Exit;                      /* 0xD8 */
    void                        *UnloadImage;               /* 0xE0 */
    EFI_EXIT_BOOT_SERVICES     ExitBootServices;            /* 0xE8 */

    /* Miscellaneous Services */
    void                        *GetNextMonotonicCount;     /* 0xF0 */
    EFI_STALL                   Stall;                      /* 0xF8 */
    void                        *SetWatchdogTimer;          /* 0x100 */

    /* DriverSupport Services */
    void                        *ConnectController;         /* 0x108 */
    void                        *DisconnectController;      /* 0x110 */

    /* Open and Close Protocol Services */
    void                        *OpenProtocol;              /* 0x118 */
    void                        *CloseProtocol;             /* 0x120 */
    void                        *OpenProtocolInformation;   /* 0x128 */

    /* Library Services */
    void                        *ProtocolsPerHandle;        /* 0x130 */
    EFI_LOCATE_HANDLE_BUFFER    LocateHandleBuffer;         /* 0x138 */
    void                        *LocateProtocol;            /* 0x140 */
    void                        *InstallMultipleProtocolInterfaces;   /* 0x148 */
    void                        *UninstallMultipleProtocolInterfaces; /* 0x150 */

    /* 32-bit CRC Services */
    void                        *CalculateCrc32;            /* 0x158 */

    /* Miscellaneous Services (continued) */
    void                        *CopyMem;                   /* 0x160 */
    void                        *SetMem;                    /* 0x168 */
    void                        *CreateEventEx;             /* 0x170 */
};

/* ================================================================
 * Section 12: EFI_RUNTIME_SERVICES
 *
 * 14 function pointer slots after EFI_TABLE_HEADER.
 * ResetSystem is at offset 0x68 (slot index 10).
 * ================================================================ */

typedef VOID (EFIAPI *EFI_RESET_SYSTEM)(
    IN EFI_RESET_TYPE   ResetType,
    IN EFI_STATUS       ResetStatus,
    IN UINTN            DataSize,
    IN VOID             *ResetData  OPTIONAL
);

struct _EFI_RUNTIME_SERVICES {
    EFI_TABLE_HEADER    Hdr;                            /* 0x00 */

    /* Time Services */
    void                *GetTime;                       /* 0x18 */
    void                *SetTime;                       /* 0x20 */
    void                *GetWakeupTime;                 /* 0x28 */
    void                *SetWakeupTime;                 /* 0x30 */

    /* Virtual Memory Services */
    void                *SetVirtualAddressMap;          /* 0x38 */
    void                *ConvertPointer;                /* 0x40 */

    /* Variable Services */
    void                *GetVariable;                   /* 0x48 */
    void                *GetNextVariableName;           /* 0x50 */
    void                *SetVariable;                   /* 0x58 */

    /* Miscellaneous Services */
    void                *GetNextHighMonotonicCount;     /* 0x60 */
    EFI_RESET_SYSTEM    ResetSystem;                    /* 0x68 */

    /* UEFI 2.0 Capsule Services */
    void                *UpdateCapsule;                 /* 0x70 */
    void                *QueryCapsuleCapabilities;      /* 0x78 */

    /* Miscellaneous UEFI 2.0 Service */
    void                *QueryVariableInfo;             /* 0x80 */
};

/* ================================================================
 * Section 13: EFI_CONFIGURATION_TABLE
 * ================================================================ */

typedef struct {
    EFI_GUID    VendorGuid;
    VOID        *VendorTable;
} EFI_CONFIGURATION_TABLE;

/* ================================================================
 * Section 14: EFI_SYSTEM_TABLE
 * ================================================================ */

struct _EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER                    Hdr;
    CHAR16                              *FirmwareVendor;
    UINT32                              FirmwareRevision;
    EFI_HANDLE                          ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL      *ConIn;
    EFI_HANDLE                          ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL     *ConOut;
    EFI_HANDLE                          StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL     *StdErr;
    EFI_RUNTIME_SERVICES                *RuntimeServices;
    EFI_BOOT_SERVICES                   *BootServices;
    UINTN                               NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE             *ConfigurationTable;
};

/* ================================================================
 * Section 15: Device Path Protocol
 * ================================================================ */

#pragma pack(push, 1)
struct _EFI_DEVICE_PATH_PROTOCOL {
    UINT8   Type;
    UINT8   SubType;
    UINT8   Length[2];
};
#pragma pack(pop)

/* Device path type/subtype constants */
#define MEDIA_DEVICE_PATH               0x04
#define MEDIA_FILEPATH_DP               0x04
#define END_DEVICE_PATH_TYPE            0x7F
#define END_ENTIRE_DEVICE_PATH_SUBTYPE  0xFF

/* File path device path node */
#pragma pack(push, 1)
typedef struct {
    EFI_DEVICE_PATH_PROTOCOL    Header;
    CHAR16                      PathName[1];
} FILEPATH_DEVICE_PATH;
#pragma pack(pop)

/* Device path utility macros */
#define DevicePathNodeLength(Node) \
    ((UINT16)((Node)->Length[0] | ((UINT16)(Node)->Length[1] << 8)))

#define NextDevicePathNode(Node) \
    ((EFI_DEVICE_PATH_PROTOCOL*)((UINT8*)(Node) + DevicePathNodeLength(Node)))

#define IsDevicePathEnd(Node) \
    ((Node)->Type == END_DEVICE_PATH_TYPE)

#define SetDevicePathNodeLength(Node, Len) do { \
    (Node)->Length[0] = (UINT8)((Len) & 0xFF); \
    (Node)->Length[1] = (UINT8)(((Len) >> 8) & 0xFF); \
} while (0)

static inline void SetDevicePathEndNode(EFI_DEVICE_PATH_PROTOCOL *Node) {
    Node->Type    = END_DEVICE_PATH_TYPE;
    Node->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
    Node->Length[0] = sizeof(EFI_DEVICE_PATH_PROTOCOL);
    Node->Length[1] = 0;
}

/* ================================================================
 * Section 16: File Protocol
 * ================================================================ */

#define EFI_FILE_MODE_READ    0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE   0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE  0x8000000000000000ULL

typedef EFI_STATUS (EFIAPI *EFI_FILE_OPEN)(
    IN  EFI_FILE_PROTOCOL       *This,
    OUT EFI_FILE_PROTOCOL       **NewHandle,
    IN  CHAR16                  *FileName,
    IN  UINT64                  OpenMode,
    IN  UINT64                  Attributes
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_CLOSE)(
    IN EFI_FILE_PROTOCOL        *This
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_DELETE)(
    IN EFI_FILE_PROTOCOL        *This
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_READ)(
    IN     EFI_FILE_PROTOCOL    *This,
    IN OUT UINTN                *BufferSize,
    OUT    VOID                 *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_WRITE)(
    IN     EFI_FILE_PROTOCOL    *This,
    IN OUT UINTN                *BufferSize,
    IN     VOID                 *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_POSITION)(
    IN  EFI_FILE_PROTOCOL       *This,
    OUT UINT64                  *Position
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_SET_POSITION)(
    IN EFI_FILE_PROTOCOL        *This,
    IN UINT64                   Position
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_INFO)(
    IN     EFI_FILE_PROTOCOL    *This,
    IN     EFI_GUID             *InformationType,
    IN OUT UINTN                *BufferSize,
    OUT    VOID                 *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_SET_INFO)(
    IN EFI_FILE_PROTOCOL        *This,
    IN EFI_GUID                 *InformationType,
    IN UINTN                    BufferSize,
    IN VOID                     *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_FLUSH)(
    IN EFI_FILE_PROTOCOL        *This
);

struct _EFI_FILE_PROTOCOL {
    UINT64                  Revision;
    EFI_FILE_OPEN           Open;
    EFI_FILE_CLOSE          Close;
    EFI_FILE_DELETE         Delete;
    EFI_FILE_READ           Read;
    EFI_FILE_WRITE          Write;
    EFI_FILE_GET_POSITION   GetPosition;
    EFI_FILE_SET_POSITION   SetPosition;
    EFI_FILE_GET_INFO       GetInfo;
    EFI_FILE_SET_INFO       SetInfo;
    EFI_FILE_FLUSH          Flush;
};

/* ================================================================
 * Section 17: EFI_FILE_INFO
 * ================================================================ */

typedef struct {
    UINT64      Size;
    UINT64      FileSize;
    UINT64      PhysicalSize;
    EFI_TIME    CreateTime;
    EFI_TIME    LastAccessTime;
    EFI_TIME    ModificationTime;
    UINT64      Attribute;
    CHAR16      FileName[1];
} EFI_FILE_INFO;

/* ================================================================
 * Section 18: Simple File System Protocol
 * ================================================================ */

typedef EFI_STATUS (EFIAPI *EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME)(
    IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
    OUT EFI_FILE_PROTOCOL               **Root
);

struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64                                      Revision;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME OpenVolume;
};

/* ================================================================
 * Section 19: Block IO Protocol (GUID only -- struct not needed)
 * ================================================================ */

/* The GUID is defined in the GUID section below */

/* ================================================================
 * Section 20: Loaded Image Protocol
 * ================================================================ */

typedef EFI_STATUS (EFIAPI *EFI_IMAGE_UNLOAD)(
    IN EFI_HANDLE   ImageHandle
);

typedef struct {
    UINT32                      Revision;
    EFI_HANDLE                  ParentHandle;
    EFI_SYSTEM_TABLE            *SystemTable;
    EFI_HANDLE                  DeviceHandle;
    EFI_DEVICE_PATH_PROTOCOL    *FilePath;
    VOID                        *Reserved;
    UINT32                      LoadOptionsSize;
    VOID                        *LoadOptions;
    VOID                        *ImageBase;
    UINT64                      ImageSize;
    EFI_MEMORY_TYPE             ImageCodeType;
    EFI_MEMORY_TYPE             ImageDataType;
    EFI_IMAGE_UNLOAD            Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

/* ================================================================
 * Section 21: Protocol GUIDs
 *
 * Defined as static const so they can be used with & in multiple
 * translation units without linker conflicts.
 * ================================================================ */

/* EFI_LOADED_IMAGE_PROTOCOL_GUID */
static const EFI_GUID gEfiLoadedImageProtocolGuid = {
    0x5B1B31A1, 0x9562, 0x11d2,
    {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

/* EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID */
static const EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x0964e5b22, 0x6459, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

/* EFI_FILE_INFO_ID */
static const EFI_GUID gEfiFileInfoGuid = {
    0x09576e92, 0x6d3f, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

/* EFI_BLOCK_IO_PROTOCOL_GUID */
static const EFI_GUID gEfiBlockIoProtocolGuid = {
    0x964e5b21, 0x6459, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

/* EFI_DEVICE_PATH_PROTOCOL_GUID */
static const EFI_GUID gEfiDevicePathProtocolGuid = {
    0x09576e91, 0x6d3f, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

/*
 * gnu-efi compatible GUID variable aliases.
 * These match the names gnu-efi uses in its headers/libs.
 */
#define LoadedImageProtocol     gEfiLoadedImageProtocolGuid
#define FileSystemProtocol      gEfiSimpleFileSystemProtocolGuid
#define GenericFileInfo         gEfiFileInfoGuid
#define BlockIoProtocol         gEfiBlockIoProtocolGuid
#define DevicePathProtocol      gEfiDevicePathProtocolGuid

/* Also provide initializer macros for use in compound literals */
#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    {0x5B1B31A1, 0x9562, 0x11d2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}}

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    {0x0964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

#define EFI_FILE_INFO_ID \
    {0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

#define EFI_BLOCK_IO_PROTOCOL_GUID \
    {0x964e5b21, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

#define EFI_DEVICE_PATH_PROTOCOL_GUID \
    {0x09576e91, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

/* ================================================================
 * Section 22: Utility Functions
 * ================================================================ */

static inline VOID CopyMem(VOID *Dest, const VOID *Src, UINTN Size) {
    UINT8       *d = (UINT8*)Dest;
    const UINT8 *s = (const UINT8*)Src;
    if (d < s) {
        while (Size--) *d++ = *s++;
    } else if (d > s) {
        d += Size;
        s += Size;
        while (Size--) *--d = *--s;
    }
}

static inline VOID SetMem(VOID *Buffer, UINTN Size, UINT8 Value) {
    UINT8 *p = (UINT8*)Buffer;
    while (Size--) *p++ = Value;
}

static inline UINTN StrLen(const CHAR16 *Str) {
    UINTN Len = 0;
    while (Str[Len] != 0) Len++;
    return Len;
}

static inline UINTN StrSize(const CHAR16 *Str) {
    return (StrLen(Str) + 1) * sizeof(CHAR16);
}

/* ================================================================
 * Section 23: Device Path Size Helpers
 * ================================================================ */

static inline UINTN DevicePathSize(const EFI_DEVICE_PATH_PROTOCOL *DevPath) {
    const EFI_DEVICE_PATH_PROTOCOL *Node = DevPath;
    UINTN Size = 0;
    if (Node == NULL) return 0;
    while (!IsDevicePathEnd(Node)) {
        UINTN NodeLen = DevicePathNodeLength(Node);
        if (NodeLen < sizeof(EFI_DEVICE_PATH_PROTOCOL)) break;
        Size += NodeLen;
        Node = NextDevicePathNode(Node);
    }
    /* Include the end node */
    Size += sizeof(EFI_DEVICE_PATH_PROTOCOL);
    return Size;
}

/* ================================================================
 * Section 24: AppendDevicePathNode
 *
 * Builds a new device path by appending DevPathNode to DevPath.
 * Allocates memory via BootServices->AllocatePool.
 * The caller must provide the BootServices pointer.
 * Returns NULL on allocation failure.
 * ================================================================ */

static inline EFI_DEVICE_PATH_PROTOCOL* AppendDevicePathNode(
    EFI_BOOT_SERVICES                   *BS,
    const EFI_DEVICE_PATH_PROTOCOL      *DevPath,
    const EFI_DEVICE_PATH_PROTOCOL      *DevPathNode
) {
    UINTN PathSize, NodeSize, NewSize;
    EFI_DEVICE_PATH_PROTOCOL *NewPath;
    UINT8 *Ptr;
    EFI_STATUS Status;

    /* Handle NULL device path: create minimal path with just the node */
    if (DevPath == NULL) {
        if (DevPathNode == NULL) {
            /* Return an end-of-path node only */
            Status = BS->AllocatePool(EfiBootServicesData,
                sizeof(EFI_DEVICE_PATH_PROTOCOL), (VOID**)&NewPath);
            if (EFI_ERROR(Status)) return NULL;
            SetDevicePathEndNode(NewPath);
            return NewPath;
        }
        NodeSize = DevicePathNodeLength(DevPathNode);
        NewSize  = NodeSize + sizeof(EFI_DEVICE_PATH_PROTOCOL);
        Status = BS->AllocatePool(EfiBootServicesData, NewSize, (VOID**)&NewPath);
        if (EFI_ERROR(Status)) return NULL;
        CopyMem(NewPath, DevPathNode, NodeSize);
        SetDevicePathEndNode(
            (EFI_DEVICE_PATH_PROTOCOL*)((UINT8*)NewPath + NodeSize));
        return NewPath;
    }

    PathSize = DevicePathSize(DevPath);  /* Includes end node (4 bytes) */
    if (DevPathNode == NULL) {
        /* Return a copy of the original path */
        Status = BS->AllocatePool(EfiBootServicesData, PathSize, (VOID**)&NewPath);
        if (EFI_ERROR(Status)) return NULL;
        CopyMem(NewPath, DevPath, PathSize);
        return NewPath;
    }

    NodeSize = DevicePathNodeLength(DevPathNode);
    /*
     * New layout: [original path minus end node] [new node] [end node]
     * = (PathSize - 4) + NodeSize + 4 = PathSize + NodeSize
     */
    NewSize = PathSize + NodeSize;
    Status = BS->AllocatePool(EfiBootServicesData, NewSize, (VOID**)&NewPath);
    if (EFI_ERROR(Status)) return NULL;

    Ptr = (UINT8*)NewPath;
    /* Copy original path without its end node */
    CopyMem(Ptr, DevPath, PathSize - sizeof(EFI_DEVICE_PATH_PROTOCOL));
    Ptr += PathSize - sizeof(EFI_DEVICE_PATH_PROTOCOL);
    /* Append the new node */
    CopyMem(Ptr, DevPathNode, NodeSize);
    Ptr += NodeSize;
    /* Append new end node */
    SetDevicePathEndNode((EFI_DEVICE_PATH_PROTOCOL*)Ptr);

    return NewPath;
}

#endif /* EFI_DEFS_H */
