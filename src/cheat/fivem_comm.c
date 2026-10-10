/*
 * PHANTOM - FiveM Usermode Communication Layer
 *
 * This is the usermode component that talks to the kernel payload
 * through the hooked syscall. It runs as a standalone process
 * or can be injected into the FiveM process.
 *
 * Communication protocol:
 *   1. Fill a COMM_REQUEST struct with the desired operation
 *   2. Call NtQueryDebugFilterState with a pointer to the request
 *   3. The kernel hook checks the magic and processes the request
 *   4. Result is written back into the same struct
 *
 * Compile: cl.exe /O2 fivem_comm.c /link ntdll.lib
 *    -or-  x86_64-w64-mingw32-gcc -O2 fivem_comm.c -o comm.exe -lntdll
 */

#ifdef _WIN32

#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#pragma comment(lib, "ntdll.lib")

/* ─── NT definitions ─── */
typedef long NTSTATUS;
#define STATUS_SUCCESS 0

/* our communication protocol */
#define COMM_MAGIC      0xBAADF00D
#define COMM_CMD_READ   0x01
#define COMM_CMD_WRITE  0x02
#define COMM_CMD_BASE   0x03

typedef struct {
    ULONG       Magic;
    ULONG       Command;
    HANDLE      ProcessId;
    void       *Address;
    void       *Buffer;
    SIZE_T      Size;
    ULONG       Protection;
    NTSTATUS    Status;
    ULONG_PTR   Result;
} COMM_REQUEST;

/* imported from ntdll */
extern NTSTATUS __stdcall NtQueryDebugFilterState(
    ULONG ComponentId, ULONG Level
);

/*
 * Send a request to the kernel payload.
 * We abuse NtQueryDebugFilterState by passing our struct pointer
 * as the ComponentId parameter. The kernel hook detects the magic
 * and processes it.
 */
static NTSTATUS SendRequest(COMM_REQUEST *req)
{
    req->Magic = COMM_MAGIC;

    /* cast our struct pointer to the syscall parameters */
    /* the kernel hook receives this as the first argument */
    return NtQueryDebugFilterState(
        (ULONG)(uintptr_t)req,  /* low 32 bits — the hook reads the full pointer from rcx */
        0
    );
}

/* ─── Public API ─── */

/*
 * Read memory from a target process.
 */
int ReadProcessMemoryKernel(DWORD pid, void *address, void *buffer, SIZE_T size)
{
    COMM_REQUEST req = {0};
    req.Command   = COMM_CMD_READ;
    req.ProcessId = (HANDLE)(uintptr_t)pid;
    req.Address   = address;
    req.Buffer    = buffer;
    req.Size      = size;

    SendRequest(&req);
    return req.Status == STATUS_SUCCESS;
}

/*
 * Write memory to a target process.
 */
int WriteProcessMemoryKernel(DWORD pid, void *address, void *buffer, SIZE_T size)
{
    COMM_REQUEST req = {0};
    req.Command   = COMM_CMD_WRITE;
    req.ProcessId = (HANDLE)(uintptr_t)pid;
    req.Address   = address;
    req.Buffer    = buffer;
    req.Size      = size;

    SendRequest(&req);
    return req.Status == STATUS_SUCCESS;
}

/*
 * Get the base address of the main module in a target process.
 */
uintptr_t GetModuleBaseKernel(DWORD pid)
{
    COMM_REQUEST req = {0};
    req.Command   = COMM_CMD_BASE;
    req.ProcessId = (HANDLE)(uintptr_t)pid;

    SendRequest(&req);
    return req.Result;
}

/* ─── FiveM-specific helpers ─── */

/*
 * Find the FiveM / GTA5 process by scanning the process list.
 * We do this from usermode via standard Windows APIs since
 * we don't want to use any hooked APIs for process enumeration.
 */
DWORD FindFiveMProcess(void)
{
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W pe = {0};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snap, &pe)) {
        do {
            /* check for FiveM_b*.exe or GTA5.exe */
            if (wcsstr(pe.szExeFile, L"FiveM") ||
                wcsstr(pe.szExeFile, L"GTA5.exe") ||
                wcsstr(pe.szExeFile, L"CitizenFX")) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return pid;
}

/*
 * Template read helper — reads a typed value from process memory.
 */
#define RPM(pid, addr, type) \
    ({ type _v; ReadProcessMemoryKernel(pid, (void*)(addr), &_v, sizeof(type)); _v; })

#define WPM(pid, addr, val) \
    ({ typeof(val) _v = (val); WriteProcessMemoryKernel(pid, (void*)(addr), &_v, sizeof(_v)); })

/*
 * Example usage / test harness.
 */
int main(int argc, char *argv[])
{
    printf("[PHANTOM] FiveM Communication Layer\n");
    printf("[*] Searching for FiveM process...\n");

    DWORD pid = FindFiveMProcess();
    if (!pid) {
        printf("[!] FiveM not found. Launch FiveM first.\n");
        printf("[*] Waiting for FiveM...\n");

        while (!pid) {
            Sleep(2000);
            pid = FindFiveMProcess();
        }
    }

    printf("[+] FiveM PID: %u\n", pid);

    /* get module base */
    uintptr_t base = GetModuleBaseKernel(pid);
    if (!base) {
        printf("[!] Failed to get module base. Is the kernel payload active?\n");
        return 1;
    }

    printf("[+] Module base: 0x%llx\n", (unsigned long long)base);

    /* read DOS header as connectivity test */
    uint16_t mz = 0;
    if (ReadProcessMemoryKernel(pid, (void *)base, &mz, sizeof(mz))) {
        printf("[+] Read DOS header: 0x%04X (%s)\n", mz,
               mz == 0x5A4D ? "MZ - valid" : "invalid");
    } else {
        printf("[!] Read failed. Kernel communication error.\n");
        return 1;
    }

    printf("[+] Kernel communication established successfully.\n");
    printf("[*] Ready for FiveM operations.\n");

    /* main loop — keep alive for external tools */
    while (1) {
        Sleep(1000);

        /* verify process still running */
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!h) {
            printf("[!] FiveM process terminated.\n");
            break;
        }
        CloseHandle(h);
    }

    return 0;
}

#else
/* non-Windows stub */
int main(void) { return 0; }
#endif
