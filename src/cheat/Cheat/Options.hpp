#pragma once
#include <Windows.h>

/*
 * PHANTOM Options — merged from Traceless Variables.hpp
 *
 * Flat structs serialized as a binary blob by the config system.
 * Every field must have a default so a fresh g_Options is sane.
 */

enum class NoClipState
{
    Disabled,
    Hold,
    Toggle
};

namespace Cheat
{
    struct AimBot_s
    {
        bool Enabled = false;
        bool ShowFov = false;
        bool TargetNPC = false;
        bool VisibleCheck = true;
        bool EnablePrediction = false;

        int ClosestFov = 0;
        int KeyBind = 0;
        int KeyBindState = 0;   // 0 Hold, 1 Toggle
        int MaxDistance = 250;
        int FOV = 100;
        int SmoothHorizontal = 0;
        int SmoothVertical = 0;
        int AimbotStyle = 0;    // 0 Custom, 1 Exponential, 2 Linear, 3 Humanized, 4 Snap
        bool HitBoxes[8] = { true, false, false, false, false, false, false, false };
        bool ShowHitboxPreview = false;
        bool ShowAimbotPreview = false;
        bool TargetCombatRoll = false;
        float AimbotPreviewColor[4] = { 0.f, 1.f, 0.f, 1.f };
        float FovColor[4] = { 1.f, 1.f, 1.f, 1.f };
    };

    struct SilentAim_s
    {
        bool Enabled = false;
        bool ShowFOV = false;
        bool ShotNPC = false;
        bool VisibleCheck = true;
        bool MagicBullet = false;
        bool Humanize = false;

        int Fov = 100;
        int KeyBind = 0;
        int KeyBindState = 0;
        int MissChance = 0;
        int ClosestFov = 0;
        int MaxDistance = 250;
        bool HitBoxes[8] = { true, false, false, false, false, false, false, false };
        bool ShowHitboxPreview = false;
        float FovColor[4] = { 1.f, 1.f, 1.f, 1.f };
    };

    struct TriggerBot_s
    {
        bool Enabled = false;
        bool ShotNPC = false;
        bool VisibleCheck = true;
        bool RequireAiming = false;

        int KeyBind = 0;
        int KeyBindState = 0;
        int MaxDistance = 250;
        int ReactionTime = 0;
        int ReactionJitter = 0;
        int FOV = 100;
        int HitChance = 100;
        int RefireDelay = 100;
    };

    struct Players_s
    {
        bool Enabled = true;
        bool Toogle = false;
        bool ShowLocalPlayer = false;
        bool ShowNPC = true;
        bool VisibleCheck = true;
        bool ExcludeDeads = false;
        bool SpectatorList = false;

        int HealthBarPos = 1;
        int ArmorBarPos = 1;
        int NamePos = 3;
        int DistPos = 2;
        int WeaponPos = 2;

        int ToggleKey = 0;
        int ToggleKeyState = 0;
        int RenderDistance = 500;
        int EspOptions = 6;
        int BoxType = 1;

        bool EnableBox = true;
        bool EnableDistance = true;
        bool EnableHeadBol = true;
        bool Skeleton = true;
        bool HealthBar = true;
        bool ArmorBar = true;
        bool WeaponName = true;
        bool Name = true;
        bool RadarEnabled = false;
        bool CustomCrosshair = true;
        bool EnableAimingWarning = true;
        bool EnableObserverWarning = true;

        bool RocketStyle = true;
        float BoxColor[4] = { 0.f, 1.f, 0.f, 1.f };
        float NormalBoxColor[4] = { 0.f, 1.f, 0.f, 1.f };
        float InVisibleBoxColor[4] = { 1.f, 0.f, 0.f, 1.f };
        float DistanceColor[4] = { 1.f, 1.f, 1.f, 1.f };
        float SkeletonColor[4] = { 0.f, 1.f, 0.f, 1.f };
        float NormalSkeletonColor[4] = { 1.f, 1.f, 1.f, 1.f };
        float InvisibleSkeletonColor[4] = { 1.f, 0.5f, 0.5f, 1.f };
        float FriendSkeletonColor[4] = { 0.f, 1.f, 1.f, 0.7f };
        float HealthBarColor[4] = { 0.f, 1.f, 0.f, 0.7f };
        float ArmorBarColor[4] = { 0.f, 0.f, 1.f, 0.7f };
        float TextColor[4] = { 1.f, 1.f, 1.f, 1.f };
        float NameColor[4] = { 1.f, 1.f, 1.f, 1.f };
        float WeaponNameColor[4] = { 1.f, 1.f, 1.f, 1.f };
        float HeadCircleColor[4] = { 0.8f, 0.8f, 0.8f, 0.12f };
        float CrosshairColor[4] = { 0.65f, 0.35f, 1.f, 1.f };
        float CrosshairDotColor[4] = { 1.f, 0.2f, 0.2f, 1.f };
        bool ShowPreview = false;

