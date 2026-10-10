#include "PlayerESP.hpp"

#include <cmath>

#include "../../FivemSDK/Fivem.hpp"
#include "../../Options.hpp"
#include <optional>
#include <utility>
#include <vector>
#include "ImGui/imgui.h"

struct Position
{
	ImVec2 Pos;
};


namespace Cheat
{
	void ESP::Players()
	{
		if (!g_Fivem.GetLocalPlayerInfo().Ped)
			return;

		for (Entity Current : g_Fivem.GetEntitiyList())
		{
			if (Current.StaticInfo.bIsLocalPlayer && !g_Options.Visuals.ESP.Players.ShowLocalPlayer)
				continue;

			if (Current.StaticInfo.bIsNPC && !g_Options.Visuals.ESP.Players.ShowNPCs)
				continue;

			if (g_Options.Visuals.ESP.Players.VisibleOnly && !Current.Visible)
				continue;

			Vector3D PedCoordinates = Current.Cordinates;

			float Distance = PedCoordinates.DistTo(g_Fivem.GetLocalPlayerInfo().WorldPos);

			if (Distance > g_Options.Visuals.ESP.Players.RenderDistance)
				continue;

			ImVec2 PedLocation = g_Fivem.WorldToScreen(PedCoordinates);

			// Compute ALL bone positions in the same render pass so they share
			// the frame-cached view matrix. Using Current.HeadPos (set during
			// UpdateEntities) with fresh W2S calls for the feet causes the head
			// and feet to use different view matrices — the box height wobbles
			// every frame as the camera moves between the two reads.
			ImVec2 Head = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_Head));
			if (!g_Fivem.IsOnScreen(Head))
				continue;

