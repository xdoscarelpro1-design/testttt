/*
 * PHANTOM DLL Entry Point — Kernel-Injected Build
 *
 * This file is compiled into a DLL, then embedded as a raw binary blob
 * inside the PHANTOM EFI/kernel payload. When FiveM launches (detected
 * by the kernel driver), the payload manually maps this DLL into the
 * target process's address space. No file ever touches disk.
 *
 * PhantomEntry() is the primary entry point. The kernel injector's
 * shellcode resolves it by export name and calls it directly after
 * mapping. DllMain is provided only as a fallback for normal PE
 * loading during development and testing.
 *
 * NOTE: Cheat::Initialize() currently shows a MessageBox on PhantomComm
 * failure. For the injected scenario this should be replaced with silent
 * retries — the kernel driver is guaranteed running (booted from USB).
 * That change belongs in Cheat.cpp, not here.
 */

#include <Windows.h>

#include "PhantomComm.hpp"
#include "Cheat/Cheat.hpp"
#include "Cheat/Features/Misc/Protection.hpp"

static HMODULE g_Module  = NULL;
static bool    g_Running = false;

// ── Main cheat thread ───────────────────────────────────────────────
// Runs after the DLL has been mapped into the target process.
static void CheatThread(HMODULE hModule)
{
    // Let the host process stabilize after injection
    Sleep(3000);

    g_Running = true;

    // The kernel driver is already loaded from EFI boot —
    // open the SSDT-hook comm channel
    PhantomComm::Initialize();

    // Anti-debug / anti-tamper before anything visible happens
    Cheat::Protection::ApplyEarly();

    // We skip Bypass::CacheRealPath() — there is no executable on disk
    // to cache; the DLL exists only in memory.

    // Initialize overlay, features, and enter the render loop.
    // This call blocks until shutdown.
    Cheat::Initialize();

    g_Running = false;
}

// ── Primary entry point (called by kernel injector shellcode) ───────
extern "C" __declspec(dllexport) void PhantomEntry(HMODULE hModule)
{
    g_Module = hModule;
    CheatThread(hModule);
}

// ── Fallback: standard DLL entry (dev / normal-load path) ──────────
BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        g_Module = hModule;
        DisableThreadLibraryCalls(hModule);

        // Launch on a separate thread so DllMain returns before the
        // loader lock causes a deadlock.
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
