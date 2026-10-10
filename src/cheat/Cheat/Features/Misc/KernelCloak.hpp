#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>

namespace Cheat
{
    namespace KernelCloak
    {
        inline bool IsKernelCloakActive() { return false; }
        inline bool ApplyFullKernelCloak(DWORD) { return false; }
        inline void UnloadVulnDriver() {}
        inline void HookWindowPIDQuery(HWND, DWORD) {}
        inline bool LoadVulnDriver() { return false; }
    }
}
