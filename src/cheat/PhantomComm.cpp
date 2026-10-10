#include "PhantomComm.hpp"

#ifdef _WIN32

#include <cstring>

typedef long NTSTATUS;
typedef NTSTATUS(NTAPI* fnNtQueryDebugFilterState)(ULONG ComponentId, ULONG Level);

namespace PhantomComm
{
    static fnNtQueryDebugFilterState pNtQueryDebugFilterState = nullptr;
    static bool s_Connected = false;

    bool Initialize()
    {
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;

        pNtQueryDebugFilterState = (fnNtQueryDebugFilterState)
            GetProcAddress(ntdll, "NtQueryDebugFilterState");

        if (!pNtQueryDebugFilterState)
            return false;

        COMM_REQUEST req = {};
        req.Magic   = COMM_MAGIC;
        req.Command = COMM_CMD_BASE;
        req.ProcessId = (HANDLE)(uintptr_t)GetCurrentProcessId();

        pNtQueryDebugFilterState((ULONG)(uintptr_t)&req, 0);

        s_Connected = (req.Status == 0 && req.Result != 0);
        return s_Connected;
    }

    bool IsConnected()
    {
        return s_Connected;
    }

    static bool SendRequest(COMM_REQUEST* req)
    {
        if (!pNtQueryDebugFilterState)
            return false;

        pNtQueryDebugFilterState((ULONG)(uintptr_t)req, 0);
        return (req->Status == 0);
    }

    bool ReadMemory(uint32_t pid, void* address, void* buffer, size_t size)
    {
        COMM_REQUEST req = {};
        req.Magic     = COMM_MAGIC;
        req.Command   = COMM_CMD_READ;
        req.ProcessId = (HANDLE)(uintptr_t)pid;
        req.Address   = address;
        req.Buffer    = buffer;
        req.Size      = size;

        return SendRequest(&req);
    }

    bool WriteMemory(uint32_t pid, void* address, void* buffer, size_t size)
    {
        COMM_REQUEST req = {};
        req.Magic     = COMM_MAGIC;
        req.Command   = COMM_CMD_WRITE;
        req.ProcessId = (HANDLE)(uintptr_t)pid;
        req.Address   = address;
        req.Buffer    = buffer;
        req.Size      = size;

        return SendRequest(&req);
    }

    uintptr_t GetModuleBase(uint32_t pid)
    {
        COMM_REQUEST req = {};
        req.Magic     = COMM_MAGIC;
        req.Command   = COMM_CMD_BASE;
        req.ProcessId = (HANDLE)(uintptr_t)pid;

        if (SendRequest(&req))
            return req.Result;
        return 0;
    }

    uintptr_t AllocateMemory(uint32_t pid, size_t size, uint32_t protect)
    {
        COMM_REQUEST req = {};
        req.Magic      = COMM_MAGIC;
        req.Command    = COMM_CMD_ALLOC;
        req.ProcessId  = (HANDLE)(uintptr_t)pid;
        req.Size       = size;
        req.Protection = protect;

        if (SendRequest(&req))
            return req.Result;
        return 0;
    }

    bool FreeMemory(uint32_t pid, uintptr_t address, size_t size)
    {
        COMM_REQUEST req = {};
        req.Magic     = COMM_MAGIC;
        req.Command   = COMM_CMD_FREE;
        req.ProcessId = (HANDLE)(uintptr_t)pid;
        req.Address   = (void*)address;
        req.Size      = size;

        return SendRequest(&req);
    }

    bool ProtectMemory(uint32_t pid, uintptr_t address, size_t size, uint32_t protect)
    {
        COMM_REQUEST req = {};
        req.Magic      = COMM_MAGIC;
        req.Command    = COMM_CMD_PROTECT;
        req.ProcessId  = (HANDLE)(uintptr_t)pid;
        req.Address    = (void*)address;
        req.Size       = size;
        req.Protection = protect;

        return SendRequest(&req);
    }
}

#endif
