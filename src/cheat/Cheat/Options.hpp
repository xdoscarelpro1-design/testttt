#pragma once
#include <Windows.h>

namespace Cheat
{
	class Options
	{
	public:
		struct LegitBot
		{
			struct AimBot
			{
				bool Enabled;
				int KeyBind = 0;
				int KeyMode = 0; // 0 = Hold, 1 = Toggle
				bool TargetNPC;
				bool VisibleCheck;
				int HitBox = 0;
				int MaxDistance = 600;
				int FOV = 90;
				int SmoothHorizontal = 80;
				int SmoothVertical = 80;
				// Lead moving targets by their velocity * bullet flight time.
				// Off = track the chosen bone directly with no lead at all.
				bool EnablePrediction = false;
			}AimBot;
			struct TriggerBot
			{
				bool Enabled;
				int KeyBind = 0;
				int KeyMode = 0; // 0 = Hold, 1 = Toggle
				bool ShotNPC;
				int FOV = 90;
				bool VisibleCheck;
				int MaxDistance = 250;
				int ReactionTime;
				// Extra delay randomization on top of ReactionTime, in ms.
				int ReactionJitter = 30;
				// Percent chance to actually fire once a target is acquired.
				int HitChance = 100;
				// Minimum gap between shots so it can't machine-gun a click.
				int RefireDelay = 90;
				// Only fire when the game itself says we're aiming.
				bool RequireAiming = true;
			}Trigger;
			struct SilentAim
			{
				bool Enabled;
				int Fov = 30;
				int KeyBind = 0;
				int KeyMode = 0; // 0 = Hold, 1 = Toggle
				bool ShotNPC;
				bool VisibleCheck;
				bool MagicBullet;
				int MaxDistance = 600;
				int HitBox;
			}SilentAim;
		}LegitBot;
		struct Visuals
		{
			struct ESP
			{
				struct Players
				{
					int KeyBind = 0;
					bool Enabled = true;
					bool ShowLocalPlayer = false;
					bool ShowNPCs;
					bool VisibleOnly = false;
					int RenderDistance = 500;
					bool FilledBox = false;
					float FilledBoxColor[4] = { 0.f, 1.f, 0.f, 0.33f };
					bool Box = false;
					float BoxColor[4] = { 0.f, 1.f, 0.f, 1.f };
					bool Skeleton = false;
					bool Head = false;
					float HeadSize = 3.f;
					float HeadColor[4] = { 0.f, 1.f, 0.f, 0.f };
					float SkeletonColor[4] = { 0.f, 1.f, 0.f, 1.f };
					bool Name = true;
					float NameColor[4] = { 0.f, 1.f, 0.f, 1.f };
					bool HealthBar = true;
					float HealthBarColor[4] = {0.f, 1.f, 0.f, 1.f};
					bool ArmorBar;
					float ArmorBarColor[4] = { 0.f, 1.f, 0.f, 1.f };
					bool WeaponName;
					float WeaponNameColor[4] = { 0.f, 1.f, 0.f, 1.f };
					bool Distance;
					float DistanceColor[4] = { 0.f, 1.f, 0.f, 1.f };
					bool SnapLines;
					float SnaplinesColor[4] = { 0.f, 1.f, 0.f, 1.f };
					bool Weapon_Misc = true;
				}Players;

				struct Vehicles
				{
					bool Enabled;
					bool IgnoreOccupiedVehicles;
					bool Marker;
					bool Distance;
					bool Name;
					float Color[4] = { 0.f, 1.f, 0.f, 1.f };
				}Vehicles;

				struct Radar
				{
					bool Enabled = false;
					float Size = 150.f;
					float Range = 200.f;
					float PosX = 20.f;
					float PosY = 20.f;
					float Opacity = 0.85f;
				}Radar;
			}ESP;
		}Visuals;
		struct Misc
		{
			struct Screen
			{
				bool ShowTriggerFov;
				float TriggerFovColor[4] = {1.f, 1.f, 1.f, 1.f};
				bool ShowAimbotFov;
				float AimbotFovColor[4] = { 1.f, 1.f, 1.f, 1.f };
				bool ShowSilentAimFov;
				float SilentFovColor[4] = { 1.f, 1.f, 1.f, 1.f };
				float PartColor[4] = { 0.27f, 0.50f, 0.70f, 1.f };
			}Screen;
			struct Exploits
			{
				struct LocalPlayer
				{
					bool Noclip;
					int NoclipBind;
					int NoclipKeyMode = 0; // 0 = Hold, 1 = Toggle
					int NoclipSpeed;
					bool shrink;
					bool norecoil;
					bool speed;
					float Player_speed = 1.0f;
					bool nospread;
					bool noreload;
					int godbind = 0;
					int tpbind = 0;
					int unlockbind = 0;

					int health_ammount;
					bool Start_Health;
					bool God;
					int KeyBind = 0;
					bool VehicleSpeed;
					float VehicleSpeedMultiplier = 1.5f;
					int VehicleSpeedBind = 0;
					bool SafeTP = true;
					bool VehicleGod = false;
					bool SuperPunch = false;
					float SuperPunchForce = 50.f;
					bool Invisible = false;
					bool Desync = false;
					int DesyncKeyBind = 0;
				}LocalPlayer;
			}Exploits;
			float FriendColor[4] = { 0.f, 0.f, 1.f, 1.f };
		}Misc;
		struct Theme
		{
			float AccentColor[4] = { 0.29f, 0.53f, 0.86f, 1.f }; // blue
			float BgColor[4] = { 0.106f, 0.106f, 0.106f, 1.f };  // dark gray
			float ChildBgColor[4] = { 0.078f, 0.078f, 0.078f, 0.95f };
			float TextColor[4] = { 1.f, 1.f, 1.f, 1.f };
			float BorderColor[4] = { 0.078f, 0.078f, 0.078f, 1.f };
			float WindowRounding = 10.f;
			float ChildRounding = 8.f;
			float ButtonRounding = 6.f;
			float FrameRounding = 4.f;
			float WindowOpacity = 1.f;
			int WindowWidth = 700;
			int WindowHeight = 500;
			int TabLayout = 0; // 0 = vertical (left), 1 = horizontal (top)
			float TabWidth = 80.f;
			int CheckboxStyle = 0; // 0 = default checkbox, 1 = toggle switch

		}Theme;
		struct General
		{
			bool ShutDown = false;
			bool MenuOpen = true;
			int PanicKey = 0;
			int MenuKey = VK_RSHIFT;
			bool CaptureBypass = false;
			int ThreadDelay = 0;
			// Include downed/dead peds in the entity list. Off by default —
			// on most servers a "dead" player is still a valid ped and would
			// otherwise soak up your aim.
			bool IncludeDeathPlayers = false;
		}General;
	};
}

inline Cheat::Options g_Options;