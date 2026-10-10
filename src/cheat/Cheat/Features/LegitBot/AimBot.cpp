#include "AimBot.hpp"
#include "../../Options.hpp"

#include <chrono>
#include <unordered_map>
#include <iterator>
#include <thread>

namespace Cheat
{
    constexpr int HEAD_INDEX = 0;
    constexpr int NECK_INDEX = 1;
    constexpr int CHEST_INDEX = 2;

    // Per-target velocity tracking for lead prediction. Keyed by ped pointer so
    // switching targets doesn't inherit the previous one's velocity.
    struct TargetTrack
    {
        Vector3D LastBonePos = Vector3D(0, 0, 0);
        Vector3D Velocity = Vector3D(0, 0, 0);
        std::chrono::steady_clock::time_point LastSeen{};
        bool Primed = false;
    };

    static Vector3D GetHitBoxPosition(const Entity& target, int hitbox, bool& ok)
    {
        ok = true;

        switch (hitbox)
        {
        case HEAD_INDEX:  return g_Fivem.GetBonePosVec3(target, SKEL_Head);
        case NECK_INDEX:  return g_Fivem.GetBonePosVec3(target, SKEL_Neck_1);
        case CHEST_INDEX: return g_Fivem.GetBonePosVec3(target, SKEL_Spine3);
        default:
            ok = false;
            return Vector3D(0, 0, 0);
        }
    }

    void AimBot::RunThread()
    {
        bool toggledOn = false;
        bool keyWasDown = false;

        std::unordered_map<CPed*, TargetTrack> tracks;

        while (!g_Options.General.ShutDown)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1 + g_Options.General.ThreadDelay));

            if (!g_Options.LegitBot.AimBot.Enabled || !g_Fivem.GetLocalPlayerInfo().Ped) {
                continue;
            }

            // No resolved camera pointer means every angle we read or write is
            // garbage. Better to do nothing than to fling the view somewhere
            // random — the UI reports this so it isn't a silent no-op.
            if (!g_Fivem.IsCameraValid()) {
                continue;
            }

            // --- Keybind logic ---
            bool keyActive = false;
            int key = g_Options.LegitBot.AimBot.KeyBind;

            if (key == 0) {
                // No keybind = always active
                keyActive = true;
            }
            else if (g_Options.LegitBot.AimBot.KeyMode == 0) {
                // Hold mode
                keyActive = (GetAsyncKeyState(key) & 0x8000) != 0;
            }
            else {
                // Toggle mode
                bool keyDown = (GetAsyncKeyState(key) & 0x8000) != 0;
                if (keyDown && !keyWasDown) {
                    toggledOn = !toggledOn;
                }
                keyWasDown = keyDown;
                keyActive = toggledOn;
            }

            if (!keyActive) {
                continue;
            }

            Entity closestEntity;
            if (!g_Fivem.FindClosestEntity(
                g_Options.LegitBot.AimBot.FOV,
                g_Options.LegitBot.AimBot.MaxDistance,
                g_Options.LegitBot.AimBot.TargetNPC,
                &closestEntity)) {
                continue;
            }

            if (closestEntity.StaticInfo.IsFriend ||
                (g_Options.LegitBot.AimBot.VisibleCheck && !closestEntity.Visible)) {
                continue;
            }

            bool hitboxOk = false;
            Vector3D bonePos = GetHitBoxPosition(closestEntity, g_Options.LegitBot.AimBot.HitBox, hitboxOk);

            if (!hitboxOk)
                continue;

            // A failed bone read comes back as garbage or origin — aiming at
            // that yanks the camera to the middle of the map.
            if (!bonePos.IsValid() || bonePos.LengthSqr() < 1e-6f)
                continue;

            // --- Lead prediction ------------------------------------------
            // Optional. When off we track the chosen bone directly, which is
            // what you want against stationary targets or when the server's
            // position interpolation is noisy enough that leading overshoots.
            Vector3D aimPos = bonePos;

            if (g_Options.LegitBot.AimBot.EnablePrediction)
            {
                // Differentiate the bone position to get target velocity, then
                // throw the aim point ahead by one bullet flight time.
                const auto now = std::chrono::steady_clock::now();
                TargetTrack& track = tracks[closestEntity.StaticInfo.Ped];

                if (track.Primed)
                {
                    const float dt = std::chrono::duration<float>(now - track.LastSeen).count();

                    // Only trust samples from a sane window. Too small and the
                    // division explodes; too large and the target has
                    // teleported or we just re-acquired them.
                    if (dt > 0.002f && dt < 0.25f)
                    {
                        const Vector3D instantVelocity = (bonePos - track.LastBonePos) * (1.f / dt);

                        if (instantVelocity.IsValid() && instantVelocity.Length() < 150.f)
                        {
                            // Smooth the derivative — raw frame-to-frame deltas
                            // on a network-interpolated position are pure noise.
                            constexpr float kVelocitySmoothing = 0.35f;
                            track.Velocity += (instantVelocity - track.Velocity) * kVelocitySmoothing;
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

                    // Rough hitscan-ish muzzle velocity for GTA small arms.
                    // Not exact per-weapon, but close enough that leading beats
                    // not leading at every range you'd actually shoot at.
                    constexpr float kProjectileSpeed = 400.f;
                    float flightTime = distance / kProjectileSpeed;

                    // Cap the lead so a laggy target doesn't fling the camera.
                    if (flightTime > 0.35f)
                        flightTime = 0.35f;

                    const Vector3D predicted = bonePos + track.Velocity * flightTime;

                    if (predicted.IsValid())
                        aimPos = predicted;
                }

                // Drop stale tracks so the map doesn't grow without bound.
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
                // Prediction was just switched off — drop the history so
                // re-enabling it doesn't apply a stale velocity.
                tracks.clear();
            }

            g_Fivem.ProcessCameraMovement(
                aimPos,
                g_Options.LegitBot.AimBot.SmoothHorizontal,
                g_Options.LegitBot.AimBot.SmoothVertical);
        }
    }
}
