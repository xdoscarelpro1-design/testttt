#include "custom.hpp"
#include "imgui_tricks.hpp"
#include <map>
#include "color.h"
#include <random>
#include <FrameWork/Render/Assets.hpp>
#include <FrameWork/Render/Assets/FontAwesome.hpp>
// Set by Interface from g_Options.Theme
int g_CheckboxStyle = 0;
float g_AccentColor[4] = { 0.29f, 0.53f, 0.86f, 1.f };


using namespace ImGui;

// Assuming g_Options.LegitBot.AimBot.TargetNPC is defined somewhere in your code
struct Options {
    struct {
        struct {
            bool TargetNPC;
        } AimBot;
    } LegitBot;
} g_Options;

struct checkbox_state {
    ImVec4 gradient_color0, gradient_color1;
    ImVec4 text_color, checkmark_color, background_color;
    float rg;
    float border_alpha; // Animação da borda
};

ImVec4 text_active = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White text
ImVec4 checkbox_bg = ImVec4(20/255.0f, 20/255.0f, 20/255.0f, 1.0f); // Dark gray background when disabled
ImVec4 checkbox_gradient_start = ImVec4(112/255.0f, 112/255.0f, 112/255.0f, 1.0f); // Cinza para o preenchimento
ImVec4 checkbox_gradient_end = ImVec4(112/255.0f, 112/255.0f, 112/255.0f, 1.0f); // Cinza para o preenchimento
ImVec4 checkbox_mark = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White checkmark
ImVec4 text = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); // Light gray text
ImVec4 text_hov = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White text on hover

ImFont* tab_icons = nullptr; // Definição da variável

void InitializeFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons_config; 
    icons_config.MergeMode = true; 
    icons_config.PixelSnapH = true;
    tab_icons = io.Fonts->AddFontFromFileTTF("path/to/fontawesome-webfont.ttf", 16.0f, &icons_config, icons_ranges);
}

namespace Custom {

    bool Checkbox(const char* label, bool* v)
    {
        // Route to toggle switch if that style is selected
        if (g_CheckboxStyle == 1)
            return ToggleSwitch(label, v);

        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        PushFont(FrameWork::Assets::InterMedium);  // Agora deve funcionar

        ImGuiContext& g = *GImGui;
        ImGuiStyle& style = g.Style;
        style.FrameRounding = 5.0f; // Ajuste para cantos arredondados

        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, NULL, true);

        // Diminuindo o tamanho do checkbox
        const float square_sz = GetFrameHeight() * 1.0f; // Reduzido de 1.2f para 0.9f
        const ImVec2 pos = window->DC.CursorPos;
        
        // Calcule a largura total disponível
        float width_available = ImGui::GetContentRegionAvail().x;
        
        // Posicione o checkbox à direita (com um offset de 10 pixels da direita)
        float offset_from_right = 10.0f; // Reduzido para 10.0f para mover mais para a direita
        ImVec2 checkbox_pos = ImVec2(pos.x + width_available - square_sz - offset_from_right, pos.y);
        
        // Ajuste o retângulo total
        const ImRect total_bb(pos, ImVec2(pos.x + width_available - offset_from_right, pos.y + square_sz));
        
