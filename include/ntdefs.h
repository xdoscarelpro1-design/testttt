#ifndef _NTDEFS_H_
#define _NTDEFS_H_

/*
 * Minimal NT kernel definitions for position-independent kernel payload.
 * These structures are used post-ExitBootServices when we're running
 * inside ntoskrnl address space with no linked libraries.
 */

#include <stdint.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef int32_t     NTSTATUS;
typedef uint64_t    ULONG_PTR;
typedef uint64_t    ULONG64;
typedef uint64_t    UINTN;
typedef uint32_t    ULONG;
typedef uint16_t    USHORT;
typedef uint8_t     UCHAR;
typedef int64_t     LONG_PTR;
typedef int32_t     LONG;
typedef void       *PVOID;
typedef uint16_t    WCHAR;
typedef WCHAR      *PWSTR;
typedef uint8_t     BOOLEAN;
typedef void       *HANDLE;
typedef ULONG_PTR   SIZE_T;
typedef uint64_t    PHYSICAL_ADDRESS;

#define NT_SUCCESS(s)       ((NTSTATUS)(s) >= 0)
#define STATUS_SUCCESS      ((NTSTATUS)0x00000000)
#define STATUS_NOT_FOUND    ((NTSTATUS)0xC0000225)

#define PAGE_SIZE           0x1000
#define PAGE_SHIFT          12

/* ─── Unicode string ─── */
typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

