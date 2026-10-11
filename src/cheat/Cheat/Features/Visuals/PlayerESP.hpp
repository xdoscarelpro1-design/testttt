#pragma once
#include <string>
#include <d3d11.h>
#include <FrameWork/Dependencies/ImGui/imgui.h>

namespace Cheat
{
	void DrawRocketText(ImFont* pFont, const std::string& text, const ImVec2& pos, float size, ImU32 color, ImU32 borderColor, bool center, ImDrawList* pDrawList = nullptr);
	namespace ESP
	{
		void Players();
		void Radar();
		void DrawPreview();
		void SpectatorList();
		void DrawCrosshair();
		void LoadSpectatorLogo(ID3D11Device* dev);
	}
}
