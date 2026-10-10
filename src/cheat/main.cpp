#include <Windows.h>
#include <gdiplus.h>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>

#include "PhantomComm.hpp"
#include "Cheat/Cheat.hpp"
#include "Cheat/Features/Misc/Bypass.hpp"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")

using namespace Gdiplus;

static HWND       g_LoaderWnd   = NULL;
static bool       g_Running     = true;
static bool       g_PhantomOk   = false;
static bool       g_Loading     = false;

struct Particle {
    float x, y, vx, vy, size, alpha;
};

static Particle g_Particles[80];
static int      g_ParticleCount = 80;

static void InitParticles(int w, int h)
{
    srand((unsigned)time(NULL));
    for (int i = 0; i < g_ParticleCount; i++)
    {
        g_Particles[i].x     = (float)(rand() % w);
        g_Particles[i].y     = (float)(rand() % h);
        g_Particles[i].vx    = ((float)(rand() % 200) - 100.f) / 100.f;
        g_Particles[i].vy    = (float)(rand() % 100 + 50) / 80.f;
        g_Particles[i].size  = (float)(rand() % 3 + 2);
        g_Particles[i].alpha = (float)(rand() % 60 + 20);
    }
}

static void UpdateParticles(int w, int h)
{
    for (int i = 0; i < g_ParticleCount; i++)
    {
        g_Particles[i].x += g_Particles[i].vx;
        g_Particles[i].y += g_Particles[i].vy;
        if (g_Particles[i].y > h) {
            g_Particles[i].y = -5.f;
            g_Particles[i].x = (float)(rand() % w);
        }
        if (g_Particles[i].x < 0) g_Particles[i].x = (float)w;
        if (g_Particles[i].x > w) g_Particles[i].x = 0;
    }
}

