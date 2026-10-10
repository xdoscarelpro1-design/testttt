#pragma once

#include <Windows.h>
#include <cstdint>

namespace Cheat
{
    namespace Protection
    {
        void ApplyEarly();

        void RunPeriodic();

        void WipeImportDirectory();

        bool IsUnderDebugger();
    }
}
