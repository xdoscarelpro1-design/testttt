#include "Interface.hpp"
#include <Windows.h>
#include <thread>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <string>

#include <Cheat/Options.hpp>
#include <Cheat/Cheat.hpp>
#include <Cheat/Features/Misc/Bypass.hpp>
#include "Cheat/FivemSDK/Fivem.hpp"
#include "FrameWork/Dependencies/ImGui/custom.hpp"
#include "FrameWork/Dependencies/ImGui/slider.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ─── Accent color shorthand ─────────────────────────────────────────────────
#define ACCENT_R g_Options.General.AccentColor[0]
#define ACCENT_G g_Options.General.AccentColor[1]
#define ACCENT_B g_Options.General.AccentColor[2]
#define ACCENT   ImVec4(ACCENT_R, ACCENT_G, ACCENT_B, 1.f)
#define ACCENT32 ImGui::ColorConvertFloat4ToU32(ACCENT)

// ═══════════════════════════════════════════════════════════════════════════════
// IN-MEMORY CONFIG — no file I/O, forensically clean
// ═══════════════════════════════════════════════════════════════════════════════

struct MemSlot
{
    char name[64] = {};
    bool used = false;
    Cheat::Options snapshot;
};

static struct {
    MemSlot slots[5];
    int sel = 0;
    char nameBuf[64] = {};

    void Save(int i) {
        if (i < 0 || i >= 5) return;
        slots[i].snapshot = g_Options;
        memcpy(slots[i].name, nameBuf, sizeof(nameBuf));
        slots[i].used = true;
    }
    void Load(int i) {
        if (i < 0 || i >= 5 || !slots[i].used) return;
        bool mo = g_Options.General.MenuOpen;
        int mk = g_Options.General.MenuKey;
        g_Options = slots[i].snapshot;
        g_Options.General.MenuOpen = mo;
        g_Options.General.MenuKey = mk;
    }
    void Delete(int i) {
        if (i < 0 || i >= 5) return;
        slots[i] = MemSlot{};
    }
} g_Cfg;

// ═══════════════════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

static const char* g_HitboxNames[] = {
    "Head", "Neck", "Chest", "Pelvis",
    "Left Arm", "Right Arm", "Left Leg", "Right Leg"
};

static void KeyBindWithMode(const char* label, int* key, int* mode)
{
    ImGui::PushID(label);
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::Text("%s", label);
    ImGui::SameLine(w - 90.f);
    ImGui::PushItemWidth(50.f);
    ImGui::KeyBind("##k", key);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::SmallButton(*mode == 0 ? "H" : "T"))
        *mode = (*mode + 1) & 1;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(*mode ? "Toggle" : "Hold");
    ImGui::PopID();
}

static void SubTabPill(const char** names, int count, int* active)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float totalW = ImGui::GetContentRegionAvail().x;
    float pillH = 26.f;
    float pillW = totalW / (float)count;

    for (int i = 0; i < count; i++)
    {
        ImVec2 p0(pos.x + pillW * i, pos.y);
        ImVec2 p1(p0.x + pillW, pos.y + pillH);
        bool hov = ImGui::IsMouseHoveringRect(p0, p1);
        bool sel = (*active == i);

        if (sel)
            dl->AddRectFilled(p0, p1, ACCENT32, 5.f);
        else if (hov)
            dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 12), 5.f);

        ImVec2 ts = ImGui::CalcTextSize(names[i]);
        dl->AddText(
            ImVec2(p0.x + (pillW - ts.x) * .5f, p0.y + (pillH - ts.y) * .5f),
            sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 160, 170, 200),
            names[i]);

        if (hov && ImGui::IsMouseClicked(0))
            *active = i;
    }
    ImGui::Dummy(ImVec2(totalW, pillH + 4.f));
}

static bool ColorBtn(const char* label, float col[4])
{
    bool c = false;
    ImGui::PushID(label);
    if (ImGui::ColorButton(label, ImVec4(col[0], col[1], col[2], col[3]),
        ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_AlphaPreview, ImVec2(18, 18)))
        ImGui::OpenPopup("##cp");
    ImGui::SameLine();
    ImGui::Text("%s", label);
    if (ImGui::BeginPopup("##cp"))
    {
        c = ImGui::ColorPicker4("##p", col, ImGuiColorEditFlags_AlphaBar);
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return c;
}

static void DrawSilhouette(ImDrawList* dl, ImVec2 c, float s, ImU32 col, bool hb[8])
{
    auto bone = [&](bool on) { return on ? col : IM_COL32(80, 80, 80, 80); };

    dl->AddCircleFilled(ImVec2(c.x, c.y - 75 * s), 12 * s, bone(hb[0]));
    dl->AddLine(ImVec2(c.x, c.y - 63 * s), ImVec2(c.x, c.y - 55 * s), bone(hb[1]), 3 * s);
    dl->AddLine(ImVec2(c.x, c.y - 55 * s), ImVec2(c.x, c.y - 20 * s), bone(hb[2]), 4 * s);
    dl->AddLine(ImVec2(c.x, c.y - 20 * s), ImVec2(c.x, c.y - 10 * s), bone(hb[3]), 4 * s);

    dl->AddLine(ImVec2(c.x, c.y - 50 * s), ImVec2(c.x - 25 * s, c.y - 30 * s), bone(hb[4]), 3 * s);
    dl->AddLine(ImVec2(c.x - 25 * s, c.y - 30 * s), ImVec2(c.x - 30 * s, c.y - 5 * s), bone(hb[4]), 3 * s);

    dl->AddLine(ImVec2(c.x, c.y - 50 * s), ImVec2(c.x + 25 * s, c.y - 30 * s), bone(hb[5]), 3 * s);
    dl->AddLine(ImVec2(c.x + 25 * s, c.y - 30 * s), ImVec2(c.x + 30 * s, c.y - 5 * s), bone(hb[5]), 3 * s);

    dl->AddLine(ImVec2(c.x, c.y - 10 * s), ImVec2(c.x - 15 * s, c.y + 30 * s), bone(hb[6]), 3 * s);
    dl->AddLine(ImVec2(c.x - 15 * s, c.y + 30 * s), ImVec2(c.x - 18 * s, c.y + 60 * s), bone(hb[6]), 3 * s);

    dl->AddLine(ImVec2(c.x, c.y - 10 * s), ImVec2(c.x + 15 * s, c.y + 30 * s), bone(hb[7]), 3 * s);
    dl->AddLine(ImVec2(c.x + 15 * s, c.y + 30 * s), ImVec2(c.x + 18 * s, c.y + 60 * s), bone(hb[7]), 3 * s);
}

// ═══════════════════════════════════════════════════════════════════════════════
// BACKGROUND EFFECTS
// ═══════════════════════════════════════════════════════════════════════════════

static void DrawScanlines(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float t)
{
    for (float y = p0.y; y < p1.y; y += 4.f)
    {
        float alpha = 0.03f + 0.01f * sinf(y * 0.1f + t * 2.f);
        dl->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y),
            IM_COL32(255, 255, 255, (int)(alpha * 255)), 1.f);
    }
}

static void DrawHexGrid(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float /*t*/)
{
    float hexR = 30.f;
    float hexH = hexR * 1.732f;
    ImU32 col = IM_COL32((int)(ACCENT_R * 60), (int)(ACCENT_G * 60), (int)(ACCENT_B * 60), 25);

    for (float x = p0.x - hexR; x < p1.x + hexR; x += hexR * 1.5f)
    {
        int row = 0;
        for (float y = p0.y - hexH; y < p1.y + hexH; y += hexH, row++)
        {
            float cx = x + (row & 1 ? hexR * 0.75f : 0.f);
            float cy = y;
            for (int i = 0; i < 6; i++)
            {
                float a1 = (i * 60.f + 30.f) * 3.14159f / 180.f;
                float a2 = ((i + 1) * 60.f + 30.f) * 3.14159f / 180.f;
                dl->AddLine(
                    ImVec2(cx + cosf(a1) * hexR * 0.9f, cy + sinf(a1) * hexR * 0.9f),
                    ImVec2(cx + cosf(a2) * hexR * 0.9f, cy + sinf(a2) * hexR * 0.9f),
                    col, 1.f);
            }
        }
    }
}

struct NeonParticle { float x, y, vx, vy; };
static NeonParticle g_Particles[40];
static bool g_ParticlesInit = false;

static void DrawParticleWeb(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float /*t*/)
{
    float w = p1.x - p0.x, h = p1.y - p0.y;
    if (!g_ParticlesInit)
    {
        for (auto& p : g_Particles)
        {
            p.x = (float)(rand() % (int)w);
            p.y = (float)(rand() % (int)h);
            p.vx = ((rand() % 100) - 50) * 0.005f;
            p.vy = ((rand() % 100) - 50) * 0.005f;
        }
        g_ParticlesInit = true;
    }

    ImU32 dotCol = IM_COL32((int)(ACCENT_R * 255), (int)(ACCENT_G * 255), (int)(ACCENT_B * 255), 120);
    ImU32 lineCol = IM_COL32((int)(ACCENT_R * 255), (int)(ACCENT_G * 255), (int)(ACCENT_B * 255), 30);

    for (auto& p : g_Particles)
    {
        p.x += p.vx;
        p.y += p.vy;
        if (p.x < 0 || p.x > w) p.vx = -p.vx;
        if (p.y < 0 || p.y > h) p.vy = -p.vy;
        dl->AddCircleFilled(ImVec2(p0.x + p.x, p0.y + p.y), 2.f, dotCol);
    }

    for (int i = 0; i < 40; i++)
    {
        for (int j = i + 1; j < 40; j++)
        {
            float dx = g_Particles[i].x - g_Particles[j].x;
            float dy = g_Particles[i].y - g_Particles[j].y;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < 120.f)
                dl->AddLine(
                    ImVec2(p0.x + g_Particles[i].x, p0.y + g_Particles[i].y),
                    ImVec2(p0.x + g_Particles[j].x, p0.y + g_Particles[j].y),
                    lineCol, 1.f);
        }
    }
}

