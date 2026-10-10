#pragma once

#include <FrameWork/FrameWork.hpp>

namespace FrameWork
{
	namespace Memory
	{
		HWND GetWindowHandleByPID(int Pid);
		HWND GetWindowHandleByName(std::wstring WindowName);
		DWORD GetProcessPidByName(std::wstring ProcessName);
		DWORD FindFiveMProcess(int& buildNumber, std::wstring& processName);
		bool IsProcessRunning(DWORD Pid);
		uint64_t GetModuleBaseByName(DWORD Pid, std::wstring ModuleName);

		void AttachProces(DWORD Pid);
		void DetachProcess();

		uint64_t AllocateMemory(SIZE_T Size);

		// Reserves executable memory within +/-2GB of NearAddress so a 5-byte
		// E9 rel32 can actually reach it. Falls back to a normal alloc only if
		// the whole window is exhausted — callers MUST verify the range.
		uint64_t AllocateMemoryNear(uint64_t NearAddress, SIZE_T Size);
		void FreeProcessMemory(uint64_t Address, SIZE_T Size);

		void ReadProcessMemoryImpl(uint64_t ReadAddress, LPVOID Read, SIZE_T Size);
		bool WriteProcessMemoryImpl(uint64_t WriteAddress, LPVOID Write, SIZE_T Size);

		std::string ReadProcessMemoryString(uint64_t ReadAddress, SIZE_T StringSize = 256);

		template <typename T, typename B>
		T ReadMemory(B ReadAddress)
		{
			T Read;
			ReadProcessMemoryImpl((uint64_t)ReadAddress, &Read, sizeof(T));
			return Read;
		}

		template <typename T, typename B>
		bool WriteMemory(B WriteAddress, T Value)
		{
			return WriteProcessMemoryImpl((uint64_t)WriteAddress, &Value, sizeof(T));
		}
	}
}