#include "SilentAim.hpp"
#include "../../Options.hpp"

#include <cstring>
#include <cstdint>
#include <thread>
#include <chrono>

namespace Cheat {

    constexpr int HEAD_INDEX = 0;
    constexpr int NECK_INDEX = 1;
    constexpr int CHEST_INDEX = 2;

    // Prologue bytes we displace with our JMP — read dynamically from the target
    // since different builds compile the three movss loads with different registers.
    static uint8_t g_OriginalPrologue[17] = {};
    static size_t g_PrologueSize = 0;
    static bool g_PrologueRead = false;

    // All known prologue sizes are 17 bytes (3x movss from [reg] / [reg+4] / [reg+8])
    constexpr size_t kPrologueSize = 17;
    constexpr size_t kJmpSize = 5;

    // Cave layout. The coordinate immediates live at fixed offsets so the hot
    // path only ever rewrites 4 bytes at a time — never executable code.
    //
    //   +0x00  mov dword [r9],    X      41 C7 01 <imm32>
    //   +0x07  mov dword [r9+4],  Y      41 C7 41 04 <imm32>
    //   +0x0F  mov dword [r9+8],  Z      41 C7 41 08 <imm32>
    //   +0x17  <original prologue, 17 bytes>
    //   +0x28  jmp back to HandleBullet+5
    constexpr size_t kOffX = 0x03;
    constexpr size_t kOffY = 0x0B;
    constexpr size_t kOffZ = 0x13;
    constexpr size_t kOffPrologue = 0x17;
    constexpr size_t kOffJmpBack = kOffPrologue + kPrologueSize;   // 0x28
    constexpr size_t kCaveSize = kOffJmpBack + kJmpSize;           // 0x2D

    // Cave state. Built exactly once, torn down on restore.
    static uint64_t g_CavePtr = 0;
    static bool g_HookInstalled = false;

    static uint32_t FloatBits(float f)
    {
        uint32_t bits;
        std::memcpy(&bits, &f, sizeof(bits));
        return bits;
    }

    static uint32_t RelativeOffset(uint64_t from, uint64_t to, size_t instructionLength)
    {
        return static_cast<uint32_t>(to - (from + instructionLength));
    }

    // Verifies a 5-byte E9 can actually span from->to. Anything outside rel32
    // silently wraps and jumps into garbage, which is how you eat a crash.
    static bool InRel32Range(uint64_t from, uint64_t to, size_t instructionLength)
    {
        const int64_t delta = static_cast<int64_t>(to) - static_cast<int64_t>(from + instructionLength);
        return delta >= INT32_MIN && delta <= INT32_MAX;
    }

    // Allocates the cave and writes every byte of it — including the return
    // jump — before the hook is ever installed. Returns false if we can't get
    // memory within rel32 reach, in which case we install nothing at all.
    static bool EnsureCave()
    {
        if (g_CavePtr)
            return true;

        // Without a resolved HandleBullet we'd be writing a JMP into whatever
        // sits at ModuleBase — the read-only PE header on builds with no
        // offset entry. That write fails and bullets just go straight.
        if (!g_Fivem.IsHandleBulletValid())
            return false;

        const uint64_t handleBullet = g_Fivem.GetHandleBulletAddress();
        if (!handleBullet)
            return false;

        // Read the actual prologue bytes from the target — we need these to
        // replay them in the cave. Only read once.
        if (!g_PrologueRead)
        {
            FrameWork::Memory::ReadProcessMemoryImpl(handleBullet, g_OriginalPrologue, kPrologueSize);
            g_PrologueSize = kPrologueSize;
            g_PrologueRead = true;

            // Basic sanity: first byte should be 0xF3 (movss prefix)
            if (g_OriginalPrologue[0] != 0xF3)
                return false;
        }

        const uint64_t cave = FrameWork::Memory::AllocateMemoryNear(handleBullet, 0x100);
        if (!cave)
            return false;

        // Both directions must fit: hook -> cave, and cave -> hook+5.
        if (!InRel32Range(handleBullet, cave, kJmpSize) ||
            !InRel32Range(cave + kOffJmpBack, handleBullet + kJmpSize, kJmpSize))
        {
            return false;
        }

        uint8_t cave_bytes[kCaveSize] = {};

        // Detect which register the prologue uses as the bullet pointer.
        // movss xmm?,[r9] = F3 41 0F 10 ?9 (ModRM byte & 0x07 == 1 means r9)
        // movss xmm?,[r8] = F3 41 0F 10 ?0 (ModRM byte & 0x07 == 0 means r8)
        uint8_t modrmByte = g_OriginalPrologue[4]; // 5th byte of first movss
        uint8_t regBit = modrmByte & 0x07; // 0=r8, 1=r9

        // Build scaffold: mov dword [rX], imm32 / [rX+4], imm32 / [rX+8], imm32
        // r9: 41 C7 01 / 41 C7 41 04 / 41 C7 41 08
        // r8: 41 C7 00 / 41 C7 40 04 / 41 C7 40 08
        const uint8_t scaffold[] = {
            0x41, 0xC7, (uint8_t)(0x00 | regBit), 0x00, 0x00, 0x00, 0x00,
            0x41, 0xC7, (uint8_t)(0x40 | regBit), 0x04, 0x00, 0x00, 0x00, 0x00,
            0x41, 0xC7, (uint8_t)(0x40 | regBit), 0x08, 0x00, 0x00, 0x00, 0x00
        };
        std::memcpy(cave_bytes, scaffold, sizeof(scaffold));

        // Replay the instructions our JMP displaced.
        std::memcpy(cave_bytes + kOffPrologue, g_OriginalPrologue, kPrologueSize);

        // jmp back to HandleBullet+5 — PAST our own JMP. Landing on +0 would
        // re-enter the hook forever and hang the game thread.
        cave_bytes[kOffJmpBack] = 0xE9;
        const uint32_t backOffset = RelativeOffset(cave + kOffJmpBack, handleBullet + kJmpSize, kJmpSize);
        std::memcpy(cave_bytes + kOffJmpBack + 1, &backOffset, sizeof(backOffset));

        if (!FrameWork::Memory::WriteProcessMemoryImpl(cave, cave_bytes, sizeof(cave_bytes)))
            return false;

        g_CavePtr = cave;
        return true;
    }

