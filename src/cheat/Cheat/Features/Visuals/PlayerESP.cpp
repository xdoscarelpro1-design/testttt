#include "PlayerESP.hpp"

#include <cmath>
#include <string>
#include <algorithm>

#include "../../FivemSDK/Fivem.hpp"
#include "../../Options.hpp"

static ID3D11ShaderResourceView* s_SpectatorLogoTex = nullptr;
static float s_LogoWidth = 0.0f;
static float s_LogoHeight = 0.0f;

namespace Cheat
{
    void DrawRocketText(ImFont* pFont, const std::string& text, const ImVec2& pos, float size, ImU32 color, ImU32 borderColor, bool center, ImDrawList* pDrawList)
    {
        if (!pFont) pFont = ImGui::GetDefaultFont();
        if (!pDrawList) pDrawList = ImGui::GetBackgroundDrawList();

        ImVec2 textSize = pFont->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
        ImVec2 drawPos = pos;
        if (center) drawPos.x -= textSize.x / 2.0f;

        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i != 0 || j != 0) {
                    pDrawList->AddText(pFont, size, ImVec2(drawPos.x + i, drawPos.y + j), borderColor, text.c_str());
                }
            }
        }
        pDrawList->AddText(pFont, size, drawPos, color, text.c_str());
    }

	void ESP::Players()
	{
		ImDrawList* DrawList = ImGui::GetBackgroundDrawList();

		int observersCount = 0;
		float closestAimDist = FLT_MAX;
		std::string closestAimName = "";

		for (Entity Current : g_Fivem.GetEntitiyList())
		{
			if (Current.StaticInfo.bIsLocalPlayer && !g_Options.Visuals.Players.ShowLocalPlayer)
				continue;

			if (Current.StaticInfo.bIsNPC && !g_Options.Visuals.Players.ShowNPC)
				continue;

			if (g_Options.Visuals.Players.ExcludeDeads)
			{
				float hp = Current.StaticInfo.Ped->GetHealth();
				if (hp <= 0.f) continue;
			}

			Vector3D PedCoordinates = Current.StaticInfo.Ped->GetCoordinate();
			float Distance = PedCoordinates.DistTo(g_Fivem.GetLocalPlayerInfo().WorldPos);

			if (!Current.StaticInfo.bIsLocalPlayer && Current.StaticInfo.IsLookingAtYou) {
				if (Distance <= 50.0f)
					observersCount++;
				if (Distance < closestAimDist) {
					closestAimDist = Distance;
					if (Current.StaticInfo.bIsNPC) closestAimName = "NPC";
					else closestAimName = "Player [" + std::to_string(Current.StaticInfo.NetId) + "]";
				}
			}

			if (Distance > g_Options.Visuals.Players.RenderDistance)
				continue;

			ImVec2 PedLocation = g_Fivem.WorldToScreen(PedCoordinates);
			if (!g_Fivem.IsOnScreen(PedLocation))
				continue;

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

            if (g_Options.Visuals.Players.RocketStyle)
            {
                float footMiddle = (LeftFoot.y + RightFoot.y) / 2.f;
                Height = std::abs(Head.y - footMiddle) * 1.35f;
                Width = (Height / 1.75f);
                PedCenterY = Head.y + (Height / 2.4f);
            }
            else
            {
                Height *= 1.2f;
            }

			ImVec2 Padding[4] = { ImVec2(0, 0), ImVec2(0, 0), ImVec2(0, 0), ImVec2(0, 0) };

			ImColor CheckBoxColor;
            if (g_Options.Visuals.Players.VisibleCheck) {
                CheckBoxColor = Current.Visible ? FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.BoxColor) : FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.InVisibleBoxColor);
            } else {
                CheckBoxColor = FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.NormalBoxColor);
            }

			if (g_Options.Visuals.Players.EnableBox)
			{
				if (g_Options.Visuals.Players.BoxType == 0)
				{
					DrawList->AddRect(ImVec2(PedLocation.x - Width / 2, PedCenterY - Height / 2.f), ImVec2(PedLocation.x + Width / 2, PedCenterY + Height / 2.f), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Players.BoxColor[3]), 0, ImDrawFlags_None, 3);
					DrawList->AddRect(ImVec2(PedLocation.x - Width / 2, PedCenterY - Height / 2.f), ImVec2(PedLocation.x + Width / 2, PedCenterY + Height / 2.f), CheckBoxColor, 0, ImDrawFlags_None, 2);
				}
				else if (g_Options.Visuals.Players.BoxType == 1)
				{
					float CornerSize = ((((Height / 2.f) / 100.f) + ((Width / 2.f) / 100.f)) / 2.f) * 30.f;
					ImVec2 TopLeftPoints[] = { { PedLocation.x - Width / 2.f, PedCenterY - Height / 2.f + CornerSize }, { PedLocation.x - Width / 2.f, PedCenterY - Height / 2.f }, { PedLocation.x - Width / 2.f + CornerSize, PedCenterY - Height / 2.f } };
					DrawList->AddPolyline(TopLeftPoints, 3, ImColor(0.f, 0.f, 0.f, 1.f), ImDrawFlags_None, 3);
					DrawList->AddPolyline(TopLeftPoints, 3, CheckBoxColor, ImDrawFlags_None, 2);
					ImVec2 TopRightPoints[] = { { PedLocation.x + Width / 2.f, PedCenterY - Height / 2.f + CornerSize }, { PedLocation.x + Width / 2.f, PedCenterY - Height / 2.f }, { PedLocation.x + Width / 2.f - CornerSize, PedCenterY - Height / 2.f } };
					DrawList->AddPolyline(TopRightPoints, 3, ImColor(0.f, 0.f, 0.f, 1.f), ImDrawFlags_None, 3);
					DrawList->AddPolyline(TopRightPoints, 3, CheckBoxColor, ImDrawFlags_None, 2);
					ImVec2 BottomLeftPoints[] = { { PedLocation.x - Width / 2.f, PedCenterY + Height / 2.f - CornerSize }, { PedLocation.x - Width / 2.f, PedCenterY + Height / 2.f }, { PedLocation.x - Width / 2.f + CornerSize, PedCenterY + Height / 2.f } };
					DrawList->AddPolyline(BottomLeftPoints, 3, ImColor(0.f, 0.f, 0.f, 1.f), ImDrawFlags_None, 3);
					DrawList->AddPolyline(BottomLeftPoints, 3, CheckBoxColor, ImDrawFlags_None, 2);
					ImVec2 BottomRightPoints[] = { { PedLocation.x + Width / 2.f, PedCenterY + Height / 2.f - CornerSize }, { PedLocation.x + Width / 2.f, PedCenterY + Height / 2.f }, { PedLocation.x + Width / 2.f - CornerSize, PedCenterY + Height / 2.f } };
					DrawList->AddPolyline(BottomRightPoints, 3, ImColor(0.f, 0.f, 0.f, 1.f), ImDrawFlags_None, 3);
					DrawList->AddPolyline(BottomRightPoints, 3, CheckBoxColor, ImDrawFlags_None, 2);
				}
			}

			if (g_Options.Visuals.Players.Skeleton)
			{
				ImVec2 Pelvis = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_Pelvis));
				ImVec2 Neck = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_Neck_1));
				ImVec2 LeftClavicle = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_L_Clavicle));
				ImVec2 RightClavicle = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_R_Clavicle));
				ImVec2 LeftUperarm = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_L_UpperArm));
				ImVec2 RightUperarm = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_R_UpperArm));
				ImVec2 RightFormArm = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_R_Forearm));
				ImVec2 LeftFormArm = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_L_Forearm));
				ImVec2 RightHand = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_R_Hand));
				ImVec2 LeftHand = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_L_Hand));
				ImVec2 LeftThigh = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_L_Thigh));
				ImVec2 LeftCalf = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_L_Calf));
				ImVec2 RightThigh = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_R_Thigh));
				ImVec2 RightCalf = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_R_Calf));

				if (g_Fivem.IsOnScreen(Neck) && g_Fivem.IsOnScreen(Pelvis)) {
					ImColor Color;
                    if (Current.StaticInfo.IsFriend) {
                        Color = FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.FriendSkeletonColor);
                    } else {
                        if (g_Options.Visuals.Players.VisibleCheck) {
                            Color = Current.Visible ? FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.SkeletonColor) : FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.InvisibleSkeletonColor);
                        } else {
                            Color = FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.NormalSkeletonColor);
                        }
                    }

					DrawList->AddLine(Neck, RightClavicle, Color, 1.f); DrawList->AddLine(Neck, LeftClavicle, Color, 1.f);
					DrawList->AddLine(RightClavicle, RightUperarm, Color, 1.f); DrawList->AddLine(LeftClavicle, LeftUperarm, Color, 1.f);
					DrawList->AddLine(RightUperarm, RightFormArm, Color, 1.f); DrawList->AddLine(LeftUperarm, LeftFormArm, Color, 1.f);
					DrawList->AddLine(RightFormArm, RightHand, Color, 1.f); DrawList->AddLine(LeftFormArm, LeftHand, Color, 1.f);
					DrawList->AddLine(Neck, Pelvis, Color, 1.f); DrawList->AddLine(Pelvis, LeftThigh, Color, 1.f);
					DrawList->AddLine(Pelvis, RightThigh, Color, 1.f); DrawList->AddLine(LeftThigh, LeftCalf, Color, 1.f);
					DrawList->AddLine(RightThigh, RightCalf, Color, 1.f); DrawList->AddLine(LeftCalf, LeftFoot, Color, 1.f);
					DrawList->AddLine(RightCalf, RightFoot, Color, 1.f);
				}
			}

			if (g_Options.Visuals.Players.EnableHeadBol)
			{
				Vector3D HeadPos3D = g_Fivem.GetBonePosVec3(Current, SKEL_Head);
				ImVec2 head_screen_pos = g_Fivem.WorldToScreen(HeadPos3D);
				if (g_Fivem.IsOnScreen(head_screen_pos)) {
					int CircleValue = Height / 30;
					if (CircleValue < 3) CircleValue = 3;
					DrawList->AddCircle(head_screen_pos, CircleValue, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.HeadCircleColor), 32, 1.5f);
				}
			}

            float slotTotalHeights[4] = { 0, 0, 0, 0 };
            auto CalcHeight = [&](int type, const std::string& text, bool isBar = false) {
                if (type < 0 || type >= 4) return;
                float itemH = 0;
                if (isBar) itemH = (type == 2 || type == 3) ? 3.0f : Height / 2.0f;
                else {
                    ImFont* font = g_Options.Visuals.Players.RocketStyle ? FrameWork::Assets::Poppins : FrameWork::Assets::InterBold12;
                    float fontSize = g_Options.Visuals.Players.RocketStyle ? 16.0f : 12.0f;
                    itemH = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str()).y;
                }
                slotTotalHeights[type] += itemH + 4;
            };

            if (g_Options.Visuals.Players.Name) CalcHeight(g_Options.Visuals.Players.NamePos, Current.StaticInfo.Name);
            if (g_Options.Visuals.Players.EnableDistance) CalcHeight(g_Options.Visuals.Players.DistPos, XorStr("100m"));
            if (g_Options.Visuals.Players.WeaponName) CalcHeight(g_Options.Visuals.Players.WeaponPos, XorStr("Weapon"));
            if (g_Options.Visuals.Players.HealthBar) CalcHeight(g_Options.Visuals.Players.HealthBarPos, "", true);
            if (g_Options.Visuals.Players.ArmorBar) CalcHeight(g_Options.Visuals.Players.ArmorBarPos, "", true);

            Padding[0].y = -slotTotalHeights[0] / 2.0f;
            Padding[1].y = -slotTotalHeights[1] / 2.0f;

            auto DrawESPText = [&](int type, const std::string& text, ImU32 color) {
                if (text.empty()) return;
                ImFont* font = g_Options.Visuals.Players.RocketStyle ? FrameWork::Assets::Poppins : FrameWork::Assets::InterBold12;
                float fontSize = g_Options.Visuals.Players.RocketStyle ? 16.0f : 12.0f;
                ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());
                ImVec2 drawPos;
                if (type == 0) { drawPos = ImVec2(PedLocation.x - Width / 2 - 22, PedCenterY + Padding[0].y); Padding[0].y += textSize.y + 4; }
                else if (type == 1) { drawPos = ImVec2(PedLocation.x + Width / 2 + 22, PedCenterY + Padding[1].y); Padding[1].y += textSize.y + 4; }
                else if (type == 2) { drawPos = ImVec2(PedLocation.x, PedCenterY + Height / 2 + Padding[2].y + 12); Padding[2].y += textSize.y + 2; }
                else { drawPos = ImVec2(PedLocation.x, PedCenterY - Height / 2 - Padding[3].y - textSize.y - 12); Padding[3].y += textSize.y + 2; }
                DrawRocketText(font, text, drawPos, fontSize, color, ImColor(0, 0, 0, 255), true);
            };

            auto DrawBar = [&](int type, float percentage, ImU32 color) {
                if (type == 0) {
                    ImVec2 drawPos = ImVec2(PedLocation.x - Width / 2 - 22 - 4, PedCenterY + Padding[0].y);
                    ImVec2 size = ImVec2(2, Height / 2.0f);
                    DrawList->AddRectFilled(drawPos - ImVec2(1, 1), drawPos + size + ImVec2(1, 1), ImColor(0, 0, 0, 255));
                    DrawList->AddRectFilled(drawPos + ImVec2(0, size.y - size.y * percentage), drawPos + size, color);
                    Padding[0].y += size.y + 4;
                } else if (type == 1) {
                    ImVec2 drawPos = ImVec2(PedLocation.x + Width / 2 + 22, PedCenterY + Padding[1].y);
                    ImVec2 size = ImVec2(2, Height / 2.0f);
                    DrawList->AddRectFilled(drawPos - ImVec2(1, 1), drawPos + size + ImVec2(1, 1), ImColor(0, 0, 0, 255));
                    DrawList->AddRectFilled(drawPos + ImVec2(0, size.y - size.y * percentage), drawPos + size, color);
                    Padding[1].y += size.y + 4;
                } else if (type == 2) {
                    ImVec2 drawPos = ImVec2(PedLocation.x - Width / 2, PedCenterY + Height / 2 + Padding[2].y + 12);
                    ImVec2 size = ImVec2(Width, 2);
                    DrawList->AddRectFilled(drawPos - ImVec2(1, 1), drawPos + size + ImVec2(1, 1), ImColor(0, 0, 0, 255));
                    DrawList->AddRectFilled(drawPos, drawPos + ImVec2(size.x * percentage, size.y), color);
                    Padding[2].y += size.y + 3;
                } else {
                    ImVec2 drawPos = ImVec2(PedLocation.x - Width / 2, PedCenterY - Height / 2 - Padding[3].y - 12 - 2);
                    ImVec2 size = ImVec2(Width, 2);
                    DrawList->AddRectFilled(drawPos - ImVec2(1, 1), drawPos + size + ImVec2(1, 1), ImColor(0, 0, 0, 255));
                    DrawList->AddRectFilled(drawPos, drawPos + ImVec2(size.x * percentage, size.y), color);
                    Padding[3].y += size.y + 5;
                }
            };

            if (g_Options.Visuals.Players.Name) {
                std::string name = Current.StaticInfo.Name;
                if (Current.StaticInfo.bIsNPC) name = XorStr("NPC");
                DrawESPText(g_Options.Visuals.Players.NamePos, name, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.NameColor));
            }
            if (g_Options.Visuals.Players.EnableDistance) {
                std::string distStr = std::to_string((int)Distance) + "m";
                DrawESPText(g_Options.Visuals.Players.DistPos, distStr, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.DistanceColor));
            }
            if (g_Options.Visuals.Players.WeaponName) {
                CWeaponManager* weaponManager = Current.StaticInfo.Ped->GetWeaponManager();
                if (weaponManager) {
                    CWeaponInfo* weaponInfo = weaponManager->GetWeaponInfo();
                    if (weaponInfo) {
                        std::string weaponName = weaponInfo->GetWeaponName();
                        if (!weaponName.empty()) DrawESPText(g_Options.Visuals.Players.WeaponPos, weaponName, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.WeaponNameColor));
                    }
                }
            }

            if (g_Options.Visuals.Players.HealthBar) {
				float Health = Current.StaticInfo.Ped->GetHealth();
				float MaxHealth = Current.StaticInfo.Ped->GetMaxHealth();
				if (MaxHealth < 100.f) MaxHealth = 200.f;
				float minHealth = 100.0f;
				if (Health < minHealth) minHealth = 0.0f;
				float healthPercentage = std::max<float>(0.0f, std::min<float>((Health - minHealth) / (MaxHealth - minHealth), 1.0f));
				DrawBar(g_Options.Visuals.Players.HealthBarPos, healthPercentage, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.HealthBarColor));
            }

            if (g_Options.Visuals.Players.ArmorBar) {
                float Armor = Current.StaticInfo.Ped->GetArmor();
                float armorPercentage = std::max<float>(0.f, std::min<float>(Armor / 100.f, 1.f));
                DrawBar(g_Options.Visuals.Players.ArmorBarPos, armorPercentage, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.ArmorBarColor));
            }
		}

		if (g_Options.Visuals.Players.EnableAimingWarning && closestAimDist != FLT_MAX)
		{
			ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

			std::string notifyStr = closestAimName + " can see you [" + std::to_string((int)closestAimDist) + "m]";
			ImFont* font = FrameWork::Assets::InterBold12;
			ImVec2 textSize = font->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, notifyStr.c_str());

			ImVec2 pMin = ImVec2(center.x + 30, center.y - 16);
			ImVec2 pMax = ImVec2(center.x + 65 + textSize.x, center.y + 10);
			DrawList->AddRectFilled(pMin, pMax, ImColor(15, 15, 15, 200), 5.0f);
			DrawList->AddRect(pMin, pMax, ImColor(50, 50, 50, 255), 5.0f);

			DrawRocketText(font, "!", ImVec2(pMin.x + 10, center.y - 12), 16.0f, ImColor(0, 255, 0, 255), ImColor(0, 0, 0, 255), false, DrawList);
			DrawRocketText(font, notifyStr, ImVec2(pMin.x + 25, center.y - 10), 14.0f, ImColor(255, 255, 255, 255), ImColor(0, 0, 0, 255), false, DrawList);
		}

		if (g_Options.Visuals.Players.EnableObserverWarning && observersCount > 0)
		{
			ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

			std::string obsStr = std::to_string(observersCount) + " Nearby observers";
			ImFont* font = FrameWork::Assets::InterBold12;
			ImVec2 textSize = font->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, obsStr.c_str());

			float width = 35 + textSize.x;
			ImVec2 pMax = ImVec2(center.x - 30, center.y + 10);
			ImVec2 pMin = ImVec2(center.x - 30 - width, center.y - 16);

			DrawList->AddRectFilled(pMin, pMax, ImColor(15, 15, 15, 200), 5.0f);
			DrawList->AddRect(pMin, pMax, ImColor(50, 50, 50, 255), 5.0f);

			DrawRocketText(font, "!", ImVec2(pMin.x + 10, center.y - 12), 16.0f, ImColor(255, 255, 0, 255), ImColor(0, 0, 0, 255), false, DrawList);
			DrawRocketText(font, obsStr, ImVec2(pMin.x + 25, center.y - 10), 14.0f, ImColor(255, 255, 255, 255), ImColor(0, 0, 0, 255), false, DrawList);
		}
	}

	void ESP::DrawPreview()
	{
		if (!g_Options.Visuals.Players.ShowPreview) return;
		ImDrawList* DrawList = ImGui::GetBackgroundDrawList();
		ImVec2 ScreenCenter = ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f);
		ImVec2 PedLocation = ImVec2(ScreenCenter.x + 400, ScreenCenter.y);
		float Height = 300.0f, Width = Height / 1.8f, PedCenterY = PedLocation.y;
		ImVec2 Head = ImVec2(PedLocation.x, PedCenterY - Height / 2.4f);

		if (g_Options.Visuals.Players.RocketStyle) { Width = (Height / 1.75f); PedCenterY = Head.y + (Height / 2.4f); }
		ImColor CheckBoxColor = g_Options.Visuals.Players.VisibleCheck ? FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.BoxColor) : FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.NormalBoxColor);

		if (g_Options.Visuals.Players.EnableBox) {
			if (g_Options.Visuals.Players.BoxType == 0) {
				DrawList->AddRect(ImVec2(PedLocation.x - Width / 2, PedCenterY - Height / 2.f), ImVec2(PedLocation.x + Width / 2, PedCenterY + Height / 2.f), ImColor(0.f, 0.f, 0.f, 1.0f), 0, ImDrawFlags_None, 3);
				DrawList->AddRect(ImVec2(PedLocation.x - Width / 2, PedCenterY - Height / 2.f), ImVec2(PedLocation.x + Width / 2, PedCenterY + Height / 2.f), CheckBoxColor, 0, ImDrawFlags_None, 2);
			} else {
				float CornerSize = Width / 4.f;
				ImVec2 tl = ImVec2(PedLocation.x - Width / 2.f, PedCenterY - Height / 2.f);
				ImVec2 tr = ImVec2(PedLocation.x + Width / 2.f, PedCenterY - Height / 2.f);
				ImVec2 bl = ImVec2(PedLocation.x - Width / 2.f, PedCenterY + Height / 2.f);
				ImVec2 br = ImVec2(PedLocation.x + Width / 2.f, PedCenterY + Height / 2.f);
				auto DrawCorner = [&](ImVec2 p, float dx, float dy) {
					DrawList->AddLine(p, ImVec2(p.x + dx, p.y), ImColor(0,0,0), 3.f);
					DrawList->AddLine(p, ImVec2(p.x, p.y + dy), ImColor(0,0,0), 3.f);
					DrawList->AddLine(p, ImVec2(p.x + dx, p.y), CheckBoxColor, 2.f);
					DrawList->AddLine(p, ImVec2(p.x, p.y + dy), CheckBoxColor, 2.f);
				};
				DrawCorner(tl, CornerSize, CornerSize); DrawCorner(tr, -CornerSize, CornerSize);
				DrawCorner(bl, CornerSize, -CornerSize); DrawCorner(br, -CornerSize, -CornerSize);
			}
		}

		if (g_Options.Visuals.Players.Skeleton) {
			ImColor Color = g_Options.Visuals.Players.VisibleCheck ? FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.SkeletonColor) : FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.NormalSkeletonColor);
			ImVec2 neck = Head + ImVec2(0, 20), pelvis = neck + ImVec2(0, 100), l_shoulder = neck + ImVec2(-40, 10), r_shoulder = neck + ImVec2(40, 10);
			ImVec2 l_elbow = l_shoulder + ImVec2(-20, 40), r_elbow = r_shoulder + ImVec2(20, 40), l_hand = l_elbow + ImVec2(-10, 30), r_hand = r_elbow + ImVec2(10, 30);
			ImVec2 l_hip = pelvis + ImVec2(-20, 0), r_hip = pelvis + ImVec2(20, 0), l_knee = l_hip + ImVec2(-10, 60), r_knee = r_hip + ImVec2(10, 60), l_foot = l_knee + ImVec2(0, 50), r_foot = r_knee + ImVec2(0, 50);
			DrawList->AddLine(neck, r_shoulder, Color, 1.f); DrawList->AddLine(neck, l_shoulder, Color, 1.f);
			DrawList->AddLine(r_shoulder, r_elbow, Color, 1.f); DrawList->AddLine(l_shoulder, l_elbow, Color, 1.f);
			DrawList->AddLine(r_elbow, r_hand, Color, 1.f); DrawList->AddLine(l_elbow, l_hand, Color, 1.f);
			DrawList->AddLine(neck, pelvis, Color, 1.f); DrawList->AddLine(pelvis, l_hip, Color, 1.f);
			DrawList->AddLine(pelvis, r_hip, Color, 1.f); DrawList->AddLine(l_hip, l_knee, Color, 1.f);
			DrawList->AddLine(r_hip, r_knee, Color, 1.f); DrawList->AddLine(l_knee, l_foot, Color, 1.f);
			DrawList->AddLine(r_knee, r_foot, Color, 1.f);
		}

		if (g_Options.Visuals.Players.EnableHeadBol) DrawList->AddCircle(Head, 10, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.HeadCircleColor), 32, 1.5f);

		int verticalOffset = 10;
		if (g_Options.Visuals.Players.Name) { DrawRocketText(FrameWork::Assets::Poppins, XorStr("PREVIEW PLAYER"), ImVec2(PedLocation.x, PedCenterY + Height / 2 + verticalOffset), 16.0f, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.NameColor), ImColor(0, 0, 0), true); verticalOffset += 18; }
		if (g_Options.Visuals.Players.EnableDistance) { DrawRocketText(FrameWork::Assets::Poppins, XorStr("25m"), ImVec2(PedLocation.x, PedCenterY + Height / 2 + verticalOffset), 16.0f, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.DistanceColor), ImColor(0, 0, 0), true); verticalOffset += 18; }
		if (g_Options.Visuals.Players.WeaponName) { DrawRocketText(FrameWork::Assets::Poppins, XorStr("Carbine Rifle"), ImVec2(PedLocation.x, PedCenterY + Height / 2 + verticalOffset), 16.0f, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.WeaponNameColor), ImColor(0, 0, 0), true); }
	}

	void ESP::Radar()
	{
		if (!g_Options.Visuals.Players.RadarEnabled) return;

		ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(0.0f);
		if (ImGui::Begin(XorStr("Radar"), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar))
		{
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			ImVec2 winPos = ImGui::GetWindowPos();
			ImVec2 winSize = ImGui::GetWindowSize();
			ImVec2 center = winPos + winSize * 0.5f;
			float radius = winSize.x * 0.5f;

			for (int i = 1; i <= 10; i++)
				DrawList->AddCircle(center, radius + i, ImColor(0, 0, 0, (int)(15 - i)), 64, 1.0f);

			DrawList->AddCircleFilled(center, radius, ImColor(15, 15, 15, 200), 64);
			DrawList->AddCircle(center, radius, ImColor(40, 40, 40, 255), 64, 2.0f);

			DrawList->AddLine(center - ImVec2(radius, 0), center + ImVec2(radius, 0), ImColor(50, 50, 50, 100));
			DrawList->AddLine(center - ImVec2(0, radius), center + ImVec2(0, radius), ImColor(50, 50, 50, 100));
			DrawList->AddCircle(center, radius * 0.5f, ImColor(50, 50, 50, 100), 64);

			LocalPEDInfo Local = g_Fivem.GetLocalPlayerInfo();
			if (Local.Ped)
			{
				Vector3D LocalPos = Local.WorldPos;

				float yaw = 0.0f;
				if (g_Fivem.GetCamGameplayDirector()) {
					auto cam = g_Fivem.GetCamGameplayDirector()->GetFollowPedCamera();
					if (cam) yaw = cam->GetViewAngles().z;
				}

				ImVec2 p1 = center + ImVec2(0, -7);
				ImVec2 p2 = center + ImVec2(-5, 5);
				ImVec2 p3 = center + ImVec2(5, 5);

				DrawList->AddTriangleFilled(p1, p2, p3, ImColor(255, 255, 255));
				DrawList->AddTriangle(p1, p2, p3, ImColor(0, 0, 0, 200), 1.5f);

				float radarRange = 150.0f;
				float cos_y = cos(-yaw);
				float sin_y = sin(-yaw);

				for (Entity Current : g_Fivem.GetEntitiyList())
				{
					if (Current.StaticInfo.bIsLocalPlayer) continue;
					if (Current.StaticInfo.bIsNPC && !g_Options.Visuals.Players.ShowNPC) continue;

					Vector3D RelativePos = Current.StaticInfo.Ped->GetCoordinate() - LocalPos;
					float dist = sqrt(RelativePos.x * RelativePos.x + RelativePos.y * RelativePos.y);

					if (dist > radarRange) continue;

					float rx = RelativePos.x * cos_y - RelativePos.y * sin_y;
					float ry = RelativePos.x * sin_y + RelativePos.y * cos_y;

					ImVec2 pointPos = center + ImVec2((rx / radarRange) * radius, (-ry / radarRange) * radius);

					ImColor pointColor;
					if (Current.StaticInfo.IsFriend) pointColor = ImColor(0, 255, 255);
					else if (Current.Visible) pointColor = ImColor(0, 255, 0);
					else pointColor = ImColor(255, 0, 0);

					DrawList->AddCircleFilled(pointPos, 3.5f, pointColor, 16);
					DrawList->AddCircle(pointPos, 3.5f, ImColor(0, 0, 0, 200), 16, 1.0f);
				}
			}
		}
		ImGui::End();
	}

	void ESP::LoadSpectatorLogo(ID3D11Device* dev)
	{
		// No D3DX11 dependency — logo is rendered as text fallback
		(void)dev;
	}

	void ESP::SpectatorList()
	{
		if (!g_Options.Visuals.Players.SpectatorList) return;

		ImGui::SetNextWindowSizeConstraints(ImVec2(200, 50), ImVec2(250, 400));
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(0.85f);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.09f, 0.95f));

		if (ImGui::Begin(XorStr("Spectator List"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
		{
			ImDrawList* draw = ImGui::GetWindowDrawList();
			ImVec2 pos = ImGui::GetWindowPos();
			ImVec2 size = ImGui::GetWindowSize();

			float headerHeight = 60.0f;

			draw->AddRectFilled(pos, pos + ImVec2(size.x, headerHeight), ImColor(25, 25, 30, 255), 8.0f, ImDrawFlags_RoundCornersTop);
			draw->AddRectFilled(pos + ImVec2(0, headerHeight - 1), pos + ImVec2(size.x, headerHeight + 1), ImColor(255, 255, 255, 200));

			ImGui::SetCursorPos(ImVec2(0, 20));
			ImGui::PushFont(FrameWork::Assets::Poppins);
			ImVec2 text_size = ImGui::CalcTextSize(XorStr("PHANTOM"));
			ImGui::SetCursorPosX((size.x - text_size.x) * 0.5f);
			ImGui::TextColored(ImVec4(1, 1, 1, 1), XorStr("PHANTOM"));
			ImGui::PopFont();

			ImGui::SetCursorPos(ImVec2(15, headerHeight + 10));
			ImGui::PushFont(FrameWork::Assets::InterBold12);
			ImGui::TextColored(ImVec4(0.0f, 0.6f, 1.0f, 1.0f), XorStr("SPECTATORS"));
			ImGui::PopFont();

			ImGui::SetCursorPos(ImVec2(15, headerHeight + 30));
			ImGui::BeginGroup();

			LocalPEDInfo Local = g_Fivem.GetLocalPlayerInfo();
			int specCount = 0;
			if (Local.Ped)
			{
				Vector3D LocalPos = Local.WorldPos;
				for (Entity Current : g_Fivem.GetEntitiyList())
				{
					if (Current.StaticInfo.bIsLocalPlayer || Current.StaticInfo.bIsNPC) continue;
					if (!Current.StaticInfo.Ped) continue;

					Vector3D PedPos = Current.StaticInfo.Ped->GetCoordinate();
					float distance = PedPos.DistTo(LocalPos);

					if (distance < 5.0f && !Current.Visible)
					{
						ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), XorStr("[~] %s"), Current.StaticInfo.Name.c_str());
						specCount++;
					}
				}
			}

			if (specCount == 0)
			{
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), XorStr("No spectators detected."));
			}

			ImGui::EndGroup();
			ImGui::Dummy(ImVec2(0, 10));
		}
		ImGui::End();

		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
	}

	void ESP::DrawCrosshair()
	{
		if (!g_Options.Visuals.Players.CustomCrosshair) return;
		if (!g_Options.LegitBot.AimBot.ShowFov && !g_Options.LegitBot.SilentAim.ShowFOV) return;

		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

		float R = 14.0f;
		float wing_length = 12.0f;
		float thickness = 3.0f;

		ImColor crossColor = FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.CrosshairColor);
		ImColor dotColor = FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.CrosshairDotColor);

		draw->AddCircleFilled(center, 3.5f, dotColor);

		draw->PathArcTo(center, R, 0.0f, IM_PI, 32);
		draw->PathStroke(crossColor, 0, thickness);

		draw->AddLine(ImVec2(center.x - R, center.y), ImVec2(center.x - R - wing_length, center.y), crossColor, thickness);
		draw->AddLine(ImVec2(center.x + R, center.y), ImVec2(center.x + R + wing_length, center.y), crossColor, thickness);
	}
}