			ImVec2 LeftFoot = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_L_Foot));
			if (!g_Fivem.IsOnScreen(LeftFoot))
				continue;

			ImVec2 RightFoot = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_R_Foot));
			if (!g_Fivem.IsOnScreen(RightFoot))
				continue;

			float Height = -Head.y;

			if (LeftFoot.y > RightFoot.y)
				Height += LeftFoot.y;
			else
				Height += RightFoot.y;

			float Width = Height / 1.8f;
			float PedCenterY = Head.y + Height / 2.f;
			Height *= 1.2f;

			ImVec2 Padding[4] = { ImVec2(0, 0), ImVec2(0, 0), ImVec2(0, 0), ImVec2(0, 0) };

			if (g_Options.Visuals.ESP.Players.Box)
			{
				ImColor Color = (Current.StaticInfo.IsFriend)
					? ImColor(g_Options.Misc.FriendColor[0], g_Options.Misc.FriendColor[1], g_Options.Misc.FriendColor[2], g_Options.Misc.FriendColor[3])
					: ImColor(g_Options.Visuals.ESP.Players.BoxColor[0], g_Options.Visuals.ESP.Players.BoxColor[1], g_Options.Visuals.ESP.Players.BoxColor[2], g_Options.Visuals.ESP.Players.BoxColor[3]);

				ImVec2 rectMin = ImVec2(PedLocation.x - Width / 2, PedCenterY - Height / 2.f);
				ImVec2 rectMax = ImVec2(PedLocation.x + Width / 2, PedCenterY + Height / 2.f);

				ImDrawList* drawList = ImGui::GetBackgroundDrawList();
				if (drawList) {
					drawList->AddRect(rectMin, rectMax, ImColor(0, 0, 0), 0, ImDrawFlags_None, 3);
					drawList->AddRect(rectMin, rectMax, Color, 0, ImDrawFlags_None, 2);
				}

				Padding[0].y += 3;
				Padding[1].x += 3;
				Padding[2].y += 3;
				Padding[3].x += 3;
			}

			if (g_Options.Visuals.ESP.Players.FilledBox)
			{
				ImColor Color = (Current.StaticInfo.IsFriend)
					? ImColor(g_Options.Misc.FriendColor[0], g_Options.Misc.FriendColor[1], g_Options.Misc.FriendColor[2], g_Options.Misc.FriendColor[3])
					: ImColor(g_Options.Visuals.ESP.Players.FilledBoxColor[0], g_Options.Visuals.ESP.Players.FilledBoxColor[1], g_Options.Visuals.ESP.Players.FilledBoxColor[2], g_Options.Visuals.ESP.Players.FilledBoxColor[3]);

				ImVec2 rectMin = ImVec2(PedLocation.x - Width / 2, PedCenterY - Height / 2.f);
				ImVec2 rectMax = ImVec2(PedLocation.x + Width / 2, PedCenterY + Height / 2.f);

				ImDrawList* drawList = ImGui::GetBackgroundDrawList();
				if (drawList) {
					drawList->AddRectFilled(rectMin, rectMax, Color);
				}

				Padding[0].y += 3;
				Padding[1].x += 3;
				Padding[2].y += 3;
				Padding[3].x += 3;
			}

			if (g_Options.Visuals.ESP.Players.Skeleton)
			{
				ImColor Color = (Current.StaticInfo.IsFriend)
					? ImColor(g_Options.Misc.FriendColor[0], g_Options.Misc.FriendColor[1], g_Options.Misc.FriendColor[2], g_Options.Misc.FriendColor[3])
					: ImColor(g_Options.Visuals.ESP.Players.SkeletonColor[0], g_Options.Visuals.ESP.Players.SkeletonColor[1], g_Options.Visuals.ESP.Players.SkeletonColor[2], g_Options.Visuals.ESP.Players.SkeletonColor[3]);

				static const std::pair<int, int> bonePairs[] = {
					{SKEL_Pelvis, SKEL_Neck_1}, {SKEL_Neck_1, SKEL_L_Clavicle},
					{SKEL_Neck_1, SKEL_R_Clavicle}, {SKEL_L_Clavicle, SKEL_L_UpperArm},
					{SKEL_R_Clavicle, SKEL_R_UpperArm}, {SKEL_L_UpperArm, SKEL_L_Forearm},
					{SKEL_R_UpperArm, SKEL_R_Forearm}, {SKEL_L_Forearm, SKEL_L_Hand},
					{SKEL_R_Forearm, SKEL_R_Hand}, {SKEL_Pelvis, SKEL_L_Thigh},
					{SKEL_Pelvis, SKEL_R_Thigh}, {SKEL_L_Thigh, SKEL_L_Calf},
					{SKEL_R_Thigh, SKEL_R_Calf}, {SKEL_L_Calf, SKEL_L_Foot},
					{SKEL_R_Calf, SKEL_R_Foot},
				};

				auto GetBoneScreenPos = [&](int boneId) -> std::optional<ImVec2> {
					ImVec2 pos = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, boneId));
					return g_Fivem.IsOnScreen(pos) ? std::optional<ImVec2>{pos} : std::nullopt;
					};

				ImDrawList* drawList = ImGui::GetBackgroundDrawList();
				if (drawList) {
					for (const auto& [boneA, boneB] : bonePairs) {
						auto boneAPos = GetBoneScreenPos(boneA);
						auto boneBPos = GetBoneScreenPos(boneB);

						if (boneAPos && boneBPos) {
							drawList->AddLine(*boneAPos, *boneBPos, Color, 1.f);
						}
					}
				}
			}

			if (g_Options.Visuals.ESP.Players.Head)
			{
				ImColor Color = (Current.StaticInfo.IsFriend)
					? ImColor(g_Options.Misc.FriendColor[0], g_Options.Misc.FriendColor[1], g_Options.Misc.FriendColor[2], g_Options.Misc.FriendColor[3])
					: ImColor(g_Options.Visuals.ESP.Players.HeadColor[0], g_Options.Visuals.ESP.Players.HeadColor[1], g_Options.Visuals.ESP.Players.HeadColor[2], g_Options.Visuals.ESP.Players.HeadColor[3]);

				auto GetBoneScreenPos = [&](int boneId) -> std::optional<ImVec2> {
					ImVec2 pos = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, boneId));
					return g_Fivem.IsOnScreen(pos) ? std::optional<ImVec2>{pos} : std::nullopt;
					};

				ImDrawList* drawList = ImGui::GetBackgroundDrawList();
				if (drawList) {
					auto headPos = GetBoneScreenPos(SKEL_Head);
					if (headPos) {
						drawList->AddCircle(*headPos, g_Options.Visuals.ESP.Players.HeadSize, Color, 16);
					}
				}
			}

			if (g_Options.Visuals.ESP.Players.HealthBar)
			{
				ImColor Color = ImColor(g_Options.Visuals.ESP.Players.HealthBarColor[0],
					g_Options.Visuals.ESP.Players.HealthBarColor[1],
					g_Options.Visuals.ESP.Players.HealthBarColor[2],
					g_Options.Visuals.ESP.Players.HealthBarColor[3]);

				float Health = Current.StaticInfo.Ped->GetHealth();
				float MaxHealth = Current.StaticInfo.Ped->GetMaxHealth();

				ImVec2 DrawPos = ImVec2(PedLocation.x - Width / 2, PedCenterY - Height / 2) - Padding[3] - ImVec2(2, 0);
				ImVec2 Size = ImVec2(0, Height);
				ImVec2 DrawSize = ImVec2(-2, (Height / MaxHealth) * Health);

				ImDrawList* drawList = ImGui::GetBackgroundDrawList();
				if (drawList) {
					drawList->AddRectFilled(DrawPos - ImVec2(1, 1), DrawPos + Size + ImVec2(1, 1), ImColor(0, 0, 0));
					drawList->AddRectFilled(DrawPos, DrawPos + Size + ImVec2(3, 0), ImColor(0, 0, 0));
					drawList->AddRectFilled(DrawPos + Size, DrawPos + Size - DrawSize, Color);
				}

				Padding[3].x += 4;
			}
			if (g_Options.Visuals.ESP.Players.ArmorBar)
			{
				ImColor Color = ImColor(g_Options.Visuals.ESP.Players.ArmorBarColor[0],
					g_Options.Visuals.ESP.Players.ArmorBarColor[1],
					g_Options.Visuals.ESP.Players.ArmorBarColor[2],
					g_Options.Visuals.ESP.Players.ArmorBarColor[3]);

				float Armor = Current.StaticInfo.Ped->GetArmor();
				const float MaxArmor = 100.0f;

				if (Armor > 0) {
					ImVec2 DrawPos = ImVec2(PedLocation.x - Width / 2, PedCenterY + Height / 2 + Padding[2].y);
					ImVec2 Size = ImVec2(Width, 2);
					ImVec2 DrawSize = ImVec2((Width / MaxArmor) * Armor, 2);

					ImDrawList* drawList = ImGui::GetBackgroundDrawList();
					if (drawList) {
						drawList->AddRectFilled(DrawPos - ImVec2(1, 1), DrawPos + Size + ImVec2(1, 1), ImColor(0, 0, 0));
						drawList->AddRectFilled(DrawPos, DrawPos + Size, ImColor(0, 0, 0));
						drawList->AddRectFilled(DrawPos, DrawPos + DrawSize, Color);
					}

					Padding[2].y += 3;
				}

			}
			if (g_Options.Visuals.ESP.Players.Name)
			{
				const std::string& Name = Current.StaticInfo.Name.empty() ? "NPC" : Current.StaticInfo.Name;

				ImVec2 TextSize = ImGui::CalcTextSize(Name.c_str());

				ImVec2 DrawPos = ImVec2(PedLocation.x - TextSize.x / 2, PedCenterY + Height / 2 + Padding[2].y);

				Padding[2].y += TextSize.y;

				ImColor Color = Current.StaticInfo.IsFriend
					? ImColor(g_Options.Misc.FriendColor[0], g_Options.Misc.FriendColor[1], g_Options.Misc.FriendColor[2], g_Options.Misc.FriendColor[3])
					: ImColor(g_Options.Visuals.ESP.Players.NameColor[0], g_Options.Visuals.ESP.Players.NameColor[1], g_Options.Visuals.ESP.Players.NameColor[2], g_Options.Visuals.ESP.Players.NameColor[3]);

				ImDrawList* drawList = ImGui::GetBackgroundDrawList();
				if (drawList) {
					drawList->AddText(DrawPos + ImVec2(1, 1), ImColor(0.f, 0.f, 0.f, Color.Value.w), Name.c_str());
					drawList->AddText(DrawPos, Color, Name.c_str());
				}

				ImGui::PushFont(FrameWork::Assets::InterRegular);
				ImGui::PopFont();

			}
			if (g_Options.Visuals.ESP.Players.WeaponName)
			{
				if (CWeaponManager* WeaponManager = Current.StaticInfo.Ped->GetWeaponManager()) {
					if (CWeaponInfo* WeaponInfo = WeaponManager->GetWeaponInfo()) {
						const std::string& WeaponName = WeaponInfo->GetWeaponName();

						if (!WeaponName.empty()) {
							ImVec2 TextSize = ImGui::CalcTextSize(WeaponName.c_str());
							ImVec2 DrawPos = ImVec2(PedLocation.x - TextSize.x / 2, PedCenterY + Height / 2 + Padding[2].y - 3);

							Padding[2].y += TextSize.y;

							ImColor WeaponColor(
								g_Options.Visuals.ESP.Players.WeaponNameColor[0],
								g_Options.Visuals.ESP.Players.WeaponNameColor[1],
								g_Options.Visuals.ESP.Players.WeaponNameColor[2],
								g_Options.Visuals.ESP.Players.WeaponNameColor[3]
							);

							ImDrawList* drawList = ImGui::GetBackgroundDrawList();
							if (drawList) {
								drawList->AddText(DrawPos + ImVec2(1, 1), ImColor(0.f, 0.f, 0.f, WeaponColor.Value.w), WeaponName.c_str());
								drawList->AddText(DrawPos, WeaponColor, WeaponName.c_str());
							}

							ImGui::PushFont(FrameWork::Assets::InterRegular);
							ImGui::PopFont();
						}

					}
				}
			}
			if (g_Options.Visuals.ESP.Players.Distance)
			{
				if (!Current.StaticInfo.bIsLocalPlayer)
				{
					std::string PlayerDistance = std::to_string(static_cast<int>(Distance)) + "m";

					ImVec2 TextSize = ImGui::CalcTextSize(PlayerDistance.c_str());
					ImVec2 DrawPos = ImVec2(PedLocation.x - TextSize.x / 2, PedCenterY + Height / 2 + Padding[2].y - 3);

					Padding[2].y += TextSize.y;

					ImColor Color(
						g_Options.Visuals.ESP.Players.DistanceColor[0],
						g_Options.Visuals.ESP.Players.DistanceColor[1],
						g_Options.Visuals.ESP.Players.DistanceColor[2],
						g_Options.Visuals.ESP.Players.DistanceColor[3]
					);

					ImDrawList* drawList = ImGui::GetBackgroundDrawList();
					if (drawList) {
						drawList->AddText(DrawPos + ImVec2(1, 1), ImColor(0.f, 0.f, 0.f, Color.Value.w), PlayerDistance.c_str());
						drawList->AddText(DrawPos, Color, PlayerDistance.c_str());
					}
				}
			}
			if (g_Options.Visuals.ESP.Players.SnapLines)
			{
				if (!Current.StaticInfo.bIsLocalPlayer) {
					ImColor Color = Current.StaticInfo.IsFriend
						? ImColor(g_Options.Misc.FriendColor[0], g_Options.Misc.FriendColor[1], g_Options.Misc.FriendColor[2], g_Options.Misc.FriendColor[3])
						: ImColor(g_Options.Visuals.ESP.Players.SnaplinesColor[0], g_Options.Visuals.ESP.Players.SnaplinesColor[1], g_Options.Visuals.ESP.Players.SnaplinesColor[2], g_Options.Visuals.ESP.Players.SnaplinesColor[3]);

					ImDrawList* drawList = ImGui::GetBackgroundDrawList();
					if (drawList) {
						drawList->AddLine(g_Fivem.GetLocalPlayerInfo().ScreenPos, PedLocation, Color);
					}
				}

			}


		}
	}
}