#include "SigScanner.hpp"

namespace FrameWork
{
    namespace SigScanner
    {
        static void ParsePattern(const char *pattern,
                                 std::vector<uint8_t> &bytes,
                                 std::vector<bool> &mask)
        {
            bytes.clear();
            mask.clear();

            const char *p = pattern;
            while (*p) {
                while (*p == ' ') p++;
                if (!*p) break;

                if (*p == '?') {
                    bytes.push_back(0x00);
                    mask.push_back(false);
                    p++;
                    if (*p == '?') p++;
                } else {
                    char hex[3] = { p[0], p[1], 0 };
                    bytes.push_back((uint8_t)strtoul(hex, nullptr, 16));
                    mask.push_back(true);
                    p += 2;
                }
            }
        }

        uint64_t FindPattern(uint64_t moduleBase, uint64_t moduleSize, const char *pattern)
        {
            std::vector<uint8_t> patternBytes;
            std::vector<bool> patternMask;
            ParsePattern(pattern, patternBytes, patternMask);

            if (patternBytes.empty()) return 0;

            const size_t patLen = patternBytes.size();
            const uint8_t *scan = (const uint8_t *)moduleBase;
            const uint8_t *scanEnd = scan + moduleSize - patLen;

            __try {
                for (const uint8_t *cur = scan; cur <= scanEnd; cur++) {
                    bool found = true;
                    for (size_t j = 0; j < patLen; j++) {
                        if (patternMask[j] && cur[j] != patternBytes[j]) {
                            found = false;
                            break;
                        }
                    }
                    if (found)
                        return (uint64_t)cur;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}

            return 0;
        }

        uint64_t ResolveRelative(uint64_t address, int instructionLength)
        {
            __try {
                int32_t displacement = *(int32_t *)(address + instructionLength - 4);
                return address + instructionLength + displacement;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

        uint64_t GetModuleSize(uint64_t moduleBase)
        {
            __try {
                auto dos = (IMAGE_DOS_HEADER *)moduleBase;
                if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
                auto nt = (IMAGE_NT_HEADERS *)((uint8_t *)moduleBase + dos->e_lfanew);
                if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
                return nt->OptionalHeader.SizeOfImage;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }
    }
}
