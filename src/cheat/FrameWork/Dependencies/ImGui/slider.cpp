#include "slider.hpp"
#include <map>

namespace Custom {
    namespace CustomSlider {
        struct SliderAnim {
            float a;
            ImVec4 b;
        };

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
            const float slider_h = 6.0f;
            const float grab_t = (*v - v_min) / (v_max - v_min);
            float grab_x = grab_t * slider_effective_w;

            bool changed = false;
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
                        changed = true;
                        ImGui::MarkItemEdited(id);
                    }
                    grab_x = new_t * slider_effective_w;
                    ImGui::SetActiveID(id, window);
                }
                else
                {
                    ImGui::ClearActiveID();
                }
            }

            ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

            const float center_y = frame_bb.Min.y + frame_bb.GetHeight() * 0.5f;
            const ImVec2 slider_min = ImVec2(frame_bb.Min.x + grab_size * 0.5f, center_y - slider_h * 0.5f);
            const ImVec2 slider_max = ImVec2(frame_bb.Max.x - grab_size * 0.5f, center_y + slider_h * 0.5f);

            window->DrawList->AddRectFilled(
                slider_min, slider_max,
                ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f)),
                slider_h * 0.5f
            );

            window->DrawList->AddRectFilled(
                slider_min,
                ImVec2(slider_min.x + grab_x, slider_max.y),
                ImGui::GetColorU32(ImVec4(0.6f, 0.6f, 0.9f, 1.0f)),
                slider_h * 0.5f
            );

            const float grab_radius = grab_size * 0.5f;
            const ImVec2 grab_pos(slider_min.x + grab_x, center_y);
            window->DrawList->AddCircleFilled(
                grab_pos, grab_radius,
                ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 1.0f, 1.0f))
            );

            char value_buf[64];
            const char* value_buf_end = value_buf + ImFormatString(value_buf, IM_ARRAYSIZE(value_buf), format, *v);
            ImGui::RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.5f));

            if (label_size.x > 0.0f)
                ImGui::RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

            return changed;
        }

        bool Int(const char* label, int* v, int v_min, int v_max, const char* format)
        {
            float fv = (float)*v;
            bool changed = Float(label, &fv, (float)v_min, (float)v_max, format);
            int rounded = (int)(fv + 0.5f);
            if (rounded != *v)
            {
                *v = rounded;
                changed = true;
            }
            return changed;
        }
    }

    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format)
    {
        return CustomSlider::Float(label, v, v_min, v_max, format);
    }

    bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format)
    {
        return CustomSlider::Int(label, v, v_min, v_max, format);
    }

    namespace Slider {
        bool Float(const char* label, float* v, float v_min, float v_max, const char* format)
        {
            return CustomSlider::Float(label, v, v_min, v_max, format);
        }

        bool Int(const char* label, int* v, int v_min, int v_max, const char* format)
        {
            return CustomSlider::Int(label, v, v_min, v_max, format);
        }
    }
}
