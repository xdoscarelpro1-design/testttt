#pragma once
#include "imgui.h"
#include "imgui_internal.h"

namespace Custom {
    namespace CustomSlider { // Mudamos o nome para evitar conflito
        bool Float(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f");
        bool Int(const char* label, int* v, int v_min, int v_max, const char* format = "%d");
    }
}