/* ─── LIST_ENTRY ─── */
typedef struct _LIST_ENTRY {
    struct _LIST_ENTRY *Flink;
    struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY;

/* ─── KLDR_DATA_TABLE_ENTRY ─── */
typedef struct _KLDR_DATA_TABLE_ENTRY {
    LIST_ENTRY      InLoadOrderLinks;
    PVOID           ExceptionTable;
    ULONG           ExceptionTableSize;
    PVOID           GpValue;
    PVOID           NonPagedDebugInfo;
    PVOID           DllBase;
    PVOID           EntryPoint;
    ULONG           SizeOfImage;
    UNICODE_STRING  FullDllName;
    UNICODE_STRING  BaseDllName;
    ULONG           Flags;
    USHORT          LoadCount;
    USHORT          __unused;
    PVOID           SectionPointer;
    ULONG           CheckSum;
    ULONG           TimeDateStamp;
} KLDR_DATA_TABLE_ENTRY, *PKLDR_DATA_TABLE_ENTRY;

/* ─── EPROCESS (partial, for offset-based access) ─── */
/* Offsets vary by Windows build. These are for Win10 22H2 / Win11 23H2 */
#define EPROCESS_UNIQUEPROCESSID_OFFSET     0x440
#define EPROCESS_ACTIVEPROCESSLINKS_OFFSET  0x448
#define EPROCESS_IMAGEFILENAME_OFFSET       0x5A8
#define EPROCESS_PEB_OFFSET                 0x550
#define EPROCESS_TOKEN_OFFSET               0x4B8
#define EPROCESS_VADROOT_OFFSET             0x7D8

/* ─── MMPFN partial ─── */
/* PFN database entry flags for hiding physical pages */
#define MMPFN_SIZE                          0x30
#define PFN_FLAGS_OFFSET                    0x18
#define PFN_REFERENCECOUNT_OFFSET           0x1C
#define PFN_STATE_ACTIVE                    0x01
#define PFN_STATE_STANDBY                   0x02
#define PFN_STATE_MODIFIED                  0x03
#define PFN_STATE_FREE                      0x07
#define PFN_STATE_BAD                       0x06

/* ─── MSR definitions ─── */
#define MSR_LSTAR                           0xC0000082
#define MSR_CSTAR                           0xC0000083
#define MSR_SFMASK                          0xC0000084
#define MSR_GS_BASE                         0xC0000101
#define MSR_KERNEL_GS_BASE                  0xC0000102

/* ─── CR register bits ─── */
#define CR0_WP                              (1ULL << 16)
#define CR4_SMEP                            (1ULL << 20)
#define CR4_SMAP                            (1ULL << 21)

/* ─── Kernel API typedefs ─── */
typedef NTSTATUS (*fn_ExAllocatePool2)(ULONG64 Flags, SIZE_T NumberOfBytes, ULONG Tag);
typedef NTSTATUS (*fn_MmCopyVirtualMemory)(
    PVOID SourceProcess, PVOID SourceAddress,
    PVOID TargetProcess, PVOID TargetAddress,
    SIZE_T BufferSize, UCHAR PreviousMode, SIZE_T *ReturnSize
);
typedef PVOID    (*fn_MmGetVirtualForPhysical)(PHYSICAL_ADDRESS PhysicalAddress);
typedef PVOID    (*fn_MmGetPhysicalAddress)(PVOID BaseAddress);
typedef PVOID    (*fn_PsGetCurrentProcess)(void);
typedef NTSTATUS (*fn_PsLookupProcessByProcessId)(HANDLE ProcessId, PVOID *Process);
typedef void     (*fn_ObfDereferenceObject)(PVOID Object);
typedef PVOID    (*fn_MmMapIoSpace)(PHYSICAL_ADDRESS PhysicalAddress, SIZE_T NumberOfBytes, ULONG CacheType);
typedef void     (*fn_MmUnmapIoSpace)(PVOID BaseAddress, SIZE_T NumberOfBytes);
typedef NTSTATUS (*fn_ZwQuerySystemInformation)(ULONG InfoClass, PVOID Buffer, ULONG Length, ULONG *ReturnLength);

/* ─── Injector API typedefs ─── */
typedef NTSTATUS (*fn_KeStackAttachProcess)(PVOID Process, PVOID ApcState);
typedef void     (*fn_KeUnstackDetachProcess)(PVOID ApcState);
typedef NTSTATUS (*fn_ZwAllocateVirtualMemory)(
    HANDLE ProcessHandle, PVOID *BaseAddress, ULONG_PTR ZeroBits,
    SIZE_T *RegionSize, ULONG AllocationType, ULONG Protect
);
typedef NTSTATUS (*fn_ZwProtectVirtualMemory)(
    HANDLE ProcessHandle, PVOID *BaseAddress, SIZE_T *RegionSize,
    ULONG NewProtect, ULONG *OldProtect
);
typedef PVOID    (*fn_PsGetProcessPeb)(PVOID Process);
typedef NTSTATUS (*fn_PsCreateSystemThread)(
    HANDLE *ThreadHandle, ULONG DesiredAccess, PVOID ObjectAttributes,
    HANDLE ProcessHandle, PVOID ClientId, PVOID StartRoutine, PVOID StartContext
);
typedef NTSTATUS (*fn_ZwClose)(HANDLE Handle);
typedef PVOID    (*fn_ExAllocatePool)(ULONG PoolType, SIZE_T NumberOfBytes);
typedef void     (*fn_ExFreePool)(PVOID P);
typedef NTSTATUS (*fn_KeDelayExecutionThread)(UCHAR WaitMode, BOOLEAN Alertable, PVOID Interval);
typedef NTSTATUS (*fn_ZwCreateThreadEx)(
    HANDLE *ThreadHandle, ULONG DesiredAccess, PVOID ObjectAttributes,
    HANDLE ProcessHandle, PVOID StartRoutine, PVOID Argument,
    ULONG CreateFlags, SIZE_T ZeroBits, SIZE_T StackSize,
    SIZE_T MaxStackSize, PVOID AttributeList
);

/* ─── APC injection typedefs (Ghost Thread method) ─── */
typedef void (*fn_KeInitializeApc)(
    PVOID Apc, PVOID Thread, UCHAR Environment,
    PVOID KernelRoutine, PVOID RundownRoutine,
    PVOID NormalRoutine, UCHAR ApcMode, PVOID NormalContext
);
typedef BOOLEAN (*fn_KeInsertQueueApc)(
    PVOID Apc, PVOID SystemArgument1, PVOID SystemArgument2, UCHAR Increment
);
typedef PVOID (*fn_PsGetNextProcessThread)(PVOID Process, PVOID Thread);

/* ─── Memory allocation constants ─── */
#define MEM_COMMIT              0x00001000
#define MEM_RESERVE             0x00002000
#define PAGE_READWRITE          0x04
#define PAGE_EXECUTE_READ       0x20
#define PAGE_EXECUTE_READWRITE  0x40
#define PAGE_NOACCESS           0x01
#define PAGE_READONLY           0x02

/* ─── PE section characteristics ─── */
#define IMAGE_SCN_MEM_EXECUTE   0x20000000
#define IMAGE_SCN_MEM_READ      0x40000000
#define IMAGE_SCN_MEM_WRITE     0x80000000

/* ─── KAPC opaque allocation size (Win10/11 x64) ─── */
#define KAPC_ALLOC_SIZE         0x58

/* ─── APC / processor mode constants ─── */
#define APC_ENV_ORIGINAL        0
#define KMODE                   0
#define UMODE                   1

/* ─── Pool types ─── */
#define POOL_NONPAGED           0

/* ─── PE relocation types ─── */
#define IMAGE_REL_BASED_DIR64   10
#define IMAGE_REL_BASED_HIGHLOW 3

/* ─── KAPC_STATE for KeStackAttachProcess ─── */
typedef struct _KAPC_STATE {
    LIST_ENTRY  ApcListHead[2];
    PVOID       Process;
    UCHAR       InProgressFlags;
    UCHAR       KernelApcPending;
    UCHAR       UserApcPendingAll;
    UCHAR       _pad;
} KAPC_STATE;

/* ─── User-mode PEB/LDR structures (read from target process) ─── */
typedef struct _PEB_LDR_DATA_UM {
    ULONG       Length;
    BOOLEAN     Initialized;
    PVOID       SsHandle;
    LIST_ENTRY  InLoadOrderModuleList;
    LIST_ENTRY  InMemoryOrderModuleList;
    LIST_ENTRY  InInitOrderModuleList;
} PEB_LDR_DATA_UM;

typedef struct _LDR_DATA_TABLE_ENTRY_UM {
    LIST_ENTRY      InLoadOrderLinks;
    LIST_ENTRY      InMemoryOrderLinks;
    LIST_ENTRY      InInitOrderLinks;
    PVOID           DllBase;
    PVOID           EntryPoint;
    ULONG           SizeOfImage;
    UNICODE_STRING  FullDllName;
    UNICODE_STRING  BaseDllName;
} LDR_DATA_TABLE_ENTRY_UM;

/* ─── Kernel API table (resolved at runtime) ─── */
typedef struct _KERNEL_API {
    fn_ExAllocatePool2              ExAllocatePool2;
    fn_MmCopyVirtualMemory          MmCopyVirtualMemory;
    fn_MmGetVirtualForPhysical      MmGetVirtualForPhysical;
    fn_MmGetPhysicalAddress         MmGetPhysicalAddress;
    fn_PsGetCurrentProcess          PsGetCurrentProcess;
    fn_PsLookupProcessByProcessId   PsLookupProcessByProcessId;
    fn_ObfDereferenceObject         ObfDereferenceObject;
    fn_MmMapIoSpace                 MmMapIoSpace;
    fn_MmUnmapIoSpace               MmUnmapIoSpace;
    fn_ZwQuerySystemInformation     ZwQuerySystemInformation;
    /* injector APIs */
    fn_KeStackAttachProcess         KeStackAttachProcess;
    fn_KeUnstackDetachProcess       KeUnstackDetachProcess;
    fn_ZwAllocateVirtualMemory      ZwAllocateVirtualMemory;
    fn_ZwProtectVirtualMemory       ZwProtectVirtualMemory;
    fn_PsGetProcessPeb              PsGetProcessPeb;
    fn_PsCreateSystemThread         PsCreateSystemThread;
    fn_ZwClose                      ZwClose;
    fn_ExAllocatePool               ExAllocatePool;
    fn_ExFreePool                   ExFreePool;
    fn_KeDelayExecutionThread       KeDelayExecutionThread;
    fn_ZwCreateThreadEx             ZwCreateThreadEx;
    /* APC injection APIs (Ghost Thread method) */
    fn_KeInitializeApc              KeInitializeApc;
    fn_KeInsertQueueApc             KeInsertQueueApc;
    fn_PsGetNextProcessThread       PsGetNextProcessThread;
} KERNEL_API;

/* ─── Communication protocol ─── */
#define COMM_MAGIC          0xBAADF00D
#define COMM_CMD_READ       0x01
#define COMM_CMD_WRITE      0x02
#define COMM_CMD_BASE       0x03
#define COMM_CMD_PROTECT    0x04
#define COMM_CMD_ALLOC      0x05
#define COMM_CMD_STATUS     0x06

typedef struct _COMM_REQUEST {
    ULONG       Magic;
    ULONG       Command;
    HANDLE      ProcessId;
    PVOID       Address;
    PVOID       Buffer;
    SIZE_T      Size;
    ULONG       Protection;
    NTSTATUS    Status;
    ULONG_PTR   Result;
} COMM_REQUEST;

/* ─── Payload shared context ─── */

#include "phantom_debug.h"

typedef struct _PAYLOAD_CONTEXT {
    ULONG           Magic;
    PVOID           NtoskrnlBase;
    UINTN           NtoskrnlSize;
    KERNEL_API      Api;
    PVOID           PfnDatabase;
    PVOID           SyscallOriginal;
    PVOID           PayloadBase;
    UINTN           PayloadSize;
    BOOLEAN         Initialized;
    BOOLEAN         Hidden;
    /* injector state */
    uint8_t        *CheatDllBlob;
    UINTN           CheatDllSize;
    BOOLEAN         CheatInjected;
    HANDLE          TargetPid;
    PVOID           MappedBase;
    /* debug / diagnostics */
    PVOID           SsdtBase;
    PVOID           CodeCaveAddr;
    ULONG           ApiResolved;
    ULONG           ApiFailed;
    PHANTOM_STATUS  DbgStatus;
} PAYLOAD_CONTEXT;

#endif /* _NTDEFS_H_ */
