#include "Fivem.hpp"

#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <cmath>
#include "FrameWork/FrameWork.hpp"
#include "../Options.hpp"
#include "OffsetTable.hpp"

namespace Cheat
{
	uint64_t EntityType;
	uint64_t Armor;
	uint64_t BoneManager;
	uint64_t PlayerInfo;
	uint64_t PlayerNetID;
	uint64_t WeaponManager;
	uint64_t FragInsNmGTA;
	uint64_t ConfigFlags;
	uint64_t MaxHealth;
	uint64_t LastVehicle;
	uint64_t Driver;
	uint64_t DoorLock;
	uint64_t Stamina;
	uint64_t VisibleFlag;
	uint64_t LocalPlayer;
	uint64_t PlayerSpeed;


	void FivemSDK::Intialize()
	{
		if (bIsIntialized)
			return;

		// Dynamic process detection — finds ANY FiveM build
		int buildNumber = 0;
		std::wstring procName;
		Pid = FrameWork::Memory::FindFiveMProcess(buildNumber, procName);

		if (!Pid)
			return;

		ModuleName = FrameWork::Misc::Wstring2String(procName);
		ModuleBase = FrameWork::Memory::GetModuleBaseByName(Pid, procName);

		if (!ModuleBase)
			return;

		// Try exact offset match first, then closest known build
		const VersionOffsets* offsets = FindOffsets(buildNumber);
		if (!offsets)
			offsets = FindClosestOffsets(buildNumber);

		if (!offsets)
			return;

		// Apply pointer offsets (relative to module base)
		World = ModuleBase + offsets->World;
		ReplayInterface = ModuleBase + offsets->ReplayInterface;
		ViewPort = ModuleBase + offsets->ViewPort;
		Camera = ModuleBase + offsets->Camera;
		bIsPlayerAiming = ModuleBase + offsets->IsPlayerAiming;
		PlayerAimingAt = ModuleBase + offsets->PlayerAimingAt;
		HandleBullet = ModuleBase + offsets->HandleBullet;
		BlipList = ModuleBase + offsets->BlipList;

		// Apply struct field offsets
		EntityType = offsets->oEntityType;
		Armor = offsets->oArmor;
		BoneManager = offsets->oBoneManager;
		PlayerInfo = offsets->oPlayerInfo;
		PlayerNetID = offsets->oPlayerNetID;
		WeaponManager = offsets->oWeaponManager;
		PlayerSpeed = offsets->oPlayerSpeed;
		FragInsNmGTA = offsets->oFragInsNmGTA;
		ConfigFlags = offsets->oConfigFlags;
		MaxHealth = offsets->oMaxHealth;
		LastVehicle = offsets->oLastVehicle;
		Driver = offsets->oDriver;
		DoorLock = offsets->oDoorLock;
		Stamina = offsets->oStamina;
		VisibleFlag = offsets->oVisibleFlag;

		// If this isn't an exact build match, or if we rely on pattern scanning (World == 0), run the scanner
		if (offsets->buildNumber != buildNumber || offsets->World == 0)
		{
			FrameWork::Memory::AttachProces(Pid);

			HANDLE hProc = OpenProcess(PROCESS_VM_READ, FALSE, Pid);
			uint64_t moduleSize = hProc ? FrameWork::SigScanner::GetModuleSize(hProc, ModuleBase) : 0;

			if (hProc && moduleSize > 0)
			{
				// Resolves a RIP-relative pointer and only accepts it if it
				// lands inside the module. Anything else means the signature
				// matched garbage — better to leave the offset at 0 and let
				// the validation below report it than to chase a bad pointer.
				auto ScanPointer = [&](const char* pattern, int instrLen) -> uint64_t
					{
						uint64_t sig = FrameWork::SigScanner::FindPattern(hProc, ModuleBase, moduleSize, pattern);
						if (!sig)
							return 0;

						uint64_t resolved = FrameWork::SigScanner::ResolveRelative(hProc, sig, instrLen);

						if (resolved <= ModuleBase || resolved >= ModuleBase + moduleSize)
							return 0;

						return resolved;
					};

				// Code addresses (hook targets) aren't RIP-relative — we want
				// the matched instruction itself, not a pointer it references.
				auto ScanCode = [&](const char* pattern) -> uint64_t
					{
						return FrameWork::SigScanner::FindPattern(hProc, ModuleBase, moduleSize, pattern);
					};

				// Pattern: 48 8B 05 ? ? ? ? 33 D2 48 8B 40 08
				if (uint64_t v = ScanPointer("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08", 7))
					World = v;

				// Pattern: 48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D
				if (uint64_t v = ScanPointer("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D", 7))
					ReplayInterface = v;

				// Pattern: 48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD
				if (uint64_t v = ScanPointer("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD", 7))
					ViewPort = v;

				// --- These two were NEVER scanned ---------------------------
				// On any build whose table entry is 0 (b3751), Camera and
				// HandleBullet stayed at ModuleBase+0 — the PE header. The
				// camera pointer then read garbage that wasn't null, so the
				// aimbot happily "aimed" using a constant bogus view vector,
				// and silent aim tried to write its JMP into read-only header
				// memory and failed silently.
				if (uint64_t v = ScanPointer("48 8B 05 ? ? ? ? 38 98 ? ? ? ? 8A C3", 7))
					Camera = v;

				// BlipList — used for waypoint teleport
				if (!BlipList || BlipList == ModuleBase)
					if (uint64_t v = ScanPointer("48 8B 05 ? ? ? ? 41 B8 ? ? ? ? 38 1D", 7))
						BlipList = v;
				if (!BlipList || BlipList == ModuleBase)
					if (uint64_t v = ScanPointer("48 8B 05 ? ? ? ? 0F B7 48 ? 66 85 C9", 7))
						BlipList = v;
				if (!BlipList || BlipList == ModuleBase)
					if (uint64_t v = ScanPointer("48 8B 05 ? ? ? ? 4C 8D 0D ? ? ? ? 41 8B D6", 7))
						BlipList = v;
				if (!BlipList || BlipList == ModuleBase)
					if (uint64_t v = ScanPointer("48 8B 05 ? ? ? ? 40 84 FF 0F 84", 7))
						BlipList = v;

				// HandleBullet prologue: loads 3 floats from the bullet end-position pointer.
				// Multiple signatures for different compiler outputs across builds.
				// Original: movss xmm3,[r9] / movss xmm0,[r9+4] / movss xmm2,[r9+8]
				if (uint64_t v = ScanCode("F3 41 0F 10 19 F3 41 0F 10 41 04 F3 41 0F 10 51 08"))
					HandleBullet = v;
				// Alt 1: movss xmm0,[r9] / movss xmm1,[r9+4] / movss xmm2,[r9+8]
				if (!HandleBullet || HandleBullet == ModuleBase)
					if (uint64_t v = ScanCode("F3 41 0F 10 01 F3 41 0F 10 49 04 F3 41 0F 10 51 08"))
						HandleBullet = v;
				// Alt 2: movss xmm0,[r8] / movss xmm1,[r8+4] / movss xmm2,[r8+8]
				if (!HandleBullet || HandleBullet == ModuleBase)
					if (uint64_t v = ScanCode("F3 41 0F 10 00 F3 41 0F 10 48 04 F3 41 0F 10 50 08"))
						HandleBullet = v;
				// Alt 3: movss xmm3,[r8] / movss xmm0,[r8+4] / movss xmm2,[r8+8]
				if (!HandleBullet || HandleBullet == ModuleBase)
					if (uint64_t v = ScanCode("F3 41 0F 10 18 F3 41 0F 10 40 04 F3 41 0F 10 50 08"))
						HandleBullet = v;
				// Alt 4: movss xmm6,[r9] / movss xmm7,[r9+4] / movss xmm8,[r9+8]
				if (!HandleBullet || HandleBullet == ModuleBase)
					if (uint64_t v = ScanCode("F3 41 0F 10 31 F3 41 0F 10 79 04 F3 44 0F 10 41 08"))
						HandleBullet = v;
				// Alt 5: shorter context — movss xmm3,[r9] followed by movss ?,[r9+4] (partial)
				if (!HandleBullet || HandleBullet == ModuleBase)
					if (uint64_t v = ScanCode("F3 41 0F 10 19 F3 41 0F 10 ? 04"))
						HandleBullet = v;
			}

			if (hProc)
				CloseHandle(hProc);
		}
		else
		{
			FrameWork::Memory::AttachProces(Pid);
		}

		// --- Validation -------------------------------------------------
		// Anything still sitting at ModuleBase is an unresolved offset. Record
		// it so the UI can say WHICH feature is dead instead of the feature
		// just doing nothing and looking broken.
		ResolvedBuild = buildNumber;
		ExactOffsetMatch = (offsets->buildNumber == buildNumber && offsets->World != 0);

		WorldValid = (World != ModuleBase);
		ReplayValid = (ReplayInterface != ModuleBase);
		ViewPortValid = (ViewPort != ModuleBase);
		CameraValid = (Camera != ModuleBase);
		HandleBulletValid = (HandleBullet != ModuleBase);
		BlipListValid = (BlipList != ModuleBase && BlipList != 0);
		AimingFlagValid = (bIsPlayerAiming != ModuleBase);

		bIsIntialized = true;
	}


