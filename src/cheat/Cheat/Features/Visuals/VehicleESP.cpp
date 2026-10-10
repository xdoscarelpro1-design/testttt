#pragma once
#include "VehicleESP.hpp"

#include "../../FivemSDK/Fivem.hpp"
#include "../../Options.hpp"

namespace Cheat
{
	void ESP::Vehicles()
	{
		if (!g_Fivem.GetLocalPlayerInfo().Ped)
			return;

		for (VehicleInfo Current : g_Fivem.GetVehicleList())
		{
			if (g_Options.Visuals.ESP.Vehicles.IgnoreOccupiedVehicles && Current.Vehicle->GetDriver())
				continue;

			ImVec2 Position = g_Fivem.WorldToScreen(Current.Vehicle->GetCoordinate());
			if (!g_Fivem.IsOnScreen(Position))
				continue;

			float OffsetY = 0;

			if (g_Options.Visuals.ESP.Vehicles.Marker)
			{
				const float radiusOuter = 4.0f;
				const float radiusInner = 3.0f;
				const ImVec2 center(Position.x, Position.y + OffsetY);
				const ImColor colorOuter(0.f, 0.f, 0.f, g_Options.Visuals.ESP.Vehicles.Color[3]);
				const ImColor colorInner(g_Options.Visuals.ESP.Vehicles.Color[0],
					g_Options.Visuals.ESP.Vehicles.Color[1],
					g_Options.Visuals.ESP.Vehicles.Color[2],
					g_Options.Visuals.ESP.Vehicles.Color[3]);

				auto* drawList = ImGui::GetBackgroundDrawList();
				drawList->AddCircleFilled(center, radiusOuter, colorOuter);
				drawList->AddCircleFilled(center, radiusInner, colorInner);

				OffsetY += 12;
			}

			ImGui::PushFont(FrameWork::Assets::InterBold12);

			if (g_Options.Visuals.ESP.Vehicles.Name)
			{
				ImVec2 textSize = ImGui::CalcTextSize(Current.Name.c_str());
				float textPositionX = Position.x - textSize.x / 2;

				auto* drawList = ImGui::GetBackgroundDrawList();
				ImColor shadowColor(0.f, 0.f, 0.f, g_Options.Visuals.ESP.Vehicles.Color[3]);
				ImColor textColor(1.f, 1.f, 1.f, g_Options.Visuals.ESP.Vehicles.Color[3]);

				drawList->AddText(ImVec2(textPositionX + 1, Position.y + OffsetY + 1), shadowColor, Current.Name.c_str());
				drawList->AddText(ImVec2(textPositionX, Position.y + OffsetY), textColor, Current.Name.c_str());

				OffsetY += 12;
			}

			if (g_Options.Visuals.ESP.Vehicles.Distance)
			{
				char bfr[24];
				sprintf(bfr, ("%dm"), (int)Current.Vehicle->GetCoordinate().DistTo(g_Fivem.GetLocalPlayerInfo().WorldPos));

				ImVec2 TextSize = ImGui::CalcTextSize(bfr);

				ImGui::GetBackgroundDrawList()->AddText(ImVec2(Position.x + 1 - TextSize.x / 2, Position.y + OffsetY + 1), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.ESP.Vehicles.Color[3]), bfr);
				ImGui::GetBackgroundDrawList()->AddText(ImVec2(Position.x - TextSize.x / 2, Position.y + OffsetY), ImColor(1.f, 1.f, 1.f, g_Options.Visuals.ESP.Vehicles.Color[3]), bfr);
				OffsetY += 12;
			}

			ImGui::PopFont();
		}
	}
}