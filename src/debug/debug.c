/*
 * PHANTOM Debug Console — Windows usermode diagnostic tool
 *
 * Queries the kernel payload via the SSDT comm channel
 * (NtQueryDebugFilterState hooked to syscall 0x117).
 * Shows real-time status of every bootkit stage.
 *
 * Build: x86_64-w64-mingw32-gcc -O2 -o phantom_debug.exe debug.c -lkernel32
 * Run:   phantom_debug.exe            (single query)
 *        phantom_debug.exe --watch    (live monitor, refresh every second)
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

/* pull in shared debug structures */
#include "../../include/phantom_debug.h"

/* COMM protocol constants (must match ntdefs.h) */
#define COMM_MAGIC      0xBAADF00D
#define COMM_CMD_STATUS 0x06

#pragma pack(push, 1)
typedef struct {
    unsigned int    Magic;
    unsigned int    Command;
    void           *ProcessId;
    void           *Address;
    void           *Buffer;
    unsigned long long Size;
    unsigned int    Protection;
    int             Status;
    unsigned long long Result;
} COMM_REQUEST;
#pragma pack(pop)

/* NtQueryDebugFilterState — we call this to reach our SSDT hook */
typedef int (__stdcall *pNtQueryDebugFilterState)(
    unsigned long long ComponentId,
    unsigned long long Level
);

static pNtQueryDebugFilterState NtQueryDbgFilter = NULL;

static HANDLE hConsole;

/* ─── console helpers ─── */

#define CLR_RESET   7
#define CLR_GREEN   10
#define CLR_CYAN    11
#define CLR_RED     12
#define CLR_YELLOW  14
#define CLR_WHITE   15
#define CLR_GRAY    8

static void con_color(int c) { SetConsoleTextAttribute(hConsole, (WORD)c); }

static void con_cls(void)
{
    COORD origin = {0, 0};
    DWORD written;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    DWORD cells = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacterA(hConsole, ' ', cells, origin, &written);
    FillConsoleOutputAttribute(hConsole, CLR_WHITE, cells, origin, &written);
    SetConsoleCursorPosition(hConsole, origin);
}

static const char *level_tag(unsigned char lvl)
{
    switch (lvl) {
    case 0: return "INFO";
    case 1: return " OK ";
    case 2: return "WARN";
    case 3: return " ERR";
    default: return " ?? ";
    }
}

static int level_color(unsigned char lvl)
{
    switch (lvl) {
    case 0: return CLR_CYAN;
    case 1: return CLR_GREEN;
    case 2: return CLR_YELLOW;
    case 3: return CLR_RED;
    default: return CLR_GRAY;
    }
}

/* ─── comm query ─── */

static int QueryPayloadStatus(PHANTOM_STATUS *out)
{
    COMM_REQUEST req;
    memset(&req, 0, sizeof(req));
    req.Magic   = COMM_MAGIC;
    req.Command = COMM_CMD_STATUS;
    req.Buffer  = out;
    req.Size    = sizeof(PHANTOM_STATUS);

    NtQueryDbgFilter((unsigned long long)&req, 0);

    if (req.Status != 0) return 0;
    if (out->Magic != PHANTOM_DBG_MAGIC) return 0;
    return 1;
}

/* ─── display ─── */

static void PrintStageFlag(const char *name, unsigned int stages, unsigned int flag)
{
    if (stages & flag) {
        con_color(CLR_GREEN);
        printf("  [OK] ");
    } else {
        con_color(CLR_RED);
        printf("  [--] ");
    }
    con_color(CLR_WHITE);
    printf("%s\n", name);
}