    // Only touches 12 bytes of immediate data. Safe to call every tick.
    static void UpdateCaveTarget(Vector3D endBulletPos)
    {
        if (!g_CavePtr)
            return;

        uint32_t x = FloatBits(endBulletPos.x);
        uint32_t y = FloatBits(endBulletPos.y);
        uint32_t z = FloatBits(endBulletPos.z);

        FrameWork::Memory::WriteProcessMemoryImpl(g_CavePtr + kOffX, &x, sizeof(x));
        FrameWork::Memory::WriteProcessMemoryImpl(g_CavePtr + kOffY, &y, sizeof(y));
        FrameWork::Memory::WriteProcessMemoryImpl(g_CavePtr + kOffZ, &z, sizeof(z));
    }

    // Installs the 5-byte JMP. The cave is already complete at this point, so
    // there's no window where the game can jump into a half-built cave.
    static void InstallHook()
    {
        if (g_HookInstalled || !g_CavePtr)
            return;

        const uint64_t handleBullet = g_Fivem.GetHandleBulletAddress();

        uint8_t jmp[kJmpSize] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };
        const uint32_t offset = RelativeOffset(handleBullet, g_CavePtr, kJmpSize);
        std::memcpy(jmp + 1, &offset, sizeof(offset));

        if (FrameWork::Memory::WriteProcessMemoryImpl(handleBullet, jmp, sizeof(jmp)))
            g_HookInstalled = true;
    }

    void SilentAim::RestoreSilent()
    {
        if (!g_HookInstalled)
            return;

        // Restore the 5 bytes we clobbered with the JMP
        if (g_PrologueRead)
            FrameWork::Memory::WriteProcessMemoryImpl(
                g_Fivem.GetHandleBulletAddress(), (LPVOID)g_OriginalPrologue, kJmpSize);

        // Free the cave memory in FiveM so memory scanners find nothing
        if (g_CavePtr)
        {
            FrameWork::Memory::FreeProcessMemory(g_CavePtr, 0);
            g_CavePtr = 0;
        }

        g_HookInstalled = false;
        g_PrologueRead = false;
    }

    void ApplySilent(Vector3D endBulletPos)
    {
        if (!EnsureCave())
            return;

        // Point the cave at the new target BEFORE arming the hook, so the very
        // first bullet through it already has valid coordinates.
        UpdateCaveTarget(endBulletPos);
        InstallHook();
    }

    void SilentAim::RunThread()
    {
        bool toggledOn = false;
        bool keyWasDown = false;

        while (!g_Options.General.ShutDown)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1 + g_Options.General.ThreadDelay));

            if (!g_Options.LegitBot.SilentAim.Enabled) {
                RestoreSilent();
                continue;
            }

            auto localPlayer = g_Fivem.GetLocalPlayerInfo();
            if (!localPlayer.Ped) {
                RestoreSilent();
                continue;
            }

            // --- Keybind logic ---
            bool keyActive = false;
            int key = g_Options.LegitBot.SilentAim.KeyBind;

            if (key == 0) {
                keyActive = true;
            }
            else if (g_Options.LegitBot.SilentAim.KeyMode == 0) {
                keyActive = (GetAsyncKeyState(key) & 0x8000) != 0;
            }
            else {
                bool keyDown = (GetAsyncKeyState(key) & 0x8000) != 0;
                if (keyDown && !keyWasDown) {
                    toggledOn = !toggledOn;
                }
                keyWasDown = keyDown;
                keyActive = toggledOn;
            }

            const float effectiveFov = static_cast<float>(g_Options.LegitBot.SilentAim.Fov);

            Entity closestEntity;
            bool entityFound = g_Fivem.FindClosestEntity(
                effectiveFov,
                g_Options.LegitBot.SilentAim.MaxDistance,
                g_Options.LegitBot.SilentAim.ShotNPC,
                &closestEntity);

            const bool shouldHook =
                entityFound &&
                keyActive &&
                !closestEntity.StaticInfo.IsFriend &&
                (!g_Options.LegitBot.SilentAim.VisibleCheck || closestEntity.Visible);

            if (!shouldHook) {
                RestoreSilent();
                continue;
            }

            Vector3D bonePos;
            switch (g_Options.LegitBot.SilentAim.HitBox) {
            case HEAD_INDEX:
                bonePos = g_Fivem.GetBonePosVec3(closestEntity, SKEL_Head);
                break;
            case NECK_INDEX:
                bonePos = g_Fivem.GetBonePosVec3(closestEntity, SKEL_Neck_1);
                break;
            case CHEST_INDEX:
                bonePos = g_Fivem.GetBonePosVec3(closestEntity, SKEL_Spine3);
                break;
            default:
                RestoreSilent();
                continue;
            }

            // A NaN or (0,0,0) bone means the read failed — writing that into
            // the bullet endpoint sends rounds to the void.
            if (!bonePos.IsValid() || bonePos.LengthSqr() < 1e-6f) {
                RestoreSilent();
                continue;
            }

            ApplySilent(bonePos);
        }

        RestoreSilent();
    }
}
