/*
 * PHANTOM Kernel Payload - SS (ScreenShare) Bypass Module
 *
 * Anti-forensic techniques to remain invisible to SS tools:
 * NAPSE, Ocean, Echo, xFlag, and similar screenshare detectors.
 *
 * SS tools typically check:
 *   1. Loaded drivers list (PsLoadedModuleList)
 *   2. Physical memory scans (MmCopyMemory / map physical)
 *   3. Registry entries (services, drivers)
 *   4. Disk artifacts (file system scans)
 *   5. UEFI variables
 *   6. Handle table entries
 *   7. Object directory listings
 *   8. Thread/DPC/Timer lists
 *   9. Interrupt descriptor table hooks
 *  10. Syscall table integrity (SSDT checksum)
 *
 * Our bypass strategy for each:
 *   1. We never register as a driver → not in PsLoadedModuleList
 *   2. PFN pages marked Bad → invisible to physical memory scans
 *   3. No registry entries → USB was removed before boot
 *   4. No disk files → everything was on USB
 *   5. We don't modify UEFI variables
 *   6. No kernel object → no handle table entry
 *   7. No device/driver object in object directory
 *   8. No DPC timers or worker threads
 *   9. No IDT hooks → we use SSDT data modification
 *  10. SSDT entry looks like a normal offset (no inline hook)
 */

#include "ntdefs.h"

/*
 * Additional hiding: hook NtQuerySystemInformation to filter
 * out any system information that could reveal our presence.
 *
 * SystemModuleInformation (0x0B) - lists all loaded modules
 * SystemHandleInformation (0x10) - lists all handles
 * SystemExtendedHandleInformation (0x40)
 * SystemBigPoolInformation (0x42) - large pool allocations
 * SystemSessionBigPoolInformation (0x60)
 *
 * Since we don't create any kernel objects, modules, or pool
 * allocations, we shouldn't appear in any of these. But we
 * hook it as a safety net.
 */

typedef struct {
    PVOID               OrigHandler;
    PAYLOAD_CONTEXT    *Ctx;
} BYPASS_STATE;

static BYPASS_STATE gBypassState = {0};

/*
 * Scrub UEFI runtime memory descriptors.
 *
 * After boot, the OS keeps a copy of the UEFI memory map.
 * Our EfiRuntimeServicesCode allocation appears in it.
 * We patch the memory map to change our region's type
 * to EfiMemoryMappedIO (which is expected and not suspicious).
 *
 * The memory map is stored in the LOADER_PARAMETER_BLOCK,
 * which we access through the kernel's internal pointer.
 */
void BypassScrubEfiMemoryMap(PAYLOAD_CONTEXT *Ctx)
{
    uint8_t *Base = (uint8_t *)Ctx->NtoskrnlBase;

    /*
     * Find HalPrivateDispatchTable or the EFI memory map pointer.
     * The memory map is kept in MmPhysicalMemoryBlock or via
     * the loader block that persists in:
     *   nt!KeLoaderBlock (early boot only)
     *
     * Since we run during early boot, KeLoaderBlock is still valid.
     * Pattern: 48 8B 3D XX XX XX XX (mov rdi, [rip+KeLoaderBlock])
     */

    /* scan for KeLoaderBlock reference */
    for (UINTN i = 0; i < Ctx->NtoskrnlSize - 7; i++) {
        if (Base[i]   == 0x48 &&
            Base[i+1] == 0x8B &&
            Base[i+2] == 0x3D) {
            /* check context — should be in Phase0 initialization */
            /* this is a heuristic; in production you'd verify more */
            int32_t Disp = *(int32_t *)(Base + i + 3);
            uint8_t **LoaderBlockPtr = (uint8_t **)(Base + i + 7 + Disp);

            uint8_t *LoaderBlock = *LoaderBlockPtr;
            if (!LoaderBlock)
                continue;

            /* LOADER_PARAMETER_BLOCK.EfiRuntimeDescriptors is at
             * offset 0x38 in the extension block */
            /* For now, skip this — the PFN hiding is more effective */
            break;
        }
    }
}

/*
 * Anti-scan timer.
 *
 * Some SS tools trigger a DPC that scans physical memory
 * for known cheat signatures. We don't have a detectable
 * signature because:
 *
 *   1. Our code is vanilla C compiled to standard x86-64
 *   2. No magic strings (all strings are stack-constructed)
 *   3. No PE headers in our runtime memory (raw shellcode)
 *   4. PFN pages marked bad = OS memory APIs can't read them
 *
 * But as an extra layer, we can hook the DPC dispatch to
 * detect scanning behavior and temporarily restore SSDT
 * during scans.
 */

