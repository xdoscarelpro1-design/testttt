#include "SilentAim.hpp"
#include "../../Options.hpp"

#include <cstring>
#include <cstdint>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <vector>

namespace Cheat {

    // *the cave breathes in the dark, waiting for the next bullet to wander through*

    static uint8_t g_OriginalPrologue[17] = {};
    static size_t g_PrologueSize = 0;
    static bool g_PrologueRead = false;

    constexpr size_t kPrologueSize = 17;
    constexpr size_t kJmpSize = 5;

    //   +0x00  mov dword [rX],    X      41 C7 0? <imm32>
    //   +0x07  mov dword [rX+4],  Y      41 C7 4? 04 <imm32>
    //   +0x0F  mov dword [rX+8],  Z      41 C7 4? 08 <imm32>
    //   +0x17  <original prologue, 17 bytes>
    //   +0x28  jmp back to HandleBullet+5
    constexpr size_t kOffX = 0x03;
    constexpr size_t kOffY = 0x0B;
    constexpr size_t kOffZ = 0x13;
    constexpr size_t kOffPrologue = 0x17;
    constexpr size_t kOffJmpBack = kOffPrologue + kPrologueSize;
    constexpr size_t kCaveSize = kOffJmpBack + kJmpSize;

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

    static bool InRel32Range(uint64_t from, uint64_t to, size_t instructionLength)
    {
        const int64_t delta = static_cast<int64_t>(to) - static_cast<int64_t>(from + instructionLength);
        return delta >= INT32_MIN && delta <= INT32_MAX;
    }

    static bool EnsureCave()
    {
        if (g_CavePtr)
            return true;

        if (!g_Fivem.IsHandleBulletValid())
            return false;

        const uint64_t handleBullet = g_Fivem.GetHandleBulletAddress();
        if (!handleBullet)
            return false;

        if (!g_PrologueRead)
        {
            FrameWork::Memory::ReadProcessMemoryImpl(handleBullet, g_OriginalPrologue, kPrologueSize);
            g_PrologueSize = kPrologueSize;
            g_PrologueRead = true;

            if (g_OriginalPrologue[0] != 0xF3)
                return false;
        }

        const uint64_t cave = FrameWork::Memory::AllocateMemoryNear(handleBullet, 0x100);
        if (!cave)
            return false;

        if (!InRel32Range(handleBullet, cave, kJmpSize) ||
            !InRel32Range(cave + kOffJmpBack, handleBullet + kJmpSize, kJmpSize))
            return false;

        uint8_t cave_bytes[kCaveSize] = {};

        uint8_t modrmByte = g_OriginalPrologue[4];
        uint8_t regBit = modrmByte & 0x07;

        const uint8_t scaffold[] = {
            0x41, 0xC7, (uint8_t)(0x00 | regBit), 0x00, 0x00, 0x00, 0x00,
            0x41, 0xC7, (uint8_t)(0x40 | regBit), 0x04, 0x00, 0x00, 0x00, 0x00,
            0x41, 0xC7, (uint8_t)(0x40 | regBit), 0x08, 0x00, 0x00, 0x00, 0x00
        };
        std::memcpy(cave_bytes, scaffold, sizeof(scaffold));
        std::memcpy(cave_bytes + kOffPrologue, g_OriginalPrologue, kPrologueSize);

        cave_bytes[kOffJmpBack] = 0xE9;
        const uint32_t backOffset = RelativeOffset(cave + kOffJmpBack, handleBullet + kJmpSize, kJmpSize);
        std::memcpy(cave_bytes + kOffJmpBack + 1, &backOffset, sizeof(backOffset));

        if (!FrameWork::Memory::WriteProcessMemoryImpl(cave, cave_bytes, sizeof(cave_bytes)))
            return false;

        g_CavePtr = cave;
        return true;
    }

    static void UpdateCaveTarget(Vector3D endBulletPos)
    {
        if (!g_CavePtr) return;
        uint32_t x = FloatBits(endBulletPos.x);
        uint32_t y = FloatBits(endBulletPos.y);
        uint32_t z = FloatBits(endBulletPos.z);
        FrameWork::Memory::WriteProcessMemoryImpl(g_CavePtr + kOffX, &x, sizeof(x));
        FrameWork::Memory::WriteProcessMemoryImpl(g_CavePtr + kOffY, &y, sizeof(y));
        FrameWork::Memory::WriteProcessMemoryImpl(g_CavePtr + kOffZ, &z, sizeof(z));
    }

