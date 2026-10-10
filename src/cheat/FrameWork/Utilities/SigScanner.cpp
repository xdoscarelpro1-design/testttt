#include "SigScanner.hpp"

namespace FrameWork
{
	namespace SigScanner
	{
		// Parse "48 8B 05 ? ? ? ?" into bytes + mask
		static void ParsePattern(const char* pattern, std::vector<uint8_t>& bytes, std::vector<bool>& mask)
		{
			bytes.clear();
			mask.clear();

			const char* p = pattern;
			while (*p)
			{
				// Skip whitespace
				while (*p == ' ') p++;
				if (!*p) break;

				if (*p == '?')
				{
					bytes.push_back(0x00);
					mask.push_back(false); // wildcard
					p++;
					if (*p == '?') p++; // handle "??" as single wildcard
				}
				else
				{
					// Parse hex byte
					char hex[3] = { p[0], p[1], 0 };
					bytes.push_back((uint8_t)strtoul(hex, nullptr, 16));
					mask.push_back(true); // must match
					p += 2;
				}
			}
		}

		uint64_t FindPattern(HANDLE hProcess, uint64_t moduleBase, uint64_t moduleSize, const char* pattern)
		{
			std::vector<uint8_t> patternBytes;
			std::vector<bool> patternMask;
			ParsePattern(pattern, patternBytes, patternMask);

			if (patternBytes.empty())
				return 0;

			const SIZE_T chunkSize = 0x10000; // 64KB chunks
			const SIZE_T patLen = patternBytes.size();
			std::vector<uint8_t> buffer(chunkSize + patLen); // overlap for boundary matches

			for (uint64_t offset = 0; offset < moduleSize; offset += chunkSize)
			{
				SIZE_T readSize = min(chunkSize + patLen, moduleSize - offset);
				SIZE_T bytesRead = 0;

				if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + offset), buffer.data(), readSize, &bytesRead))
					continue;

				if (bytesRead < patLen)
					continue;

				// Scan this chunk
				for (SIZE_T i = 0; i <= bytesRead - patLen; i++)
				{
					bool found = true;
					for (SIZE_T j = 0; j < patLen; j++)
					{
						if (patternMask[j] && buffer[i + j] != patternBytes[j])
						{
							found = false;
							break;
						}
					}

					if (found)
						return moduleBase + offset + i;
				}
			}

			return 0;
		}

		uint64_t ResolveRelative(HANDLE hProcess, uint64_t address, int instructionLength)
		{
			int32_t displacement = 0;
			SIZE_T bytesRead = 0;

			// Read the 4-byte displacement at the end of the instruction
			if (!ReadProcessMemory(hProcess, (LPCVOID)(address + instructionLength - 4), &displacement, sizeof(displacement), &bytesRead))
				return 0;

			// RIP-relative: result = instruction_end + displacement
			return address + instructionLength + displacement;
		}

		uint64_t GetModuleSize(HANDLE hProcess, uint64_t moduleBase)
		{
			IMAGE_DOS_HEADER dosHeader;
			SIZE_T bytesRead = 0;

			if (!ReadProcessMemory(hProcess, (LPCVOID)moduleBase, &dosHeader, sizeof(dosHeader), &bytesRead))
				return 0;

			if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
				return 0;

			IMAGE_NT_HEADERS ntHeaders;
			if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + dosHeader.e_lfanew), &ntHeaders, sizeof(ntHeaders), &bytesRead))
				return 0;

			if (ntHeaders.Signature != IMAGE_NT_SIGNATURE)
				return 0;

			return ntHeaders.OptionalHeader.SizeOfImage;
		}
	}
}
