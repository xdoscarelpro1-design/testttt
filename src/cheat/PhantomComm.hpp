#pragma once

/*
 * PHANTOM EFI Bootkit — Usermode Communication Layer
 *
 * Replaces the old direct-syscall mechanism (Syscalls.hpp) with the
 * PHANTOM kernel payload's SSDT hook. The hook intercepts
 * NtQueryDebugFilterState and checks for a magic value in the first
 * argument. If matched, it processes the request struct and writes
 * the result back — all from ring-0, completely invisible to any
 * usermode anti-cheat hooks.
 *
 * The old approach:
 *   1. Read syscall numbers from ntdll prologue
 *   2. Build raw syscall stubs in RWX memory
 *   3. Call NtReadVirtualMemory / NtWriteVirtualMemory directly
 *
 * The PHANTOM approach:
 *   1. Fill COMM_REQUEST with magic + command
 *   2. Call NtQueryDebugFilterState (any call path — hooked or not)
 *   3. Kernel payload processes request via MmCopyVirtualMemory
 *   4. Result written back to the same struct
 *
 * Advantages over direct syscalls:
 *   - No process handle needed for R/W (kernel does cross-process copy)
 *   - No VirtualAlloc(RWX) for syscall stubs (huge red flag for AC)
 *   - No ntdll prologue parsing (AC can detect that read pattern)
 *   - Stack looks clean — just a normal NtQueryDebugFilterState call
 */

#ifdef _WIN32

#include <Windows.h>
#include <cstdint>
#include <cstddef>

namespace PhantomComm
{
    constexpr uint32_t COMM_MAGIC       = 0xBAADF00D;
    constexpr uint32_t COMM_CMD_READ    = 0x01;
    constexpr uint32_t COMM_CMD_WRITE   = 0x02;
    constexpr uint32_t COMM_CMD_BASE    = 0x03;
    constexpr uint32_t COMM_CMD_ALLOC   = 0x04;
    constexpr uint32_t COMM_CMD_FREE    = 0x05;
    constexpr uint32_t COMM_CMD_PROTECT = 0x06;

#pragma pack(push, 1)
    struct COMM_REQUEST
    {
        uint32_t    Magic;
        uint32_t    Command;
        HANDLE      ProcessId;
        void*       Address;
        void*       Buffer;
        size_t      Size;
        uint32_t    Protection;
        long        Status;
        uintptr_t   Result;
    };
#pragma pack(pop)

    bool Initialize();
    bool IsConnected();

    bool ReadMemory(uint32_t pid, void* address, void* buffer, size_t size);
    bool WriteMemory(uint32_t pid, void* address, void* buffer, size_t size);
    uintptr_t GetModuleBase(uint32_t pid);

    uintptr_t AllocateMemory(uint32_t pid, size_t size, uint32_t protect);
    bool FreeMemory(uint32_t pid, uintptr_t address, size_t size);
    bool ProtectMemory(uint32_t pid, uintptr_t address, size_t size, uint32_t protect);
}

#endif