    static void InstallHook()
    {
        if (g_HookInstalled || !g_CavePtr) return;
        const uint64_t handleBullet = g_Fivem.GetHandleBulletAddress();
        uint8_t jmp[kJmpSize] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };
        const uint32_t offset = RelativeOffset(handleBullet, g_CavePtr, kJmpSize);
        std::memcpy(jmp + 1, &offset, sizeof(offset));
        if (FrameWork::Memory::WriteProcessMemoryImpl(handleBullet, jmp, sizeof(jmp)))
            g_HookInstalled = true;
    }

    static void ApplySilent(Vector3D endBulletPos)
    {
        if (!EnsureCave()) return;
        UpdateCaveTarget(endBulletPos);
        InstallHook();
    }

    void SilentAim::RestoreSilent()
    {
        if (!g_HookInstalled) return;
        if (g_PrologueRead)
            FrameWork::Memory::WriteProcessMemoryImpl(
                g_Fivem.GetHandleBulletAddress(), (LPVOID)g_OriginalPrologue, kJmpSize);
        if (g_CavePtr)
        {
            FrameWork::Memory::FreeProcessMemory(g_CavePtr, 0);
            g_CavePtr = 0;
        }
        g_HookInstalled = false;
        g_PrologueRead = false;
    }

    // --- multi-hitbox: pick whichever enabled bone is closest to crosshair ---
    static Vector3D SelectBestBone(const Entity& target, const bool hitboxes[8])
    {
        ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f);
        Vector3D bestPos = Vector3D(0, 0, 0);
        float minDist = FLT_MAX;

        for (int i = 0; i < 8; i++)
        {
            if (!hitboxes[i]) continue;

            std::vector<int> bones;
            switch (i)
            {
            case 0: bones = { SKEL_Head };                              break;
            case 1: bones = { SKEL_Neck_1 };                            break;
            case 2: bones = { SKEL_Spine3 };                            break;
            case 3: bones = { SKEL_Pelvis };                            break;
            case 4: bones = { SKEL_L_UpperArm, SKEL_L_Forearm };       break;
            case 5: bones = { SKEL_R_UpperArm, SKEL_R_Forearm };       break;
            case 6: bones = { SKEL_L_Thigh, SKEL_L_Calf };             break;
            case 7: bones = { SKEL_R_Thigh, SKEL_R_Calf };             break;
            }

            for (int boneId : bones)
            {
                Vector3D pos = g_Fivem.GetBonePosVec3(target, boneId);
                if (!pos.IsValid() || pos.LengthSqr() < 1e-6f) continue;

                ImVec2 sp = g_Fivem.WorldToScreen(pos);
                if (sp.x == 0.f && sp.y == 0.f) continue;

                float dx = sp.x - center.x;
                float dy = sp.y - center.y;
                float d = sqrtf(dx * dx + dy * dy);
                if (d < minDist)
                {
                    minDist = d;
                    bestPos = pos;
                }
            }
        }

        if (minDist == FLT_MAX)
            bestPos = g_Fivem.GetBonePosVec3(target, SKEL_Head);

        return bestPos;
    }

    void SilentAim::RunThread()
    {
        // --- combat roll tracker ---
        struct RollState {
            float prevSpeed = 0.f;
            bool  isRolling = false;
            std::chrono::steady_clock::time_point rollingUntil{};
        };
        std::unordered_map<uintptr_t, RollState> rollTracker;

        constexpr float ROLL_SPEED_THRESHOLD = 4.5f;
        constexpr int   ROLL_LATCH_MS = 450;

        auto IsLikelyCombatRolling = [&](CPed* ped) -> bool
        {
            if (!ped) return false;
            uintptr_t nav = FrameWork::Memory::ReadMemory<uintptr_t>((uintptr_t)ped + 0x30);
            if (!nav) return false;
            Vector3D vel = FrameWork::Memory::ReadMemory<Vector3D>(nav + 0x1C);
            float speed = sqrtf(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);

            uintptr_t key = (uintptr_t)ped;
            auto& rs = rollTracker[key];
            auto now = std::chrono::steady_clock::now();

            if (speed > ROLL_SPEED_THRESHOLD && rs.prevSpeed <= ROLL_SPEED_THRESHOLD)
            {
                rs.isRolling = true;
                rs.rollingUntil = now + std::chrono::milliseconds(ROLL_LATCH_MS);
            }
            if (rs.isRolling && now >= rs.rollingUntil)
                rs.isRolling = false;

            rs.prevSpeed = speed;
            return rs.isRolling;
        };

        bool toggledOn = false;
        bool keyWasDown = false;
        bool silentApplied = false;

        while (!g_Options.General.ShutDown)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            if (!g_Options.LegitBot.SilentAim.Enabled)
            {
                if (silentApplied) { RestoreSilent(); silentApplied = false; }
                continue;
            }

            if (!g_Fivem.GetLocalPlayerInfo().Ped)
            {
                if (silentApplied) { RestoreSilent(); silentApplied = false; }
                continue;
            }

            // --- keybind: hold or toggle ---
            bool keyActive = false;
            int key = g_Options.LegitBot.SilentAim.KeyBind;

            if (key == 0)
            {
                keyActive = true;
            }
            else if (g_Options.LegitBot.SilentAim.KeyBindState == 0)
            {
                keyActive = (SafeCall(GetAsyncKeyState)(key) & 0x8000) != 0;
            }
            else
            {
                bool keyDown = (SafeCall(GetAsyncKeyState)(key) & 0x8000) != 0;
                if (keyDown && !keyWasDown)
                    toggledOn = !toggledOn;
                keyWasDown = keyDown;
                keyActive = toggledOn;
            }

            if (!keyActive)
            {
                if (silentApplied) { RestoreSilent(); silentApplied = false; }
                continue;
            }

            // --- find target ---
            Entity closestEntity{};
            bool found = g_Fivem.FindClosestEntity(
                (float)g_Options.LegitBot.SilentAim.Fov,
                g_Options.LegitBot.SilentAim.MaxDistance,
                g_Options.LegitBot.SilentAim.VisibleCheck,
                g_Options.LegitBot.SilentAim.ShotNPC,
                g_Options.LegitBot.SilentAim.ClosestFov,
                &closestEntity);

            if (!found)
            {
                if (silentApplied) { RestoreSilent(); silentApplied = false; }
                continue;
            }

            // --- combat roll priority override ---
            if (g_Options.LegitBot.AimBot.TargetCombatRoll)
            {
                ImVec2 screenCenter = ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f);
                auto entities = g_Fivem.GetEntitiyList();
                for (auto& e : entities)
                {
                    if (!e.StaticInfo.Ped) continue;
                    if (e.StaticInfo.bIsLocalPlayer) continue;
                    if (e.StaticInfo.IsFriend) continue;
                    if (!g_Options.LegitBot.SilentAim.ShotNPC && e.StaticInfo.bIsNPC) continue;
                    if ((uint64_t)e.StaticInfo.Ped >= 0xCCCCCCCCCCCCCC) continue;

                    ImVec2 headScreen = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(e, SKEL_Head));
                    if (headScreen.x == 0.f && headScreen.y == 0.f) continue;
                    float dist = sqrtf(powf(headScreen.x - screenCenter.x, 2) + powf(headScreen.y - screenCenter.y, 2));
                    if (dist > (float)g_Options.LegitBot.SilentAim.Fov) continue;

                    if (IsLikelyCombatRolling(e.StaticInfo.Ped))
                    {
                        closestEntity = e;
                        break;
                    }
                }
            }

            if (closestEntity.StaticInfo.IsFriend)
            {
                if (silentApplied) { RestoreSilent(); silentApplied = false; }
                continue;
            }

            if ((uint64_t)closestEntity.StaticInfo.Ped >= 0xCCCCCCCCCCCCCC)
            {
                if (silentApplied) { RestoreSilent(); silentApplied = false; }
                continue;
            }

            if (g_Options.LegitBot.SilentAim.VisibleCheck && !closestEntity.Visible)
            {
                if (silentApplied) { RestoreSilent(); silentApplied = false; }
                continue;
            }

            // --- miss chance: random % skip ---
            if (g_Options.LegitBot.SilentAim.MissChance > 0 && (rand() % 100) < g_Options.LegitBot.SilentAim.MissChance)
            {
                if (silentApplied) { RestoreSilent(); silentApplied = false; }
                continue;
            }

            // --- multi-hitbox bone selection ---
            Vector3D bonePos = SelectBestBone(closestEntity, g_Options.LegitBot.SilentAim.HitBoxes);

            if (!bonePos.IsValid() || bonePos.LengthSqr() < 1e-6f)
            {
                if (silentApplied) { RestoreSilent(); silentApplied = false; }
                continue;
            }

            ApplySilent(bonePos);
            silentApplied = true;

            // --- magic bullet: write spawn position near target ---
            bool magicShouldFire = g_Options.LegitBot.SilentAim.MagicBullet &&
                (!g_Options.LegitBot.SilentAim.VisibleCheck || closestEntity.Visible);

            if (magicShouldFire)
            {
                auto localPed = g_Fivem.GetLocalPlayerInfo().Ped;
                if (localPed)
                {
                    uintptr_t weaponManager = (uintptr_t)localPed->GetWeaponManager();
                    if (weaponManager)
                    {
                        uintptr_t weaponObj = FrameWork::Memory::ReadMemory<uintptr_t>(weaponManager + 0x78);
                        if (weaponObj)
                        {
                            uintptr_t CWeapon = FrameWork::Memory::ReadMemory<uintptr_t>(weaponObj + 0x320);
                            if (CWeapon)
                            {
                                Vector3D heading = FrameWork::Memory::ReadMemory<Vector3D>((uintptr_t)closestEntity.StaticInfo.Ped + 0x70);
                                Vector3D spawnPos = {
                                    bonePos.x + heading.x * 5.0f,
                                    bonePos.y + heading.y * 5.0f,
                                    bonePos.z + heading.z * 5.0f
                                };
                                FrameWork::Memory::WriteMemory<Vector3D>(CWeapon + 0x20, spawnPos);
                            }
                        }
                    }
                }
            }
        }

        RestoreSilent();
    }
}
