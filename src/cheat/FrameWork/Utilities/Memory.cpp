#include "Memory.hpp"
#include <Psapi.h>

namespace FrameWork
{
    namespace Memory
    {
        HWND GetGameWindow()
        {
            HWND hw = FindWindowA("grcWindow", NULL);
            if (hw) return hw;

            struct FindData { DWORD pid; HWND result; };
            FindData data = { GetCurrentProcessId(), NULL };

            EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                FindData *fd = (FindData *)lParam;
                DWORD winPid = 0;
                GetWindowThreadProcessId(hwnd, &winPid);
                if (winPid == fd->pid && IsWindowVisible(hwnd)) {
                    fd->result = hwnd;
                    return FALSE;
                }
                return TRUE;
            }, (LPARAM)&data);

            return data.result;
        }

        uint64_t GetModuleBase()
        {
            return (uint64_t)GetModuleHandleW(NULL);
        }

        uint64_t GetModuleBase(const wchar_t *ModuleName)
        {
            return (uint64_t)GetModuleHandleW(ModuleName);
        }

        uint64_t GetModuleSize(const wchar_t *ModuleName)
        {
            HMODULE hMod = ModuleName ? GetModuleHandleW(ModuleName) : GetModuleHandleW(NULL);
            if (!hMod) return 0;

            MODULEINFO mi = {};
            if (GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi)))
                return mi.SizeOfImage;

            auto dos = (IMAGE_DOS_HEADER *)hMod;
            if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
            auto nt = (IMAGE_NT_HEADERS *)((uint8_t *)hMod + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
            return nt->OptionalHeader.SizeOfImage;
        }

        int DetectBuildNumber()
        {
            wchar_t path[MAX_PATH] = {};
            GetModuleFileNameW(NULL, path, MAX_PATH);

            std::wstring name(path);
            size_t slash = name.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
                name = name.substr(slash + 1);

            /* FiveM executables: FiveM_b2802_GTAProcess.exe, fivem_b3095.exe, etc. */
            std::wstring lower = name;
            for (auto &c : lower) c = towlower(c);

            size_t bpos = lower.find(L"b");
            while (bpos != std::wstring::npos) {
                if (bpos + 1 < lower.size() && lower[bpos + 1] >= L'0' && lower[bpos + 1] <= L'9') {
                    std::wstring digits;
                    for (size_t i = bpos + 1; i < lower.size() && lower[i] >= L'0' && lower[i] <= L'9'; i++)
                        digits += lower[i];
                    if (!digits.empty()) {
                        int build = _wtoi(digits.c_str());
                        if (build >= 1000) return build;
                    }
                }
                bpos = lower.find(L"b", bpos + 1);
            }
            return 0;
        }

        uint64_t AllocateMemory(SIZE_T Size)
        {
            LPVOID mem = VirtualAlloc(NULL, Size,
                                      MEM_COMMIT | MEM_RESERVE,
                                      PAGE_EXECUTE_READWRITE);
            return (uint64_t)mem;
        }

        uint64_t AllocateMemoryNear(uint64_t NearAddress, SIZE_T Size)
        {
            uintptr_t minAddr = (NearAddress > 0x70000000)
                                    ? (NearAddress - 0x70000000)
                                    : 0x10000;
            uintptr_t maxAddr = NearAddress + 0x70000000;

            MEMORY_BASIC_INFORMATION mbi = {};
            uintptr_t addr = minAddr;

            while (addr < maxAddr)
            {
                if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) == 0)
                    break;

                if (mbi.State == MEM_FREE && mbi.RegionSize >= Size) {
                    LPVOID mem = VirtualAlloc((LPVOID)addr, Size,
                                              MEM_COMMIT | MEM_RESERVE,
                                              PAGE_EXECUTE_READWRITE);
                    if (mem)
                        return (uint64_t)mem;
                }
                addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
            }

            return AllocateMemory(Size);
        }

        void FreeProcessMemory(uint64_t Address, SIZE_T)
        {
            VirtualFree((LPVOID)Address, 0, MEM_RELEASE);
        }

        uint64_t CreateCodeCave(size_t Size)
        {
            return AllocateMemory(Size);
        }

        bool FreeCave(uintptr_t Addr)
        {
            return VirtualFree((LPVOID)Addr, 0, MEM_RELEASE) != 0;
        }

        static bool ReadBytesRaw(uintptr_t Addr, void* outBuf, size_t Size)
        {
            __try {
                memcpy(outBuf, (const void *)Addr, Size);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        std::vector<uint8_t> ReadBytes(uintptr_t Addr, size_t Size)
        {
            std::vector<uint8_t> result(Size, 0);
            if (!ReadBytesRaw(Addr, result.data(), Size))
                result.clear();
            return result;
        }

        bool WriteBytes(uintptr_t Addr, const std::vector<uint8_t> &Bytes)
        {
            return WriteProcessMemoryImpl(Addr, (LPVOID)Bytes.data(), Bytes.size());
        }

        bool HookJump(uintptr_t HookAddress, uintptr_t JmpToAddress)
        {
            /* FF 25 00 00 00 00 [8-byte absolute address] — 14-byte jmp */
            uint8_t patch[14];
            patch[0] = 0xFF;
            patch[1] = 0x25;
            patch[2] = patch[3] = patch[4] = patch[5] = 0x00;
            memcpy(patch + 6, &JmpToAddress, 8);
            return WriteBytes(HookAddress, std::vector<uint8_t>(patch, patch + 14));
        }
    }
}