        ItemSize(total_bb, style.FramePadding.y);
        if (!ItemAdd(total_bb, id))
        {
            IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | (*v ? ImGuiItemStatusFlags_Checked : 0));
            return false;
        }

        bool hovered, held;
        bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
        if (IsItemClicked())
        {
            *v = !(*v);
            MarkItemEdited(id);
        }

        static std::map<ImGuiID, checkbox_state> anim;
        auto it_anim = anim.find(id);

        if (it_anim == anim.end())
        {
            anim.insert({ id, checkbox_state() });
            it_anim = anim.find(id);
        }

        // Animação da borda
        it_anim->second.border_alpha = ImLerp(
            it_anim->second.border_alpha,
            hovered ? 1.0f : 0.0f,
            g.IO.DeltaTime * 10.0f  // Velocidade da animação
        );

        it_anim->second.gradient_color0 = ImLerp(
            it_anim->second.gradient_color0, 
            *v ? checkbox_gradient_start : ImVec4(20/255.0f, 20/255.0f, 20/255.0f, 1.0f), 
            g.IO.DeltaTime * 6.f
        );
        it_anim->second.gradient_color1 = ImLerp(
            it_anim->second.gradient_color1, 
            *v ? checkbox_gradient_end : ImVec4(20/255.0f, 20/255.0f, 20/255.0f, 1.0f), 
            g.IO.DeltaTime * 6.f
        );
        it_anim->second.checkmark_color = ImLerp(it_anim->second.checkmark_color, *v ? checkbox_mark : (hovered ? ColorConvertU32ToFloat4(ImColor(0, 0, 0, 0)) : ColorConvertU32ToFloat4(ImColor(0, 0, 0, 0))), g.IO.DeltaTime * 9.f);
        it_anim->second.rg = ImLerp(it_anim->second.rg, *v ? 0.f : 10.f, g.IO.DeltaTime * 5.f);

        it_anim->second.text_color = ImLerp(it_anim->second.text_color, *v ? text_active : hovered ? text_hov : text, g.IO.DeltaTime * 6.f);

        // Ajuste o retângulo do checkbox
        const ImRect check_bb(checkbox_pos, checkbox_pos + ImVec2(square_sz, square_sz));
        
        RenderNavHighlight(total_bb, id);

        // Renderiza o fundo do checkbox
        RenderFrame(check_bb.Min, check_bb.Max, GetColorU32(checkbox_bg), true, style.FrameRounding);

        // Adiciona a borda com animação
        if (it_anim->second.border_alpha > 0.0f)
        {
            window->DrawList->AddRect(
                check_bb.Min, 
                check_bb.Max, 
                IM_COL32(112, 112, 112, (int)(255 * it_anim->second.border_alpha)), // Cor da borda com alpha animado
                style.FrameRounding,
                0,
                1.0f
            );
        }

        // Modifique a parte onde renderiza o retângulo preenchido
        const int vtx_idx_1 = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        window->DrawList->AddRectFilled(
            check_bb.Min + ImVec2(it_anim->second.rg, it_anim->second.rg), 
            check_bb.Max - ImVec2(it_anim->second.rg, it_anim->second.rg), 
            GetColorU32(it_anim->second.gradient_color0), 
            style.FrameRounding
        );

        ImGui::ShadeVertsLinearColorGradientKeepAlpha(
            ImGui::GetWindowDrawList(), 
            vtx_idx_1, 
            ImGui::GetWindowDrawList()->VtxBuffer.Size, 
            check_bb.Min, 
            check_bb.Max, 
            GetColorU32(it_anim->second.gradient_color1), 
            GetColorU32(it_anim->second.gradient_color0)
        );

        // Replace the RenderCheckMark with a circle
        const float pad = 3.0f; // Aumentado para criar mais espaço
        const float circle_radius = (square_sz - pad * 4.5f) / 2.0f; // Aumentado o multiplicador para diminuir o círculo
        const ImVec2 circle_center = check_bb.Min + ImVec2(square_sz/2, square_sz/2 + it_anim->second.rg);
        window->DrawList->AddCircleFilled(
            circle_center,
            circle_radius,
            GetColorU32(it_anim->second.checkmark_color)
        );

        // Posicione o texto à esquerda
        ImVec2 label_pos = ImVec2(pos.x, pos.y + style.FramePadding.y);
        window->DrawList->AddText(label_pos, GetColorU32(it_anim->second.text_color), label);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | (*v ? ImGuiItemStatusFlags_Checked : 0));
        
        PopFont();
        return pressed;
    }

    void draw_circle(ImVec2 pos, int radius, ImU32 color)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        unsigned char* color_ptr = (unsigned char*)&color;
        draw_list->AddCircleFilled(pos, radius - 1, IM_COL32(color_ptr[0], color_ptr[1], color_ptr[2], color_ptr[3]));
    }

    void draw_line(ImVec2 pos1, ImVec2 pos2, ImU32 color, int radius)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        float distance = std::sqrt(
            std::pow(pos2.x - pos1.x, 2) + // std:: pow
            std::pow(pos2.y - pos1.y, 2)    // std:: pow
        );
        float alpha;
        if (distance <= 20.0f) {
            alpha = 255.0f;
        }
        else {
            alpha = (1.0f - ((distance - 20.0f) / 25.0f)) * 255.0f;
        }

        int r = (color & 0xFF0000) >> 16; // Extract red component
        int g = (color & 0x00FF00) >> 8;  // Extract green component
        int b = (color & 0x0000FF);       // Extract blue component

        unsigned char* color_ptr = (unsigned char*)&color;

        draw_list->AddLine(pos1, pos2, IM_COL32(color_ptr[0], color_ptr[1], color_ptr[2], static_cast<int>(alpha)), 1.0f);
        if (distance >= 40.0f) {
            draw_list->AddCircleFilled(pos1, radius - 0.96f, IM_COL32(color_ptr[0], color_ptr[1], color_ptr[2], alpha * 200.0f));
            draw_list->AddCircleFilled(pos2, radius - 0.96f, IM_COL32(color_ptr[0], color_ptr[1], color_ptr[2], alpha * 200.0f));
        }
        else if (distance <= 20.0f) {
            draw_list->AddCircleFilled(pos1, radius, IM_COL32(color_ptr[0], color_ptr[1], color_ptr[2], alpha * 200.0f));
            draw_list->AddCircleFilled(pos2, radius, IM_COL32(color_ptr[0], color_ptr[1], color_ptr[2], alpha * 200.0f));
        }
        else {
            float radius_factor = 1.0f - ((distance - 20.0f) / 20.0f);
            float offset_factor = 1.0f - radius_factor;
            float offset = (radius - radius * radius_factor) * offset_factor;
            draw_list->AddCircleFilled(pos1, radius - offset, IM_COL32(color_ptr[0], color_ptr[1], color_ptr[2], alpha * 200.0f));
            draw_list->AddCircleFilled(pos2, radius - offset, IM_COL32(color_ptr[0], color_ptr[1], color_ptr[2], alpha * 200.0f));
        }
    }

    bool TabButton(const char* label, bool selected)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

        const float button_height = 35.0f;  
        const float button_width = 110.0f; 
        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = ImVec2(button_width, button_height);

        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        // Animações
        static std::map<ImGuiID, float> hover_animation;
        static std::map<ImGuiID, float> select_animation;
        static std::map<ImGuiID, float> press_animation;

        if (hover_animation.find(id) == hover_animation.end())
            hover_animation[id] = 0.0f;
        if (select_animation.find(id) == select_animation.end())
            select_animation[id] = 0.0f;
        if (press_animation.find(id) == press_animation.end())
            press_animation[id] = 0.0f;

        const float hover_speed = 6.0f;
        const float select_speed = 5.0f;
        const float press_speed = 12.0f;

        hover_animation[id] = ImLerp(hover_animation[id], hovered ? 1.0f : 0.0f, g.IO.DeltaTime * hover_speed);
        select_animation[id] = ImLerp(select_animation[id], selected ? 1.0f : 0.0f, g.IO.DeltaTime * select_speed);
        press_animation[id] = ImLerp(press_animation[id], held ? 1.0f : 0.0f, g.IO.DeltaTime * press_speed);

        // Cores
        ImVec4 base_color = ImVec4(0.027f, 0.027f, 0.027f, 0.0f);
        ImVec4 hover_color = ImVec4(0.09f, 0.09f, 0.09f, 1.0f);
        ImVec4 selected_color = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);

        ImVec4 current_bg = ImLerp(
            ImLerp(base_color, hover_color, hover_animation[id]),
            selected_color,
            select_animation[id]
        );

        // Cores do texto
        ImVec4 text_base = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        ImVec4 text_hover = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        ImVec4 text_selected = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);

        ImVec4 current_text = ImLerp(
            ImLerp(text_base, text_hover, hover_animation[id]),
            text_selected,
            select_animation[id]
        );

        // Desenha o fundo do botão
        window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(current_bg), 6.0f);

        // Posiciona e desenha o texto
        float text_width = label_size.x;
        float text_height = label_size.y;
        ImVec2 text_pos(
            bb.Min.x + (size.x - text_width) * 0.5f,
            bb.Min.y + (size.y - text_height) * 0.5f
        );

        // Sombra do texto quando selecionado
        if (select_animation[id] > 0.3f)
        {
            window->DrawList->AddText(
                ImVec2(text_pos.x + 1, text_pos.y + 1),
                IM_COL32(0, 0, 0, (int)(50 * select_animation[id])),
                label
            );
        }

        // Texto principal
        window->DrawList->AddText(text_pos, ImGui::GetColorU32(current_text), label);

        return pressed;
    }

    bool TabButton2(const char* icon, const char* label, bool selected)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
        const ImVec2 icon_size = ImGui::CalcTextSize(icon, NULL, true);

        const float button_height = 35.0f;  
        const float button_width = 40.0f; // Reduzido para ficar mais compacto
        ImVec2 pos = window->DC.CursorPos;
        ImVec2 size = ImVec2(button_width, button_height);

        const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
        ImGui::ItemSize(bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        static std::map<ImGuiID, float> hover_animation;
        static std::map<ImGuiID, float> select_animation;

        if (hover_animation.find(id) == hover_animation.end())
            hover_animation[id] = 0.0f;
        if (select_animation.find(id) == select_animation.end())
            select_animation[id] = 0.0f;

        // Animações
        hover_animation[id] = ImLerp(hover_animation[id], hovered ? 1.0f : 0.0f, g.IO.DeltaTime * 6.0f);
        select_animation[id] = ImLerp(select_animation[id], selected ? 1.0f : 0.0f, g.IO.DeltaTime * 6.0f);

        // Cores
        ImVec4 base_color = ImVec4(0.07f, 0.07f, 0.07f, 1.0f);      // Cor base escura
        ImVec4 hover_color = ImVec4(0.11f, 0.11f, 0.11f, 1.0f);     // Cor hover um pouco mais clara
        ImVec4 selected_color = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);  // Cor selecionada ainda mais clara

        // Interpola as cores baseado nas animações
        ImVec4 current_bg = ImLerp(
            ImLerp(base_color, hover_color, hover_animation[id]),
            selected_color,
            select_animation[id]
        );

        // Desenha o fundo do botão
        window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(current_bg), 0.0f);

        // Calcula a posição do ícone (centralizado)
        ImVec2 icon_pos = ImVec2(
            bb.Min.x + (size.x - icon_size.x) * 0.5f,
            bb.Min.y + (size.y - icon_size.y) * 0.5f
        );

        // Desenha o ícone
        ImVec4 icon_color = ImLerp(
            ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
            select_animation[id]
        );

        window->DrawList->AddText(tab_icons, 16.0f, icon_pos, ImGui::GetColorU32(icon_color), icon);

        // Se selecionado, adiciona uma linha indicadora embaixo
        if (select_animation[id] > 0.0f)
        {
            const float line_thickness = 2.0f;
            window->DrawList->AddRectFilled(
                ImVec2(bb.Min.x, bb.Max.y - line_thickness),
                bb.Max,
                IM_COL32(112, 112, 112, (int)(select_animation[id] * 255))
            );
        }

        return pressed;
    }

    namespace Slider {
        bool Float(const char* label, float* v, float v_min, float v_max, const char* format)
        {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window->SkipItems)
                return false;

            ImGuiContext& g = *GImGui;
            const ImGuiStyle& style = g.Style;
            const ImGuiID id = window->GetID(label);
            const float w = ImGui::CalcItemWidth();

            const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
            const ImRect frame_bb(
                window->DC.CursorPos,
                window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f)
            );
            const ImRect total_bb(
                frame_bb.Min,
                frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f)
            );

            ImGui::ItemSize(total_bb, style.FramePadding.y);
            if (!ImGui::ItemAdd(total_bb, id, &frame_bb))
                return false;

            const bool hovered = ImGui::IsItemHovered();
            const bool clicked = hovered && ImGui::IsMouseClicked(0);
            if (clicked)
            {
                ImGui::SetActiveID(id, window);
                ImGui::SetFocusID(id, window);
                ImGui::FocusWindow(window);
            }

            const float grab_size = 16.0f;
            const float slider_effective_w = w - grab_size;
            const float slider_h = 6.0f; // Increased height for better visibility
            const float grab_t = (*v - v_min) / (v_max - v_min);
            float grab_x = grab_t * slider_effective_w;

            if (g.ActiveId == id)
            {
                if (g.IO.MouseDown[0])
                {
                    const float mouse_x_pos = g.IO.MousePos.x - frame_bb.Min.x - grab_size * 0.5f;
                    float new_t = mouse_x_pos / slider_effective_w;
                    new_t = ImClamp(new_t, 0.0f, 1.0f);
                    float new_v = v_min + (v_max - v_min) * new_t;
                    if (*v != new_v)
                    {
                        *v = new_v;
                        return true;
                    }
                }
                else
                {
                    ImGui::ClearActiveID();
                }
            }

            const float center_y = frame_bb.Min.y + frame_bb.GetHeight() * 0.5f;
            const ImVec2 slider_min = ImVec2(frame_bb.Min.x + grab_size * 0.5f, center_y - slider_h * 0.5f);
            const ImVec2 slider_max = ImVec2(frame_bb.Max.x - grab_size * 0.5f, center_y + slider_h * 0.5f);

            // Background
            window->DrawList->AddRectFilled(
                slider_min,
                slider_max,
                ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f)), // Darker background
                slider_h * 0.5f
            );

            // Filled portion
            window->DrawList->AddRectFilled(
                slider_min,
                ImVec2(slider_min.x + grab_x, slider_max.y),
                ImGui::GetColorU32(ImVec4(0.6f, 0.6f, 0.9f, 1.0f)), // Custom color for filled portion
                slider_h * 0.5f
            );

            // Grab circle
            const float grab_radius = grab_size * 0.5f;
            const ImVec2 grab_pos(slider_min.x + grab_x, center_y);
            window->DrawList->AddCircleFilled(
                grab_pos,
                grab_radius,
                ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 1.0f, 1.0f)) // Lighter color for grab
            );

            // Display value and label
            char value_buf[64];
            const char* value_buf_end = value_buf + ImFormatString(value_buf, IM_ARRAYSIZE(value_buf), format, *v);
            ImGui::RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.5f));

            if (label_size.x > 0.0f)
                ImGui::RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

            return false;
        }

        bool Int(const char* label, int* v, int v_min, int v_max, const char* format)
        {
            float v_float = static_cast<float>(*v);
            bool changed = Float(label, &v_float, static_cast<float>(v_min), static_cast<float>(v_max), format);
            *v = static_cast<int>(v_float);
            return changed;
        }
    }
}

