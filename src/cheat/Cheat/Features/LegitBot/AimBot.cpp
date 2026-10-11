#include "AimBot.hpp"
#include "../../Options.hpp"

#include <chrono>
#include <unordered_map>
#include <iterator>
#include <thread>
#include <vector>

namespace Cheat
{
    // *whispers to the stack frame* you're about to become a ballistics computer

    struct TargetTrack
    {
        Vector3D LastBonePos = Vector3D(0, 0, 0);
        Vector3D Velocity = Vector3D(0, 0, 0);
        std::chrono::steady_clock::time_point LastSeen{};
        bool Primed = false;
    };

    static Vector3D SelectBestBone(const Entity& target, const bool hitboxes[8], float& outScreenDist)
    {
        ImVec2 screenCenter = ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f);
        Vector3D bestPos = Vector3D(0, 0, 0);
        float minDist = FLT_MAX;

        for (int i = 0; i < 8; i++)
        {
            if (!hitboxes[i])
                continue;

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
                Vector3D bonePos = g_Fivem.GetBonePosVec3(target, boneId);
                if (!bonePos.IsValid() || bonePos.LengthSqr() < 1e-6f)
                    continue;

                ImVec2 screenPos = g_Fivem.WorldToScreen(bonePos);
                if (screenPos.x == 0.f && screenPos.y == 0.f)
                    continue;

                float dx = screenPos.x - screenCenter.x;
                float dy = screenPos.y - screenCenter.y;
                float dist = sqrtf(dx * dx + dy * dy);

                if (dist < minDist)
                {
                    minDist = dist;
                    bestPos = bonePos;
                }
            }
        }

        outScreenDist = minDist;
        return bestPos;
    }

    void AimBot::RunThread()
    {
        bool toggledOn = false;
        bool keyWasDown = false;

        std::unordered_map<CPed*, TargetTrack> tracks;

        while (!g_Options.General.ShutDown)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));

            if (!g_Options.LegitBot.AimBot.Enabled || !g_Fivem.GetLocalPlayerInfo().Ped)
                continue;

            if (!g_Fivem.IsCameraValid())
                continue;

            // --- keybind: hold or toggle ---
            bool keyActive = false;
            int key = g_Options.LegitBot.AimBot.KeyBind;

            if (key == 0)
            {
                keyActive = true;
            }
            else if (g_Options.LegitBot.AimBot.KeyBindState == 0)
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
                continue;

            // --- find target ---
            Entity closestEntity{};
            if (!g_Fivem.FindClosestEntity(
                g_Options.LegitBot.AimBot.FOV,
                g_Options.LegitBot.AimBot.MaxDistance,
                g_Options.LegitBot.AimBot.VisibleCheck,
                g_Options.LegitBot.AimBot.TargetNPC,
                g_Options.LegitBot.AimBot.ClosestFov,
                &closestEntity))
                continue;

            if (closestEntity.StaticInfo.IsFriend)
                continue;

            // --- multi-hitbox bone selection (closest to crosshair) ---
            float screenDist = FLT_MAX;
            Vector3D bonePos = SelectBestBone(closestEntity, g_Options.LegitBot.AimBot.HitBoxes, screenDist);

            if (!bonePos.IsValid() || bonePos.LengthSqr() < 1e-6f)
                continue;

            // --- lead prediction ---
            Vector3D aimPos = bonePos;

            if (g_Options.LegitBot.AimBot.EnablePrediction)
            {
                const auto now = std::chrono::steady_clock::now();
                TargetTrack& track = tracks[closestEntity.StaticInfo.Ped];

                if (track.Primed)
                {
                    const float dt = std::chrono::duration<float>(now - track.LastSeen).count();

                    if (dt > 0.002f && dt < 0.25f)
                    {
                        const Vector3D instantVel = (bonePos - track.LastBonePos) * (1.f / dt);

                        if (instantVel.IsValid() && instantVel.Length() < 150.f)
                        {
                            constexpr float kSmooth = 0.35f;
                            track.Velocity += (instantVel - track.Velocity) * kSmooth;
                        }
                    }
                    else if (dt >= 0.25f)
                    {
                        track.Velocity = Vector3D(0, 0, 0);
                    }
                }

                track.LastBonePos = bonePos;
                track.LastSeen = now;
                track.Primed = true;

                if (track.Velocity.LengthSqr() > 0.01f)
                {
                    const float distance = bonePos.DistTo(g_Fivem.GetLocalPlayerInfo().WorldPos);
                    constexpr float kProjectileSpeed = 400.f;
                    float flightTime = distance / kProjectileSpeed;
                    if (flightTime > 0.35f)
                        flightTime = 0.35f;

                    const Vector3D predicted = bonePos + track.Velocity * flightTime;
                    if (predicted.IsValid())
                        aimPos = predicted;
                }

                if (tracks.size() > 256)
                {
                    for (auto it = tracks.begin(); it != tracks.end(); )
                    {
                        const float age = std::chrono::duration<float>(now - it->second.LastSeen).count();
                        it = (age > 5.f) ? tracks.erase(it) : std::next(it);
                    }
                }
            }
            else if (!tracks.empty())
            {
                tracks.clear();
            }

            // --- aimbot style smoothing ---
            int smoothX = g_Options.LegitBot.AimBot.SmoothHorizontal;
            int smoothY = g_Options.LegitBot.AimBot.SmoothVertical;

            switch (g_Options.LegitBot.AimBot.AimbotStyle)
            {
            case 0: // Custom — user-defined smooth values, already set
                break;

            case 1: // Exponential — smooth scales with screen distance
                smoothX = (int)(screenDist * 0.5f) + 5;
                smoothY = smoothX;
                break;

            case 2: // Linear — fixed moderate speed
                smoothX = 15;
                smoothY = 15;
                break;

            case 3: // Humanized — micro-jitter on top of base smooth
            {
                static int jitterX = 0, jitterY = 0;
                if (rand() % 5 == 0)
                {
                    jitterX = (rand() % 5) - 2;
                    jitterY = (rand() % 5) - 2;
                }
                smoothX = 20 + jitterX;
                smoothY = 20 + jitterY;
                break;
            }

            case 4: // Snap — near-instant lock
                smoothX = 1;
                smoothY = 1;
                break;
            }

            g_Fivem.ProcessCameraMovement(aimPos, smoothX, smoothY);
        }
    }
}
