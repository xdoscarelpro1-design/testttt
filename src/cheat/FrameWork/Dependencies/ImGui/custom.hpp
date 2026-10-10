#pragma once
#include "imgui.h"
#include "imgui_internal.h"

extern ImFont* tab_icons;
extern int g_CheckboxStyle;
extern float g_AccentColor[4];

namespace Custom {
	bool Checkbox(const char* label, bool* v);
	bool ToggleSwitch(const char* label, bool* v);
	bool TabButton(const char* label, bool selected);
	bool TabButton2(const char* icon, const char* label, bool selected);

	// Declare as funções com suas assinaturas completas
	bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f");
	bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d");

	namespace Slider {
		bool Float(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f");
		bool Int(const char* label, int* v, int v_min, int v_max, const char* format = "%d");
	}
};
