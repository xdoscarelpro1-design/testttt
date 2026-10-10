#include "Cheat.hpp"
#include "Features/Misc/Bypass.hpp"
#include "Features/Misc/Protection.hpp"
#include "Features/Misc/Exploits.hpp"
#include "Features/LegitBot/AimBot.hpp"
#include "Features/LegitBot/TriggerBot.hpp"
#include "Features/LegitBot/SilentAim.hpp"
#include "FivemSDK/Fivem.hpp"
#include "../PhantomComm.hpp"

#include <FrameWork/FrameWork.hpp>
#include <thread>
#include <chrono>

namespace Cheat
{
    static bool s_Running = false;

    static void StealthThread()
    {
        Bypass::SpoofProcessName();
        Bypass::BlockScannerAccess();
        Bypass::UnlinkFromProcessList();
        Bypass::ProtectMemoryRegions();
    }

    static void FeatureThreads()
    {
        while (s_Running)
        {
            if (g_Options.LegitBot.TriggerBot.Enabled)
                TriggerBot::RunThread();

            if (g_Options.LegitBot.AimBot.Enabled)
                AimBot::RunThread();

            if (g_Options.Misc.Exploits.LocalPlayer.God)
                Exploits::GodMode();

            if (g_Options.Misc.Exploits.LocalPlayer.Noclip)
                Exploits::Noclip();

            std::this_thread::sleep_for(std::chrono::milliseconds(
                g_Options.General.ThreadDelay > 0 ? g_Options.General.ThreadDelay : 1));
        }
    }

    void Initialize()
    {
        s_Running = true;

        Protection::ApplyEarly();

        if (!PhantomComm::IsConnected())
        {
            if (!PhantomComm::Initialize())
            {
                MessageBoxA(NULL,
                    "PHANTOM driver not detected.\n"
                    "Boot from USB first to install the EFI payload.",
                    "PHANTOM", MB_ICONERROR);
                return;
            }
        }

        int retries = 0;
        while (!g_Fivem.IsInitialized() && retries < 30)
        {
            g_Fivem.Intialize();
            if (!g_Fivem.IsInitialized())
            {
                std::this_thread::sleep_for(std::chrono::seconds(2));
                retries++;
            }
        }

        if (!g_Fivem.IsInitialized())
        {
            MessageBoxA(NULL, "FiveM not found. Start the game first.", "PHANTOM", MB_ICONWARNING);
            return;
        }

        FrameWork::Overlay::Setup(g_Fivem.GetPid());
        FrameWork::Overlay::Initialize();

        FrameWork::Overlay::dxInitialize();

        HWND overlayWnd = FrameWork::Overlay::GetOverlayWindow();
        HWND targetWnd  = FrameWork::Overlay::GetTargetWindow();

        Bypass::HideFromTaskbar(overlayWnd);

        if (g_Options.General.CaptureBypass)
            Bypass::EnforceStreamProof(overlayWnd);

        FrameWork::Interface gui(overlayWnd, targetWnd,
                                 FrameWork::Overlay::dxGetDevice(),
                                 FrameWork::Overlay::dxGetDeviceContext());
        gui.UpdateStyle();

        FrameWork::Overlay::SetupWindowProcHook(
            [&gui](HWND h, UINT m, WPARAM w, LPARAM l) { gui.WindowProc(h, m, w, l); });

        std::thread stealth(StealthThread);
        stealth.detach();

        std::thread features(FeatureThreads);
        features.detach();

        SilentAim::Initialize();

        while (!g_Options.General.ShutDown)
        {
            FrameWork::Overlay::UpdateWindowPos();

            gui.HandleMenuKey();

            if (!g_Fivem.UpdateEntities())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            g_Fivem.UpdateVehicles();

            FrameWork::Overlay::dxRefresh();

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            PlayerESP::Render();
            VehicleESP::Render();

            gui.RenderGui();

            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            FrameWork::Overlay::dxGetSwapChain()->Present(1, 0);
        }

        s_Running = false;
    }

    void ShutDown()
    {
        g_Options.General.ShutDown = true;
        s_Running = false;

        SilentAim::RestoreSilent();

        FrameWork::Overlay::dxShutDown();
        FrameWork::Overlay::ShutDown();
    }
}
