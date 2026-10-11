#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace FrameWork
{
    namespace SigScanner
    {
        uint64_t FindPattern(uint64_t moduleBase, uint64_t moduleSize, const char *pattern);
        uint64_t ResolveRelative(uint64_t address, int instructionLength);
        uint64_t GetModuleSize(uint64_t moduleBase);
    }
}