	bool FivemSDK::UpdateEntities()
	{
		if (!pWorld)
		{
			pWorld = (CWorld*)FrameWork::Memory::ReadMemory<uint64_t>(World);

			if (!pWorld)
				return false;

#ifdef _DEBUG
			std::cout << ("[FivemSDK] World: 0x") << std::hex << std::uppercase << pWorld << std::endl;
#endif // _DEBUG
		}

		pLocalPlayer = pWorld->LocalPlayer();

		if (!pLocalPlayer)
			return false;

#ifdef _DEBUG
		static bool Logged = false;
		if (!Logged)
		{
			std::cout << ("[FivemSDK] LocalPlayer: 0x") << std::hex << std::uppercase << pLocalPlayer << std::endl;
			Logged = true;
		}
#endif // _DEBUG

		if (!pReplayInterface)
		{
			pReplayInterface = (CReplayInterface*)FrameWork::Memory::ReadMemory<uint64_t>(ReplayInterface);

			if (!pReplayInterface)
				return false;

#ifdef _DEBUG
			std::cout << ("[FivemSDK] ReplayInterface: 0x") << std::hex << std::uppercase << pReplayInterface << std::endl;
#endif // _DEBUG
		}

		if (!pPedInterface)
		{
			pPedInterface = pReplayInterface->PedInterface();

			if (!pPedInterface)
				return false;

#ifdef _DEBUG
			std::cout << ("[FivemSDK] PedInterface: 0x") << std::hex << std::uppercase << pPedInterface << std::endl;
#endif // _DEBUG
		}

		if (!pCamGameplayDirector)
		{
			pCamGameplayDirector = (CCamGameplayDirector*)FrameWork::Memory::ReadMemory<uint64_t>(Camera);

			if (!pCamGameplayDirector)
				return false;

#ifdef _DEBUG
			std::cout << ("[FivemSDK] CamGameplayDirector: 0x") << std::hex << std::uppercase << pCamGameplayDirector << std::endl;
#endif // _DEBUG
		}


		LockLists.lock();

		EntityList.clear();
		EntityList.shrink_to_fit();

		for (size_t i = 0; i < pPedInterface->PedMaximum(); i++)
		{
			CPed* Ped = pPedInterface->PedList()->Ped(i);
			if (!Ped)
				continue;

			if ((uint64_t)Ped > 0xCCCCCCCCCCCCC)
				continue;

			if (Ped->IsAnimal())
				continue;

			// Only skip actually-dead peds. The old check was
			// `GetHealth() <= 101.f`, which threw away every player at or
			// below 101 HP — and FiveM players sit at 100-200 by default.
			// That emptied EntityList, which is why aimbot and silent aim
			// both silently did nothing: FindClosestEntity never matched.
			if (!g_Options.General.IncludeDeathPlayers && Ped->GetHealth() <= 0.f)
				continue;

			PedStaticInfo StaticInfo;

			{
				StaticInfo.Ped = Ped;
				StaticInfo.iIndex = i;
				StaticInfo.bIsLocalPlayer = (Ped == pLocalPlayer);
				StaticInfo.bIsNPC = Ped->IsNPC();

				if (FriendList.find(Ped) != FriendList.end())
					StaticInfo.IsFriend = true;
				else
					StaticInfo.IsFriend = false;

				if (!StaticInfo.bIsNPC)
				{
					StaticInfo.NetId = Ped->GetPlayerInfo()->GetPlayerID();
				}
				else
				{
					StaticInfo.NetId = -1;
				}

				StaticInfo.crSkeletonData = FrameWork::Memory::ReadMemory<uint64_t>(FrameWork::Memory::ReadMemory<uint64_t>(FrameWork::Memory::ReadMemory<uint64_t>(Ped + FragInsNmGTA) + 0x68) + 0x178);
			}

			Entity CurrentEntity;
			CurrentEntity.StaticInfo = StaticInfo;

			if (CurrentEntity.StaticInfo.Name.empty())
			{
				if (!PlayersInfo.empty() && !PlayerIdToName.empty())
				{
					auto it = PlayerIdToName.find(CurrentEntity.StaticInfo.NetId);
					if (it != PlayerIdToName.end() && CurrentEntity.StaticInfo.NetId != -1)
					{
						CurrentEntity.StaticInfo.Name = it->second;
					}
					else
					{
						CurrentEntity.StaticInfo.Name = ("username");
					}
				}
			}

			CurrentEntity.Cordinates = Ped->GetCoordinate();

			CurrentEntity.HeadPos = WorldToScreen(GetBonePosVec3(CurrentEntity, SKEL_Head));

			CurrentEntity.Visible = Ped->IsVisible();

			if (CurrentEntity.StaticInfo.bIsLocalPlayer)
			{
				LocalPlayerInfo.Ped = Ped;
				LanGame = StaticInfo.bIsNPC;
				LocalPlayerInfo.iIndex = i;
				LocalPlayerInfo.WorldPos = CurrentEntity.Cordinates;
				LocalPlayerInfo.ScreenPos = WorldToScreen(CurrentEntity.Cordinates);
			}

			EntityList.push_back(CurrentEntity);
		}

		LockLists.unlock();

		if (!LanGame)
		{
			if (ServerIp.size() < 2)
			{
				if (FivemFolder.empty() || CrashoMetryLocation.empty())
				{
					HKEY hKey;
					WCHAR Buffer[MAX_PATH];
					DWORD BufferSize = sizeof(Buffer);
					if (SafeCall(RegOpenKeyEx)(HKEY_CURRENT_USER, (L"SOFTWARE\\CitizenFX\\FiveM"), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
					{
						if (RegQueryValueEx(hKey, (L"Last Run Location"), NULL, NULL, (LPBYTE)Buffer, &BufferSize) == ERROR_SUCCESS)
						{
							std::string AAA = FrameWork::Misc::Wstring2String(std::wstring(Buffer)) + ("data\\cache\\crashometry");
							if (std::filesystem::exists(AAA))
							{
								FivemFolder = FrameWork::Misc::Wstring2String(std::wstring(Buffer));
								CrashoMetryLocation = FrameWork::Misc::Wstring2String(std::wstring(Buffer)) + ("data\\cache\\crashometry");
							}
						}

						SafeCall(RegCloseKey)(hKey);
					}
				}

				if (!CrashoMetryLocation.empty() || ServerIp.empty())
				{
					std::ifstream Crashometry(CrashoMetryLocation, std::ios::binary);

					std::string RawText;

					if (Crashometry.is_open())
					{
						std::vector<char> Buffer((std::istreambuf_iterator<char>(Crashometry)), std::istreambuf_iterator<char>());

						RawText = std::string(Buffer.begin(), Buffer.end());

						if (RawText.size() > 0)
						{
							size_t StartPos = RawText.find(("last_server_url"));
							if (StartPos != std::string::npos)
							{
								std::string TempServerIp = RawText.substr(StartPos + 15);

								if (TempServerIp.find(("last_server_url")))
								{
									TempServerIp = TempServerIp.substr(TempServerIp.find(("last_server_url")) + 15);
								}

								StartPos = TempServerIp.find(("last_server"));

								TempServerIp = TempServerIp.substr(StartPos + 11);

								StartPos = TempServerIp.find((":"));

								ServerIp = TempServerIp.substr(0, StartPos);

								ServerPort = TempServerIp.substr(StartPos + 1, StartPos + 5);
							}
						}

						Crashometry.close();
					}
				}

				if (ServerIp.size() < 5 || ServerPort.size() < 2)
				{
					LanGame = true;
				}
				else
				{
#ifdef _DEBUG
					std::cout << ("[FivemSDK] Server IP: ") << ServerIp << (":") << ServerPort << std::endl;
#endif // _DEBUG

					std::thread([&]() { this->UpdateNamesThread(); }).detach();
				}
			}
		}

		return true;
	}

	bool FivemSDK::UpdateVehicles()
	{
		if (!pCamGameplayDirector)
			return false;

		if (!pVehicleInterface)
		{
			pVehicleInterface = pReplayInterface->VehicleInterface();

			if (!pVehicleInterface)
				return false;

#ifdef _DEBUG
			std::cout << ("[FivemSDK] VehicleInterface: 0x") << std::hex << std::uppercase << pVehicleInterface << std::endl;
#endif // _DEBUG
		}

		LockLists2.lock();

		VehicleList.clear();
		VehicleList.shrink_to_fit();

		for (size_t i = 0; i < pVehicleInterface->VehicleMaximum(); i++)
		{
			CVehicle* Vehicle = pVehicleInterface->VehicleList()->Vehicle(i);
			if (!Vehicle)
				continue;

			if (Vehicle->GetCoordinate().DistTo(GetLocalPlayerInfo().WorldPos) > 600)
				continue;

			VehicleInfo CurrentVeh;
			CurrentVeh.Vehicle = Vehicle;
			CurrentVeh.ModelInfo = Vehicle->GetModelInfo();
			if (CurrentVeh.ModelInfo)
			{
				CurrentVeh.Name = FrameWork::Memory::ReadProcessMemoryString(CurrentVeh.ModelInfo + 0x298, 24);
				if (CurrentVeh.Name.size() > 24)
					continue;
			}
			else
				continue;
			CurrentVeh.iIndex = i;

			VehicleList.push_back(CurrentVeh);
		}

		LockLists2.unlock();

		return true;
	}

	Vector3D GetBonePosByInstFragAndID(uint64_t crSkeletonData, unsigned int BoneID)
	{
		Matrix4x4 v4 = FrameWork::Memory::ReadMemory<Matrix4x4>(FrameWork::Memory::ReadMemory<uint64_t>(crSkeletonData + 0x8));
		Matrix4x4 Result = FrameWork::Memory::ReadMemory<Matrix4x4>(FrameWork::Memory::ReadMemory<uint64_t>(crSkeletonData + 0x18) + (BoneID << 6));

		Vector3D vec1(v4._11, v4._12, v4._13);
		Vector3D vec2(v4._21, v4._22, v4._23);
		Vector3D vec3(v4._31, v4._32, v4._33);
		Vector3D vec4(v4._41, v4._42, v4._43);
		Vector3D vec5(Result._41, Result._42, Result._43);

		return Vector3D(
			vec1.x * vec5.x + vec4.x + vec2.x * vec5.y + vec3.x * vec5.z,
			vec1.y * vec5.x + vec4.y + vec2.y * vec5.y + vec3.y * vec5.z,
			vec1.z * vec5.x + vec4.z + vec2.z * vec5.y + vec3.z * vec5.z
		);
	}

	Vector3D FivemSDK::GetBonePosVec3(Entity Ped, unsigned int Mask)
	{
		uint64_t FragInstNMGta = FrameWork::Memory::ReadMemory<uint64_t>(Ped.StaticInfo.Ped + FragInsNmGTA);
		if (FragInstNMGta)
		{
			Ped.StaticInfo.crSkeletonData = FrameWork::Memory::ReadMemory<uint64_t>(FrameWork::Memory::ReadMemory<uint64_t>(FragInstNMGta + 0x68) + 0x178);

			auto it = Ped.StaticInfo.MaskToBoneId.find(Mask);
			if (it == Ped.StaticInfo.MaskToBoneId.end())
			{
				unsigned int BoneId = 0;
				if (GetPedBoneIndex(Ped, Mask, BoneId))
				{
					if (BoneId)
					{
						Ped.StaticInfo.MaskToBoneId[Mask] = BoneId;
						return GetBonePosByInstFragAndID(Ped.StaticInfo.crSkeletonData, BoneId);
					}
				}
			}
			else
			{
				return GetBonePosByInstFragAndID(Ped.StaticInfo.crSkeletonData, it->second);
			}
		}

		return Vector3D(0, 0, 0);
	}

	CPed* FivemSDK::GetAimingEntity()
	{
		return (CPed*)FrameWork::Memory::ReadMemory<uint64_t>(PlayerAimingAt);
	}

	bool FivemSDK::IsPlayerAiming()
	{
		// Unresolved on builds with no offset entry — reading ModuleBase+0
		// returns PE header bytes, which are nonzero and would report
		// "aiming" forever. Treat unknown as not-aiming's opposite: callers
		// use this as a gate, so failing open keeps the feature usable
		// instead of silently dead.
		if (!AimingFlagValid)
			return true;

		return FrameWork::Memory::ReadMemory<bool>(bIsPlayerAiming);
	}

	bool FivemSDK::GetPedBoneIndex(Entity Ped, unsigned int Mask, unsigned int& newIdx)
	{
		uint64_t crSkeletonData = FrameWork::Memory::ReadMemory<uint64_t>(Ped.StaticInfo.crSkeletonData);

		if (FrameWork::Memory::ReadMemory<int16_t>(crSkeletonData + 0x1A))
		{
			uint16_t v1 = FrameWork::Memory::ReadMemory<uint16_t>(crSkeletonData + 0x18);
			if (v1)
			{
				int64_t v2 = FrameWork::Memory::ReadMemory<int64_t>(crSkeletonData + 0x10);
				int Count = 0;
				for (int64_t i = FrameWork::Memory::ReadMemory<int64_t>(v2 + 0x8 * (Mask % v1)); ; i = FrameWork::Memory::ReadMemory<int64_t>(i + 0x8))
				{
					Count++;
					if (!i || i >= 0xCCCCCCCCCCCCCC || Count > 3)
						return false;

					int v5 = FrameWork::Memory::ReadMemory<int>(i);
					if (Mask == v5)
					{
						int v6 = FrameWork::Memory::ReadMemory<int>(i + 0x4);
						newIdx = v6;
						return true;
					}
				}
			}
		}
		else if (Mask < FrameWork::Memory::ReadMemory<uint64_t>(crSkeletonData + 0x5E))
		{
			newIdx = Mask;
			return true;
		}

		return false;
	}

	ImVec2 FivemSDK::GetClosestHitBox(Entity Ped)
	{
		const ImVec2 Center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

		const ImVec2 Bones[3] = {
			Ped.HeadPos,
			g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Ped, SKEL_Neck_1)),
			g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Ped, SKEL_Spine3))
		};

		// Take the best ON-SCREEN bone. The old version bailed out entirely if
		// ANY of the three was off-screen, so anyone half-covered by the edge
		// of your monitor was simply untargetable.
		ImVec2 Best = ImVec2(0, 0);
		float BestDistance = FLT_MAX;

		for (const ImVec2& Bone : Bones)
		{
			if (!g_Fivem.IsOnScreen(Bone))
				continue;

			const float Distance = Bone.DistTo(Center);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Bone;
			}
		}

		return Best;
	}

	bool FivemSDK::FindClosestEntity(float Fov, int MaxDistance, bool NPC, Entity* Output)
	{
		Entity Closest;
		float ClosestScreenDistance = FLT_MAX;

		bool Found = false;

		const ImVec2 Center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
		const Vector3D LocalPos = GetLocalPlayerInfo().WorldPos;

		LockLists.lock();
		for (Entity Current : EntityList)
		{
			if (Current.StaticInfo.bIsLocalPlayer)
				continue;

			if (Current.StaticInfo.bIsNPC && !NPC)
				continue;

			if (Current.Cordinates.DistTo(LocalPos) > MaxDistance)
				continue;

			ImVec2 ClosestBone = GetClosestHitBox(Current);
			if (ClosestBone.x == 0 && ClosestBone.y == 0)
				continue;

			const float ScreenDistance = ClosestBone.DistTo(Center);

			if (ScreenDistance > Fov)
				continue;

			// Single pass, single criterion. The old version pushed every
			// improving candidate into a vector and then re-picked the
			// WORLD-closest one out of it — so a distant target under your
			// crosshair lost to a nearby one off to the side.
			if (ScreenDistance < ClosestScreenDistance)
			{
				ClosestScreenDistance = ScreenDistance;
				Closest = Current;
				Found = true;
			}
		}
		LockLists.unlock();

		if (Found)
			*Output = Closest;

		return Found;
	}

	void FivemSDK::ProcessCameraMovement(Vector3D WorldPosition, int SmoothHorizontal, int SmoothVertical)
	{
		if (!pCamGameplayDirector)
			return;

		auto FollowPedCamera = pCamGameplayDirector->GetFollowPedCamera();
		if (!FollowPedCamera)
			return;

		Vector3D CrosshairPosition = FollowPedCamera->GetCrosshairPosition();
		Vector3D ViewAngles = FollowPedCamera->GetViewAngles();

		if (!WorldPosition.IsValid() || !CrosshairPosition.IsValid() || !ViewAngles.IsValid())
			return;

		// These are unit direction vectors, NOT euler angles. Everything below
		// has to stay on the unit sphere or the camera jerks.
		Vector3D AimDir = (WorldPosition - CrosshairPosition).Normalized();
		if (AimDir.LengthSqr() < 1e-6f)
			return;

		Vector3D CurrentDir = ViewAngles.Normalized();
		if (CurrentDir.LengthSqr() < 1e-6f)
			CurrentDir = AimDir;

		// Sliders are 0-100 "smoothness". Map to a per-second rate, then
		// convert to this tick's blend with an exponential so the aim travels
		// the same arc regardless of how fast the thread happens to be
		// running. A fixed per-tick fraction makes the camera speed ride on
		// framerate, which is exactly what makes an aimbot feel gritty.
		static auto LastMove = std::chrono::steady_clock::now();
		const auto Now = std::chrono::steady_clock::now();
		float Dt = std::chrono::duration<float>(Now - LastMove).count();
		LastMove = Now;

		// First call, a stall, or a hitch — clamp to something sane.
		if (Dt <= 0.f || Dt > 0.1f)
			Dt = 0.008f;

		auto SmoothToBlend = [Dt](int smooth) -> float
			{
				if (smooth <= 0)
					return 1.f;
				if (smooth > 100)
					smooth = 100;

				// Approach rate in "units per second". 100 -> slow and heavy,
				// 1 -> near-instant.
				const float t = static_cast<float>(smooth) / 100.f;
				const float rate = 60.f / (1.f + t * t * 80.f);

				float blend = 1.f - std::exp(-rate * Dt);

				if (blend > 1.f) blend = 1.f;
				if (blend < 0.f) blend = 0.f;

				return blend;
			};

		const float blendH = SmoothToBlend(SmoothHorizontal);
		const float blendV = SmoothToBlend(SmoothVertical);

		// Horizontal (x,y) and vertical (z) get independent rates, then the
		// whole thing is re-normalized — that renormalize is the fix. Lerping
		// two unit vectors gives you a SHORTER vector; writing it back
		// un-normalized is what made the old aimbot stutter.
		Vector3D FinalDir;
		FinalDir.x = CurrentDir.x + (AimDir.x - CurrentDir.x) * blendH;
		FinalDir.y = CurrentDir.y + (AimDir.y - CurrentDir.y) * blendH;
		FinalDir.z = CurrentDir.z + (AimDir.z - CurrentDir.z) * blendV;

		FinalDir = FinalDir.Normalized();

		if (FinalDir.LengthSqr() < 1e-6f || !FinalDir.IsValid())
			return;

		// The third-person camera carries a pitch offset from the aim camera;
		// preserve it so the view doesn't snap when the aimbot lets go.
		Vector3D ThirdPerson = FollowPedCamera->GetThirdpersonViewAngles();
		const float PitchDelta = ViewAngles.z - ThirdPerson.z;

		Vector3D ThirdPersonAngles = FinalDir;
		ThirdPersonAngles.z = FinalDir.z - PitchDelta;

		FollowPedCamera->SetThirdpersonViewAngles(ThirdPersonAngles);
		FollowPedCamera->SetViewAngles(FinalDir);
	}

	void FivemSDK::TeleportObject(uintptr_t Object, uintptr_t Navigation, uintptr_t ModelInfo, Vector3D Position, Vector3D VisualPosition, bool Stop)
	{
		float BackupMagic = 0.f;
		if (Stop)
		{
			BackupMagic = FrameWork::Memory::ReadMemory<float>(ModelInfo + 0x2C);
			FrameWork::Memory::WriteMemory(ModelInfo + 0x2C, 0.f);
		}

		// Physics/navigation first, then the visual coordinate. The renderer
		// reads 0x90; if we wrote that first and the frame landed between the
		// two writes, you'd see the model at the new spot while collision was
		// still at the old one — a visible one-frame lurch.
		FrameWork::Memory::WriteMemory(Navigation + 0x50, Position);
		FrameWork::Memory::WriteMemory(Object + 0x90, VisualPosition);

		if (Stop)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(40));
			FrameWork::Memory::WriteMemory(ModelInfo + 0x2C, BackupMagic);
		}
	}

	ImVec2 FivemSDK::WorldToScreen(Vector3D Pos)
	{
		if (!pViewPort)
		{
			pViewPort = FrameWork::Memory::ReadMemory<uint64_t>(ViewPort);
			if (!pViewPort)
				return ImVec2(0, 0);
		}

		// Cache the view matrix per frame so all W2S calls within the same
		// render tick use identical transforms. Without this, each call reads
		// the matrix fresh from memory — if the camera moves between calls,
		// head and feet positions drift relative to each other and the ESP box
		// vibrates up and down. Single-read eliminates the intra-frame mismatch.
		static Matrix4x4 CachedMatrix;
		static int CachedFrame = -1;
		int CurrentFrame = ImGui::GetFrameCount();
		if (CurrentFrame != CachedFrame)
		{
			CachedMatrix = FrameWork::Memory::ReadMemory<Matrix4x4>(pViewPort + 0x24C);
			CachedMatrix.TransposeThisMatrix();
			CachedFrame = CurrentFrame;
		}
		Matrix4x4 ViewMatrix = CachedMatrix;

		Vector4D VecX(ViewMatrix._21, ViewMatrix._22, ViewMatrix._23, ViewMatrix._24);
		Vector4D VecY(ViewMatrix._31, ViewMatrix._32, ViewMatrix._33, ViewMatrix._34);
		Vector4D VecZ(ViewMatrix._41, ViewMatrix._42, ViewMatrix._43, ViewMatrix._44);

		Vector3D ScreenPos;
		ScreenPos.x = (VecX.x * Pos.x) + (VecX.y * Pos.y) + (VecX.z * Pos.z) + VecX.w;
		ScreenPos.y = (VecY.x * Pos.x) + (VecY.y * Pos.y) + (VecY.z * Pos.z) + VecY.w;
		ScreenPos.z = (VecZ.x * Pos.x) + (VecZ.y * Pos.y) + (VecZ.z * Pos.z) + VecZ.w;

		if (ScreenPos.z <= 0.1f)
			return ImVec2(0, 0);

		ScreenPos.z = 1.0f / ScreenPos.z;
		ScreenPos.x *= ScreenPos.z;
		ScreenPos.y *= ScreenPos.z;

		ScreenPos.x += ImGui::GetIO().DisplaySize.x / 2 + float(0.5f * ScreenPos.x * ImGui::GetIO().DisplaySize.x + 0.5f);
		ScreenPos.y = ImGui::GetIO().DisplaySize.y / 2 - float(0.5f * ScreenPos.y * ImGui::GetIO().DisplaySize.y + 0.5f);

		return ImVec2(ScreenPos.x, ScreenPos.y);
	}

	void FivemSDK::UpdateNamesThread()
	{
		while (!g_Options.General.ShutDown)
		{
			if (ServerIp.size() > 5 && ServerPort.size() > 2)
			{
				PlayersInfo = GetServerInfo();

				for (const auto& Player : PlayersInfo)
				{
					PlayerIdToName[Player[("id")]] = Player[("name")];
				}

#ifdef _DEBUG
				std::cout << ("[FivemSDK] Found Players Infos: ") << PlayerIdToName.size() << std::endl;
#endif // _DEBUG

				AllEntitesList.clear();

				std::this_thread::sleep_for(std::chrono::seconds(10));
			}

			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

	nlohmann::json FivemSDK::GetServerInfo()
	{
		if (!ServerIp.empty())
		{
			std::string RawServerInfo = FrameWork::Misc::DownloadServerInfo(FrameWork::Misc::String2WString(ServerIp), std::atoi(ServerPort.c_str()));

			return nlohmann::json::parse(RawServerInfo);
		}

		return NULL;
	}

	bool FivemSDK::IsOnScreen(ImVec2 Pos)
	{
		if (Pos.x <= 1.f && Pos.y <= 1.f)
			return false;

		return true;
	}
}