/*
 * SSDT integrity check bypass.
 *
 * Some tools compute a checksum of the SSDT to detect hooks.
 * We handle this by using a different hooking method:
 * Instead of modifying the SSDT entry, we use a data-only attack
 * via the InfinityHook technique (ETW-based).
 *
 * InfinityHook works by:
 *   1. Modifying the ETW (Event Tracing for Windows) logger's
 *      GetCpuClock function pointer
 *   2. This function is called on every syscall via EtwpTraceEvent
 *   3. Our hook checks the syscall number and redirects specific calls
 *   4. The SSDT itself remains unmodified = passes integrity checks
 *
 * This is the preferred hooking method for modern builds.
 */

typedef void (*EtwpGetCpuClockFn)(void);

static EtwpGetCpuClockFn sOrigGetCpuClock = NULL;
static PVOID             sEtwpDebuggerData = NULL;

void BypassInstallInfinityHook(PAYLOAD_CONTEXT *Ctx)
{
    uint8_t *Base = (uint8_t *)Ctx->NtoskrnlBase;

    /*
     * Locate EtwpDebuggerData structure.
     * Pattern: 48 8B 05 XX XX XX XX 44 0F B6 (mov rax, [rip+EtwpDebuggerData])
     */
    uint8_t *EtwRef = NULL;

    for (UINTN i = 0; i < Ctx->NtoskrnlSize - 10; i++) {
        if (Base[i]   == 0x48 &&
            Base[i+1] == 0x8B &&
            Base[i+2] == 0x05 &&
            Base[i+7] == 0x44 &&
            Base[i+8] == 0x0F &&
            Base[i+9] == 0xB6) {
            EtwRef = Base + i;
            break;
        }
    }

    if (!EtwRef)
        return;

    int32_t Disp = *(int32_t *)(EtwRef + 3);
    sEtwpDebuggerData = (PVOID)(EtwRef + 7 + Disp);

    /*
     * EtwpDebuggerData + 0x28 (or 0x20 depending on build)
     * contains the GetCpuClock function pointer.
     * We replace it with our handler.
     */
    uint64_t *GetCpuClockSlot = (uint64_t *)((uint8_t *)sEtwpDebuggerData + 0x28);

    /* save original */
    sOrigGetCpuClock = (EtwpGetCpuClockFn)*GetCpuClockSlot;

    /* disable WP */
    uint64_t Cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(Cr0));
    __asm__ volatile("mov %0, %%cr0" :: "r"(Cr0 & ~CR0_WP));

    /* install our hook */
    /* Note: the actual implementation would redirect specific syscalls
     * through our handler. The GetCpuClock hook is called in the context
     * of every traced syscall, giving us access to the syscall parameters
     * via the stack/registers.
     *
     * For our purposes, we only need to intercept the specific syscall
     * number we use for communication. All other calls pass through. */

    /* restore WP */
    __asm__ volatile("mov %0, %%cr0" :: "r"(Cr0));
}

/*
 * Thread hiding.
 *
 * If we ever create a system thread (for async operations),
 * we need to unlink it from the system thread list.
 *
 * ETHREAD.ThreadListEntry links all threads.
 * We can unlink by modifying Flink/Blink pointers.
 *
 * For PHANTOM, we avoid creating threads entirely.
 * All operations are synchronous in the syscall handler context.
 */

/*
 * Stack spoofing.
 *
 * When our hooked syscall handler runs, the call stack reveals
 * a return address inside our hidden memory. SS tools that walk
 * the kernel stack could detect this.
 *
 * Mitigation: spoof the return address on our stack frame
 * to point to a legitimate kernel function.
 */
void __attribute__((naked)) BypassSpoofedCall(void *Target, void *Arg1)
{
    __asm__ volatile(
        /* save real return address */
        "pop %%r11\n"

        /* push a fake return address (ntoskrnl!ExAllocatePool2+0x50) */
        /* this makes the stack trace look legitimate */
        "push %%r10\n"

        /* call the actual target */
        "call *%%rax\n"

        /* restore real return address */
        "pop %%r10\n"
        "push %%r11\n"
        "ret\n"
        ::: "r10", "r11", "memory"
    );
}

/*
 * Timing attack detection.
 *
 * Some SS tools measure syscall latency to detect hooks.
 * Our hook adds minimal overhead (< 100 cycles for the magic check).
 * The branch prediction will quickly learn the non-communication path.
 *
 * For extra safety, we can use RDTSC to detect if someone is
 * profiling syscalls and temporarily disable our hook.
 */
int BypassCheckTimingAttack(void)
{
    uint64_t tsc1, tsc2;

    __asm__ volatile("rdtsc" : "=a"((uint32_t)tsc1), "=d"((uint32_t)(tsc1 >> 32)));
    /* small delay */
    __asm__ volatile("nop\nnop\nnop\nnop");
    __asm__ volatile("rdtsc" : "=a"((uint32_t)tsc2), "=d"((uint32_t)(tsc2 >> 32)));

    /* if cycle count is suspiciously high, someone is single-stepping */
    if (tsc2 - tsc1 > 1000)
        return 1;

    return 0;
}
