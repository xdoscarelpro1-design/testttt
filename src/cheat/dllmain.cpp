/*
 * PHANTOM DLL Entry Point — Kernel-Injected Build (Internal Mode)
 *
 * Compiled into a DLL, embedded as a raw binary blob inside the
 * PHANTOM EFI/kernel payload. The kernel driver manually maps this
 * DLL into FiveM's address space. No file touches disk.
 *
 * PhantomEntry() is called by the kernel injector's shellcode after
 * mapping. DllMain is the fallback for dev/testing with a normal
 * injector.
 *
 * Internal mode: we're inside FiveM's process, so all memory reads
 * are direct pointer dereferences — no cross-process comm needed.
 */

#include <Windows.h>

#include "Cheat/Cheat.hpp"
#include "Cheat/Features/Misc/Protection.hpp"

static HMODULE g_Module  = NULL;
static bool    g_Running = false;

static void CheatThread(HMODULE hModule)
{
    Sleep(3000);

    g_Running = true;

    Cheat::Protection::ApplyEarly();

    Cheat::Initialize();

    g_Running = false;
}

extern "C" __declspec(dllexport) void PhantomEntry(HMODULE hModule)
{
    g_Module = hModule;
    CheatThread(hModule);
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        g_Module = hModule;
        DisableThreadLibraryCalls(hModule);

        CreateThread(NULL, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(CheatThread),
            hModule, 0, NULL);
        break;

    case DLL_PROCESS_DETACH:
        if (g_Running)
            Cheat::ShutDown();
        break;
    }

    return TRUE;
}