static void PaintLoader(HWND hwnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    SelectObject(memDC, memBmp);

    Graphics gfx(memDC);
    gfx.SetSmoothingMode(SmoothingModeHighQuality);
    gfx.SetTextRenderingHint(TextRenderingHintAntiAlias);

    SolidBrush bgBrush(Color(255, 18, 18, 24));
    gfx.FillRectangle(&bgBrush, 0, 0, w, h);

    UpdateParticles(w, h);
    for (int i = 0; i < g_ParticleCount; i++)
    {
        SolidBrush pb(Color((BYTE)g_Particles[i].alpha, 100, 100, 120));
        gfx.FillEllipse(&pb,
            g_Particles[i].x, g_Particles[i].y,
            g_Particles[i].size, g_Particles[i].size);
    }

    Pen border(Color(255, 75, 135, 220), 2.f);
    gfx.DrawRectangle(&border, 1, 1, w - 2, h - 2);

    FontFamily ff(L"Segoe UI");
    Font titleFont(&ff, 28, FontStyleBold, UnitPixel);
    Font subtitleFont(&ff, 14, FontStyleRegular, UnitPixel);
    Font btnFont(&ff, 16, FontStyleBold, UnitPixel);
    Font statusFont(&ff, 12, FontStyleRegular, UnitPixel);

    SolidBrush white(Color(255, 240, 240, 245));
    SolidBrush gray(Color(255, 140, 140, 155));
    SolidBrush accent(Color(255, 75, 135, 220));

    StringFormat center;
    center.SetAlignment(StringAlignmentCenter);

    RectF titleRect(0, 40, (float)w, 40);
    gfx.DrawString(L"P H A N T O M", -1, &titleFont, titleRect, &center, &accent);

    RectF subRect(0, 85, (float)w, 25);
    gfx.DrawString(L"EFI Bootkit  \x2022  FiveM External", -1, &subtitleFont, subRect, &center, &gray);

    SolidBrush statusColor(g_PhantomOk ?
        Color(255, 80, 200, 80) : Color(255, 200, 80, 80));
    RectF statusRect(0, 120, (float)w, 20);
    gfx.DrawString(
        g_PhantomOk ? L"[+] PHANTOM driver connected" : L"[-] PHANTOM driver not detected",
        -1, &statusFont, statusRect, &center, &statusColor);

    if (!g_Loading)
    {
        int btnW = 200, btnH = 42;
        int btnX = (w - btnW) / 2;

        SolidBrush btnBg(Color(255, 75, 135, 220));
        Pen btnPen(Color(255, 100, 160, 240), 1.f);
        RectF loadBtn((float)btnX, 170, (float)btnW, (float)btnH);
        gfx.FillRectangle(&btnBg, loadBtn);
        gfx.DrawRectangle(&btnPen, loadBtn);
        SolidBrush btnText(Color(255, 255, 255, 255));
        RectF loadTxt((float)btnX, 178, (float)btnW, (float)btnH);
        gfx.DrawString(L"LOAD CHEAT", -1, &btnFont, loadTxt, &center, &btnText);

        SolidBrush nukeBg(Color(255, 180, 50, 50));
        Pen nukePen(Color(255, 200, 70, 70), 1.f);
        RectF nukeBtn((float)btnX, 230, (float)btnW, (float)btnH);
        gfx.FillRectangle(&nukeBg, nukeBtn);
        gfx.DrawRectangle(&nukePen, nukeBtn);
        SolidBrush nukeTxt(Color(255, 255, 255, 255));
        RectF nukeRect((float)btnX, 238, (float)btnW, (float)btnH);
        gfx.DrawString(L"BYPASS & NUKE", -1, &btnFont, nukeRect, &center, &nukeTxt);
    }
    else
    {
        SolidBrush loadTxt(Color(255, 75, 135, 220));
        RectF loadRect(0, 190, (float)w, 30);
        gfx.DrawString(L"Loading...", -1, &btnFont, loadRect, &center, &loadTxt);
    }

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

LRESULT CALLBACK LoaderWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintLoader(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        if (g_Loading) break;
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right;
        int mx = LOWORD(lParam), my = HIWORD(lParam);

        int btnW = 200, btnH = 42;
        int btnX = (w - btnW) / 2;

        if (mx >= btnX && mx <= btnX + btnW)
        {
            if (my >= 170 && my <= 170 + btnH)
            {
                g_Loading = true;
                InvalidateRect(hwnd, NULL, FALSE);
                std::thread([]() {
                    Cheat::Initialize();
                    g_Running = false;
                }).detach();
            }
            else if (my >= 230 && my <= 230 + btnH)
            {
                Cheat::Bypass::ActivateAndNuke();
                g_Running = false;
            }
        }
        break;
    }
    case WM_TIMER:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_DESTROY:
        g_Running = false;
        PostQuitMessage(0);
        return 0;
    case WM_NCHITTEST:
    {
        LRESULT hit = DefWindowProc(hwnd, msg, wParam, lParam);
        if (hit == HTCLIENT) return HTCAPTION;
        return hit;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    Cheat::Bypass::CacheRealPath();

    GdiplusStartupInput gsi;
    ULONG_PTR gdiToken;
    GdiplusStartup(&gdiToken, &gsi, NULL);

    g_PhantomOk = PhantomComm::Initialize();

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = LoaderWndProc;
    wc.hInstance      = hInstance;
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName  = L"PhantomLoader";
    wc.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);

    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 420, winH = 320;

    g_LoaderWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"PhantomLoader", L"PHANTOM",
        WS_POPUP | WS_VISIBLE,
        (scrW - winW) / 2, (scrH - winH) / 2, winW, winH,
        NULL, NULL, hInstance, NULL);

    InitParticles(winW, winH);
    SetTimer(g_LoaderWnd, 1, 33, NULL);

    MSG msg;
    while (g_Running && GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    KillTimer(g_LoaderWnd, 1);
    DestroyWindow(g_LoaderWnd);
    UnregisterClassW(L"PhantomLoader", hInstance);

    GdiplusShutdown(gdiToken);
    return 0;
}
