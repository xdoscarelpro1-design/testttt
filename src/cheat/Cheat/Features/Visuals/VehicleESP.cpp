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
			ImVec2 Position = g_Fivem.WorldToScreen(Current.Vehicle->GetCoordinate());
			if (!g_Fivem.IsOnScreen(Position))
				continue;

			float OffsetY = 0;

			if (g_Options.Visuals.Vehicles.Marker)
			{
				ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(Position.x, Position.y + OffsetY), 4, ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Vehicles.MarkerColor[3]));
				ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(Position.x, Position.y + OffsetY), 3, ImColor(g_Options.Visuals.Vehicles.MarkerColor[0], g_Options.Visuals.Vehicles.MarkerColor[1], g_Options.Visuals.Vehicles.MarkerColor[2], g_Options.Visuals.Vehicles.MarkerColor[3]));

				OffsetY += 12;
			}

			ImGui::PushFont(FrameWork::Assets::InterBold12);

			if (g_Options.Visuals.Vehicles.Name)
			{
				std::string DisplayName = Current.Name;
				if (g_Options.Visuals.Vehicles.ShowLockState)
				{
					if (Current.Vehicle->GetLockState() == CARLOCK_LOCKED)
						DisplayName += XorStr(" - LOCKED");
					else if (Current.Vehicle->GetLockState() == CARLOCK_UNLOCKED)
						DisplayName += XorStr(" - UNLOCKED");
				}

				ImVec2 TextSize = ImGui::CalcTextSize(DisplayName.c_str());

				ImGui::GetBackgroundDrawList()->AddText(ImVec2(Position.x + 1 - TextSize.x / 2, Position.y + OffsetY + 1), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Vehicles.NameColor[3]), DisplayName.c_str());
				ImGui::GetBackgroundDrawList()->AddText(ImVec2(Position.x - TextSize.x / 2, Position.y + OffsetY), ImColor(g_Options.Visuals.Vehicles.NameColor[0], g_Options.Visuals.Vehicles.NameColor[1], g_Options.Visuals.Vehicles.NameColor[2], g_Options.Visuals.Vehicles.NameColor[3]), DisplayName.c_str());

				OffsetY += 12;
			}

			if (g_Options.Visuals.Vehicles.Distance)
			{
				char bfr[24];
				sprintf(bfr, ("%dm"), (int)Current.Vehicle->GetCoordinate().DistTo(g_Fivem.GetLocalPlayerInfo().WorldPos));

				ImVec2 TextSize = ImGui::CalcTextSize(bfr);

				ImGui::GetBackgroundDrawList()->AddText(ImVec2(Position.x + 1 - TextSize.x / 2, Position.y + OffsetY + 1), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Vehicles.DistanceColor[3]), bfr);
				ImGui::GetBackgroundDrawList()->AddText(ImVec2(Position.x - TextSize.x / 2, Position.y + OffsetY), ImColor(g_Options.Visuals.Vehicles.DistanceColor[0], g_Options.Visuals.Vehicles.DistanceColor[1], g_Options.Visuals.Vehicles.DistanceColor[2], g_Options.Visuals.Vehicles.DistanceColor[3]), bfr);
				OffsetY += 12;
			}

			ImGui::PopFont();
		}
	}
}
