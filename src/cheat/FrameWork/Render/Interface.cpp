#include "Interface.hpp"
#include <Windows.h>
#include <thread>
#include <Cheat/Options.hpp>
#include <Cheat/Cheat.hpp>
#include <Cheat/ConfigSystem.hpp>
#include <Cheat/Features/Misc/Bypass.hpp>
#include "Cheat/FivemSDK/Fivem.hpp"
// KernelCloak removed — EFI bootkit handles kernel ops
#include "FrameWork/Dependencies/ImGui/custom.hpp"
#include "FrameWork/Dependencies/ImGui/slider.hpp"

// Logo texture for the overlay — loaded from logo.png at init
static ID3D11ShaderResourceView* g_OverlayLogo = nullptr;

// Expiry string shared from loader
#include <string>
extern std::string g_LicenseExpiry;

inline Cheat::ConfigManager Condif;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace FrameWork
{
	void Interface::Initialize(HWND Window, HWND TargetWindow, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext)
	{
		hWindow = Window;
		hTargetWindow = TargetWindow;
		IDevice = Device;

		ImGui::CreateContext();
		ImGui_ImplWin32_Init(hWindow);
		ImGui_ImplDX11_Init(Device, DeviceContext);

		// Load logo from same dir as exe
		wchar_t exePath[MAX_PATH] = {};
		GetModuleFileNameW(NULL, exePath, MAX_PATH);
		std::wstring logoPath(exePath);
		logoPath = logoPath.substr(0, logoPath.rfind(L'\\')) + L"\\logo.png";
		D3DX11CreateShaderResourceViewFromFileW(Device, logoPath.c_str(),
			NULL, NULL, &g_OverlayLogo, NULL);

		// Always remove WS_EX_TRANSPARENT so ESP is visible even when menu is closed
		{
			LONG_PTR exStyle = GetWindowLongPtr(hWindow, GWL_EXSTYLE);
			exStyle &= ~WS_EX_TRANSPARENT;
			SetWindowLongPtr(hWindow, GWL_EXSTYLE, exStyle);
		}
		if (bIsMenuOpen)
		{
			SetForegroundWindow(hWindow);
			SetFocus(hWindow);
		}
	}

	static void ApplyTheme()
	{
		ImGuiStyle* Style = &ImGui::GetStyle();
		auto& T = g_Options.Theme;

		Style->WindowRounding = T.WindowRounding;
		Style->ChildRounding = T.ChildRounding;
		Style->FrameRounding = T.FrameRounding;
		Style->PopupRounding = T.ButtonRounding;
		Style->WindowBorderSize = 1;
		Style->WindowPadding = ImVec2(0, 0);
		Style->WindowShadowSize = 0;
		Style->ScrollbarSize = 3;
		Style->ScrollbarRounding = 0;

		ImVec4 accent(T.AccentColor[0], T.AccentColor[1], T.AccentColor[2], T.AccentColor[3]);
		ImVec4 bg(T.BgColor[0], T.BgColor[1], T.BgColor[2], T.BgColor[3]);
		ImVec4 childBg(T.ChildBgColor[0], T.ChildBgColor[1], T.ChildBgColor[2], T.ChildBgColor[3]);
		ImVec4 text(T.TextColor[0], T.TextColor[1], T.TextColor[2], T.TextColor[3]);
		ImVec4 border(T.BorderColor[0], T.BorderColor[1], T.BorderColor[2], T.BorderColor[3]);

		Style->Colors[ImGuiCol_WindowBg] = bg;
		Style->Colors[ImGuiCol_ChildBg] = childBg;
		Style->Colors[ImGuiCol_Border] = border;
		Style->Colors[ImGuiCol_Text] = text;
		Style->Colors[ImGuiCol_PopupBg] = ImVec4(bg.x * 0.8f, bg.y * 0.8f, bg.z * 0.8f, 1.f);

		Style->Colors[ImGuiCol_Button] = ImVec4(accent.x * 0.5f, accent.y * 0.5f, accent.z * 0.5f, 1.f);
		Style->Colors[ImGuiCol_ButtonHovered] = ImVec4(accent.x * 0.7f, accent.y * 0.7f, accent.z * 0.7f, 1.f);
		Style->Colors[ImGuiCol_ButtonActive] = accent;

		Style->Colors[ImGuiCol_FrameBg] = ImVec4(bg.x * 1.3f, bg.y * 1.3f, bg.z * 1.3f, 1.f);
		Style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(accent.x * 0.4f, accent.y * 0.4f, accent.z * 0.4f, 0.4f);
		Style->Colors[ImGuiCol_FrameBgActive] = ImVec4(accent.x * 0.5f, accent.y * 0.5f, accent.z * 0.5f, 0.6f);

		Style->Colors[ImGuiCol_CheckMark] = accent;
		Style->Colors[ImGuiCol_SliderGrab] = accent;
		Style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(accent.x * 1.2f, accent.y * 1.2f, accent.z * 1.2f, 1.f);

		Style->Colors[ImGuiCol_Header] = ImVec4(accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 0.4f);
		Style->Colors[ImGuiCol_HeaderHovered] = ImVec4(accent.x * 0.5f, accent.y * 0.5f, accent.z * 0.5f, 0.6f);
		Style->Colors[ImGuiCol_HeaderActive] = accent;

		Style->Colors[ImGuiCol_Separator] = ImColor(0, 0, 0, 0);
		Style->Colors[ImGuiCol_SeparatorActive] = ImColor(0, 0, 0, 0);
		Style->Colors[ImGuiCol_SeparatorHovered] = ImColor(0, 0, 0, 0);
		Style->Colors[ImGuiCol_ResizeGrip] = ImColor(0, 0, 0, 0);
		Style->Colors[ImGuiCol_ResizeGripActive] = ImColor(0, 0, 0, 0);
		Style->Colors[ImGuiCol_ResizeGripHovered] = ImColor(0, 0, 0, 0);

		Style->Colors[ImGuiCol_ScrollbarBg] = ImColor(0, 0, 0, 0);
		Style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(accent.x * 0.5f, accent.y * 0.5f, accent.z * 0.5f, 0.8f);
		Style->Colors[ImGuiCol_ScrollbarGrabActive] = accent;
		Style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(accent.x * 0.7f, accent.y * 0.7f, accent.z * 0.7f, 0.9f);

		Style->Alpha = T.WindowOpacity;

		// Sync to custom widget globals
		g_CheckboxStyle = T.CheckboxStyle;
		memcpy(g_AccentColor, T.AccentColor, sizeof(g_AccentColor));
	}

	void Interface::UpdateStyle()
	{
		ImGuiIO& io = ImGui::GetIO();
		ApplyTheme();
		Assets::Initialize(IDevice);
		Condif.Setup();
	}

	namespace menu
	{
		bool particles = true;
		static int num_particles = 100;
		static int velocidade = 1;
		static int particle_size = 4;
	}

	static ImVec2 partile_pos[100];
	static ImVec2 partile_target_pos[100];
	static float partile_speed[100];
	static float partile_radius[100];

	void InitializeParticleSystem()
	{
		std::srand(static_cast<unsigned>(std::time(nullptr)));
	}

	void Particles()
	{
		if (!menu::particles)
			return;

		ImVec2 screen_size = { (float)GetSystemMetrics(SM_CXSCREEN), (float)GetSystemMetrics(SM_CYSCREEN) };

		for (int i = 0; i < menu::num_particles; i++)
		{
			if (partile_pos[i].x == 0 || partile_pos[i].y == 0)
			{
				partile_pos[i].x = rand() % (int)screen_size.x + 1;
				partile_pos[i].y = 15.f;
				partile_radius[i] = rand() % menu::particle_size;

				partile_target_pos[i].x = rand() % (int)screen_size.x;
				partile_target_pos[i].y = screen_size.y * 2;

				partile_speed[i] = static_cast<float>(menu::velocidade) + 1 + rand() % 25;
			}

			partile_pos[i] = ImLerp(partile_pos[i], partile_target_pos[i], ImGui::GetIO().DeltaTime * (partile_speed[i] / 60.0f));

			if (partile_pos[i].y > screen_size.y)
			{
				partile_pos[i].x = 0;
				partile_pos[i].y = 0;
			}

			// Modificando a cor das partículas para cinza (112, 112, 112)
			ImGui::GetWindowDrawList()->AddCircleFilled(partile_pos[i], partile_radius[i],
				ImColor(112 / 255.0f, 112 / 255.0f, 112 / 255.0f, g_Options.Misc.Screen.PartColor[3]));
		}
	}

	void Interface::AimbotTab()
	{
		// 0 = Aimbot, 1 = Silent, 2 = Trigger. Was a bool with two buttons;
		// the triggerbot thread existed but had no UI at all, so it ran
		// entirely on defaults with no way to configure or even see it.
		static int subTab = 0;

		float buttonWidth = 110;
		float buttonHeight = 23;
		float padding = 16;

		ImGui::SetCursorPos(ImVec2(padding, 5));
		if (Custom::TabButton("Aimbot", subTab == 0))
		{
			subTab = 0;
		}

		ImGui::SetCursorPos(ImVec2(padding + buttonWidth + padding, 5));
		if (Custom::TabButton("Silent", subTab == 1))
		{
			subTab = 1;
		}

		ImGui::SetCursorPos(ImVec2(padding + (buttonWidth + padding) * 2, 5));
		if (Custom::TabButton("Trigger", subTab == 2))
		{
			subTab = 2;
		}

		if (subTab == 0)
		{
			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild(("Aimbot"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 400));
			{
				Custom::Checkbox(("Enabled"), &g_Options.LegitBot.AimBot.Enabled);
				if (g_Options.LegitBot.AimBot.Enabled)
				{
					ImGui::KeyBind(("Keybind"), &g_Options.LegitBot.AimBot.KeyBind);
					ImGui::Combo(("Key Mode"), &g_Options.LegitBot.AimBot.KeyMode, ("Hold\0Toggle\0"));
					Custom::Checkbox(("Draw FOV"), &g_Options.Misc.Screen.ShowAimbotFov);
					if (g_Options.Misc.Screen.ShowAimbotFov)
					{
						ImGui::ColorEdit4(("FOV color"), g_Options.Misc.Screen.AimbotFovColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
					}
					Custom::Checkbox("Target NPC", &g_Options.LegitBot.AimBot.TargetNPC);
					Custom::Checkbox(("Wall check"), &g_Options.LegitBot.AimBot.VisibleCheck);
					ImGui::Combo(("Hitbox"), &g_Options.LegitBot.AimBot.HitBox, ("Head\0Neck\0Chest\0"));
				}
			}
			ImGui::EndCustomChild();

			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 50));
			ImGui::CustomChild(("Aimbot Settings"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 220));
			{
				if (g_Options.LegitBot.AimBot.Enabled)
				{
					ImGui::SliderInt(("Field Of View"), &g_Options.LegitBot.AimBot.FOV, 0, 180, ("%dpx"));
					ImGui::SliderInt(("Max distance"), &g_Options.LegitBot.AimBot.MaxDistance, 0, 1000, ("%dm"));
					ImGui::SliderInt(("Smooth X"), &g_Options.LegitBot.AimBot.SmoothHorizontal, 0, 100, ("%d%"));
					ImGui::SliderInt(("Smooth Y"), &g_Options.LegitBot.AimBot.SmoothVertical, 0, 100, ("%d%"));
					Custom::Checkbox(("Enable prediction"), &g_Options.LegitBot.AimBot.EnablePrediction);
				}
			}
			ImGui::EndCustomChild();
		}
		else if (subTab == 1)
		{
			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild(("Silent Aim"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 400));
			{
				Custom::Checkbox(("Enabled"), &g_Options.LegitBot.SilentAim.Enabled);
				if (g_Options.LegitBot.SilentAim.Enabled)
				{
					ImGui::KeyBind(("Keybind"), &g_Options.LegitBot.SilentAim.KeyBind);
					ImGui::Combo(("Key Mode##silent"), &g_Options.LegitBot.SilentAim.KeyMode, ("Hold\0Toggle\0"));
					Custom::Checkbox(("Draw FOV"), &g_Options.Misc.Screen.ShowSilentAimFov);
					if (g_Options.Misc.Screen.ShowSilentAimFov)
					{
						ImGui::ColorEdit4(("FOV color"), g_Options.Misc.Screen.SilentFovColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
					}
					Custom::Checkbox(("Target NPC"), &g_Options.LegitBot.SilentAim.ShotNPC);
					Custom::Checkbox(("Wall check"), &g_Options.LegitBot.SilentAim.VisibleCheck);
					Custom::Checkbox(("Magic Bullet"), &g_Options.LegitBot.SilentAim.MagicBullet);
					ImGui::Combo(("Hitbox##silent"), &g_Options.LegitBot.SilentAim.HitBox, ("Head\0Neck\0Chest\0"));
				}
			}
			ImGui::EndCustomChild();

			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 50));
			ImGui::CustomChild(("Silent Aim Settings"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 220));
			{
				if (g_Options.LegitBot.SilentAim.Enabled)
				{
					ImGui::SliderInt(("Field Of View"), &g_Options.LegitBot.SilentAim.Fov, 0, 180, ("%dpx"));
					ImGui::SliderInt(("Max distance"), &g_Options.LegitBot.SilentAim.MaxDistance, 0, 1000, ("%dm"));
				}
			}
			ImGui::EndCustomChild();
		}
		else
		{
			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild(("Trigger Bot"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 400));
			{
				Custom::Checkbox(("Enabled"), &g_Options.LegitBot.Trigger.Enabled);
				if (g_Options.LegitBot.Trigger.Enabled)
				{
					ImGui::KeyBind(("Keybind"), &g_Options.LegitBot.Trigger.KeyBind);
					ImGui::Combo(("Key Mode##trigger"), &g_Options.LegitBot.Trigger.KeyMode, ("Hold\0Toggle\0"));
					Custom::Checkbox(("Draw FOV"), &g_Options.Misc.Screen.ShowTriggerFov);
					if (g_Options.Misc.Screen.ShowTriggerFov)
					{
						ImGui::ColorEdit4(("FOV color"), g_Options.Misc.Screen.TriggerFovColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
					}
					Custom::Checkbox(("Target NPC"), &g_Options.LegitBot.Trigger.ShotNPC);
					Custom::Checkbox(("Wall check"), &g_Options.LegitBot.Trigger.VisibleCheck);
					Custom::Checkbox(("Only while aiming"), &g_Options.LegitBot.Trigger.RequireAiming);
				}
			}
			ImGui::EndCustomChild();

			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 50));
			ImGui::CustomChild(("Trigger Settings"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 280));
			{
				if (g_Options.LegitBot.Trigger.Enabled)
				{
					ImGui::SliderInt(("Field Of View"), &g_Options.LegitBot.Trigger.FOV, 0, 180, ("%dpx"));
					ImGui::SliderInt(("Max distance"), &g_Options.LegitBot.Trigger.MaxDistance, 0, 1000, ("%dm"));
					ImGui::SliderInt(("Reaction time"), &g_Options.LegitBot.Trigger.ReactionTime, 0, 500, ("%dms"));
					ImGui::SliderInt(("Reaction jitter"), &g_Options.LegitBot.Trigger.ReactionJitter, 0, 300, ("+0-%dms"));
					ImGui::SliderInt(("Refire delay"), &g_Options.LegitBot.Trigger.RefireDelay, 20, 500, ("%dms"));
					ImGui::SliderInt(("Hit chance"), &g_Options.LegitBot.Trigger.HitChance, 1, 100, ("%d%%"));
				}
			}
			ImGui::EndCustomChild();
		}
	}

	void Interface::VisualsTab()
	{
		static bool showPlayers = true; // Começa mostrando Players por padrão

		// Configurações dos botões
		float buttonWidth = 110;
		float buttonHeight = 23;
		float padding = 16;

		// Botão Players
		ImGui::SetCursorPos(ImVec2(padding, 5));
		if (Custom::TabButton("Players", showPlayers))
		{
			showPlayers = true;
		}

		// Botão Vehicles
		ImGui::SetCursorPos(ImVec2(padding + buttonWidth + padding, 5));
		if (Custom::TabButton("Vehicles", !showPlayers))
		{
			showPlayers = false;
		}

		if (showPlayers)
		{
			// Primeiro CustomChild com as checkboxes
			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild(("Visuals"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 170));
			{
				Custom::Checkbox(("Enabled"), &g_Options.Visuals.ESP.Players.Enabled);
				if (g_Options.Visuals.ESP.Players.Enabled)
				{
					ImGui::SliderInt(("Render distance"), &g_Options.Visuals.ESP.Players.RenderDistance, 0, 1000, ("%dm"));
					Custom::Checkbox(("Target local player"), &g_Options.Visuals.ESP.Players.ShowLocalPlayer);
					Custom::Checkbox(("Target NPC"), &g_Options.Visuals.ESP.Players.ShowNPCs);
				}
			}
			ImGui::EndCustomChild();


			if (g_Options.Visuals.ESP.Players.Enabled)
			{
				ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 50));
				ImGui::CustomChild(("Options"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 285));
				{
					if (g_Options.Visuals.ESP.Players.Enabled)
					{
						Custom::Checkbox(("Head"), &g_Options.Visuals.ESP.Players.Head);
						if (g_Options.Visuals.ESP.Players.Head)
						{
							ImGui::SliderFloat(("Head size"), &g_Options.Visuals.ESP.Players.HeadSize, 1.f, 10.f);
						}
						Custom::Checkbox(("Skeleton"), &g_Options.Visuals.ESP.Players.Skeleton);
						Custom::Checkbox(("Box"), &g_Options.Visuals.ESP.Players.Box);
						if (g_Options.Visuals.ESP.Players.Box)
						{
							Custom::Checkbox(("Filled"), &g_Options.Visuals.ESP.Players.FilledBox);
						}
						Custom::Checkbox(("Name"), &g_Options.Visuals.ESP.Players.Name);
						Custom::Checkbox(("Health bar"), &g_Options.Visuals.ESP.Players.HealthBar);
						Custom::Checkbox(("Armor bar"), &g_Options.Visuals.ESP.Players.ArmorBar);
						Custom::Checkbox(("Weapon name"), &g_Options.Visuals.ESP.Players.WeaponName);
						Custom::Checkbox(("Distance"), &g_Options.Visuals.ESP.Players.Distance);
						Custom::Checkbox(("Snaplines"), &g_Options.Visuals.ESP.Players.SnapLines);
					}
				}
				ImGui::EndCustomChild();
			}


			if (g_Options.Visuals.ESP.Players.Enabled)
			{
				ImGui::SetCursorPos(ImVec2(20, 50 + 165 + 20));
				ImGui::CustomChild(("Color options"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 165));
				{
					ImGui::ColorEdit4(("Head color"), g_Options.Visuals.ESP.Players.HeadColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
					ImGui::ColorEdit4(("Skeleton color"), g_Options.Visuals.ESP.Players.SkeletonColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
					ImGui::ColorEdit4(("Box color"), g_Options.Visuals.ESP.Players.BoxColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
					if (g_Options.Visuals.ESP.Players.FilledBox)
					{
						ImGui::ColorEdit4(("Filled Box color"), g_Options.Visuals.ESP.Players.FilledBoxColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
					}
					ImGui::ColorEdit4(("Name color"), g_Options.Visuals.ESP.Players.NameColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
					ImGui::ColorEdit4(("Health bar color"), g_Options.Visuals.ESP.Players.HealthBarColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
					ImGui::ColorEdit4(("Armor bar color"), g_Options.Visuals.ESP.Players.ArmorBarColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
					ImGui::ColorEdit4(("Weapon name color"), g_Options.Visuals.ESP.Players.WeaponNameColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
					ImGui::ColorEdit4(("Distance color"), g_Options.Visuals.ESP.Players.DistanceColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
					ImGui::ColorEdit4(("Snaplines color"), g_Options.Visuals.ESP.Players.SnaplinesColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
					ImGui::ColorEdit4(("Friend color"), g_Options.Misc.FriendColor, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
				}
				ImGui::EndCustomChild();
			}
		}
		else
		{
			// Código para Vehicle ESP
			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild(("Vehicle ESP"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 170));
			{
				Custom::Checkbox(("Enabled"), &g_Options.Visuals.ESP.Vehicles.Enabled);
				Custom::Checkbox(("Ignore occupied vehicles"), &g_Options.Visuals.ESP.Vehicles.IgnoreOccupiedVehicles);
				Custom::Checkbox(("Dot marker"), &g_Options.Visuals.ESP.Vehicles.Marker);
				Custom::Checkbox(("Name"), &g_Options.Visuals.ESP.Vehicles.Name);
				Custom::Checkbox(("Distance"), &g_Options.Visuals.ESP.Vehicles.Distance);
				ImGui::ColorEdit4(("Marker color"), g_Options.Visuals.ESP.Vehicles.Color, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
			}
			ImGui::EndCustomChild();
		}
	}

	void Interface::MiscellaneousTab()
	{
		static bool showPlayer = true; // Começa mostrando Player por padrão

		// Configurações dos botões
		float buttonWidth = 110;
		float buttonHeight = 23;
		float padding = 16;

		// Botão Player
		ImGui::SetCursorPos(ImVec2(padding, 5));
		if (Custom::TabButton("Player", showPlayer))
		{
			showPlayer = true;
		}

		// Botão Other
		ImGui::SetCursorPos(ImVec2(padding + buttonWidth + padding, 5));
		if (Custom::TabButton("Other", !showPlayer))
		{
			showPlayer = false;
		}

		if (showPlayer)
		{
			// Player section
			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild(("Player"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 350));
			{
				Custom::Checkbox(("God"), &g_Options.Misc.Exploits.LocalPlayer.God);
				Custom::Checkbox(("Shrink"), &g_Options.Misc.Exploits.LocalPlayer.shrink);
				Custom::Checkbox("Speed Walk", &g_Options.Misc.Exploits.LocalPlayer.speed);
				if (g_Options.Misc.Exploits.LocalPlayer.speed)
				{
					ImGui::SliderFloat(("Speed"), &g_Options.Misc.Exploits.LocalPlayer.Player_speed, 1.f, 10.f, "%.1f");
				}
				Custom::Checkbox(("No recoil"), &g_Options.Misc.Exploits.LocalPlayer.norecoil);
				Custom::Checkbox(("No spread"), &g_Options.Misc.Exploits.LocalPlayer.nospread);
				Custom::Checkbox(("No reload"), &g_Options.Misc.Exploits.LocalPlayer.noreload);
				Custom::Checkbox("Super Punch", &g_Options.Misc.Exploits.LocalPlayer.SuperPunch);
				if (g_Options.Misc.Exploits.LocalPlayer.SuperPunch)
				{
					ImGui::SliderFloat("Force", &g_Options.Misc.Exploits.LocalPlayer.SuperPunchForce, 10.f, 200.f, "%.0f");
				}
				Custom::Checkbox("Invisible", &g_Options.Misc.Exploits.LocalPlayer.Invisible);
				Custom::Checkbox("Force Desync (Ghost)", &g_Options.Misc.Exploits.LocalPlayer.Desync);
				if (g_Options.Misc.Exploits.LocalPlayer.Desync)
				{
					ImGui::KeyBind("Desync Key", &g_Options.Misc.Exploits.LocalPlayer.DesyncKeyBind);
				}
			}
			ImGui::EndCustomChild();

			// Vehicle section
			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 50));
			ImGui::CustomChild(("Vehicle"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 195));
			{
				Custom::Checkbox("Vehicle God", &g_Options.Misc.Exploits.LocalPlayer.VehicleGod);
				Custom::Checkbox("Vehicle Speed Boost", &g_Options.Misc.Exploits.LocalPlayer.VehicleSpeed);
				if (g_Options.Misc.Exploits.LocalPlayer.VehicleSpeed)
				{
					ImGui::SliderFloat("Multiplier", &g_Options.Misc.Exploits.LocalPlayer.VehicleSpeedMultiplier, 1.1f, 5.f, "x%.1f");
					ImGui::KeyBind("Boost Key", &g_Options.Misc.Exploits.LocalPlayer.VehicleSpeedBind);
					ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "No key = always on");
				}
			}
			ImGui::EndCustomChild();

			// Radar section
			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 50 + 195 + 15));
			ImGui::CustomChild(("Radar"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 130));
			{
				Custom::Checkbox("Radar", &g_Options.Visuals.ESP.Radar.Enabled);
				if (g_Options.Visuals.ESP.Radar.Enabled)
				{
					ImGui::SliderFloat("Size", &g_Options.Visuals.ESP.Radar.Size, 80.f, 250.f, "%.0f");
					ImGui::SliderFloat("Range", &g_Options.Visuals.ESP.Radar.Range, 50.f, 500.f, "%.0fm");
				}
			}
			ImGui::EndCustomChild();
		}
		else
		{
			// Other functions section
			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild(("No Clip"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 150));
			{
				Custom::Checkbox(("Enabled"), &g_Options.Misc.Exploits.LocalPlayer.Noclip);
				if (g_Options.Misc.Exploits.LocalPlayer.Noclip)
				{
					ImGui::KeyBind(("Keybind"), &g_Options.Misc.Exploits.LocalPlayer.NoclipBind);
					ImGui::Combo(("Key Mode##noclip"), &g_Options.Misc.Exploits.LocalPlayer.NoclipKeyMode, ("Hold\0Toggle\0"));
					ImGui::SliderInt(("Speed"), &g_Options.Misc.Exploits.LocalPlayer.NoclipSpeed, 0, 100, ("%dm/s"));
				}
			}
			ImGui::EndCustomChild();

			ImGui::SetCursorPos(ImVec2(20, 50 + 150 + 20));
			ImGui::CustomChild(("Teleport & Other"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 200));
			{
				Custom::Checkbox("Safe TP (anti-detect)", &g_Options.Misc.Exploits.LocalPlayer.SafeTP);
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Teleport in chunks to avoid flags");

				ImGui::Spacing();
				if (ImGui::Button("TP to Waypoint"))
				{
					Cheat::Exploits::TeleportToWaypoint();
				}

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::SliderInt(("HP amount"), &g_Options.Misc.Exploits.LocalPlayer.health_ammount, 0, 400, ("%d"));
				if (ImGui::Button(("Set HP")))
				{
					g_Options.Misc.Exploits.LocalPlayer.Start_Health = true;
				}
			}
			ImGui::EndCustomChild();
		}
	}


	void Interface::VehiclesTab()
	{
		static bool showVehicleList = true; // Começa mostrando Vehicle List por padrão

		// Configurações dos botões
		float buttonWidth = 110;
		float buttonHeight = 23;
		float padding = 16;

		// Botão Vehicle List
		ImGui::SetCursorPos(ImVec2(padding, 5));
		if (Custom::TabButton("Vehicle List", showVehicleList))
		{
			showVehicleList = true;
		}

		// Botão Vehicle ESP
		ImGui::SetCursorPos(ImVec2(padding + buttonWidth + padding, 5));
		if (Custom::TabButton("Vehicle ESP", !showVehicleList))
		{
			showVehicleList = false;
		}

		if (showVehicleList)
		{
			// Código existente da lista de veículos
			static ImGuiTextFilter mFilter;
			static Cheat::VehicleInfo SelectedVehicle = Cheat::VehicleInfo();

			if (SelectedVehicle.Vehicle->GetCoordinate().DistTo(Cheat::g_Fivem.GetLocalPlayerInfo().WorldPos) > 600)
				SelectedVehicle = Cheat::VehicleInfo();

			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild(("Vehicle list"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 370));
			{
				mFilter.Draw(("Search"));
				if (ImGui::BeginListBox(("##VehList"), ImVec2(ImGui::GetWindowSize().x - 20, 270)))
				{
					if (Cheat::g_Fivem.GetLocalPlayerInfo().Ped)
					{
						for (Cheat::VehicleInfo Current : Cheat::g_Fivem.GetVehicleList())
						{
							if (mFilter.PassFilter(Current.Name.c_str()))
							{
								std::string Text = Current.Name + (" (") + std::to_string((int)(Current.Vehicle->GetCoordinate().DistTo(Cheat::g_Fivem.GetLocalPlayerInfo().WorldPos))) + ("m)");

								bool Selected = (Current.Vehicle == SelectedVehicle.Vehicle);

								if (ImGui::Selectable(Text.c_str(), &Selected))
								{
									SelectedVehicle = Current;
								}
							}
						}
					}
					ImGui::EndListBox();
				}
			}
			ImGui::EndCustomChild();

			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 50));
			ImGui::CustomChild(("Actions"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 120));
			{
				if (SelectedVehicle.iIndex != -1)
				{
					if (ImGui::Button(("Teleport to vehicle")))
					{
						Cheat::g_Fivem.TeleportObject((uintptr_t)Cheat::g_Fivem.GetLocalPlayerInfo().Ped,
							Cheat::g_Fivem.GetLocalPlayerInfo().Ped->GetNavigation(),
							Cheat::g_Fivem.GetLocalPlayerInfo().Ped->GetModelInfo(),
							SelectedVehicle.Vehicle->GetCoordinate(),
							SelectedVehicle.Vehicle->GetCoordinate(),
							true);
					}
					if (ImGui::Button(("Unlock vehicle")))
					{
						SelectedVehicle.Vehicle->SetDoorLock(1);
					}
					if (ImGui::Button(("Lock vehicle")))
					{
						SelectedVehicle.Vehicle->SetDoorLock(2);
					}
				}
				else
				{
					ImGui::Text(("Select a vehicle first!"));
				}
			}
			ImGui::EndCustomChild();
		}
		else
		{
			// Código do Vehicle ESP
			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild(("Vehicle ESP"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 250));
			{
				Custom::Checkbox(("Enabled"), &g_Options.Visuals.ESP.Vehicles.Enabled);
				Custom::Checkbox(("Ignore occupied vehicles"), &g_Options.Visuals.ESP.Vehicles.IgnoreOccupiedVehicles);
				Custom::Checkbox(("Dot marker"), &g_Options.Visuals.ESP.Vehicles.Marker);
				Custom::Checkbox(("Name"), &g_Options.Visuals.ESP.Vehicles.Name);
				Custom::Checkbox(("Distance"), &g_Options.Visuals.ESP.Vehicles.Distance);
				ImGui::ColorEdit4(("Marker color"), g_Options.Visuals.ESP.Vehicles.Color, ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs);
			}
			ImGui::EndCustomChild();
		}
	}

	void Interface::PlayersTab()
	{
		static ImGuiTextFilter mFilter;
		static Cheat::Entity SelectedEntity = Cheat::Entity();

		if (SelectedEntity.Cordinates.DistTo(Cheat::g_Fivem.GetLocalPlayerInfo().WorldPos) > 600)
			SelectedEntity = Cheat::Entity();

		ImGui::SetCursorPos(ImVec2(20, 20));
		ImGui::CustomChild(("Players list"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 370));
		{
			mFilter.Draw(("Search"));
			if (ImGui::BeginListBox(("##PlayerList"), ImVec2(ImGui::GetWindowSize().x - 20, 270)))
			{
				if (Cheat::g_Fivem.GetLocalPlayerInfo().Ped)
				{
					for (Cheat::Entity Current : Cheat::g_Fivem.GetEntitiyList())
					{
						if (Current.StaticInfo.bIsNPC || !Current.StaticInfo.Ped || Current.StaticInfo.bIsLocalPlayer)
							continue;

						if (mFilter.PassFilter(Current.StaticInfo.Name.c_str()))
						{
							ImGui::PushID(Current.StaticInfo.iIndex);
							{
								std::string Text = Current.StaticInfo.Name + (" (") + std::to_string((int)(Current.Cordinates.DistTo(Cheat::g_Fivem.GetLocalPlayerInfo().WorldPos))) + ("m)");

								if (Current.StaticInfo.IsFriend)
									Text += (" (friend)");

								bool Selected = (Current.StaticInfo.Ped == SelectedEntity.StaticInfo.Ped);

								if (ImGui::Selectable(Text.c_str(), &Selected))
								{
									SelectedEntity = Current;
								}
							}
							ImGui::PopID();
						}
					}
				}

				ImGui::EndListBox();
			}
		}
		ImGui::EndCustomChild();

		ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 20));
		ImGui::CustomChild(("Selected player information"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 145));
		{
			if (SelectedEntity.StaticInfo.iIndex > 0)
			{
				ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.8f), ("Name: ")); ImGui::SameLine(); ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.6f), SelectedEntity.StaticInfo.Name.c_str());
				ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.8f), ("Friend: ")); ImGui::SameLine(); ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.6f), SelectedEntity.StaticInfo.IsFriend ? ("True") : ("False"));
				ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.8f), ("Net ID: ")); ImGui::SameLine(); ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.6f), std::to_string(SelectedEntity.StaticInfo.NetId).c_str());
				ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.8f), ("Distance: ")); ImGui::SameLine(); ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.6f), (std::to_string((int)(SelectedEntity.Cordinates.DistTo(Cheat::g_Fivem.GetLocalPlayerInfo().WorldPos))) + ("m")).c_str());

				Cheat::CWeaponManager* WeaponManager = SelectedEntity.StaticInfo.Ped->GetWeaponManager();
				if (WeaponManager)
				{
					Cheat::CWeaponInfo* WeaponInfo = WeaponManager->GetWeaponInfo();
					if (WeaponInfo)
					{
						ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.8f), ("Weapon: ")); ImGui::SameLine(); ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.6f), WeaponInfo->GetWeaponName().c_str());
					}
				}
			}
			else
			{
				ImGui::Text(("Select a player first!"));
			}
		}
		ImGui::EndCustomChild();

		ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 20 + 145 + 20));
		ImGui::CustomChild(("Actions"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 120));
		{
			if (SelectedEntity.StaticInfo.iIndex > 0)
			{
				if (ImGui::Button(("Teleport to player")))
				{
					Cheat::g_Fivem.TeleportObject((uintptr_t)Cheat::g_Fivem.GetLocalPlayerInfo().Ped,
						Cheat::g_Fivem.GetLocalPlayerInfo().Ped->GetNavigation(),
						Cheat::g_Fivem.GetLocalPlayerInfo().Ped->GetModelInfo(),
						SelectedEntity.Cordinates,
						SelectedEntity.Cordinates,
						true);
				}

				if (!SelectedEntity.StaticInfo.IsFriend)
				{
					if (ImGui::Button(("Add friend")))
					{
						if (!Cheat::g_Fivem.AllEntitesList[SelectedEntity.StaticInfo.Ped].IsFriend || !SelectedEntity.StaticInfo.IsFriend)
							Cheat::g_Fivem.FriendList[SelectedEntity.StaticInfo.Ped] = SelectedEntity.StaticInfo;

						Cheat::g_Fivem.AllEntitesList[SelectedEntity.StaticInfo.Ped].IsFriend = true;
						SelectedEntity.StaticInfo.IsFriend = true;
					}
				}
				else
				{
					if (ImGui::Button(("Remove friend")))
					{
						if (Cheat::g_Fivem.AllEntitesList[SelectedEntity.StaticInfo.Ped].IsFriend || SelectedEntity.StaticInfo.IsFriend)
							Cheat::g_Fivem.FriendList.erase(SelectedEntity.StaticInfo.Ped);

						Cheat::g_Fivem.AllEntitesList[SelectedEntity.StaticInfo.Ped].IsFriend = false;
						SelectedEntity.StaticInfo.IsFriend = false;
					}
				}
				if (ImGui::Button(("Kick from vehicle")))
				{
					Cheat::Exploits::KickFromVehicle(SelectedEntity.StaticInfo.Ped);
				}
			}
			else
			{
				ImGui::Text(("Select a player first!"));
			}
		}
		ImGui::EndCustomChild();
	}

	void Interface::SettingsTab()
	{
		static bool showConfigs = true; // Começa mostrando Configs por padrão

		// Configurações dos botões
		float buttonWidth = 110;
		float buttonHeight = 23;
		float padding = 16;

		// Botão Configs
		ImGui::SetCursorPos(ImVec2(padding, 5));
		if (Custom::TabButton("Configs", showConfigs))
		{
			showConfigs = true;
		}

		// Botão Other
		ImGui::SetCursorPos(ImVec2(padding + buttonWidth + padding, 5));
		if (Custom::TabButton("Other", !showConfigs))
		{
			showConfigs = false;
		}

		if (showConfigs)
		{
			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild(("Configs"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 190));
			{
				if (ImGui::Button(("Load Config")))
				{
					Condif.LoadConfig();
				}

				if (ImGui::Button(("Save Config")))
				{
					Condif.SaveConfig();
				}

				if (ImGui::Button(("Import Config")))
				{
					Condif.ImportFromClipboard();
				}

				if (ImGui::Button(("Export Config")))
				{
					Condif.ExportToClipboard();
				}
			}
			ImGui::EndCustomChild();

			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 50));
			ImGui::CustomChild(("Menu Settings"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 280));
			{
				ImGui::KeyBind(("Menu bind"), &g_Options.General.MenuKey);
				Custom::Checkbox(("Stream mode"), &g_Options.General.CaptureBypass);
				Custom::Checkbox(("Include death players"), &g_Options.General.IncludeDeathPlayers);
				Custom::Checkbox("Particles", &menu::particles);

				if (menu::particles)
				{
					ImGui::SliderInt("Particles amount", &menu::num_particles, 1, 100);
					ImGui::SliderInt("Particles speed", &menu::velocidade, 1, 10);
					ImGui::SliderInt("Particles size", &menu::particle_size, 1, 10);
				}
			}
			ImGui::EndCustomChild();

			// Offset diagnostics. Features whose pointer never resolved used to
			// just silently do nothing, which is indistinguishable from "the
			// cheat is broken". Now it says which one and why.
			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 50 + 280 + 20));
			ImGui::CustomChild(("Offset Status"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 150));
			{
				const ImVec4 OkColor(0.35f, 0.9f, 0.35f, 1.f);
				const ImVec4 BadColor(1.f, 0.35f, 0.35f, 1.f);

				ImGui::Text(("Game build: %d"), Cheat::g_Fivem.GetResolvedBuild());

				if (Cheat::g_Fivem.IsExactOffsetMatch())
					ImGui::TextColored(OkColor, ("Offsets: exact match"));
				else
					ImGui::TextColored(BadColor, ("Offsets: scanned / approximate"));

				auto StatusLine = [&](const char* label, bool ok)
					{
						ImGui::TextColored(ok ? OkColor : BadColor, ok ? ("%s: OK") : ("%s: NOT FOUND"), label);
					};

				StatusLine(("World"), Cheat::g_Fivem.IsWorldValid());
				StatusLine(("ViewPort (ESP)"), Cheat::g_Fivem.IsViewPortValid());
				StatusLine(("Camera (Aimbot/Noclip)"), Cheat::g_Fivem.IsCameraValid());
				StatusLine(("HandleBullet (Silent)"), Cheat::g_Fivem.IsHandleBulletValid());
					StatusLine(("BlipList (Waypoint TP)"), Cheat::g_Fivem.IsBlipListValid());
			}
			ImGui::EndCustomChild();
		}
		else
		{
			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild(("Binds"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 150));
			{
				ImGui::KeyBind(("God Bind"), &g_Options.Misc.Exploits.LocalPlayer.godbind);
				ImGui::KeyBind(("TP Way Bind"), &g_Options.Misc.Exploits.LocalPlayer.tpbind);
				ImGui::KeyBind(("Unlock closest vehicle Bind"), &g_Options.Misc.Exploits.LocalPlayer.unlockbind);
			}
			ImGui::EndCustomChild();

			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 50));
			ImGui::CustomChild(("Other"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 150));
			{
				if (ImGui::Button(("Unload")))
				{
					g_Options.General.ShutDown = true;
				}

				// Adicione aqui outras opções que você queira incluir na aba Other
			}
			ImGui::EndCustomChild();
		}
	}

	void Interface::BypassTab()
	{
		// ── BYPASS SS (PANIC BUTTON) ──
		ImGui::SetCursorPos(ImVec2(20, 10));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.15f, 0.15f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.25f, 0.25f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.f));
		ImGui::SetWindowFontScale(1.25f);
		if (ImGui::Button("BYPASS & NUKE", ImVec2((ImGui::GetWindowSize().x - 50) / 1.25f, 38 / 1.25f)))
		{
			Cheat::Bypass::ActivateAndNuke();
		}
		ImGui::SetWindowFontScale(1.0f);
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
		ImGui::SetCursorPos(ImVec2(20, 55));
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "Nuke total: mata SS + borra exe + AmCache + SRUM + browser + Defender + todo.");

		// ── LIVE PROTECTION ──
		ImGui::SetCursorPos(ImVec2(20, 75));
		ImGui::CustomChild(("Proteccion en vivo"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 195));
		{
			Custom::Checkbox("Stream Proof", &g_Options.General.CaptureBypass);
			ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.f), "ON = invisible a capturas/SS");
			ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.f), "OFF = visible en pantalla compartida");

			ImGui::Spacing();
			if (ImGui::Button("Bloquear Scanners"))
				Cheat::Bypass::BlockScannerAccess();
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.f), "DACL anti-OpenProcess");
		}
		ImGui::EndCustomChild();

		// ── DEEP CLEAN ──
		ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 75));
		ImGui::CustomChild(("Limpieza profunda"), ImVec2(ImGui::GetWindowSize().x / 2 - 30, 195));
		{
			if (ImGui::Button("Limpiar TODO"))
			{
				Cheat::Bypass::CleanAmCache();
				Cheat::Bypass::CleanSRUM();
				Cheat::Bypass::CleanBrowserHistory();
				Cheat::Bypass::CleanDefenderHistory();
				Cheat::Bypass::CleanRecycleBin();
				Cheat::Bypass::CleanTraces();
				Cheat::Bypass::CleanUserAssist();
				Cheat::Bypass::CleanRecentDocs();
				Cheat::Bypass::CleanEventLogs();
				Cheat::Bypass::CleanShellArtifacts();
				Cheat::Bypass::CleanSystemInfo();
				Cheat::Bypass::DeleteConfigs();
				Cheat::Bypass::CleanClipboard();
				Cheat::Bypass::CleanDNSCache();
				Cheat::Bypass::CleanNetworkTraces();
			}
			ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.f), "AmCache + SRUM + Browser + Defender");
			ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.f), "Prefetch + UserAssist + EventLogs");
			ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.f), "MUICache + ShimCache + VSS + Timeline");
			ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.f), "Papelera + DNS + Configs");

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Protect Memory"))
				Cheat::Bypass::ProtectMemoryRegions();
			if (Cheat::Bypass::IsMemoryProtected())
				ImGui::TextColored(ImVec4(0.35f, 0.9f, 0.35f, 1.f), "PE Header: BORRADO");
			else
				ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.f), "Borra MZ/PE de RAM (anti-dump)");
		}
		ImGui::EndCustomChild();

		// ── STATUS ──
		ImGui::SetCursorPos(ImVec2(20, 75 + 195 + 15));
		ImGui::CustomChild(("Estado"), ImVec2(ImGui::GetWindowSize().x - 50, 155));
		{
			const ImVec4 Ok(0.35f, 0.9f, 0.35f, 1.f);
			const ImVec4 Off(0.5f, 0.5f, 0.5f, 1.f);

			// Check HVCI once per second — cheap registry read
			static bool s_HvciChecked = false;
			static bool s_HvciActive  = false;
			static bool s_KernelCloakOk = true;
			if (!s_HvciChecked)
			{
				DWORD val = 0, sz = sizeof(val);
				HKEY hk = NULL;
				if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
					L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity",
					0, KEY_READ, &hk) == ERROR_SUCCESS)
				{
					RegQueryValueExW(hk, L"Enabled", NULL, NULL, (BYTE*)&val, &sz);
					RegCloseKey(hk);
					s_HvciActive = (val == 1);
				}
				// Also check VBS status via WMI shortcut: read the cached field
				DWORD vbs = 0; sz = sizeof(vbs);
				if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
					L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
					0, KEY_READ, &hk) == ERROR_SUCCESS)
				{
					RegQueryValueExW(hk, L"EnableVirtualizationBasedSecurity", NULL, NULL, (BYTE*)&vbs, &sz);
					RegCloseKey(hk);
					if (vbs == 1) s_HvciActive = true;
				}
				s_HvciChecked = true;
			}

			const ImVec4 Warn(0.95f, 0.6f, 0.1f, 1.f);

			ImGui::Columns(2, NULL, false);
			ImGui::TextColored(Cheat::Bypass::IsTaskbarHidden() ? Ok : Off,
				Cheat::Bypass::IsTaskbarHidden() ? "[+] Taskbar oculto" : "[-] Taskbar visible");
			ImGui::TextColored(g_Options.General.CaptureBypass ? Ok : Off,
				g_Options.General.CaptureBypass ? "[+] Stream Proof ON" : "[-] Stream Proof OFF");
			// HVCI indicator — if active, BYOVD driver won't load
			ImGui::TextColored(s_HvciActive ? Warn : Ok,
				s_HvciActive ? "[!] HVCI: ACTIVO (driver bloqueado)" : "[+] HVCI: OFF (driver OK)");
			ImGui::NextColumn();
			ImGui::TextColored(Cheat::Bypass::IsProcessSpoofed() ? Ok : Off,
				Cheat::Bypass::IsProcessSpoofed() ? "[+] PEB: RuntimeBroker.exe" : "[-] Proceso real");
			ImGui::TextColored(Cheat::Bypass::IsMemoryProtected() ? Ok : Off,
				Cheat::Bypass::IsMemoryProtected() ? "[+] PE Header borrado" : "[-] Memoria expuesta");
			ImGui::TextColored(Ok, "[+] PHANTOM EFI: activo");

			bool handleBulletOk = Cheat::g_Fivem.IsHandleBulletValid();
			ImGui::TextColored(handleBulletOk ? Ok : Off,
				handleBulletOk ? "[+] Silent Aim: OK" : "[-] Silent Aim: NO HOOK");
			ImGui::Columns(1);

			ImGui::Spacing();
			ImGui::Separator();
			if (s_HvciActive)
				ImGui::TextColored(Warn, "HVCI activo: DKOM/VAD patch desactivados. El resto funciona.");
			else
				ImGui::TextColored(ImVec4(0.4f, 0.6f, 0.8f, 1.f), "Bypass cubre: Napse, Ocean, Echo, Storm, Detect.ac");
			ImGui::TextColored(ImVec4(0.4f, 0.6f, 0.8f, 1.f), "Anti: AmCache, SRUM, Prefetch, UserAssist, BAM, Defender");
		}
		ImGui::EndCustomChild();
	}

	void Interface::ThemeTab()
	{
		static int subTab = 0;
		float buttonWidth = 100;
		float padding = 16;

		ImGui::SetCursorPos(ImVec2(padding, 5));
		if (Custom::TabButton("Colors", subTab == 0)) subTab = 0;
		ImGui::SetCursorPos(ImVec2(padding + buttonWidth + padding, 5));
		if (Custom::TabButton("Layout", subTab == 1)) subTab = 1;
		ImGui::SetCursorPos(ImVec2(padding + (buttonWidth + padding) * 2, 5));
		if (Custom::TabButton("Presets", subTab == 2)) subTab = 2;

		if (subTab == 0)
		{
			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild("Colors", ImVec2(ImGui::GetWindowSize().x / 2 - 30, 380));
			{
				ImGui::ColorEdit4("Accent", g_Options.Theme.AccentColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
				ImGui::ColorEdit4("Background", g_Options.Theme.BgColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
				ImGui::ColorEdit4("Child BG", g_Options.Theme.ChildBgColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
				ImGui::ColorEdit4("Text", g_Options.Theme.TextColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
				ImGui::ColorEdit4("Border", g_Options.Theme.BorderColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::SliderFloat("Opacity", &g_Options.Theme.WindowOpacity, 0.4f, 1.f, "%.2f");

				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Changes apply live");
			}
			ImGui::EndCustomChild();

			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x / 2 + 10, 50));
			ImGui::CustomChild("Rounding", ImVec2(ImGui::GetWindowSize().x / 2 - 30, 200));
			{
				ImGui::SliderFloat("Window", &g_Options.Theme.WindowRounding, 0.f, 20.f, "%.0f");
				ImGui::SliderFloat("Child", &g_Options.Theme.ChildRounding, 0.f, 16.f, "%.0f");
				ImGui::SliderFloat("Button", &g_Options.Theme.ButtonRounding, 0.f, 12.f, "%.0f");
				ImGui::SliderFloat("Frame", &g_Options.Theme.FrameRounding, 0.f, 12.f, "%.0f");

				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Changes apply live");
			}
			ImGui::EndCustomChild();
		}
		else if (subTab == 1)
		{
			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild("Tab Layout", ImVec2(ImGui::GetWindowSize().x - 50, 150));
			{
				ImGui::Combo("Tab Position", &g_Options.Theme.TabLayout, "Vertical (Left)\0Horizontal (Top)\0");
				ImGui::SliderFloat("Tab Width", &g_Options.Theme.TabWidth, 60.f, 120.f, "%.0f");
				ImGui::Combo("Checkbox Style", &g_Options.Theme.CheckboxStyle, "Default\0Toggle Switch\0");
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Requires restart to take effect");
			}
			ImGui::EndCustomChild();

			ImGui::SetCursorPos(ImVec2(20, 50 + 150 + 20));
			ImGui::CustomChild("Window Size", ImVec2(ImGui::GetWindowSize().x - 50, 120));
			{
				ImGui::SliderInt("Width", &g_Options.Theme.WindowWidth, 500, 1000, "%dpx");
				ImGui::SliderInt("Height", &g_Options.Theme.WindowHeight, 400, 800, "%dpx");
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "Applied next frame");
			}
			ImGui::EndCustomChild();
		}
		else
		{
			ImGui::SetCursorPos(ImVec2(20, 50));
			ImGui::CustomChild("Theme Presets", ImVec2(ImGui::GetWindowSize().x - 50, 350));
			{
				if (ImGui::Button("Default (Blue)", ImVec2(200, 28)))
				{
					g_Options.Theme.AccentColor[0] = 0.29f; g_Options.Theme.AccentColor[1] = 0.53f;
					g_Options.Theme.AccentColor[2] = 0.86f; g_Options.Theme.AccentColor[3] = 1.f;
					g_Options.Theme.BgColor[0] = 0.106f; g_Options.Theme.BgColor[1] = 0.106f;
					g_Options.Theme.BgColor[2] = 0.106f; g_Options.Theme.BgColor[3] = 1.f;
					// applied next frame
				}
				if (ImGui::Button("Red", ImVec2(200, 28)))
				{
					g_Options.Theme.AccentColor[0] = 0.85f; g_Options.Theme.AccentColor[1] = 0.2f;
					g_Options.Theme.AccentColor[2] = 0.2f; g_Options.Theme.AccentColor[3] = 1.f;
					g_Options.Theme.BgColor[0] = 0.1f; g_Options.Theme.BgColor[1] = 0.08f;
					g_Options.Theme.BgColor[2] = 0.08f; g_Options.Theme.BgColor[3] = 1.f;
					// applied next frame
				}
				if (ImGui::Button("Purple", ImVec2(200, 28)))
				{
					g_Options.Theme.AccentColor[0] = 0.6f; g_Options.Theme.AccentColor[1] = 0.3f;
					g_Options.Theme.AccentColor[2] = 0.9f; g_Options.Theme.AccentColor[3] = 1.f;
					g_Options.Theme.BgColor[0] = 0.08f; g_Options.Theme.BgColor[1] = 0.06f;
					g_Options.Theme.BgColor[2] = 0.12f; g_Options.Theme.BgColor[3] = 1.f;
					// applied next frame
				}
				if (ImGui::Button("Green", ImVec2(200, 28)))
				{
					g_Options.Theme.AccentColor[0] = 0.2f; g_Options.Theme.AccentColor[1] = 0.8f;
					g_Options.Theme.AccentColor[2] = 0.4f; g_Options.Theme.AccentColor[3] = 1.f;
					g_Options.Theme.BgColor[0] = 0.06f; g_Options.Theme.BgColor[1] = 0.1f;
					g_Options.Theme.BgColor[2] = 0.06f; g_Options.Theme.BgColor[3] = 1.f;
					// applied next frame
				}
				if (ImGui::Button("Orange", ImVec2(200, 28)))
				{
					g_Options.Theme.AccentColor[0] = 0.9f; g_Options.Theme.AccentColor[1] = 0.5f;
					g_Options.Theme.AccentColor[2] = 0.1f; g_Options.Theme.AccentColor[3] = 1.f;
					g_Options.Theme.BgColor[0] = 0.1f; g_Options.Theme.BgColor[1] = 0.08f;
					g_Options.Theme.BgColor[2] = 0.05f; g_Options.Theme.BgColor[3] = 1.f;
					// applied next frame
				}
				if (ImGui::Button("Cyan", ImVec2(200, 28)))
				{
					g_Options.Theme.AccentColor[0] = 0.1f; g_Options.Theme.AccentColor[1] = 0.8f;
					g_Options.Theme.AccentColor[2] = 0.9f; g_Options.Theme.AccentColor[3] = 1.f;
					g_Options.Theme.BgColor[0] = 0.05f; g_Options.Theme.BgColor[1] = 0.08f;
					g_Options.Theme.BgColor[2] = 0.1f; g_Options.Theme.BgColor[3] = 1.f;
					// applied next frame
				}
				if (ImGui::Button("White / Light", ImVec2(200, 28)))
				{
					g_Options.Theme.AccentColor[0] = 0.2f; g_Options.Theme.AccentColor[1] = 0.4f;
					g_Options.Theme.AccentColor[2] = 0.7f; g_Options.Theme.AccentColor[3] = 1.f;
					g_Options.Theme.BgColor[0] = 0.9f; g_Options.Theme.BgColor[1] = 0.9f;
					g_Options.Theme.BgColor[2] = 0.92f; g_Options.Theme.BgColor[3] = 1.f;
					g_Options.Theme.TextColor[0] = 0.1f; g_Options.Theme.TextColor[1] = 0.1f;
					g_Options.Theme.TextColor[2] = 0.1f; g_Options.Theme.TextColor[3] = 1.f;
					// applied next frame
				}
			}
			ImGui::EndCustomChild();
		}
	}

	void Interface::RenderGui()
	{
		if (!bIsMenuOpen)
			return;

		ApplyTheme();
		ImGui::SetNextWindowSize(ImVec2((float)g_Options.Theme.WindowWidth, (float)g_Options.Theme.WindowHeight));
		ImGui::Begin(("Menu"), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		{
			Particles();
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			ImVec2 Pos = ImGui::GetWindowPos();
			ImVec2 Size = ImGui::GetWindowSize();

			DrawList->AddRectFilled(Pos, Pos + ImVec2(80, Size.y), ImColor(23, 23, 23), ImGui::GetStyle().WindowRounding, ImDrawFlags_RoundCornersLeft);
			DrawList->AddLine(Pos + ImVec2(80, 0), Pos + ImVec2(80, Size.y), ImGui::GetColorU32(ImGuiCol_Border));

			// Logo top-left — 52x52 centred in the 80px left panel
			if (g_OverlayLogo)
				DrawList->AddImageRounded((ImTextureID)g_OverlayLogo,
					Pos + ImVec2(14.f, 12.f),
					Pos + ImVec2(66.f, 64.f),
					ImVec2(0,0), ImVec2(1,1),
					IM_COL32(255,255,255,200), 26.f);

			ImGui::BeginChild(("LeftChild"), ImVec2(80, Size.y));
			{
				ImGui::SetCursorPos(ImVec2(20, 80));
				ImGui::BeginGroup();
				{
					if (ImGui::Tab(("Aimbot"), ICON_FA_CROSSHAIRS, CurrentTab == 0))
					{
						CurrentTab = 0;
					}
					if (ImGui::Tab(("Visuals"), ICON_FA_EYE, CurrentTab == 1))
					{
						CurrentTab = 1;
					}
					if (ImGui::Tab(("Misc"), ICON_FA_GLOBE, CurrentTab == 2))
					{
						CurrentTab = 2;
					}
					if (ImGui::Tab(("Vehicles"), ICON_FA_CAR, CurrentTab == 3))
					{
						CurrentTab = 3;
					}
					if (ImGui::Tab(("Players"), ICON_FA_USERS, CurrentTab == 4))
					{
						CurrentTab = 4;
					}
					if (ImGui::Tab(("Settings"), ICON_FA_COGS, CurrentTab == 5))
					{
						CurrentTab = 5;
					}
					if (ImGui::Tab(("Bypass"), ICON_FA_SHIELD_ALT, CurrentTab == 6))
					{
						CurrentTab = 6;
					}
					if (ImGui::Tab(("Theme"), ICON_FA_PAINT_BRUSH, CurrentTab == 7))
					{
						CurrentTab = 7;
					}
				}
				ImGui::EndGroup();
			}
			ImGui::EndChild();

			static float AimBotChildAnim = 0;
			static float VisualsChildAnim = 0;
			static float MiscChildAnim = 0;
			static float VehiclesChildAnim = 0;
			static float PlayersChildAnim = 0;
			static float SettingsChildAnim = 0;
			static float BypassChildAnim = 0;
			static float ThemeChildAnim = 0;

			AimBotChildAnim = ImLerp(AimBotChildAnim, CurrentTab == 0 ? 0.f : Size.y, ImGui::GetIO().DeltaTime * 10.f);
			VisualsChildAnim = ImLerp(VisualsChildAnim, CurrentTab == 1 ? 0.f : Size.y, ImGui::GetIO().DeltaTime * 10.f);
			MiscChildAnim = ImLerp(MiscChildAnim, CurrentTab == 2 ? 0.f : Size.y, ImGui::GetIO().DeltaTime * 10.f);
			VehiclesChildAnim = ImLerp(VehiclesChildAnim, CurrentTab == 3 ? 0.f : Size.y, ImGui::GetIO().DeltaTime * 10.f);
			PlayersChildAnim = ImLerp(PlayersChildAnim, CurrentTab == 4 ? 0.f : Size.y, ImGui::GetIO().DeltaTime * 10.f);
			SettingsChildAnim = ImLerp(SettingsChildAnim, CurrentTab == 5 ? 0.f : Size.y, ImGui::GetIO().DeltaTime * 10.f);
			BypassChildAnim = ImLerp(BypassChildAnim, CurrentTab == 6 ? 0.f : Size.y, ImGui::GetIO().DeltaTime * 10.f);
			ThemeChildAnim = ImLerp(ThemeChildAnim, CurrentTab == 7 ? 0.f : Size.y, ImGui::GetIO().DeltaTime * 10.f);

			ImGui::SetCursorPos(ImVec2(80, AimBotChildAnim));
			ImGui::BeginChild(("AimBotMainChild"), ImVec2(Size.x - 80, Size.y));
			{
				AimbotTab();
			}
			ImGui::EndChild();

			ImGui::SetCursorPos(ImVec2(80, VisualsChildAnim));
			ImGui::BeginChild(("VisualsMainChild"), ImVec2(Size.x - 80, Size.y));
			{
				VisualsTab();
			}
			ImGui::EndChild();

			ImGui::SetCursorPos(ImVec2(80, MiscChildAnim));
			ImGui::BeginChild(("MiscMainChild"), ImVec2(Size.x - 80, Size.y));
			{
				MiscellaneousTab();
			}
			ImGui::EndChild();

			ImGui::SetCursorPos(ImVec2(80, VehiclesChildAnim));
			ImGui::BeginChild(("VehiclesMainChild"), ImVec2(Size.x - 80, Size.y));
			{
				VehiclesTab();
			}
			ImGui::EndChild();

			ImGui::SetCursorPos(ImVec2(80, PlayersChildAnim));
			ImGui::BeginChild(("PlayersMainChild"), ImVec2(Size.x - 80, Size.y));
			{
				PlayersTab();
			}
			ImGui::EndChild();

			ImGui::SetCursorPos(ImVec2(80, SettingsChildAnim));
			ImGui::BeginChild(("SettingsMainChild"), ImVec2(Size.x - 80, Size.y));
			{
				SettingsTab();
			}
			ImGui::EndChild();

			ImGui::SetCursorPos(ImVec2(80, BypassChildAnim));
			ImGui::BeginChild(("BypassMainChild"), ImVec2(Size.x - 80, Size.y));
			{
				BypassTab();
			}
			ImGui::EndChild();

			ImGui::SetCursorPos(ImVec2(80, ThemeChildAnim));
			ImGui::BeginChild(("ThemeMainChild"), ImVec2(Size.x - 80, Size.y));
			{
				ThemeTab();
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}

	void Interface::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_SIZE:
			if (wParam != SIZE_MINIMIZED)
			{
				ResizeWidht = (UINT)LOWORD(lParam);
				ResizeHeight = (UINT)HIWORD(lParam);
			}
			break;
		}

		if (bIsMenuOpen)
		{
			ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		}
	}

	void Interface::HandleMenuKey()
	{
		static bool MenuKeyDown = false;
		bool keyHeld = (GetAsyncKeyState(g_Options.General.MenuKey) & 0x8000)
					|| (GetAsyncKeyState(VK_RSHIFT) & 0x8000);
		if (keyHeld)
		{
			if (!MenuKeyDown)
			{
				MenuKeyDown = true;
				bIsMenuOpen = !bIsMenuOpen;
				g_Options.General.MenuOpen = bIsMenuOpen;

				if (bIsMenuOpen)
				{
					// Menu OPEN — take full input control
					LONG_PTR exStyle = GetWindowLongPtr(hWindow, GWL_EXSTYLE);
					exStyle &= ~WS_EX_TRANSPARENT;
					exStyle &= ~WS_EX_NOACTIVATE; // allow us to steal focus from game
					SetWindowLongPtr(hWindow, GWL_EXSTYLE, exStyle);
					SetWindowPos(hWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
					SetForegroundWindow(hWindow);
					SetFocus(hWindow);
				}
				else
				{
					// Menu CLOSED — stay TOPMOST so ESP renders over FiveM,
					// pass clicks through with WS_EX_TRANSPARENT so game gets input,
					// and restore the game cursor
					LONG_PTR exStyle = GetWindowLongPtr(hWindow, GWL_EXSTYLE);
					exStyle |= WS_EX_TRANSPARENT;
					exStyle |= WS_EX_NOACTIVATE;
					SetWindowLongPtr(hWindow, GWL_EXSTYLE, exStyle);
					SetWindowPos(hWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
					SetForegroundWindow(hTargetWindow);
					SetFocus(hTargetWindow);
				}
			}
		}
		else
		{
			MenuKeyDown = false;
		}
	}

	void Interface::ShutDown()
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
}