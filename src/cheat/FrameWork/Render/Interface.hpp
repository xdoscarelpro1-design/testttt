#pragma once

#include <FrameWork/FrameWork.hpp>
#include <d3d11.h>

namespace FrameWork
{
    class Interface
    {
    public:
        Interface() { }
        Interface(HWND Window, HWND TargetWindow, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext) { Initialize(Window, TargetWindow, Device, DeviceContext); }
        ~Interface() { ShutDown(); }

        void Initialize(HWND Window, HWND TargetWindow, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext);
        void UpdateStyle();

        void RenderGui();

        void WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        void HandleMenuKey();

        void ShutDown();

        bool GetMenuOpen() { return bIsMenuOpen; }

    private:
        HWND hWindow = nullptr;
        HWND hTargetWindow = nullptr;
        ID3D11Device* IDevice = nullptr;

        bool bIsMenuOpen = true;
        bool m_ShutDown = false;

        int ActiveTab = 0;
        int CurrentSubTab = 0;

        float SidebarWidth = 65.f;
        float TabAlpha = 1.f;
        int PrevTab = -1;

    public:
        UINT ResizeWidht = 0;
        UINT ResizeHeight = 0;
    };
}