static void PrintStatus(PHANTOM_STATUS *s)
{
    con_cls();

    con_color(CLR_CYAN);
    printf("  ======================================================\n");
    printf("      P H A N T O M   D E B U G   C O N S O L E\n");
    printf("  ======================================================\n\n");

    /* ─── stage status ─── */
    con_color(CLR_YELLOW);
    printf("  --- Stage Status ---\n\n");

    PrintStageFlag("Payload Entry Ran",     s->Stages, STAGE_ENTRY_RAN);
    PrintStageFlag("Kernel APIs Resolved",  s->Stages, STAGE_API_RESOLVED);
    PrintStageFlag("DSE Patched",           s->Stages, STAGE_DSE_PATCHED);
    PrintStageFlag("SSDT Hook Installed",   s->Stages, STAGE_SSDT_HOOKED);
    PrintStageFlag("Memory Hidden (PFN)",   s->Stages, STAGE_MEMORY_HIDDEN);
    PrintStageFlag("Monitor Started",       s->Stages, STAGE_MONITOR_STARTED);
    PrintStageFlag("FiveM Process Found",   s->Stages, STAGE_TARGET_FOUND);
    PrintStageFlag("Cheat DLL Injected",    s->Stages, STAGE_CHEAT_INJECTED);

    /* ─── addresses ─── */
    printf("\n");
    con_color(CLR_YELLOW);
    printf("  --- Addresses ---\n\n");
    con_color(CLR_WHITE);

    printf("  ntoskrnl Base     : 0x%016llX  (size: 0x%llX)\n",
        s->NtoskrnlBase, s->NtoskrnlSize);
    printf("  Payload Base      : 0x%016llX  (size: 0x%llX)\n",
        s->PayloadBase, s->PayloadSize);
    printf("  SSDT Base         : 0x%016llX\n", s->SsdtBase);
    printf("  Code Cave         : 0x%016llX\n", s->CodeCaveAddr);
    printf("  PFN Database      : 0x%016llX\n", s->PfnDatabase);

    /* ─── counters ─── */
    printf("\n");
    con_color(CLR_YELLOW);
    printf("  --- Counters ---\n\n");
    con_color(CLR_WHITE);

    printf("  APIs Resolved     : %u / %u  (failed: %u)\n",
        s->ApiResolved, s->ApiResolved + s->ApiFailed, s->ApiFailed);
    printf("  Syscall Count     : %u\n", s->SyscallCount);
    printf("  Monitor Scans     : %u\n", s->ScanCount);
    printf("  Cheat DLL Size    : %u bytes\n", s->CheatDllSize);

    /* ─── injection ─── */
    if (s->Stages & STAGE_TARGET_FOUND) {
        printf("\n");
        con_color(CLR_YELLOW);
        printf("  --- Injection ---\n\n");
        con_color(CLR_GREEN);
        printf("  Target PID        : %llu\n", s->TargetPid);
        printf("  Mapped DLL Base   : 0x%016llX\n", s->MappedBase);
    }

    /* ─── debug log ─── */
    printf("\n");
    con_color(CLR_YELLOW);
    printf("  --- Debug Log (last %u entries) ---\n\n",
        s->LogCount < DBG_LOG_MAX_ENTRIES ? s->LogCount : DBG_LOG_MAX_ENTRIES);

    unsigned int count = s->LogCount < DBG_LOG_MAX_ENTRIES ?
                         s->LogCount : DBG_LOG_MAX_ENTRIES;

    /* print from oldest to newest */
    unsigned int start;
    if (s->LogCount <= DBG_LOG_MAX_ENTRIES)
        start = 0;
    else
        start = s->LogHead;

    for (unsigned int i = 0; i < count; i++) {
        unsigned int idx = (start + i) % DBG_LOG_MAX_ENTRIES;
        DBG_LOG_ENTRY *e = &s->Log[idx];

        con_color(level_color(e->Level));
        printf("  [%s] ", level_tag(e->Level));
        con_color(CLR_WHITE);
        printf("%s\n", e->Msg);
    }

    printf("\n");
    con_color(CLR_GRAY);
    printf("  Press Ctrl+C to exit\n");
    con_color(CLR_RESET);
}

/* ─── main ─── */

int main(int argc, char **argv)
{
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    SetConsoleTitleA("PHANTOM Debug Console");

    int watch_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--watch") == 0 || strcmp(argv[i], "-w") == 0)
            watch_mode = 1;
    }

    /* resolve NtQueryDebugFilterState from ntdll */
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) {
        con_color(CLR_RED);
        printf("[!] Failed to get ntdll.dll handle\n");
        return 1;
    }

    NtQueryDbgFilter = (pNtQueryDebugFilterState)
        GetProcAddress(ntdll, "NtQueryDebugFilterState");
    if (!NtQueryDbgFilter) {
        con_color(CLR_RED);
        printf("[!] NtQueryDebugFilterState not found in ntdll\n");
        return 1;
    }

    con_cls();
    con_color(CLR_CYAN);
    printf("\n  PHANTOM Debug Console\n");
    printf("  Querying kernel payload...\n\n");
    con_color(CLR_RESET);

    PHANTOM_STATUS status;

    if (!watch_mode) {
        /* single query */
        memset(&status, 0, sizeof(status));
        if (QueryPayloadStatus(&status)) {
            PrintStatus(&status);
        } else {
            con_color(CLR_RED);
            printf("  [!] Payload not responding.\n");
            printf("      Either the bootkit is not active, or the SSDT\n");
            printf("      hook is not installed. Did you boot from USB?\n\n");
            con_color(CLR_RESET);
        }
        return 0;
    }

    /* watch mode — loop every second */
    int consecutive_fails = 0;

    while (1) {
        memset(&status, 0, sizeof(status));

        if (QueryPayloadStatus(&status)) {
            consecutive_fails = 0;
            PrintStatus(&status);
        } else {
            con_cls();
            con_color(CLR_YELLOW);
            printf("\n  PHANTOM Debug Console — WAITING\n\n");
            con_color(CLR_GRAY);

            consecutive_fails++;
            int dots = consecutive_fails % 4;

            printf("  Payload not responding");
            for (int d = 0; d < dots; d++) printf(".");
            printf("\n");
            printf("  Retrying every second. Boot from USB to activate.\n\n");
            con_color(CLR_RESET);
        }

        Sleep(1000);
    }

    return 0;
}
