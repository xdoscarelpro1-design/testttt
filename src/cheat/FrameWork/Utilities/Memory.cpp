#include "Memory.hpp"
#include "../../PhantomComm.hpp"
#include <TlHelp32.h>
#include <algorithm>
#include <string>

namespace FrameWork
{
    namespace Memory
    {
        static DWORD  s_Pid = 0;
        static HANDLE s_Handle = NULL;

        HWND GetWindowHandleByPID(int Pid)
        {
            struct FindData { DWORD pid; HWND result; };
            FindData data = { (DWORD)Pid, NULL };

            EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                FindData* fd = (FindData*)lParam;
                DWORD winPid = 0;
                GetWindowThreadProcessId(hwnd, &winPid);
                if (winPid == fd->pid && IsWindowVisible(hwnd))
                {
                    fd->result = hwnd;
                    return FALSE;
                }
                return TRUE;
            }, (LPARAM)&data);

            return data.result;
        }

        HWND GetWindowHandleByName(std::wstring WindowName)
        {
            return FindWindowW(NULL, WindowName.c_str());
        }

        DWORD GetProcessPidByName(std::wstring ProcessName)
        {
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap == INVALID_HANDLE_VALUE) return 0;

            PROCESSENTRY32W pe = { sizeof(pe) };
            DWORD pid = 0;

            std::wstring targetLower = ProcessName;
            std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::towlower);

            if (Process32FirstW(snap, &pe))
            {
                do
                {
                    std::wstring current(pe.szExeFile);
                    std::transform(current.begin(), current.end(), current.begin(), ::towlower);
                    if (current == targetLower)
                    {
                        pid = pe.th32ProcessID;
                        break;
                    }
                } while (Process32NextW(snap, &pe));
            }
            CloseHandle(snap);
            return pid;
        }

        DWORD FindFiveMProcess(int& buildNumber, std::wstring& processName)
        {
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap == INVALID_HANDLE_VALUE) return 0;

            PROCESSENTRY32W pe = { sizeof(pe) };
            DWORD resultPid = 0;

            if (Process32FirstW(snap, &pe))
            {
                do
                {
                    std::wstring name(pe.szExeFile);
                    std::wstring nameLower = name;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);

                    if (nameLower.find(L"fivem") != std::wstring::npos ||
                        nameLower.find(L"gta5") != std::wstring::npos ||
                        nameLower.find(L"fxserver") != std::wstring::npos)
                    {
                        if (nameLower.find(L"b") != std::wstring::npos)
                        {
                            size_t pos = nameLower.find(L"b");
                            if (pos != std::wstring::npos && pos + 1 < nameLower.size())
                            {
                                std::wstring buildStr;
                                for (size_t i = pos + 1; i < nameLower.size(); i++)
                                {
                                    if (nameLower[i] >= L'0' && nameLower[i] <= L'9')
                                        buildStr += nameLower[i];
                                    else
                                        break;
                                }
                                if (!buildStr.empty())
                                    buildNumber = _wtoi(buildStr.c_str());
                            }
                        }

                        processName = name;
                        resultPid = pe.th32ProcessID;
                        break;
                    }
                } while (Process32NextW(snap, &pe));
            }
            CloseHandle(snap);
            return resultPid;
        }

        bool IsProcessRunning(DWORD Pid)
        {
            HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, Pid);
            if (!proc) return false;

            DWORD exitCode = 0;
            GetExitCodeProcess(proc, &exitCode);
            CloseHandle(proc);
            return exitCode == STILL_ACTIVE;
        }

        uint64_t GetModuleBaseByName(DWORD Pid, std::wstring ModuleName)
        {
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, Pid);
            if (snap == INVALID_HANDLE_VALUE) return 0;

            MODULEENTRY32W me = { sizeof(me) };
            uint64_t base = 0;

            std::wstring targetLower = ModuleName;
            std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::towlower);

            if (Module32FirstW(snap, &me))
            {
                do
                {
                    std::wstring current(me.szModule);
                    std::transform(current.begin(), current.end(), current.begin(), ::towlower);
                    if (current == targetLower)
                    {
                        base = (uint64_t)me.modBaseAddr;
                        break;
                    }
                } while (Module32NextW(snap, &me));
            }
            CloseHandle(snap);
            return base;
        }

        void AttachProces(DWORD Pid)
        {
            s_Pid = Pid;
            s_Handle = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE |
                                   PROCESS_QUERY_INFORMATION, FALSE, Pid);
        }

        void DetachProcess()
        {
            if (s_Handle)
            {
                CloseHandle(s_Handle);
                s_Handle = NULL;
            }
            s_Pid = 0;
        }

        uint64_t AllocateMemory(SIZE_T Size)
        {
            if (!s_Handle) return 0;
            LPVOID mem = VirtualAllocEx(s_Handle, NULL, Size,
                                        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            return (uint64_t)mem;
        }

        uint64_t AllocateMemoryNear(uint64_t NearAddress, SIZE_T Size)
        {
            if (!s_Handle) return 0;

            uintptr_t minAddr = (NearAddress > 0x70000000) ? (NearAddress - 0x70000000) : 0x10000;
            uintptr_t maxAddr = NearAddress + 0x70000000;

            MEMORY_BASIC_INFORMATION mbi = {};
            uintptr_t addr = minAddr;

            while (addr < maxAddr)
            {
                if (VirtualQueryEx(s_Handle, (LPCVOID)addr, &mbi, sizeof(mbi)) == 0)
                    break;

                if (mbi.State == MEM_FREE && mbi.RegionSize >= Size)
                {
                    LPVOID mem = VirtualAllocEx(s_Handle, (LPVOID)addr, Size,
                                               MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                    if (mem)
                        return (uint64_t)mem;
                }

                addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
            }

            return AllocateMemory(Size);
        }

        void FreeProcessMemory(uint64_t Address, SIZE_T Size)
        {
            if (!s_Handle) return;
            VirtualFreeEx(s_Handle, (LPVOID)Address, 0, MEM_RELEASE);
        }

        void ReadProcessMemoryImpl(uint64_t ReadAddress, LPVOID Read, SIZE_T Size)
        {
            if (PhantomComm::IsConnected() && s_Pid)
            {
                PhantomComm::ReadMemory(s_Pid, (void*)ReadAddress, Read, Size);
                return;
            }

            if (s_Handle)
                ReadProcessMemory(s_Handle, (LPCVOID)ReadAddress, Read, Size, NULL);
        }

        bool WriteProcessMemoryImpl(uint64_t WriteAddress, LPVOID Write, SIZE_T Size)
        {
            if (PhantomComm::IsConnected() && s_Pid)
                return PhantomComm::WriteMemory(s_Pid, (void*)WriteAddress, Write, Size);

            if (!s_Handle) return false;
            SIZE_T written = 0;
            return WriteProcessMemory(s_Handle, (LPVOID)WriteAddress, Write, Size, &written) != 0;
        }

        std::string ReadProcessMemoryString(uint64_t ReadAddress, SIZE_T StringSize)
        {
            std::string result(StringSize, '\0');
            ReadProcessMemoryImpl(ReadAddress, &result[0], StringSize);
            size_t null_pos = result.find('\0');
            if (null_pos != std::string::npos)
                result.resize(null_pos);
            return result;
        }
    }
}
