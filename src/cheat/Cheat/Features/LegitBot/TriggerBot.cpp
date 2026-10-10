#include "TriggerBot.hpp"

#include <thread>
#include <chrono>
#include <random>

#include "../../Options.hpp"

namespace Cheat
{
    // Seeded once from the clock so two sessions don't fire on identical
    // rhythms — a perfectly fixed cadence is the easiest behavioural tell.
    static std::mt19937& Rng()
    {
        static std::mt19937 engine(
            static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
        return engine;
    }

    static int RandomInt(int lo, int hi)
    {
        if (hi <= lo)
            return lo;

        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(Rng());
    }

    // Used on every early-out path. The old version could `continue` while
    // Shooting was still true — the left button stayed physically held down
    // and you'd keep firing at nothing until the next target wandered in.
    static void ReleaseMouse(bool& shooting)
    {
        if (!shooting)
            return;

        SafeCall(mouse_event)(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        shooting = false;
    }

    void TriggerBot::RunThread()
    {
        bool shooting = false;
        bool toggledOn = false;
        bool keyWasDown = false;

        // Which ped we're committed to, and when we locked on. Reaction delay
        // is measured from acquisition rather than slept in the loop — the old
        // code called sleep_for() mid-iteration, which stalled every other
        // check in the thread and made the delay behave like a freeze.
        CPed* committedTarget = nullptr;
        std::chrono::steady_clock::time_point acquiredAt{};
        int pendingReactionMs = 0;
        bool rolledThisTarget = false;
        bool willFireThisTarget = false;

        auto lastShot = std::chrono::steady_clock::now();

        while (!g_Options.General.ShutDown)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1 + g_Options.General.ThreadDelay));

            if (!g_Options.LegitBot.Trigger.Enabled || !g_Fivem.GetLocalPlayerInfo().Ped)
            {
                ReleaseMouse(shooting);
                committedTarget = nullptr;
                continue;
            }

            // --- Keybind: hold or toggle, matching the other features -------
            bool keyActive = false;
            const int key = g_Options.LegitBot.Trigger.KeyBind;

            if (key == 0)
            {
                keyActive = true;
            }
            else if (g_Options.LegitBot.Trigger.KeyMode == 0)
            {
                keyActive = (GetAsyncKeyState(key) & 0x8000) != 0;
            }
            else
            {
                const bool keyDown = (GetAsyncKeyState(key) & 0x8000) != 0;
                if (keyDown && !keyWasDown)
                    toggledOn = !toggledOn;

                keyWasDown = keyDown;
                keyActive = toggledOn;
            }

            if (!keyActive)
            {
                ReleaseMouse(shooting);
                committedTarget = nullptr;
                continue;
            }

            // Firing from the hip while the game says we're not aiming is the
            // loudest tell there is. Only meaningful if the offset resolved.
            if (g_Options.LegitBot.Trigger.RequireAiming && !g_Fivem.IsPlayerAiming())
            {
                ReleaseMouse(shooting);
                committedTarget = nullptr;
                continue;
            }

            Entity target;
            const bool found = g_Fivem.FindClosestEntity(
                static_cast<float>(g_Options.LegitBot.Trigger.FOV),
                g_Options.LegitBot.Trigger.MaxDistance,
                g_Options.LegitBot.Trigger.ShotNPC,
                &target);

            // The old code tested `!closestEntity.Visible` unconditionally,
            // ignoring the VisibleCheck option entirely.
            const bool valid =
                found &&
                !target.StaticInfo.IsFriend &&
                (!g_Options.LegitBot.Trigger.VisibleCheck || target.Visible);

            if (!valid)
            {
                ReleaseMouse(shooting);
                committedTarget = nullptr;
                continue;
            }

            const auto now = std::chrono::steady_clock::now();

            // New target: restart the reaction clock and roll hit chance once.
            // Rolling per-tick would make HitChance meaningless — a 50% roll
            // retried a thousand times a second always passes eventually.
            if (target.StaticInfo.Ped != committedTarget)
            {
                committedTarget = target.StaticInfo.Ped;
                acquiredAt = now;
                rolledThisTarget = false;

                const int jitter = g_Options.LegitBot.Trigger.ReactionJitter;
                pendingReactionMs = g_Options.LegitBot.Trigger.ReactionTime
                    + (jitter > 0 ? RandomInt(0, jitter) : 0);

                ReleaseMouse(shooting);
                continue;
            }

            if (!rolledThisTarget)
            {
                const int chance = g_Options.LegitBot.Trigger.HitChance;
                willFireThisTarget = (chance >= 100) || (RandomInt(1, 100) <= chance);
                rolledThisTarget = true;
            }

            if (!willFireThisTarget)
            {
                ReleaseMouse(shooting);
                continue;
            }

            // Reaction delay, without blocking the loop.
            const int heldMs = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - acquiredAt).count());

            if (heldMs < pendingReactionMs)
            {
                ReleaseMouse(shooting);
                continue;
            }

            const int sinceShot = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - lastShot).count());

            if (shooting)
            {
                // Hold the click just long enough for the game to register it,
                // then let go.
                if (sinceShot >= 20)
                    ReleaseMouse(shooting);

                continue;
            }

            // Refire limiter — holding the key shouldn't emit a click every
            // millisecond. No human produces that pattern.
            if (sinceShot < g_Options.LegitBot.Trigger.RefireDelay)
                continue;

            SafeCall(mouse_event)(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            shooting = true;
            lastShot = now;
        }

        ReleaseMouse(shooting);
    }
}