bool Custom::ToggleSwitch(const char* label, bool* v)
{
    using namespace ImGui;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    const float height = GetFrameHeight() * 0.8f;
    const float width = height * 1.8f;
    const float radius = height * 0.45f;
    const ImVec2 pos = window->DC.CursorPos;

    float totalWidth = GetContentRegionAvail().x;
    ImVec2 togglePos(pos.x + totalWidth - width - 10.f, pos.y + 2.f);

    const ImRect total_bb(pos, ImVec2(pos.x + totalWidth - 10.f, pos.y + height + 4.f));
    ItemSize(total_bb, g.Style.FramePadding.y);
    if (!ItemAdd(total_bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed)
    {
        *v = !(*v);
        MarkItemEdited(id);
    }

    // Animation
    static std::map<ImGuiID, float> animMap;
    float& anim = animMap[id];
    float target = *v ? 1.f : 0.f;
    anim += (target - anim) * g.IO.DeltaTime * 12.f;

    ImDrawList* dl = window->DrawList;

    // Track background
    ImVec4 accent(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], g_AccentColor[3]);
    ImU32 bgOff = ImColor(40, 40, 45, 255);
    ImU32 bgOn = ImColor((int)(accent.x * 255), (int)(accent.y * 255), (int)(accent.z * 255), 255);

    float r = height * 0.5f;
    ImVec2 tMin = togglePos;
    ImVec2 tMax(togglePos.x + width, togglePos.y + height);

    // Interpolate background color
    ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(ImLerp(
        ImGui::ColorConvertU32ToFloat4(bgOff),
        ImGui::ColorConvertU32ToFloat4(bgOn),
        anim));

    dl->AddRectFilled(tMin, tMax, bgColor, r);

    // Border
    if (hovered)
        dl->AddRect(tMin, tMax, ImColor(255, 255, 255, 40), r, 0, 1.f);

    // Circle knob
    float knobX = togglePos.x + radius + anim * (width - radius * 2.f);
    float knobY = togglePos.y + height * 0.5f;
    dl->AddCircleFilled(ImVec2(knobX, knobY), radius - 2.f, ImColor(255, 255, 255, 255));

    // Label text (left side)
    ImVec2 textPos(pos.x, pos.y + (height - label_size.y) * 0.5f + 2.f);
    dl->AddText(textPos, ImColor(220, 220, 220, 255), label);

    return pressed;
}


