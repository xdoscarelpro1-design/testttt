/*
 * PHANTOM Cheat Core — Internal Injection Build
 *
 * We're already inside FiveM's address space (kernel-injected DLL).
 * No cross-process handles, no PID lookups. Overlay::Setup(0) signals
 * internal mode — the overlay finds grcWindow by walking the current
 * process's window list.
 */

#include "Cheat.hpp"
#include "Features/Misc/Protection.hpp"
#include "FivemSDK/Fivem.hpp"

#include <FrameWork/FrameWork.hpp>
#include <thread>
#include <chrono>

namespace Cheat
{
	void Initialize()
	{
		g_Options.General.CaptureBypass = true;

		// --- 1. Overlay: create immediately (PID 0 = internal, we ARE the process) ---
		FrameWork::Overlay::Setup(0);
		FrameWork::Overlay::Initialize();

		if (!FrameWork::Overlay::IsInitialized())
			return;

		FrameWork::Overlay::dxInitialize();

		// --- 2. Interface ---
		FrameWork::Interface Interface(
			FrameWork::Overlay::GetOverlayWindow(),
			FrameWork::Overlay::GetTargetWindow(),
			FrameWork::Overlay::dxGetDevice(),
			FrameWork::Overlay::dxGetDeviceContext());
		Interface.UpdateStyle();

		ESP::LoadSpectatorLogo(FrameWork::Overlay::dxGetDevice());

		HWND hOverlay = FrameWork::Overlay::GetOverlayWindow();

		SafeCall(SetWindowLongW)(hOverlay, GWL_EXSTYLE,
			WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED);
		SafeCall(SetWindowPos)(hOverlay, HWND_TOPMOST,
			0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
		SafeCall(ShowWindow)(hOverlay, SW_SHOW);
		SafeCall(UpdateWindow)(hOverlay);
		SafeCall(SetForegroundWindow)(hOverlay);

		FrameWork::Overlay::SetupWindowProcHook(
			std::bind(&FrameWork::Interface::WindowProc, &Interface,
				std::placeholders::_1, std::placeholders::_2,
				std::placeholders::_3, std::placeholders::_4));

		// --- 3. Wait for FiveM SDK init (render UI while waiting) ---
		auto lastInitAttempt = std::chrono::steady_clock::now();

		while (!g_Fivem.IsInitialized())
		{
			auto now = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastInitAttempt).count() >= 500)
			{
				lastInitAttempt = now;
				g_Fivem.Intialize();
			}

			if (!g_Fivem.IsInitialized())
			{
				MSG msg;
				while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}

				ImGui_ImplDX11_NewFrame();
				ImGui_ImplWin32_NewFrame();
				ImGui::NewFrame();
				{
					Interface.RenderGui();
				}
				ImGui::EndFrame();
				ImGui::Render();
				FrameWork::Overlay::dxRefresh();
				ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
				FrameWork::Overlay::dxGetSwapChain()->Present(0, 0U);

				std::this_thread::sleep_for(std::chrono::milliseconds(7));
			}
		}

		// --- 4. Re-acquire game window now that FiveM's grcWindow exists ---
		{
			HWND hGameWin = nullptr;
			for (int tries = 0; tries < 50 && !hGameWin; tries++)
			{
				FrameWork::Overlay::Setup(0);
				hGameWin = FrameWork::Overlay::GetTargetWindow();
				if (!hGameWin)
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			FrameWork::Overlay::UpdateWindowPos();
		}

		SafeCall(SetWindowPos)(hOverlay, HWND_TOPMOST,
			0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
		SafeCall(ShowWindow)(hOverlay, SW_SHOW);
		SafeCall(SetForegroundWindow)(hOverlay);

		// --- 5. Stealth ---
		Bypass::HideFromTaskbar(hOverlay);

		if (g_Options.General.CaptureBypass)
			SafeCall(SetWindowDisplayAffinity)(hOverlay, 0x11);

		// --- 6. Feature threads ---
		std::thread([&]() {
			while (!g_Options.General.ShutDown)
			{
				Exploits::ToggleCarLock();
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		}).detach();

		std::thread([&]() {
			while (!g_Options.General.ShutDown)
			{
				Exploits::RunThread();
				std::this_thread::sleep_for(std::chrono::microseconds(10));
			}
		}).detach();

		std::thread([&]() { AimBot::RunThread(); }).detach();
		std::thread([&]() { SilentAim::RunThread(); }).detach();
		std::thread([&]() { TriggerBot::RunThread(); }).detach();

		// --- 7. Main render loop (144fps target) ---
		auto TargetTP = std::chrono::steady_clock::now();
		MSG Message;
		ZeroMemory(&Message, sizeof(Message));

		while (PeekMessage(&Message, NULL, 0, 0, PM_REMOVE)) {}

		while (!g_Options.General.ShutDown)
		{
			TargetTP += std::chrono::microseconds(std::chrono::seconds(1)) / int(144.f);
			std::this_thread::sleep_until(TargetTP);

			if (PeekMessage(&Message, NULL, 0, 0, PM_REMOVE))
			{
				if (Message.message == WM_QUIT) break;
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			}

			g_Fivem.UpdateEntities();
			g_Fivem.UpdateVehicles();

			ImGui::GetIO().MouseDrawCursor = Interface.GetMenuOpen();

			if (Interface.ResizeHeight != 0 || Interface.ResizeWidht != 0)
			{
				FrameWork::Overlay::dxCleanupRenderTarget();
				FrameWork::Overlay::dxGetSwapChain()->ResizeBuffers(
					0, Interface.ResizeWidht, Interface.ResizeHeight,
					DXGI_FORMAT_UNKNOWN, 0);
				Interface.ResizeHeight = Interface.ResizeWidht = 0;
				FrameWork::Overlay::dxCreateRenderTarget();
			}

			Interface.HandleMenuKey();

			static auto lastWindowSync = std::chrono::steady_clock::now();
			{
				auto nowSync = std::chrono::steady_clock::now();
				if (std::chrono::duration_cast<std::chrono::seconds>(nowSync - lastWindowSync).count() >= 2)
				{
					lastWindowSync = nowSync;
					FrameWork::Overlay::Setup(0);
					FrameWork::Overlay::UpdateWindowPos();
				}
			}

			static bool CaptureBypassOn = !g_Options.General.CaptureBypass;
			if (g_Options.General.CaptureBypass != CaptureBypassOn)
			{
				CaptureBypassOn = g_Options.General.CaptureBypass;
				SafeCall(SetWindowDisplayAffinity)(hOverlay, CaptureBypassOn ? 0x11 : 0);
			}

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			{
				if (g_Options.Visuals.Players.Toogle && GetAsyncKeyState(g_Options.Visuals.Players.ToggleKey) & 1)
					g_Options.Visuals.Players.Enabled = !g_Options.Visuals.Players.Enabled;

				if (g_Options.Visuals.Vehicles.Toogle && GetAsyncKeyState(g_Options.Visuals.Vehicles.ToggleKey) & 1)
					g_Options.Visuals.Vehicles.Enabled = !g_Options.Visuals.Vehicles.Enabled;

				if (g_Options.LegitBot.AimBot.ShowFov)
					ImGui::GetBackgroundDrawList()->AddCircle(
						ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
						(float)g_Options.LegitBot.AimBot.FOV,
						FrameWork::Misc::Float4ToImColor(g_Options.LegitBot.AimBot.FovColor));

				if (g_Options.LegitBot.SilentAim.ShowFOV)
					ImGui::GetBackgroundDrawList()->AddCircle(
						ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
						(float)g_Options.LegitBot.SilentAim.Fov,
						FrameWork::Misc::Float4ToImColor(g_Options.LegitBot.SilentAim.FovColor));

				if (g_Options.Visuals.Vehicles.Enabled) ESP::Vehicles();
				if (g_Options.Visuals.Players.Enabled) ESP::Players();
				ESP::Radar();
				if (g_Options.Visuals.Players.SpectatorList) ESP::SpectatorList();
				if (g_Options.Visuals.Players.CustomCrosshair) ESP::DrawCrosshair();

				Interface.RenderGui();
			}
			ImGui::EndFrame();
			ImGui::Render();

			FrameWork::Overlay::dxRefresh();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			FrameWork::Overlay::dxGetSwapChain()->Present(g_Options.General.Vsync, 0U);
		}

		Interface.ShutDown();
		ShutDown();
	}

	void ShutDown()
	{
		SilentAim::RestoreSilent();
		FrameWork::Overlay::ShutDown();
		FrameWork::Overlay::dxShutDown();
	}
}