static float g_MatrixDrops[60];
static bool g_MatrixInit = false;

static void DrawMatrixRain(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float t)
{
    float w = p1.x - p0.x;
    int cols = (int)(w / 14.f);
    if (cols > 60) cols = 60;

    if (!g_MatrixInit)
    {
        for (int i = 0; i < 60; i++)
            g_MatrixDrops[i] = (float)(rand() % 500);
        g_MatrixInit = true;
    }

    float dt = ImGui::GetIO().DeltaTime;
    for (int c = 0; c < cols; c++)
    {
        g_MatrixDrops[c] += dt * (60.f + (c % 7) * 15.f);
        float x = p0.x + c * 14.f + 7.f;
        float baseY = fmodf(g_MatrixDrops[c], p1.y - p0.y + 200.f) + p0.y - 100.f;

        for (int r = 0; r < 8; r++)
        {
            float y = baseY - r * 16.f;
            if (y < p0.y || y > p1.y) continue;
            float alpha = 1.f - r * 0.12f;
            char ch = '0' + ((c * 7 + r * 13 + (int)(t * 3.f)) % 36);
            if (ch > '9') ch = 'A' + (ch - '9' - 1);
            char buf[2] = { ch, 0 };
            dl->AddText(ImVec2(x, y), IM_COL32(
                (int)(ACCENT_R * 200 * alpha),
                (int)(ACCENT_G * 200 * alpha),
                (int)(ACCENT_B * 200 * alpha),
                (int)(50 * alpha)), buf);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERFACE IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════════

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
        Assets::Initialize(Device);
        UpdateStyle();
    }

    void Interface::UpdateStyle()
    {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding    = 12.f;
        s.ChildRounding     = 8.f;
        s.FrameRounding     = 6.f;
        s.GrabRounding      = 4.f;
        s.PopupRounding     = 8.f;
        s.ScrollbarRounding = 4.f;
        s.TabRounding       = 6.f;
        s.WindowPadding     = ImVec2(8, 8);
        s.FramePadding      = ImVec2(6, 4);
        s.ItemSpacing       = ImVec2(8, 6);
        s.ScrollbarSize     = 10.f;
        s.GrabMinSize       = 8.f;
        s.WindowBorderSize  = 0.f;
        s.ChildBorderSize   = 1.f;
        s.FrameBorderSize   = 0.f;

        ImVec4* c = s.Colors;
        c[ImGuiCol_WindowBg]         = ImVec4(0.039f, 0.039f, 0.039f, 1.f);
        c[ImGuiCol_ChildBg]          = ImVec4(0.063f, 0.063f, 0.063f, 1.f);
        c[ImGuiCol_PopupBg]          = ImVec4(0.055f, 0.055f, 0.055f, 0.95f);
        c[ImGuiCol_Border]           = ImVec4(0.275f, 0.275f, 0.275f, 0.43f);
        c[ImGuiCol_FrameBg]          = ImVec4(0.055f, 0.055f, 0.055f, 1.f);
        c[ImGuiCol_FrameBgHovered]   = ImVec4(0.075f, 0.075f, 0.075f, 1.f);
        c[ImGuiCol_FrameBgActive]    = ImVec4(0.09f, 0.09f, 0.09f, 1.f);
        c[ImGuiCol_TitleBg]          = ImVec4(0.04f, 0.04f, 0.04f, 1.f);
        c[ImGuiCol_TitleBgActive]    = ImVec4(0.04f, 0.04f, 0.04f, 1.f);
        c[ImGuiCol_ScrollbarBg]      = ImVec4(0.02f, 0.02f, 0.02f, 0.5f);
        c[ImGuiCol_ScrollbarGrab]    = ImVec4(0.2f, 0.2f, 0.2f, 1.f);
        c[ImGuiCol_CheckMark]        = ACCENT;
        c[ImGuiCol_SliderGrab]       = ACCENT;
        c[ImGuiCol_SliderGrabActive] = ACCENT;
        c[ImGuiCol_Button]           = ImVec4(0.08f, 0.08f, 0.08f, 1.f);
        c[ImGuiCol_ButtonHovered]    = ImVec4(ACCENT_R * 0.6f, ACCENT_G * 0.6f, ACCENT_B * 0.6f, 0.4f);
        c[ImGuiCol_ButtonActive]     = ImVec4(ACCENT_R * 0.8f, ACCENT_G * 0.8f, ACCENT_B * 0.8f, 0.6f);
        c[ImGuiCol_Header]           = ImVec4(0.08f, 0.08f, 0.08f, 1.f);
        c[ImGuiCol_HeaderHovered]    = ImVec4(0.12f, 0.12f, 0.12f, 1.f);
        c[ImGuiCol_HeaderActive]     = ImVec4(0.15f, 0.15f, 0.15f, 1.f);
        c[ImGuiCol_Text]             = ImVec4(0.82f, 0.82f, 0.82f, 1.f);
        c[ImGuiCol_TextDisabled]     = ImVec4(0.43f, 0.43f, 0.51f, 1.f);
        c[ImGuiCol_Separator]        = ImVec4(0.2f, 0.2f, 0.2f, 0.5f);
    }

    // ═════════════════════════════════════════════════════════════════════════
    // RENDER GUI — the beast
    // ═════════════════════════════════════════════════════════════════════════

    void Interface::RenderGui()
    {
        if (m_ShutDown) return;
        if (!bIsMenuOpen && !g_Options.General.ShowWatermark) return;

        ImGuiIO& io = ImGui::GetIO();
        float t = (float)ImGui::GetTime();

        // ─── Rainbow accent ────────────────────────────────────────────────
        if (g_Options.General.RainbowAccent)
        {
            float h = fmodf(t * 0.15f, 1.f);
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB(h, 0.7f, 0.9f, r, g, b);
            g_Options.General.AccentColor[0] = r;
            g_Options.General.AccentColor[1] = g;
            g_Options.General.AccentColor[2] = b;
        }

        // ─── Sync UI colors from options ───────────────────────────────────
        ImGuiStyle& st = ImGui::GetStyle();
        st.Colors[ImGuiCol_WindowBg] = ImVec4(
            g_Options.General.ColorBg[0], g_Options.General.ColorBg[1],
            g_Options.General.ColorBg[2], g_Options.General.ColorBg[3]);
        st.Colors[ImGuiCol_ChildBg] = ImVec4(
            g_Options.General.ColorChildBg[0], g_Options.General.ColorChildBg[1],
            g_Options.General.ColorChildBg[2], g_Options.General.ColorChildBg[3]);
        st.Colors[ImGuiCol_FrameBg] = ImVec4(
            g_Options.General.ColorBoxBg[0], g_Options.General.ColorBoxBg[1],
            g_Options.General.ColorBoxBg[2], g_Options.General.ColorBoxBg[3]);
        st.Colors[ImGuiCol_Border] = ImVec4(
            g_Options.General.ColorOutline[0], g_Options.General.ColorOutline[1],
            g_Options.General.ColorOutline[2], g_Options.General.ColorOutline[3]);
        st.Colors[ImGuiCol_Text] = ImVec4(
            g_Options.General.ColorText[0], g_Options.General.ColorText[1],
            g_Options.General.ColorText[2], g_Options.General.ColorText[3]);
        st.Colors[ImGuiCol_TextDisabled] = ImVec4(
            g_Options.General.ColorTextDim[0], g_Options.General.ColorTextDim[1],
            g_Options.General.ColorTextDim[2], g_Options.General.ColorTextDim[3]);

        st.Colors[ImGuiCol_CheckMark]        = ACCENT;
        st.Colors[ImGuiCol_SliderGrab]       = ACCENT;
        st.Colors[ImGuiCol_SliderGrabActive] = ACCENT;

        // ─── Menu style ────────────────────────────────────────────────────
        switch (g_Options.General.MenuStyle)
        {
        case 0:
            st.WindowRounding = 12.f;
            st.ChildRounding  = 8.f;
            st.ChildBorderSize = 1.f;
            break;
        case 1:
            st.WindowRounding = 0.f;
            st.ChildRounding  = 0.f;
            st.ChildBorderSize = 0.f;
            break;
        case 2:
            st.WindowRounding  = 16.f;
            st.ChildRounding   = 10.f;
            st.ChildBorderSize = 1.f;
            st.Colors[ImGuiCol_WindowBg].w = 0.85f;
            st.Colors[ImGuiCol_ChildBg].w  = 0.7f;
            break;
        }

        st.WindowBorderSize = g_Options.General.BorderSize;
        if (g_Options.General.MenuScale > 0.5f && g_Options.General.MenuScale < 2.f)
            io.FontGlobalScale = g_Options.General.MenuScale;

        // ─── Watermark (always visible) ────────────────────────────────────
        if (g_Options.General.ShowWatermark)
        {
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            if (Assets::InterBold)
                fg->AddText(Assets::InterBold, 16.f, ImVec2(12, 8), ACCENT32, "PHANTOM");
            else
                fg->AddText(ImVec2(12, 8), ACCENT32, "PHANTOM");
        }

        if (!bIsMenuOpen) return;

        // ═══════════════════════════════════════════════════════════════════
        // MAIN WINDOW
        // ═══════════════════════════════════════════════════════════════════

        ImGui::SetNextWindowSize(ImVec2(920, 580), ImGuiCond_Once);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##phantom", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wPos  = ImGui::GetWindowPos();
        ImVec2 wSize = ImGui::GetWindowSize();
        ImVec2 wEnd  = ImVec2(wPos.x + wSize.x, wPos.y + wSize.y);

        // ─── Background effects ────────────────────────────────────────────
        dl->PushClipRect(wPos, wEnd);
        switch (g_Options.General.BackgroundStyle)
        {
        case 1: DrawScanlines(dl, wPos, wEnd, t);   break;
        case 2: DrawHexGrid(dl, wPos, wEnd, t);     break;
        case 3: DrawParticleWeb(dl, wPos, wEnd, t);  break;
        case 4: DrawMatrixRain(dl, wPos, wEnd, t);   break;
        }
        dl->PopClipRect();

        // ─── Mouse radial glow ─────────────────────────────────────────────
        if (io.MousePos.x >= wPos.x && io.MousePos.x <= wEnd.x &&
            io.MousePos.y >= wPos.y && io.MousePos.y <= wEnd.y)
        {
            dl->AddCircleFilled(io.MousePos, 80.f,
                IM_COL32((int)(ACCENT_R * 255), (int)(ACCENT_G * 255), (int)(ACCENT_B * 255), 8));
        }

        // ─── Perimeter glow ────────────────────────────────────────────────
        float pulse = 0.5f + 0.5f * sinf(t * 2.f);
        int glowA = (int)(12.f + 6.f * pulse);
        for (int i = 0; i < 3; i++)
        {
            dl->AddRect(
                ImVec2(wPos.x - (float)i, wPos.y - (float)i),
                ImVec2(wEnd.x + (float)i, wEnd.y + (float)i),
                IM_COL32((int)(ACCENT_R * 255), (int)(ACCENT_G * 255), (int)(ACCENT_B * 255), glowA - i * 3),
                st.WindowRounding + (float)i, 0, 1.f);
        }

        // ═══════════════════════════════════════════════════════════════════
        // SIDEBAR — animated: 65px collapsed -> expanded on hover
        // ═══════════════════════════════════════════════════════════════════

        ImVec2 sidebarP0 = wPos;
        ImVec2 sidebarP1 = ImVec2(wPos.x + SidebarWidth, wEnd.y);
        bool sidebarHov = ImGui::IsMouseHoveringRect(sidebarP0, sidebarP1);
        float targetW = sidebarHov ? 200.f : 65.f;
        SidebarWidth += (targetW - SidebarWidth) * io.DeltaTime * 12.f;

        dl->AddRectFilled(sidebarP0, sidebarP1, IM_COL32(8, 8, 8, 240),
            st.WindowRounding, ImDrawFlags_RoundCornersLeft);
        dl->AddLine(
            ImVec2(sidebarP1.x, sidebarP0.y + 10),
            ImVec2(sidebarP1.x, sidebarP1.y - 10),
            IM_COL32((int)(ACCENT_R * 180), (int)(ACCENT_G * 180), (int)(ACCENT_B * 180), 40));

        // Logo
        ImGui::SetCursorPos(ImVec2(14, 12));
        if (Assets::Logo)
            ImGui::Image((ImTextureID)Assets::Logo, ImVec2(36, 36));
        else
        {
            if (Assets::InterBold) ImGui::PushFont(Assets::InterBold);
            ImGui::TextColored(ACCENT, "P");
            if (Assets::InterBold) ImGui::PopFont();
        }

        // Tab buttons
        struct TabDef { const char* icon; const char* label; };
        TabDef tabs[] = {
            { ICON_FA_CROSSHAIRS, "Aim"      },
            { ICON_FA_EYE,        "Visuals"  },
            { ICON_FA_USERS,      "Players"  },
            { ICON_FA_GLOBE,      "World"    },
            { ICON_FA_SAVE,       "Configs"  },
            { ICON_FA_COG,        "Settings" },
            { ICON_FA_PALETTE,    "Menu"     },
            { ICON_FA_SHIELD_ALT, "Bypass"   },
        };

        float btnY = 60.f;
        for (int i = 0; i < 8; i++)
        {
            ImGui::SetCursorPos(ImVec2(4, btnY));
            ImGui::PushID(i);

            bool sel = (ActiveTab == i);
            float btnW = SidebarWidth - 8.f;
            float btnH = 36.f;
            ImVec2 btnPos = ImGui::GetCursorScreenPos();
            ImVec2 btnEnd = ImVec2(btnPos.x + btnW, btnPos.y + btnH);
            bool btnHov = ImGui::IsMouseHoveringRect(btnPos, btnEnd);

            if (sel)
                dl->AddRectFilled(btnPos, btnEnd,
                    IM_COL32((int)(ACCENT_R * 255), (int)(ACCENT_G * 255), (int)(ACCENT_B * 255), 35), 6.f);
            else if (btnHov)
                dl->AddRectFilled(btnPos, btnEnd, IM_COL32(255, 255, 255, 8), 6.f);

            if (sel)
                dl->AddRectFilled(
                    ImVec2(btnPos.x, btnPos.y + 6),
                    ImVec2(btnPos.x + 3, btnEnd.y - 6),
                    ACCENT32, 2.f);

            ImFont* iconFont = Assets::FontAwesomeSolid18
                ? Assets::FontAwesomeSolid18
                : Assets::FontAwesomeSolid;

            if (iconFont) ImGui::PushFont(iconFont);
            ImVec2 iconSize = ImGui::CalcTextSize(tabs[i].icon);
            float iconX = btnPos.x + (57.f - iconSize.x) * 0.5f;
            float iconY = btnPos.y + (btnH - iconSize.y) * 0.5f;
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(iconX, iconY),
                sel ? ACCENT32 : IM_COL32(160, 160, 170, 200),
                tabs[i].icon);
            if (iconFont) ImGui::PopFont();

            if (SidebarWidth > 100.f)
            {
                ImVec2 labelSize = ImGui::CalcTextSize(tabs[i].label);
                float labelX = btnPos.x + 58.f;
                float labelY = btnPos.y + (btnH - labelSize.y) * 0.5f;
                float labelAlpha = (SidebarWidth - 100.f) / 100.f;
                if (labelAlpha > 1.f) labelAlpha = 1.f;
                dl->AddText(ImVec2(labelX, labelY),
                    IM_COL32(200, 200, 200, (int)(labelAlpha * 255)),
                    tabs[i].label);
            }

            ImGui::InvisibleButton("##tab", ImVec2(btnW, btnH));
            if (ImGui::IsItemClicked())
            {
                if (ActiveTab != i) { PrevTab = ActiveTab; TabAlpha = 0.f; }
                ActiveTab = i;
                CurrentSubTab = 0;
            }

            ImGui::PopID();
            btnY += 42.f;
        }

        // ═══════════════════════════════════════════════════════════════════
        // CONTENT AREA
        // ═══════════════════════════════════════════════════════════════════

        float contentX = SidebarWidth + 8.f;
        float contentW = wSize.x - contentX - 8.f;
        float contentH = wSize.y - 16.f;

        ImGui::SetCursorPos(ImVec2(contentX, 8.f));
        ImGui::BeginGroup();

        TabAlpha += io.DeltaTime * 5.f;
        if (TabAlpha > 1.f) TabAlpha = 1.f;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, TabAlpha);

        // Search bar
        if (Assets::FontAwesomeSolid) ImGui::PushFont(Assets::FontAwesomeSolid);
        ImGui::Text(ICON_FA_SEARCH);
        if (Assets::FontAwesomeSolid) ImGui::PopFont();
        ImGui::SameLine();
        ImGui::PushItemWidth(contentW - 40.f);
        ImGui::InputText("##search", g_UISearchBuf, sizeof(g_UISearchBuf));
        ImGui::PopItemWidth();
        ImGui::Spacing();

        float halfW = (contentW - 12.f) * 0.5f;
        float panelH = contentH - 80.f;

        // ═══════════════════════════════════════════════════════════════════
        // TAB 0: AIM
        // ═══════════════════════════════════════════════════════════════════
        if (ActiveTab == 0)
        {
            const char* aimSubs[] = { "Aimbot", "Silent Aim", "Trigger Bot" };
            SubTabPill(aimSubs, 3, &CurrentSubTab);

            if (CurrentSubTab == 0)
            {
                ImGui::BeginChild("##aim_l", ImVec2(halfW, panelH), true);
                ImGui::Text("Aimbot");
                ImGui::Separator();

                Custom::Checkbox("Enabled", &g_Options.LegitBot.AimBot.Enabled);
                Custom::Checkbox("Show FOV Circle", &g_Options.LegitBot.AimBot.ShowFov);
                Custom::Checkbox("Target NPC", &g_Options.LegitBot.AimBot.TargetNPC);
                Custom::Checkbox("Visible Check", &g_Options.LegitBot.AimBot.VisibleCheck);
                Custom::Checkbox("Prediction", &g_Options.LegitBot.AimBot.EnablePrediction);
                Custom::Checkbox("Target Combat Roll", &g_Options.LegitBot.AimBot.TargetCombatRoll);

                KeyBindWithMode("Key Bind", &g_Options.LegitBot.AimBot.KeyBind,
                    &g_Options.LegitBot.AimBot.KeyBindState);

                Custom::SliderInt("FOV", &g_Options.LegitBot.AimBot.FOV, 1, 800);
                Custom::SliderInt("Max Distance", &g_Options.LegitBot.AimBot.MaxDistance, 1, 1000);

                const char* styles[] = { "Custom", "Exponential", "Linear", "Humanized", "Snap" };
                ImGui::Combo("Style", &g_Options.LegitBot.AimBot.AimbotStyle, styles, 5);

                if (g_Options.LegitBot.AimBot.AimbotStyle == 0)
                {
                    Custom::SliderInt("Smooth H", &g_Options.LegitBot.AimBot.SmoothHorizontal, 0, 100);
                    Custom::SliderInt("Smooth V", &g_Options.LegitBot.AimBot.SmoothVertical, 0, 100);
                }

                const char* fovPri[] = { "Distance", "Crosshair" };
                ImGui::Combo("FOV Priority", &g_Options.LegitBot.AimBot.ClosestFov, fovPri, 2);

                ImGui::EndChild();
                ImGui::SameLine();

                ImGui::BeginChild("##aim_r", ImVec2(halfW, panelH), true);
                ImGui::Text("Hitboxes");
                ImGui::Separator();

                for (int i = 0; i < 8; i++)
                    Custom::Checkbox(g_HitboxNames[i], &g_Options.LegitBot.AimBot.HitBoxes[i]);

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Colors");
                ColorBtn("FOV Circle", g_Options.LegitBot.AimBot.FovColor);
                ColorBtn("Aimbot Path", g_Options.LegitBot.AimBot.AimbotPreviewColor);

                ImGui::Spacing();
                Custom::Checkbox("Show Hitbox Preview", &g_Options.LegitBot.AimBot.ShowHitboxPreview);
                Custom::Checkbox("Show Aimbot Preview", &g_Options.LegitBot.AimBot.ShowAimbotPreview);

                ImGui::EndChild();
            }
            else if (CurrentSubTab == 1)
            {
                ImGui::BeginChild("##sa_l", ImVec2(halfW, panelH), true);
                ImGui::Text("Silent Aim");
                ImGui::Separator();

                Custom::Checkbox("Enabled", &g_Options.LegitBot.SilentAim.Enabled);
                Custom::Checkbox("Show FOV", &g_Options.LegitBot.SilentAim.ShowFOV);
                Custom::Checkbox("Target NPC", &g_Options.LegitBot.SilentAim.ShotNPC);
                Custom::Checkbox("Visible Check", &g_Options.LegitBot.SilentAim.VisibleCheck);
                Custom::Checkbox("Magic Bullet", &g_Options.LegitBot.SilentAim.MagicBullet);
                Custom::Checkbox("Humanize", &g_Options.LegitBot.SilentAim.Humanize);

                KeyBindWithMode("Key Bind", &g_Options.LegitBot.SilentAim.KeyBind,
                    &g_Options.LegitBot.SilentAim.KeyBindState);

                Custom::SliderInt("FOV", &g_Options.LegitBot.SilentAim.Fov, 1, 800);
                Custom::SliderInt("Max Distance", &g_Options.LegitBot.SilentAim.MaxDistance, 1, 1000);
                Custom::SliderInt("Miss Chance", &g_Options.LegitBot.SilentAim.MissChance, 0, 100);

                const char* fovPri[] = { "Distance", "Crosshair" };
                ImGui::Combo("FOV Priority", &g_Options.LegitBot.SilentAim.ClosestFov, fovPri, 2);

                ImGui::EndChild();
                ImGui::SameLine();

                ImGui::BeginChild("##sa_r", ImVec2(halfW, panelH), true);
                ImGui::Text("Hitboxes");
                ImGui::Separator();

                for (int i = 0; i < 8; i++)
                    Custom::Checkbox(g_HitboxNames[i], &g_Options.LegitBot.SilentAim.HitBoxes[i]);

                ImGui::Spacing();
                ImGui::Separator();
                ColorBtn("FOV Circle", g_Options.LegitBot.SilentAim.FovColor);

                ImGui::Spacing();
                Custom::Checkbox("Show Hitbox Preview", &g_Options.LegitBot.SilentAim.ShowHitboxPreview);

                ImGui::EndChild();
            }
            else if (CurrentSubTab == 2)
            {
                ImGui::BeginChild("##tb_l", ImVec2(halfW, panelH), true);
                ImGui::Text("Trigger Bot");
                ImGui::Separator();

                Custom::Checkbox("Enabled", &g_Options.LegitBot.Trigger.Enabled);
                Custom::Checkbox("Target NPC", &g_Options.LegitBot.Trigger.ShotNPC);
                Custom::Checkbox("Visible Check", &g_Options.LegitBot.Trigger.VisibleCheck);
                Custom::Checkbox("Require Aiming", &g_Options.LegitBot.Trigger.RequireAiming);

                KeyBindWithMode("Key Bind", &g_Options.LegitBot.Trigger.KeyBind,
                    &g_Options.LegitBot.Trigger.KeyBindState);

                Custom::SliderInt("FOV", &g_Options.LegitBot.Trigger.FOV, 1, 800);
                Custom::SliderInt("Max Distance", &g_Options.LegitBot.Trigger.MaxDistance, 1, 1000);

                ImGui::EndChild();
                ImGui::SameLine();

                ImGui::BeginChild("##tb_r", ImVec2(halfW, panelH), true);
                ImGui::Text("Timing");
                ImGui::Separator();

                Custom::SliderInt("Reaction Time", &g_Options.LegitBot.Trigger.ReactionTime, 0, 500);
                Custom::SliderInt("Reaction Jitter", &g_Options.LegitBot.Trigger.ReactionJitter, 0, 200);
                Custom::SliderInt("Hit Chance", &g_Options.LegitBot.Trigger.HitChance, 1, 100);
                Custom::SliderInt("Refire Delay", &g_Options.LegitBot.Trigger.RefireDelay, 20, 1000);

                ImGui::EndChild();
            }
        }

        // ═══════════════════════════════════════════════════════════════════
        // TAB 1: VISUALS
        // ═══════════════════════════════════════════════════════════════════
        else if (ActiveTab == 1)
        {
            const char* visSubs[] = { "Player ESP", "Vehicle ESP", "Colors" };
            SubTabPill(visSubs, 3, &CurrentSubTab);

            if (CurrentSubTab == 0)
            {
                ImGui::BeginChild("##pesp_l", ImVec2(halfW, panelH), true);
                ImGui::Text("Player ESP");
                ImGui::Separator();

                Custom::Checkbox("Enabled", &g_Options.Visuals.Players.Enabled);
                Custom::Checkbox("Box", &g_Options.Visuals.Players.EnableBox);
                Custom::Checkbox("Skeleton", &g_Options.Visuals.Players.Skeleton);
                Custom::Checkbox("Health Bar", &g_Options.Visuals.Players.HealthBar);
                Custom::Checkbox("Armor Bar", &g_Options.Visuals.Players.ArmorBar);
                Custom::Checkbox("Head Circle", &g_Options.Visuals.Players.EnableHeadBol);
                Custom::Checkbox("Name", &g_Options.Visuals.Players.Name);
                Custom::Checkbox("Distance", &g_Options.Visuals.Players.EnableDistance);
                Custom::Checkbox("Weapon", &g_Options.Visuals.Players.WeaponName);
                Custom::Checkbox("Visible Check", &g_Options.Visuals.Players.VisibleCheck);
                Custom::Checkbox("Show NPC", &g_Options.Visuals.Players.ShowNPC);
                Custom::Checkbox("Show Local Player", &g_Options.Visuals.Players.ShowLocalPlayer);
                Custom::Checkbox("Exclude Dead", &g_Options.Visuals.Players.ExcludeDeads);
                Custom::Checkbox("Spectator List", &g_Options.Visuals.Players.SpectatorList);

                ImGui::EndChild();
                ImGui::SameLine();

                ImGui::BeginChild("##pesp_r", ImVec2(halfW, panelH), true);
                ImGui::Text("Options");
                ImGui::Separator();

                const char* boxTypes[] = { "2D", "Corner", "3D" };
                ImGui::Combo("Box Type", &g_Options.Visuals.Players.BoxType, boxTypes, 3);

                Custom::Checkbox("Custom Crosshair", &g_Options.Visuals.Players.CustomCrosshair);
                Custom::Checkbox("Aiming Warning", &g_Options.Visuals.Players.EnableAimingWarning);
                Custom::Checkbox("Observer Warning", &g_Options.Visuals.Players.EnableObserverWarning);
                Custom::Checkbox("Radar", &g_Options.Visuals.Players.RadarEnabled);
                Custom::Checkbox("Rocket Style", &g_Options.Visuals.Players.RocketStyle);

                KeyBindWithMode("Toggle Key",
                    &g_Options.Visuals.Players.ToggleKey,
                    &g_Options.Visuals.Players.ToggleKeyState);

                Custom::SliderInt("Render Distance",
                    &g_Options.Visuals.Players.RenderDistance, 1, 2000);

                const char* barPos[] = { "Off", "Left", "Right", "Top", "Bottom" };
                ImGui::Combo("Health Bar Pos", &g_Options.Visuals.Players.HealthBarPos, barPos, 5);
                ImGui::Combo("Armor Bar Pos", &g_Options.Visuals.Players.ArmorBarPos, barPos, 5);

                const char* textPos[] = { "Off", "Top", "Bottom", "Left", "Right" };
                ImGui::Combo("Name Pos", &g_Options.Visuals.Players.NamePos, textPos, 5);
                ImGui::Combo("Dist Pos", &g_Options.Visuals.Players.DistPos, textPos, 5);
                ImGui::Combo("Weapon Pos", &g_Options.Visuals.Players.WeaponPos, textPos, 5);

                Custom::Checkbox("Show Preview", &g_Options.Visuals.Players.ShowPreview);

                ImGui::EndChild();
            }
            else if (CurrentSubTab == 1)
            {
                ImGui::BeginChild("##vesp", ImVec2(contentW, panelH), true);
                ImGui::Text("Vehicle ESP");
                ImGui::Separator();

                Custom::Checkbox("Enabled", &g_Options.Visuals.Vehicles.Enabled);
                Custom::Checkbox("Distance", &g_Options.Visuals.Vehicles.Distance);
                Custom::Checkbox("Name", &g_Options.Visuals.Vehicles.Name);
                Custom::Checkbox("Lock State", &g_Options.Visuals.Vehicles.ShowLockState);
                Custom::Checkbox("Marker", &g_Options.Visuals.Vehicles.Marker);

                KeyBindWithMode("Toggle Key",
                    &g_Options.Visuals.Vehicles.ToggleKey,
                    &g_Options.Visuals.Vehicles.ToggleKeyState);

                ImGui::Spacing();
                ImGui::Text("Colors");
                ImGui::Separator();
                ColorBtn("Marker", g_Options.Visuals.Vehicles.MarkerColor);
                ColorBtn("Distance", g_Options.Visuals.Vehicles.DistanceColor);
                ColorBtn("Name", g_Options.Visuals.Vehicles.NameColor);

                ImGui::EndChild();
            }
            else if (CurrentSubTab == 2)
            {
                ImGui::BeginChild("##cols_l", ImVec2(halfW, panelH), true);
                ImGui::Text("Box Colors");
                ImGui::Separator();

                ColorBtn("Box (Visible)", g_Options.Visuals.Players.NormalBoxColor);
                ColorBtn("Box (Invisible)", g_Options.Visuals.Players.InVisibleBoxColor);
                ColorBtn("Snap Lines", g_Options.Visuals.Players.SnapLinesColor);
                ColorBtn("Friend", g_Options.Visuals.Players.FriendColor);
                ColorBtn("Head Circle", g_Options.Visuals.Players.HeadCircleColor);
                ColorBtn("Crosshair", g_Options.Visuals.Players.CrosshairColor);
                ColorBtn("Crosshair Dot", g_Options.Visuals.Players.CrosshairDotColor);

                ImGui::EndChild();
                ImGui::SameLine();

                ImGui::BeginChild("##cols_r", ImVec2(halfW, panelH), true);
                ImGui::Text("Text & Skeleton");
                ImGui::Separator();

                ColorBtn("Skeleton (Visible)", g_Options.Visuals.Players.NormalSkeletonColor);
                ColorBtn("Skeleton (Invisible)", g_Options.Visuals.Players.InvisibleSkeletonColor);
                ColorBtn("Skeleton (Friend)", g_Options.Visuals.Players.FriendSkeletonColor);
                ColorBtn("Health Bar", g_Options.Visuals.Players.HealthBarColor);
                ColorBtn("Armor Bar", g_Options.Visuals.Players.ArmorBarColor);
                ColorBtn("Name", g_Options.Visuals.Players.NameColor);
                ColorBtn("Distance", g_Options.Visuals.Players.DistanceColor);
                ColorBtn("Weapon Name", g_Options.Visuals.Players.WeaponNameColor);

                ImGui::EndChild();
            }
        }

        // ═══════════════════════════════════════════════════════════════════
        // TAB 2: PLAYER & VEHICLE LIST
        // ═══════════════════════════════════════════════════════════════════
        else if (ActiveTab == 2)
        {
            const char* listSubs[] = { "Players", "Vehicles" };
            SubTabPill(listSubs, 2, &CurrentSubTab);

            if (CurrentSubTab == 0)
            {
                ImGui::BeginChild("##plist", ImVec2(contentW, panelH), true);
                ImGui::Text("Player List");
                ImGui::Separator();

                auto entities = g_Fivem.GetEntitiyList();
                for (auto& ent : entities)
                {
                    if (ent.StaticInfo.bIsLocalPlayer) continue;

                    ImGui::PushID(ent.StaticInfo.NetId);
                    bool isFriend = ent.StaticInfo.IsFriend;

                    ImGui::TextColored(
                        isFriend ? ImVec4(0, 1, 1, 1) : ImVec4(1, 1, 1, 1),
                        "%s", ent.StaticInfo.Name.c_str());

                    ImGui::SameLine();
                    ImGui::TextDisabled("[%dm]",
                        (int)ent.Cordinates.DistTo(g_Fivem.GetLocalPlayerInfo().WorldPos));

                    ImGui::SameLine();
                    if (ImGui::SmallButton(isFriend ? "Unfriend" : "Friend"))
                    {
                        if (isFriend)
                            g_Fivem.FriendList.erase(ent.StaticInfo.iIndex);
                        else
                            g_Fivem.FriendList[ent.StaticInfo.iIndex] = ent.StaticInfo;
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("TP"))
                    {
                        g_Fivem.TeleportToObject(
                            (uintptr_t)ent.StaticInfo.Ped, 0, 0,
                            ent.Cordinates, ent.Cordinates, true);
                    }

                    ImGui::PopID();
                }

                ImGui::EndChild();
            }
            else
            {
                ImGui::BeginChild("##vlist", ImVec2(contentW, panelH), true);
                ImGui::Text("Vehicle List");
                ImGui::Separator();

                auto vehicles = g_Fivem.GetVehicleList();
                for (auto& veh : vehicles)
                {
                    ImGui::PushID(veh.iIndex);
                    ImGui::Text("%s", veh.Name.c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled("[ID: %d]", veh.iIndex);
                    ImGui::PopID();
                }

                ImGui::EndChild();
            }
        }

        // ═══════════════════════════════════════════════════════════════════
        // TAB 3: WORLD (LOCAL PLAYER + MISC)
        // ═══════════════════════════════════════════════════════════════════
        else if (ActiveTab == 3)
        {
            ImGui::BeginChild("##world_l", ImVec2(halfW, panelH), true);
            ImGui::Text("Local Player");
            ImGui::Separator();

            Custom::Checkbox("God Mode", &g_Options.Exploits.Self.GodMode);
            KeyBindWithMode("God Key",
                &g_Options.Exploits.Self.GodKey,
                &g_Options.Exploits.Self.GodKeyState);

            Custom::Checkbox("Invisible", &g_Options.Exploits.Self.Invisible);
            KeyBindWithMode("Invis Key",
                &g_Options.Exploits.Self.InvisibleKey,
                &g_Options.Exploits.Self.InvisibleKeyState);

            Custom::Checkbox("No Ragdoll", &g_Options.Exploits.Self.NoRagDoll);
            Custom::Checkbox("Remove Spread", &g_Options.Exploits.Self.RemoveSpread);
            Custom::Checkbox("Remove Recoil", &g_Options.Exploits.Self.RemoveRecoil);
            Custom::Checkbox("Immortality", &g_Options.Exploits.Self.Immortality);

            Custom::Checkbox("Super Punch", &g_Options.Exploits.Self.SuperPunch);
            if (g_Options.Exploits.Self.SuperPunch)
                Custom::SliderFloat("Punch Force",
                    &g_Options.Exploits.Self.SuperPunchForce, 1.f, 500.f);

            Custom::Checkbox("Desync", &g_Options.Exploits.Self.Desync);
            ImGui::KeyBind("Desync Key", &g_Options.Exploits.Self.DesyncKeyBind);

            Custom::SliderFloat("Weapon Size",
                &g_Options.Exploits.Self.WeaponSize, 0.1f, 10.f);

            ImGui::Spacing();
            ImGui::Text("Health");
            ImGui::Separator();

            Custom::Checkbox("Start Health", &g_Options.Exploits.Self.Start_Health);
            Custom::SliderInt("Health Amount",
                &g_Options.Exploits.Self.health_ammount, 0, 1000);
            Custom::Checkbox("Absorption", &g_Options.Exploits.Self.Absorption);
            Custom::SliderInt("Absorption %",
                &g_Options.Exploits.Self.AbsorptionPercent, 0, 100);

            ImGui::EndChild();
            ImGui::SameLine();

            ImGui::BeginChild("##world_r", ImVec2(halfW, panelH), true);
            ImGui::Text("Movement & Vehicle");
            ImGui::Separator();

            Custom::Checkbox("No Clip", &g_Options.Exploits.Self.NoClip);
            KeyBindWithMode("NoClip Key",
                &g_Options.Exploits.Self.NoClipKey,
                &g_Options.Exploits.Self.NoClipKeyState);
            Custom::SliderFloat("NoClip Speed",
                &g_Options.Exploits.Self.NoClipSpeed, 1.f, 500.f);

            const char* ncModes[] = { "Fly", "Walk" };
            ImGui::Combo("NoClip Mode", &g_Options.Exploits.Self.NoClipMode, ncModes, 2);

            Custom::Checkbox("Strafe", &g_Options.Exploits.Self.StrafeEnabled);
            KeyBindWithMode("Strafe Key",
                &g_Options.Exploits.Self.StrafeKey,
                &g_Options.Exploits.Self.StrafeKeyState);
            Custom::SliderFloat("Strafe Speed",
                &g_Options.Exploits.Self.StrafeSpeed, 0.1f, 5.f);

            Custom::Checkbox("Seatbelt", &g_Options.Exploits.Self.SeatBelt);
            KeyBindWithMode("Seatbelt Key",
                &g_Options.Exploits.Self.SeatbeltKey,
                &g_Options.Exploits.Self.SeatbeltKeyState);

            Custom::Checkbox("Vehicle Speed", &g_Options.Exploits.Self.VehicleSpeed);
            Custom::SliderFloat("Speed Multi",
                &g_Options.Exploits.Self.VehicleSpeedMultiplier, 0.5f, 10.f);
            ImGui::KeyBind("Speed Key", &g_Options.Exploits.Self.VehicleSpeedBind);

            Custom::Checkbox("Vehicle God", &g_Options.Exploits.Self.VehicleGod);
            Custom::Checkbox("Steal Vehicle", &g_Options.Exploits.Self.StealVehicle);
            Custom::Checkbox("Unlock All Vehicles", &g_Options.Exploits.Self.UnLockAllVehicles);
            Custom::Checkbox("Lock All Vehicles", &g_Options.Exploits.Self.LockAllVehicles);

            ImGui::Spacing();
            ImGui::Text("Teleport & Misc");
            ImGui::Separator();

            Custom::Checkbox("Replay TP", &g_Options.Exploits.Self.ReplayTP);
            KeyBindWithMode("Replay Key",
                &g_Options.Exploits.Self.ReplayTPBind,
                &g_Options.Exploits.Self.ReplayTPBindState);

            Custom::Checkbox("Anti HS", &g_Options.Exploits.Self.AntHS);

            Custom::Checkbox("Bubbles", &g_Options.Exploits.Self.Bubbles);
            KeyBindWithMode("Bubbles Key",
                &g_Options.Exploits.Self.BubblesKey,
                &g_Options.Exploits.Self.BubblesKeyState);

            Custom::Checkbox("Anti Bubbles", &g_Options.Exploits.Self.AntiBubbles);
            KeyBindWithMode("Anti Bub Key",
                &g_Options.Exploits.Self.AntiBubblesKey,
                &g_Options.Exploits.Self.AntiBubblesKeyState);

            ImGui::Spacing();
            ImGui::Text("Peak System");
            ImGui::Separator();
            ImGui::KeyBind("Create Peak", &g_Options.Exploits.Self.CreatePeakBind);
            ImGui::KeyBind("Return Peak", &g_Options.Exploits.Self.ReturnToPeakBind);
            if (g_Options.Exploits.Self.PeakSet)
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Peak: %.0f, %.0f, %.0f",
                    g_Options.Exploits.Self.PeakPositionX,
                    g_Options.Exploits.Self.PeakPositionY,
                    g_Options.Exploits.Self.PeakPositionZ);

            KeyBindWithMode("TP Waypoint",
                &g_Options.Exploits.Self.TeleportWaypointKey,
                &g_Options.Exploits.Self.TeleportWaypointKeyState);
            KeyBindWithMode("Suicide Key",
                &g_Options.Exploits.Self.SuicideKey,
                &g_Options.Exploits.Self.SuicideKeyState);

            ImGui::EndChild();
        }

        // ═══════════════════════════════════════════════════════════════════
        // TAB 4: CONFIGS (IN-MEMORY, 5 SLOTS)
        // ═══════════════════════════════════════════════════════════════════
        else if (ActiveTab == 4)
        {
            ImGui::BeginChild("##cfgs", ImVec2(contentW, panelH), true);
            ImGui::Text("In-Memory Configs (no files, forensically clean)");
            ImGui::Separator();

            ImGui::InputText("Config Name", g_Cfg.nameBuf, sizeof(g_Cfg.nameBuf));
            ImGui::Spacing();

            for (int i = 0; i < 5; i++)
            {
                ImGui::PushID(i);
                bool sel = (g_Cfg.sel == i);

                if (sel)
                    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        ImVec4(ACCENT_R * 0.15f, ACCENT_G * 0.15f, ACCENT_B * 0.15f, 1.f));

                ImGui::BeginChild("##slot", ImVec2(contentW - 30.f, 50.f), true);

                if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0))
                    g_Cfg.sel = i;

                ImGui::Text("Slot %d: %s", i + 1,
                    g_Cfg.slots[i].used ? g_Cfg.slots[i].name : "(empty)");

                ImGui::SameLine(contentW - 260.f);

                if (g_Cfg.slots[i].used)
                {
                    if (ImGui::SmallButton(ICON_FA_DOWNLOAD " Load"))
                        g_Cfg.Load(i);
                    ImGui::SameLine();
                    if (ImGui::SmallButton(ICON_FA_UPLOAD " Overwrite"))
                        g_Cfg.Save(i);
                    ImGui::SameLine();
                    if (ImGui::SmallButton(ICON_FA_TRASH " Delete"))
                        g_Cfg.Delete(i);
                }
                else
                {
                    if (ImGui::SmallButton(ICON_FA_SAVE " Save"))
                        g_Cfg.Save(i);
                }

                ImGui::EndChild();
                if (sel) ImGui::PopStyleColor();

                ImGui::PopID();
            }

            ImGui::EndChild();
        }

        // ═══════════════════════════════════════════════════════════════════
        // TAB 5: SETTINGS
        // ═══════════════════════════════════════════════════════════════════
        else if (ActiveTab == 5)
        {
            ImGui::BeginChild("##set_l", ImVec2(halfW, panelH), true);
            ImGui::Text("General");
            ImGui::Separator();

            Custom::Checkbox("Stream Proof", &g_Options.General.CaptureBypass);
            Custom::Checkbox("NVIDIA Bypass", &g_Options.General.NvidiaBypass);
            Custom::Checkbox("VSync", &g_Options.General.Vsync);
            Custom::Checkbox("Watermark", &g_Options.General.ShowWatermark);
            Custom::Checkbox("Show Keybinds", &g_Options.General.ShowKeybinds);
            Custom::Checkbox("Notifications", &g_Options.General.Notifications);

            ImGui::KeyBind("Menu Key", &g_Options.General.MenuKey);
            Custom::SliderInt("Thread Delay", &g_Options.General.ThreadDelay, 0, 50);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Dangerous");
            ImGui::Separator();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.f));
            if (ImGui::Button("Unload Cheat", ImVec2(-1, 30)))
                g_Options.General.ShutDown = true;
            if (ImGui::Button("Self Destruct", ImVec2(-1, 30)))
                Cheat::Bypass::SelfDestruct();
            ImGui::PopStyleColor();

            ImGui::EndChild();
            ImGui::SameLine();

            ImGui::BeginChild("##set_r", ImVec2(halfW, panelH), true);
            ImGui::Text("Offset Status");
            ImGui::Separator();

            auto status = [](const char* name, bool ok) {
                ImGui::TextColored(ok ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                    ok ? ICON_FA_CHECK : ICON_FA_TIMES);
                ImGui::SameLine();
                ImGui::Text("%s", name);
            };

            status("World",         g_Fivem.IsWorldValid());
            status("Replay",        g_Fivem.IsReplayValid());
            status("ViewPort",      g_Fivem.IsViewPortValid());
            status("Camera",        g_Fivem.IsCameraValid());
            status("Handle Bullet", g_Fivem.IsHandleBulletValid());
            status("Blip List",     g_Fivem.IsBlipListValid());
            status("Initialized",   g_Fivem.IsInitialized());

            ImGui::Spacing();
            ImGui::Text("Build: %d", g_Fivem.GetResolvedBuild());
            ImGui::Text("Game Version: %d", g_Fivem.GetGameVersion());

            ImGui::EndChild();
        }

        // ═══════════════════════════════════════════════════════════════════
        // TAB 6: MENU & COLORS
        // ═══════════════════════════════════════════════════════════════════
        else if (ActiveTab == 6)
        {
            ImGui::BeginChild("##menu_l", ImVec2(halfW, panelH), true);
            ImGui::Text("Menu Style");
            ImGui::Separator();

            const char* bgStyles[] = { "None", "Scanlines", "Hex Grid", "Particles", "Matrix Rain" };
            ImGui::Combo("Background", &g_Options.General.BackgroundStyle, bgStyles, 5);

            const char* menuStyles[] = { "Panel", "Flat", "Glass" };
            ImGui::Combo("Menu Style", &g_Options.General.MenuStyle, menuStyles, 3);

            Custom::Checkbox("Rainbow Accent", &g_Options.General.RainbowAccent);
            Custom::SliderFloat("Border Size", &g_Options.General.BorderSize, 0.f, 4.f);
            Custom::SliderFloat("Menu Scale", &g_Options.General.MenuScale, 0.6f, 1.5f);

            ImGui::Spacing();
            ImGui::Text("Theme Presets");
            ImGui::Separator();

            struct Preset { const char* n; float r, g, b; };
            Preset presets[] = {
                { "Purple", 0.58f, 0.58f, 0.72f },
                { "Red",    0.85f, 0.20f, 0.20f },
                { "Blue",   0.20f, 0.40f, 0.85f },
                { "Green",  0.20f, 0.75f, 0.30f },
                { "Orange", 0.90f, 0.55f, 0.15f },
                { "Cyan",   0.15f, 0.80f, 0.85f },
                { "Pink",   0.90f, 0.35f, 0.60f },
                { "Gold",   0.85f, 0.75f, 0.20f },
            };

            for (int i = 0; i < 8; i++)
            {
                if (i > 0) ImGui::SameLine();
                ImGui::PushID(i);
                ImVec4 pc(presets[i].r, presets[i].g, presets[i].b, 1.f);
                if (ImGui::ColorButton("##preset", pc, 0, ImVec2(24, 24)))
                {
                    g_Options.General.AccentColor[0] = presets[i].r;
                    g_Options.General.AccentColor[1] = presets[i].g;
                    g_Options.General.AccentColor[2] = presets[i].b;
                    g_Options.General.RainbowAccent = false;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", presets[i].n);
                ImGui::PopID();
            }

            ImGui::Spacing();
            ColorBtn("Accent Color", g_Options.General.AccentColor);

            ImGui::EndChild();
            ImGui::SameLine();

            ImGui::BeginChild("##menu_r", ImVec2(halfW, panelH), true);
            ImGui::Text("UI Colors");
            ImGui::Separator();

            ColorBtn("Background", g_Options.General.ColorBg);
            ColorBtn("Child BG", g_Options.General.ColorChildBg);
            ColorBtn("Box BG", g_Options.General.ColorBoxBg);
            ColorBtn("Outline", g_Options.General.ColorOutline);
            ColorBtn("Text", g_Options.General.ColorText);
            ColorBtn("Text Dim", g_Options.General.ColorTextDim);

            ImGui::EndChild();
        }

        // ═══════════════════════════════════════════════════════════════════
        // TAB 7: BYPASS
        // ═══════════════════════════════════════════════════════════════════
        else if (ActiveTab == 7)
        {
            ImGui::BeginChild("##byp_l", ImVec2(halfW, panelH), true);
            ImGui::Text("PHANTOM Bypass");
            ImGui::Separator();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.f));
            if (ImGui::Button("Nuke & Clean", ImVec2(-1, 35)))
                Cheat::Bypass::ActivateAndNuke();
            ImGui::PopStyleColor();

            ImGui::Spacing();

            if (ImGui::Button("Block Scanner Access", ImVec2(-1, 28)))
                Cheat::Bypass::BlockScannerAccess();
            if (ImGui::Button("Protect Memory", ImVec2(-1, 28)))
                Cheat::Bypass::ProtectMemoryRegions();
            if (ImGui::Button("Spoof Process Name", ImVec2(-1, 28)))
                Cheat::Bypass::SpoofProcessName();
            if (ImGui::Button("Unlink Process List", ImVec2(-1, 28)))
                Cheat::Bypass::UnlinkFromProcessList();
            if (ImGui::Button("Kill SS Tools", ImVec2(-1, 28)))
                Cheat::Bypass::KillSSTools();

            ImGui::Spacing();
            ImGui::Text("Forensic Cleaning");
            ImGui::Separator();

            if (ImGui::Button("Clean Traces", ImVec2(-1, 24)))
                Cheat::Bypass::CleanTraces();
            if (ImGui::Button("Clean UserAssist", ImVec2(-1, 24)))
                Cheat::Bypass::CleanUserAssist();
            if (ImGui::Button("Clean ShellBags", ImVec2(-1, 24)))
                Cheat::Bypass::CleanShellBags();
            if (ImGui::Button("Clean Recent Docs", ImVec2(-1, 24)))
                Cheat::Bypass::CleanRecentDocs();
            if (ImGui::Button("Clean Event Logs", ImVec2(-1, 24)))
                Cheat::Bypass::CleanEventLogs();
            if (ImGui::Button("Clean AmCache", ImVec2(-1, 24)))
                Cheat::Bypass::CleanAmCache();
            if (ImGui::Button("Clean SRUM", ImVec2(-1, 24)))
                Cheat::Bypass::CleanSRUM();
            if (ImGui::Button("Clean Browser History", ImVec2(-1, 24)))
                Cheat::Bypass::CleanBrowserHistory();
            if (ImGui::Button("Clean Recycle Bin", ImVec2(-1, 24)))
                Cheat::Bypass::CleanRecycleBin();
            if (ImGui::Button("Clean Defender History", ImVec2(-1, 24)))
                Cheat::Bypass::CleanDefenderHistory();
            if (ImGui::Button("Clean Registry Logs", ImVec2(-1, 24)))
                Cheat::Bypass::CleanRegistryTransactionLogs();
            if (ImGui::Button("Clean System Info", ImVec2(-1, 24)))
                Cheat::Bypass::CleanSystemInfo();
            if (ImGui::Button("Clean Shell Artifacts", ImVec2(-1, 24)))
                Cheat::Bypass::CleanShellArtifacts();
            if (ImGui::Button("Clean DNS Cache", ImVec2(-1, 24)))
                Cheat::Bypass::CleanDNSCache();
            if (ImGui::Button("Clean Network Traces", ImVec2(-1, 24)))
                Cheat::Bypass::CleanNetworkTraces();
            if (ImGui::Button("Clean Clipboard", ImVec2(-1, 24)))
                Cheat::Bypass::CleanClipboard();
            if (ImGui::Button("Wipe Strings", ImVec2(-1, 24)))
                Cheat::Bypass::WipeStrings();

            ImGui::EndChild();
            ImGui::SameLine();

            ImGui::BeginChild("##byp_r", ImVec2(halfW, panelH), true);
            ImGui::Text("Status");
            ImGui::Separator();

            auto bstat = [](const char* name, bool on) {
                ImGui::TextColored(
                    on ? ImVec4(0, 1, 0, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1),
                    "%s %s", on ? ICON_FA_CHECK : ICON_FA_TIMES, name);
            };

            bstat("Stream Proof",    Cheat::Bypass::IsStreamProofActive());
            bstat("Taskbar Hidden",  Cheat::Bypass::IsTaskbarHidden());
            bstat("Remote Blocked",  Cheat::Bypass::IsRemoteAccessBlocked());
            bstat("Memory Protected", Cheat::Bypass::IsMemoryProtected());
            bstat("Process Spoofed", Cheat::Bypass::IsProcessSpoofed());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Additional");
            ImGui::Separator();

            if (ImGui::Button("Hide From Taskbar", ImVec2(-1, 28)))
                Cheat::Bypass::HideFromTaskbar(hWindow);
            if (ImGui::Button("Block Remote Access", ImVec2(-1, 28)))
                Cheat::Bypass::BlockRemoteAccess();
            if (ImGui::Button("Anti Screen Capture", ImVec2(-1, 28)))
                Cheat::Bypass::AntiScreenCapture(hWindow);
            if (ImGui::Button("Disable Window Ghosting", ImVec2(-1, 28)))
                Cheat::Bypass::DisableWindowGhosting();
            if (ImGui::Button("Hide Console Window", ImVec2(-1, 28)))
                Cheat::Bypass::HideConsoleWindow();
            if (ImGui::Button("Monitor SS Tools", ImVec2(-1, 28)))
                Cheat::Bypass::MonitorSSTools();

            ImGui::EndChild();
        }

        ImGui::PopStyleVar(); // Alpha
        ImGui::EndGroup();
        ImGui::PopStyleVar(); // WindowPadding
        ImGui::End();

        // ═══════════════════════════════════════════════════════════════════
        // PREVIEW WINDOWS
        // ═══════════════════════════════════════════════════════════════════

        // ─── Hitbox Preview ────────────────────────────────────────────────
        if (g_Options.LegitBot.AimBot.ShowHitboxPreview ||
            g_Options.LegitBot.SilentAim.ShowHitboxPreview)
        {
            ImGui::SetNextWindowSize(ImVec2(180, 280), ImGuiCond_Once);
            ImGui::Begin("Hitbox Preview", nullptr,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

            ImDrawList* pdl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetWindowPos();
            ImVec2 ws = ImGui::GetWindowSize();
            ImVec2 center(wp.x + ws.x * 0.5f, wp.y + ws.y * 0.5f + 20.f);

            bool* activeHB = g_Options.LegitBot.AimBot.ShowHitboxPreview
                ? g_Options.LegitBot.AimBot.HitBoxes
                : g_Options.LegitBot.SilentAim.HitBoxes;

            DrawSilhouette(pdl, center, 1.2f, ACCENT32, activeHB);

            ImVec2 bonePos[] = {
                ImVec2(center.x, center.y - 75 * 1.2f),
                ImVec2(center.x, center.y - 59 * 1.2f),
                ImVec2(center.x, center.y - 37 * 1.2f),
                ImVec2(center.x, center.y - 15 * 1.2f),
                ImVec2(center.x - 27 * 1.2f, center.y - 17 * 1.2f),
                ImVec2(center.x + 27 * 1.2f, center.y - 17 * 1.2f),
                ImVec2(center.x - 16 * 1.2f, center.y + 45 * 1.2f),
                ImVec2(center.x + 16 * 1.2f, center.y + 45 * 1.2f),
            };

            for (int i = 0; i < 8; i++)
            {
                ImVec2 bp = bonePos[i];
                bool hov = (io.MousePos.x >= bp.x - 8 && io.MousePos.x <= bp.x + 8 &&
                            io.MousePos.y >= bp.y - 8 && io.MousePos.y <= bp.y + 8);
                if (hov)
                {
                    pdl->AddCircle(bp, 10.f, ACCENT32, 12, 2.f);
                    ImGui::SetTooltip("%s: %s", g_HitboxNames[i],
                        activeHB[i] ? "ON" : "OFF");
                    if (ImGui::IsMouseClicked(0))
                        activeHB[i] = !activeHB[i];
                }
            }

            ImGui::End();
        }

        // ─── ESP Preview ──────────────────────────────────────────────────
        if (g_Options.Visuals.Players.ShowPreview)
        {
            ImGui::SetNextWindowSize(ImVec2(200, 350), ImGuiCond_Once);
            ImGui::Begin("ESP Preview", nullptr,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

            ImDrawList* edl = ImGui::GetWindowDrawList();
            ImVec2 ep = ImGui::GetWindowPos();
            float ew = ImGui::GetWindowSize().x;
            float eh = ImGui::GetWindowSize().y;
            ImVec2 boxP0(ep.x + ew * 0.3f, ep.y + 60.f);
            ImVec2 boxP1(ep.x + ew * 0.7f, ep.y + eh - 40.f);

            float cLen = 12.f;
            ImU32 boxCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                g_Options.Visuals.Players.NormalBoxColor[0],
                g_Options.Visuals.Players.NormalBoxColor[1],
                g_Options.Visuals.Players.NormalBoxColor[2],
                g_Options.Visuals.Players.NormalBoxColor[3]));

            // Corner box
            edl->AddLine(boxP0, ImVec2(boxP0.x + cLen, boxP0.y), boxCol, 2.f);
            edl->AddLine(boxP0, ImVec2(boxP0.x, boxP0.y + cLen), boxCol, 2.f);
            edl->AddLine(ImVec2(boxP1.x, boxP0.y), ImVec2(boxP1.x - cLen, boxP0.y), boxCol, 2.f);
            edl->AddLine(ImVec2(boxP1.x, boxP0.y), ImVec2(boxP1.x, boxP0.y + cLen), boxCol, 2.f);
            edl->AddLine(ImVec2(boxP0.x, boxP1.y), ImVec2(boxP0.x + cLen, boxP1.y), boxCol, 2.f);
            edl->AddLine(ImVec2(boxP0.x, boxP1.y), ImVec2(boxP0.x, boxP1.y - cLen), boxCol, 2.f);
            edl->AddLine(boxP1, ImVec2(boxP1.x - cLen, boxP1.y), boxCol, 2.f);
            edl->AddLine(boxP1, ImVec2(boxP1.x, boxP1.y - cLen), boxCol, 2.f);

            // Health bar
            float barX = boxP0.x - 8.f;
            edl->AddRectFilled(ImVec2(barX, boxP0.y), ImVec2(barX + 3, boxP1.y),
                IM_COL32(40, 40, 40, 200));
            edl->AddRectFilled(
                ImVec2(barX, boxP0.y + (boxP1.y - boxP0.y) * 0.2f),
                ImVec2(barX + 3, boxP1.y),
                IM_COL32(0, 255, 0, 200));

            edl->AddText(ImVec2(boxP0.x, boxP0.y - 16.f),
                IM_COL32(255, 255, 255, 255), "Player");
            edl->AddText(ImVec2(boxP0.x, boxP1.y + 4.f),
                IM_COL32(200, 200, 200, 200), "50m");

            ImGui::End();
        }

        // ─── Aimbot Path Preview ──────────────────────────────────────────
        if (g_Options.LegitBot.AimBot.ShowAimbotPreview)
        {
            ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_Once);
            ImGui::Begin("Aimbot Preview", nullptr,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

            ImDrawList* adl = ImGui::GetWindowDrawList();
            ImVec2 ap = ImGui::GetWindowPos();
            float aw = ImGui::GetWindowSize().x;
            float ah = ImGui::GetWindowSize().y;
            ImVec2 center(ap.x + aw * 0.5f, ap.y + ah * 0.5f);

            adl->AddLine(ImVec2(center.x - 15, center.y),
                ImVec2(center.x + 15, center.y), ACCENT32, 1.5f);
            adl->AddLine(ImVec2(center.x, center.y - 15),
                ImVec2(center.x, center.y + 15), ACCENT32, 1.5f);

            float angle = t * 1.5f;
            float radius = 40.f + 20.f * sinf(t * 0.7f);
            ImVec2 target(center.x + cosf(angle) * radius,
                center.y + sinf(angle) * radius * 0.6f);
            adl->AddCircle(target, 6.f, IM_COL32(255, 80, 80, 200), 12, 2.f);

            int style = g_Options.LegitBot.AimBot.AimbotStyle;
            ImVec2 curr = center;
            int steps = 12;
            for (int si = 1; si <= steps; si++)
            {
                float prog = (float)si / steps;
                ImVec2 next;

                switch (style)
                {
                case 0: case 2:
                    next = ImVec2(
                        center.x + (target.x - center.x) * prog,
                        center.y + (target.y - center.y) * prog);
                    break;
                case 1:
                {
                    float ep = prog * prog;
                    next = ImVec2(
                        center.x + (target.x - center.x) * ep,
                        center.y + (target.y - center.y) * ep);
                    break;
                }
                case 3:
                {
                    float jit = sinf(si * 2.3f + t * 5.f) * 3.f;
                    next = ImVec2(
                        center.x + (target.x - center.x) * prog + jit,
                        center.y + (target.y - center.y) * prog + jit * 0.5f);
                    break;
                }
                case 4:
                    next = target;
                    break;
                default:
                    next = target;
                }

                adl->AddLine(curr, next,
                    IM_COL32((int)(ACCENT_R * 255), (int)(ACCENT_G * 255),
                        (int)(ACCENT_B * 255), 100 + (int)(155 * prog)), 1.5f);
                curr = next;
            }

            const char* styleNames[] = { "Custom", "Exponential", "Linear", "Humanized", "Snap" };
            if (style >= 0 && style < 5)
                adl->AddText(ImVec2(ap.x + 8, ap.y + ah - 22),
                    IM_COL32(200, 200, 200, 180), styleNames[style]);

            ImGui::End();
        }
    }

    // ═════════════════════════════════════════════════════════════════════════
    // HANDLE MENU KEY
    // ═════════════════════════════════════════════════════════════════════════

    void Interface::HandleMenuKey()
    {
        static bool keyWasDown = false;
        bool keyDown = (SafeCall(GetAsyncKeyState)(g_Options.General.MenuKey) & 0x8000) != 0;

        if (keyDown && !keyWasDown)
        {
            bIsMenuOpen = !bIsMenuOpen;
            g_Options.General.MenuOpen = bIsMenuOpen;

            LONG_PTR exStyle = SafeCall(GetWindowLongPtrW)(hWindow, GWL_EXSTYLE);
            if (bIsMenuOpen)
            {
                exStyle &= ~WS_EX_TRANSPARENT;
                SafeCall(SetForegroundWindow)(hWindow);
            }
            else
            {
                exStyle |= WS_EX_TRANSPARENT;
            }
            SafeCall(SetWindowLongPtrW)(hWindow, GWL_EXSTYLE, exStyle);
        }
        keyWasDown = keyDown;
    }

    // ═════════════════════════════════════════════════════════════════════════
    // WINDOW PROC
    // ═════════════════════════════════════════════════════════════════════════

    void Interface::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (uMsg == WM_SIZE)
        {
            ResizeWidht  = LOWORD(lParam);
            ResizeHeight = HIWORD(lParam);
        }
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
    }

    // ═════════════════════════════════════════════════════════════════════════
    // SHUTDOWN
    // ═════════════════════════════════════════════════════════════════════════

    void Interface::ShutDown()
    {
        if (m_ShutDown) return;
        m_ShutDown = true;

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
}