        float SnapLinesColor[4] = { 0.f, 1.f, 0.f, 1.f };
        float FriendColor[4] = { 0.f, 0.f, 1.f, 1.f };
    };

    struct Vehicles_s
    {
        bool Enabled = true;
        bool Toogle = false;
        bool Distance = true;
        bool Name = true;
        bool ShowLockState = true;

        int ToggleKey = 0;
        int ToggleKeyState = 0;
        int EspOptions = 0;

        bool Marker = false;
        float MarkerColor[4] = { 1.f, 1.f, 1.f, 0.7f };
        float DistanceColor[4] = { 0.8f, 0.8f, 0.8f, 0.7f };
        float NameColor[4] = { 1.f, 1.f, 1.f, 0.7f };
    };

    struct Self_s
    {
        bool Invisible = false;
        bool GodMode = false;
        bool NoClip = false;
        bool NoRagDoll = false;
        bool RemoveSpread = false;
        bool RemoveRecoil = false;
        bool SeatBelt = false;
        bool StealVehicle = false;
        bool UnLockAllVehicles = false;
        bool LockAllVehicles = false;
        bool ReplayTP = false;
        bool AntHS = false;
        bool Bubbles = false;
        bool AntiBubbles = false;
        bool Absorption = false;
        int AbsorptionPercent = 0;
        bool Start_Health = false;
        int health_ammount = 0;
        bool Immortality = false;
        bool SuperPunch = false;
        float SuperPunchForce = 50.f;
        bool VehicleSpeed = false;
        float VehicleSpeedMultiplier = 1.5f;
        int VehicleSpeedBind = 0;
        bool VehicleGod = false;
        bool Desync = false;
        int DesyncKeyBind = 0;

        float WeaponSize = 1.f;
        float NoClipSpeed = 100.f;

        int NoClipKey = 0;
        int NoClipKeyState = 0;
        int GodKey = 0;
        int GodKeyState = 0;
        int ReplayTPBind = 0;
        int ReplayTPBindState = 0;
        int NoClipMode = 0;

        float PeakPositionX = 0.f;
        float PeakPositionY = 0.f;
        float PeakPositionZ = 0.f;
        bool PeakSet = false;
        int CreatePeak = 0;
        int ReturnToPeak = 0;
        int CreatePeakBind = 0;
        int ReturnToPeakBind = 0;

        bool StrafeEnabled = false;
        int StrafeKey = 0;
        int StrafeKeyState = 0;
        float StrafeSpeed = 0.5f;

        int InvisibleKey = 0;
        int InvisibleKeyState = 0;
        int SeatbeltKey = 0;
        int SeatbeltKeyState = 0;
        int BubblesKey = 0;
        int BubblesKeyState = 0;
        int AntiBubblesKey = 0;
        int AntiBubblesKeyState = 0;
        int TeleportWaypointKey = 0;
        int TeleportWaypointKeyState = 0;
        int SuicideKey = 0;
        int SuicideKeyState = 0;
    };

    class Options
    {
    public:
        struct LegitBot_Config
        {
            AimBot_s AimBot;
            SilentAim_s SilentAim;
            TriggerBot_s Trigger;
        } LegitBot;

        struct Visuals_Config
        {
            Players_s Players;
            Vehicles_s Vehicles;
        } Visuals;

        struct Exploits_Config
        {
            Self_s Self;
        } Exploits;

        struct General_Config
        {
            bool ShutDown = false;
            bool CaptureBypass = false;
            bool NvidiaBypass = false;
            bool Vsync = false;
            bool SecondMonitor = false;
            bool ShowWatermark = true;
            bool IncludeDeathPlayers = false;
            int MenuKey = VK_INSERT;
            int MenuKeyState = 0;
            float AccentColor[4] = { 0.58f, 0.58f, 0.72f, 1.f };

            int BackgroundStyle = 4;    // 0 None, 1 Scanlines, 2 Hex, 3 Particles, 4 Matrix
            bool RainbowAccent = false;
            float BorderSize = 0.0f;
            float MenuScale = 1.0f;
            int MenuStyle = 0;          // 0 Panel, 1 Flat, 2 Glass

            bool ShowKeybinds = true;
            bool Notifications = true;
            int ThreadDelay = 0;
            bool MenuOpen = false;

            float ColorBg[4]       = { 10/255.f, 10/255.f, 10/255.f, 1.f };
            float ColorChildBg[4]  = { 16/255.f, 16/255.f, 16/255.f, 1.f };
            float ColorBoxBg[4]    = { 14/255.f, 14/255.f, 14/255.f, 1.f };
            float ColorOutline[4]  = { 70/255.f, 70/255.f, 70/255.f, 0.43f };
            float ColorText[4]     = { 210/255.f, 210/255.f, 210/255.f, 1.f };
            float ColorTextDim[4]  = { 110/255.f, 110/255.f, 130/255.f, 1.f };
        } General;
    };
}

inline Cheat::Options g_Options;
inline char g_UISearchBuf[128] = {};
