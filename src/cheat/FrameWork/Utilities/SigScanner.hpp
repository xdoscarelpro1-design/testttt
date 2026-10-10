#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace FrameWork
{
	namespace SigScanner
	{
		// Scan remote process module for byte pattern. ? = wildcard
		// Returns absolute address of match, or 0
		uint64_t FindPattern(HANDLE hProcess, uint64_t moduleBase, uint64_t moduleSize, const char* pattern);

		// Resolve RIP-relative address: result = address + instrLen + *(int32_t*)(address + instrLen - 4)
		uint64_t ResolveRelative(HANDLE hProcess, uint64_t address, int instructionLength);

		// Get SizeOfImage from remote PE headers
		uint64_t GetModuleSize(HANDLE hProcess, uint64_t moduleBase);
	}
}
