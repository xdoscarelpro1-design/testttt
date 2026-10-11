#pragma once

#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

/*
 * PHANTOM Memory Layer — INTERNAL MODE
 *
 * The DLL lives inside FiveM's address space (kernel-injected at boot).
 * ReadMemory<T> is a direct pointer dereference behind SEH.
 * WriteMemory<T> does a direct write, falling back to VirtualProtect
 * for code/read-only pages. No cross-process handles, no PID, no
 * OpenProcess. PhantomComm remains available through its own header
 * for operations that genuinely need ring-0.
 */

namespace FrameWork
{
    namespace Memory
    {
        HWND GetGameWindow();

        uint64_t GetModuleBase();
        uint64_t GetModuleBase(const wchar_t *ModuleName);
        uint64_t GetModuleSize(const wchar_t *ModuleName);

        int DetectBuildNumber();

        uint64_t AllocateMemory(SIZE_T Size);
        uint64_t AllocateMemoryNear(uint64_t NearAddress, SIZE_T Size);
        void FreeProcessMemory(uint64_t Address, SIZE_T Size);

        uint64_t CreateCodeCave(size_t Size);
        bool FreeCave(uintptr_t Addr);

        std::vector<uint8_t> ReadBytes(uintptr_t Addr, size_t Size);
        bool WriteBytes(uintptr_t Addr, const std::vector<uint8_t>& Bytes);
        bool HookJump(uintptr_t HookAddress, uintptr_t JmpToAddress);

        inline void ReadProcessMemoryImpl(uint64_t ReadAddress, LPVOID Read, SIZE_T Size)
        {
            __try {
                memcpy(Read, (const void *)ReadAddress, Size);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                memset(Read, 0, Size);
            }
        }

        inline bool WriteProcessMemoryImpl(uint64_t WriteAddress, LPVOID Write, SIZE_T Size)
        {
            __try {
                memcpy((void *)WriteAddress, Write, Size);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}

            DWORD oldProt = 0;
            if (!VirtualProtect((LPVOID)WriteAddress, Size, PAGE_EXECUTE_READWRITE, &oldProt))
                return false;

            __try {
                memcpy((void *)WriteAddress, Write, Size);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                VirtualProtect((LPVOID)WriteAddress, Size, oldProt, &oldProt);
                return false;
            }

            VirtualProtect((LPVOID)WriteAddress, Size, oldProt, &oldProt);
            return true;
        }

        inline bool ReadRawString(uint64_t ReadAddress, char* outBuf, size_t bufSize, size_t* outLen)
        {
            __try {
                const char *str = (const char *)ReadAddress;
                size_t len = 0;
                while (len < bufSize - 1 && str[len] != '\0')
                    len++;
                memcpy(outBuf, str, len);
                outBuf[len] = '\0';
                *outLen = len;
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                outBuf[0] = '\0';
                *outLen = 0;
                return false;
            }
        }

        inline std::string ReadProcessMemoryString(uint64_t ReadAddress, SIZE_T StringSize = 256)
        {
            char buf[512];
            size_t len = 0;
            size_t cap = (StringSize < sizeof(buf)) ? StringSize : sizeof(buf) - 1;
            ReadRawString(ReadAddress, buf, cap + 1, &len);
            return std::string(buf, len);
        }

        template <typename T, typename B>
        T ReadMemory(B ReadAddress)
        {
            if ((uint64_t)ReadAddress < 0x1000)
                return T{};
            __try {
                return *(T *)(uint64_t)ReadAddress;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return T{};
            }
        }

        template <typename T, typename B>
        bool WriteMemory(B WriteAddress, T Value)
        {
            return WriteProcessMemoryImpl((uint64_t)WriteAddress, &Value, sizeof(T));
        }
    }
}
