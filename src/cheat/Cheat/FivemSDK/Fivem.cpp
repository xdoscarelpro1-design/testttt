#include "Fivem.hpp"
#include "../Options.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <regex>

struct visibleCache {
    bool is_visible;
    uint32_t query_id;
    std::chrono::steady_clock::time_point last_check;
    static constexpr auto cacheMs = std::chrono::milliseconds(50);
};

static std::unordered_map<uintptr_t, visibleCache> visibility_cache;
static std::mutex cache_mutex;

namespace Cheat
{
    void FivemSDK::Intialize()
    {
        if (bIsIntialized)
            return;

        ModuleBase = 0;
        ModuleBaseSize = 0;
        RealGameVersion = 0;
        GameVersion = 0;
        Invisible = 0;
        CanCombatRoll = 0;
        NetSync = 0;
        NetIdToNamesPtr = 0;
        GameplayCamHolder = 0;
        GameplayCamTarget = 0;
        OcclusionArray = 0;
        CitizemPlayerNamesModule = 0;

        pWorld = nullptr;
        pLocalPlayer = nullptr;
        pReplayInterface = nullptr;
        pPedInterface = nullptr;
        pVehicleInterface = nullptr;
        pCamGameplayDirector = nullptr;
        pViewPort = 0;

        ModuleBase = FrameWork::Memory::GetModuleBase();
        if (!ModuleBase)
            return;

        ModuleBaseSize = FrameWork::Memory::GetModuleSize(nullptr);
        if (!ModuleBaseSize)
            ModuleBaseSize = 0x4000000;

        GameVersion = FrameWork::Memory::DetectBuildNumber();

        static const std::vector<uint8_t> CheckTable1 = { 0x48, 0xB8, 0x01, 0x00, 0x00 };
        static const std::vector<uint8_t> CheckTable2 = { 0x48, 0x89, 0x5C, 0x24, 0x10 };

        std::vector<std::pair<uint64_t, int>> VersionsMap =
        {
            { 0x6FFE78, 2802 },
            { 0x7014E0, 2944 },
            { 0x7072C0, 3095 },
            { 0x70A064, 3258 },
            { 0x70A814, 3323 },
            { 0x711AC0, 3407 },
            { 0x711AC0, 3570 },
        };

        for (size_t i = 0; i < VersionsMap.size(); i++)
        {
            int Checks1 = 0;
            int Checks2 = 0;

            for (size_t x = 0; x < CheckTable1.size(); x++)
            {
                uint8_t Byte = FrameWork::Memory::ReadMemory<uint8_t>(ModuleBase + VersionsMap.at(i).first + x);
                if (Byte == CheckTable1[x])
                    Checks1++;
                if (Byte == CheckTable2[x])
                    Checks2++;
            }

            if (Checks1 >= (int)CheckTable1.size() - 1 || Checks2 >= (int)CheckTable2.size() - 1)
            {
                RealGameVersion = VersionsMap.at(i).second;
                break;
            }
        }

        if (RealGameVersion == 0)
            RealGameVersion = 3570;

        if (RealGameVersion == 2802)
        {
            World = ModuleBase + 0x254D448;
            ReplayInterface = ModuleBase + 0x1F5B820;
            Camera = ModuleBase + 0x1FBCFA8;
            ViewPort = ModuleBase + 0x1FBC100;

            PlayerAimingAt = ModuleBase + 0x1FCA160;
            HandleBullet = ModuleBase + 0xFF716C;

            CanCombatRoll = ModuleBase + 0x6FFE78;
            GameplayCamHolder = ModuleBase + 0x288D72;
            GameplayCamTarget = ModuleBase + 0x1FBD298;
            BlipList = ModuleBase + 0x1FBD6E0;

            Offsets::EntityType = 0x1098;
            Offsets::FragInsNmGTA = 0x1430;
            Offsets::ConfigFlags = 0x1444;
            Offsets::PlayerInfo = 0x10A8;
            Offsets::PlayerNetID = 0x88;
            Offsets::WeaponManager = 0x10B8;
            Offsets::VisibleFlag = 0x145C;
            Offsets::MaxHealth = 0x284;
            Offsets::SeatBelt = 0x143C;
            Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
            Offsets::Armor = 0x150C;
            Offsets::SpeedModifier = 0xCF0;
            Offsets::DoorLock = 0x13C0;
            Offsets::LastVehicle = 0xD30;
            Offsets::Driver = 0xC68;

            bIsIntialized = true;
        }
        else if (RealGameVersion == 2944)
        {
            World = ModuleBase + 0x257BEA0;
            ReplayInterface = ModuleBase + 0x1F42068;
            Camera = ModuleBase + 0x1FEB968;
            ViewPort = ModuleBase + 0x1FEAAC0;

            CanCombatRoll = ModuleBase + 0x7014E0;
            GameplayCamHolder = ModuleBase + 0x28A916;
            GameplayCamTarget = ModuleBase + 0x1FEBC58;
            BlipList = ModuleBase + 0x1121F0;

            PlayerAimingAt = ModuleBase + 0x1FF8AF0;
            HandleBullet = ModuleBase + 0x1003F80;

            Offsets::EntityType = 0x1098;
            Offsets::FragInsNmGTA = 0x1430;
            Offsets::ConfigFlags = 0x1444;
            Offsets::SeatBelt = 0x143C;
            Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
            Offsets::PlayerInfo = 0x10A8;
            Offsets::PlayerNetID = 0xE8;
            Offsets::WeaponManager = 0x10B8;
            Offsets::VisibleFlag = 0x145C;
            Offsets::MaxHealth = 0x284;
            Offsets::Armor = 0x150C;
            Offsets::DoorLock = 0x13C0;
            Offsets::LastVehicle = 0xD30;
            Offsets::Driver = 0xC68;

            bIsIntialized = true;
        }
        else if (RealGameVersion == 3095)
        {
            World = ModuleBase + 0x2593320;
            ReplayInterface = ModuleBase + 0x1F58B58;
            Camera = ModuleBase + 0x2002888;
            ViewPort = ModuleBase + 0x20019E0;

            PlayerAimingAt = ModuleBase + 0x200FA10;
            HandleBullet = ModuleBase + 0x100F5A4;

            Invisible = ModuleBase + 0x1386426;
            CanCombatRoll = ModuleBase + 0x7072C0;
            GameplayCamHolder = ModuleBase + 0x28E5DA;
            GameplayCamTarget = ModuleBase + 0x2002B78;
            BlipList = ModuleBase + 0x2002FA0;

            Offsets::EntityType = 0x1098;
            Offsets::FragInsNmGTA = 0x1430;
            Offsets::ConfigFlags = 0x1444;
            Offsets::PlayerInfo = 0x10A8;
            Offsets::WeaponManager = 0x10B8;
            Offsets::PlayerNetID = 0xE8;
            Offsets::SeatBelt = 0x143C;
            Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
            Offsets::VisibleFlag = 0x145C;
            Offsets::MaxHealth = 0x284;
            Offsets::Armor = 0x150C;
            Offsets::SpeedModifier = 0xD40;
            Offsets::DoorLock = 0x13C0;
            Offsets::LastVehicle = 0xD30;
            Offsets::Driver = 0xC68;

            bIsIntialized = true;
        }
        else if (RealGameVersion == 3258)
        {
            World = ModuleBase + 0x25B14B0;
            ReplayInterface = ModuleBase + 0x1FBD4F0;
            Camera = ModuleBase + 0x201ED50;
            ViewPort = ModuleBase + 0x201DBA0;

            PlayerAimingAt = ModuleBase + 0X202C8D0;
            HandleBullet = ModuleBase + 0x101A660;

            Invisible = ModuleBase + 0x119345A;
            CanCombatRoll = ModuleBase + 0x70A064;
            GameplayCamHolder = ModuleBase + 0x29029A;
            GameplayCamTarget = ModuleBase + 0x201ED90;
            BlipList = ModuleBase + 0x2023400;

            Offsets::EntityType = 0x1098;
            Offsets::FragInsNmGTA = 0x1430;
            Offsets::ConfigFlags = 0x1444;
            Offsets::PlayerInfo = 0x10A8;
            Offsets::PlayerNetID = 0xE8;
            Offsets::WeaponManager = 0x10B8;
            Offsets::SeatBelt = 0x143C;
            Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
            Offsets::VisibleFlag = 0x145C;
            Offsets::MaxHealth = 0x284;
            Offsets::Armor = 0x150C;
            Offsets::SpeedModifier = 0xD40;
            Offsets::DoorLock = 0x13C0;
            Offsets::LastVehicle = 0xD30;
            Offsets::Driver = 0xC68;

            bIsIntialized = true;
        }
        else if (RealGameVersion == 3323)
        {
            World = ModuleBase + 0x25C15B0;
            ReplayInterface = ModuleBase + 0x1F85458;
            Camera = ModuleBase + 0x202EB48;
            ViewPort = ModuleBase + 0x202DC50;

            PlayerAimingAt = ModuleBase + 0X203C970;
            HandleBullet = ModuleBase + 0x1026CB0;

            Invisible = ModuleBase + 0x11A174C;
            CanCombatRoll = ModuleBase + 0x70A814;
            GameplayCamHolder = ModuleBase + 0x290274;
            GameplayCamTarget = ModuleBase + 0x202EE38;
            BlipList = ModuleBase + 0x2CEDFC;

            Offsets::EntityType = 0x1098;
            Offsets::FragInsNmGTA = 0x1430;
            Offsets::ConfigFlags = 0x1444;
            Offsets::PlayerInfo = 0x10A8;
            Offsets::PlayerNetID = 0xE8;
            Offsets::WeaponManager = 0x10B8;
            Offsets::SeatBelt = 0x143C;
            Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
            Offsets::VisibleFlag = 0x145C;
            Offsets::MaxHealth = 0x284;
            Offsets::Armor = 0x150C;
            Offsets::SpeedModifier = 0xD40;
            Offsets::DoorLock = 0x13C0;
            Offsets::LastVehicle = 0xD30;
            Offsets::Driver = 0xC68;

            bIsIntialized = true;
        }
        else if (RealGameVersion == 3407)
        {
            World = ModuleBase + 0x25D7108;
            ReplayInterface = ModuleBase + 0x1F9A9D8;
            Camera = ModuleBase + 0x20440C8;
            ViewPort = ModuleBase + 0x20431C0;

            PlayerAimingAt = ModuleBase + 0x2051EE0;
            HandleBullet = ModuleBase + 0x102FF8C;

            Invisible = ModuleBase + 0x11B1A2C;
            CanCombatRoll = ModuleBase + 0x711AC0;
            GameplayCamHolder = ModuleBase + 0x290804;
            GameplayCamTarget = ModuleBase + 0x20443B8;
            BlipList = ModuleBase + 0x2CF3E0;

            Offsets::EntityType = 0x1098;
            Offsets::FragInsNmGTA = 0x1430;
            Offsets::ConfigFlags = 0x1444;
            Offsets::PlayerInfo = 0x10A8;
            Offsets::PlayerNetID = 0xE8;
            Offsets::WeaponManager = 0x10B8;
            Offsets::VisibleFlag = 0x145C;
            Offsets::MaxHealth = 0x284;
            Offsets::SeatBelt = 0x143C;
            Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
            Offsets::Armor = 0x150C;
            Offsets::SpeedModifier = 0xD40;
            Offsets::DoorLock = 0x13C0;
            Offsets::LastVehicle = 0xD30;
            Offsets::Driver = 0xC68;

            bIsIntialized = true;
        }
        else if (RealGameVersion == 3570)
        {
            World = ModuleBase + 0x25EC580;
            ReplayInterface = ModuleBase + 0x1FB0418;
            Camera = ModuleBase + 0x2059A48;
            ViewPort = ModuleBase + 0x2058BA0;

            PlayerAimingAt = ModuleBase + 0x20678E0;
            HandleBullet = ModuleBase + 0x102D550;

            Invisible = ModuleBase + 0x11B1A2C;
            CanCombatRoll = ModuleBase + 0x711AC0;
            GameplayCamHolder = ModuleBase + 0x290804;
            GameplayCamTarget = ModuleBase + 0x20443B8;
            BlipList = ModuleBase + 0x2061870;

            Offsets::EntityType = 0x1098;
            Offsets::FragInsNmGTA = 0x1430;
            Offsets::ConfigFlags = 0x1444;
            Offsets::PlayerInfo = 0x10A8;
            Offsets::PlayerNetID = 0xE8;
            Offsets::WeaponManager = 0x10B8;
            Offsets::VisibleFlag = 0x145C;
            Offsets::MaxHealth = 0x284;
            Offsets::SeatBelt = 0x143C;
            Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
            Offsets::Armor = 0x150C;
            Offsets::SpeedModifier = 0xD40;
            Offsets::DoorLock = 0x13C0;
            Offsets::LastVehicle = 0xD30;
            Offsets::Driver = 0xC68;

            bIsIntialized = true;
        }

        if (!bIsIntialized)
        {
            uint64_t moduleSize = ModuleBaseSize;

            auto ScanPointer = [&](const char* pattern, int instrLen) -> uint64_t
            {
                uint64_t sig = FrameWork::SigScanner::FindPattern(ModuleBase, moduleSize, pattern);
                if (!sig) return 0;
                uint64_t resolved = FrameWork::SigScanner::ResolveRelative(sig, instrLen);
                if (resolved <= ModuleBase || resolved >= ModuleBase + moduleSize) return 0;
                return resolved;
            };

            auto ScanCode = [&](const char* pattern) -> uint64_t
            {
                return FrameWork::SigScanner::FindPattern(ModuleBase, moduleSize, pattern);
            };

            if (uint64_t v = ScanPointer("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08", 7))
                World = v;
            if (uint64_t v = ScanPointer("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D", 7))
                ReplayInterface = v;
            if (uint64_t v = ScanPointer("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD", 7))
                ViewPort = v;
            if (uint64_t v = ScanPointer("48 8B 05 ? ? ? ? 38 98 ? ? ? ? 8A C3", 7))
                Camera = v;

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

            if (uint64_t v = ScanCode("F3 41 0F 10 19 F3 41 0F 10 41 04 F3 41 0F 10 51 08"))
                HandleBullet = v;
            if (!HandleBullet || HandleBullet == ModuleBase)
                if (uint64_t v = ScanCode("F3 41 0F 10 01 F3 41 0F 10 49 04 F3 41 0F 10 51 08"))
                    HandleBullet = v;
            if (!HandleBullet || HandleBullet == ModuleBase)
                if (uint64_t v = ScanCode("F3 41 0F 10 00 F3 41 0F 10 48 04 F3 41 0F 10 50 08"))
                    HandleBullet = v;
            if (!HandleBullet || HandleBullet == ModuleBase)
                if (uint64_t v = ScanCode("F3 41 0F 10 18 F3 41 0F 10 40 04 F3 41 0F 10 50 08"))
                    HandleBullet = v;
            if (!HandleBullet || HandleBullet == ModuleBase)
                if (uint64_t v = ScanCode("F3 41 0F 10 31 F3 41 0F 10 79 04 F3 44 0F 10 41 08"))
                    HandleBullet = v;
            if (!HandleBullet || HandleBullet == ModuleBase)
                if (uint64_t v = ScanCode("F3 41 0F 10 19 F3 41 0F 10 ? 04"))
                    HandleBullet = v;

            Offsets::EntityType = 0x1098;
            Offsets::FragInsNmGTA = 0x1430;
            Offsets::ConfigFlags = 0x1444;
            Offsets::PlayerInfo = 0x10A8;
            Offsets::PlayerNetID = 0xE8;
            Offsets::WeaponManager = 0x10B8;
            Offsets::VisibleFlag = 0x145C;
            Offsets::MaxHealth = 0x284;
            Offsets::SeatBelt = 0x143C;
            Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
            Offsets::Armor = 0x150C;
            Offsets::SpeedModifier = 0xD40;
            Offsets::DoorLock = 0x13C0;
            Offsets::LastVehicle = 0xD30;
            Offsets::Driver = 0xC68;

            if (World && World != ModuleBase)
                bIsIntialized = true;
        }

        CitizemPlayerNamesModule = FrameWork::Memory::GetModuleBase(XorStr(L"citizen-playernames-five.dll"));

        if (CitizemPlayerNamesModule)
        {
            uint64_t cpnSize = FrameWork::Memory::GetModuleSize(XorStr(L"citizen-playernames-five.dll"));
            if (!cpnSize) cpnSize = 0x200000;

            uint64_t SigAddr = FrameWork::SigScanner::FindPattern(CitizemPlayerNamesModule, cpnSize, "48 8B 15 ? ? ? ? 48 C1 E1");
            if (SigAddr)
            {
                int Offset = FrameWork::Memory::ReadMemory<int>(SigAddr + 3);
                NetIdToNamesPtr = SigAddr + 7 + Offset;
            }
            else
            {
                SigAddr = FrameWork::SigScanner::FindPattern(CitizemPlayerNamesModule, cpnSize, "48 8B CA 48 0F 45 C8");
                if (SigAddr)
                {
                    for (uint64_t i = 1; i < 64; i++)
                    {
                        uint64_t SearchAddr = SigAddr - i;
                        if (FrameWork::Memory::ReadMemory<uint8_t>(SearchAddr) == 0x48 &&
                            FrameWork::Memory::ReadMemory<uint8_t>(SearchAddr + 1) == 0x8B &&
                            FrameWork::Memory::ReadMemory<uint8_t>(SearchAddr + 2) == 0x05)
                        {
                            int Off = FrameWork::Memory::ReadMemory<int>(SearchAddr + 3);
                            NetIdToNamesPtr = SearchAddr + 7 + Off;
                            break;
                        }
                    }
                }
            }

            if (!NetIdToNamesPtr)
            {
                if (RealGameVersion >= 3570) NetIdToNamesPtr = CitizemPlayerNamesModule + 0x33438;
                else if (RealGameVersion >= 3258) NetIdToNamesPtr = CitizemPlayerNamesModule + 0x32038;
                else NetIdToNamesPtr = CitizemPlayerNamesModule + 0x30D38;
            }
        }

        if (Offsets::m_PlayerPosWriteData == 0)
        {
            Offsets::m_PlayerPosWriteData = FrameWork::SigScanner::FindPattern(
                ModuleBase, ModuleBaseSize,
                "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 30");
        }

        ResolvedBuild = RealGameVersion;
        WorldValid = (World != 0 && World != ModuleBase);
        ReplayValid = (ReplayInterface != 0 && ReplayInterface != ModuleBase);
        ViewPortValid = (ViewPort != 0 && ViewPort != ModuleBase);
        CameraValid = (Camera != 0 && Camera != ModuleBase);
        HandleBulletValid = (HandleBullet != 0 && HandleBullet != ModuleBase);
        BlipListValid = (BlipList != 0 && BlipList != ModuleBase);
        AimingFlagValid = (PlayerAimingAt != 0 && PlayerAimingAt != ModuleBase);
    }


    bool FivemSDK::UpdateEntities()
    {
        if (!pWorld)
        {
            pWorld = (CWorld*)FrameWork::Memory::ReadMemory<uint64_t>(World);
            if (!pWorld)
                return false;
        }

        pLocalPlayer = pWorld->LocalPlayer();

        if (pReplayInterface && !pLocalPlayer)
        {
            g_Options.General.ShutDown = true;
            return false;
        }

        if (!pLocalPlayer)
            return false;

        if (!pReplayInterface)
        {
            pReplayInterface = (CReplayInterface*)FrameWork::Memory::ReadMemory<uint64_t>(ReplayInterface);
            if (!pReplayInterface)
                return false;
        }

        if (!pPedInterface)
        {
            pPedInterface = pReplayInterface->PedInterface();
            if (!pPedInterface)
                return false;
        }

        if (!pCamGameplayDirector)
        {
            pCamGameplayDirector = (CCamGameplayDirector*)FrameWork::Memory::ReadMemory<uint64_t>(Camera);
            if (!pCamGameplayDirector)
                return false;
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

            if (g_Options.Visuals.Players.ExcludeDeads && Ped->GetHealth() <= 0.f)
                continue;

            PedStaticInfo StaticInfo;
            {
                StaticInfo.Ped = Ped;
                StaticInfo.iIndex = i;
                StaticInfo.bIsLocalPlayer = (Ped == pLocalPlayer);
                StaticInfo.bIsNPC = Ped->IsNPC();

                bool is_visible = false;
                bool from_cache = false;
                uintptr_t pedAddress = (uintptr_t)Ped;

                uint32_t original_flags = FrameWork::Memory::ReadMemory<uint32_t>(pedAddress + 0xC0);
                if (!(original_flags & (1 << 29)))
                    FrameWork::Memory::WriteMemory<uint32_t>(pedAddress + 0xC0, original_flags | (1 << 29));

                auto now = std::chrono::steady_clock::now();
                {
                    std::lock_guard<std::mutex> lock(cache_mutex);
                    auto cache_it = visibility_cache.find(pedAddress);
                    if (cache_it != visibility_cache.end())
                    {
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - cache_it->second.last_check);
                        if (elapsed < visibleCache::cacheMs)
                        {
                            is_visible = cache_it->second.is_visible;
                            from_cache = true;
                        }
                    }
                }

                if (!from_cache)
                {
                    uint64_t fx_draw_data = FrameWork::Memory::ReadMemory<uint64_t>(pedAddress + 0x48);
                    if (fx_draw_data)
                    {
                        uint32_t query_id = static_cast<uint32_t>(FrameWork::Memory::ReadMemory<uint8_t>(fx_draw_data + 0x2B)) & 0xFF;
                        if (query_id > 0 && query_id <= 1000)
                        {
                            uint64_t occ_array = OcclusionArray ? OcclusionArray : ModuleBase + 0x2559A50;
                            int visibility_value = FrameWork::Memory::ReadMemory<int>(occ_array + ((query_id - 1) * 0x80) + 0x78);
                            is_visible = visibility_value > 100;
                            {
                                std::lock_guard<std::mutex> lock(cache_mutex);
                                visibility_cache[pedAddress] = { is_visible, query_id, now };
                            }
                        }
                    }
                }

                StaticInfo.VisibleCheck = is_visible;
                StaticInfo.IsLookingAtYou = false;

                if (!StaticInfo.bIsLocalPlayer && pLocalPlayer)
                    StaticInfo.IsLookingAtYou = Ped->IsLookingAtYou(pLocalPlayer->GetCoordinate());

                CPlayerInfo* pPlayerInfo = Ped->GetPlayerInfo();
                if (!pPlayerInfo)
                    pPlayerInfo = (CPlayerInfo*)FrameWork::Memory::ReadMemory<uint64_t>((uint64_t)Ped + 0x10C8);

                if (pPlayerInfo && FriendList.find(pPlayerInfo->GetPlayerID()) != FriendList.end())
                    StaticInfo.IsFriend = true;
                else
                    StaticInfo.IsFriend = false;

                if (!StaticInfo.bIsNPC && pPlayerInfo)
                {
                    StaticInfo.NetId = pPlayerInfo->GetPlayerID();
                    StaticInfo.Name = GetPlayerName((uint64_t)pPlayerInfo + 0x20, StaticInfo.NetId);
                }
                else
                {
                    StaticInfo.NetId = -1;
                    StaticInfo.Name = XorStr("NPC");
                }

                StaticInfo.crSkeletonData = FrameWork::Memory::ReadMemory<uint64_t>(
                    FrameWork::Memory::ReadMemory<uint64_t>(
                        FrameWork::Memory::ReadMemory<uint64_t>((uintptr_t)Ped + Offsets::FragInsNmGTA) + 0x68) + 0x178);
            }

            Entity CurrentEntity;
            CurrentEntity.StaticInfo = StaticInfo;
            CurrentEntity.Cordinates = Ped->GetCoordinate();

            float DistanceToPlayer = CurrentEntity.Cordinates.DistTo(LocalPlayerInfo.WorldPos);
            if (DistanceToPlayer > 400.0f && !StaticInfo.bIsLocalPlayer)
            {
                CurrentEntity.Visible = false;
                EntityList.push_back(CurrentEntity);
                continue;
            }

            CurrentEntity.HeadPos = WorldToScreen(GetBonePosVec3(CurrentEntity, SKEL_Head));
            CurrentEntity.Visible = StaticInfo.VisibleCheck;

            if (CurrentEntity.StaticInfo.bIsLocalPlayer)
            {
                LocalPlayerInfo.WorldPos = CurrentEntity.Cordinates;
                LocalPlayerInfo.Ped = Ped;
                LocalPlayerInfo.iIndex = i;
                LocalPlayerInfo.ScreenPos = WorldToScreen(CurrentEntity.Cordinates);
            }

            EntityList.push_back(CurrentEntity);
        }

        LockLists.unlock();

        static auto last_cleanup = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_cleanup).count() > 5)
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            for (auto it = visibility_cache.begin(); it != visibility_cache.end();)
            {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.last_check);
                if (elapsed > visibleCache::cacheMs * 10)
                    it = visibility_cache.erase(it);
                else
                    ++it;
            }
            last_cleanup = now;
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
                if (CurrentVeh.Name.find('?') != std::string::npos)
                    continue;
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

    void FivemSDK::TeleportToObject(uintptr_t Object, uintptr_t Navigation, uintptr_t ModelInfo, Vector3D Position, Vector3D VisualPosition, bool Stop)
    {
        float BackupMagic = 0.f;
        auto Ped = GetLocalPlayerInfo().Ped;
        bool origInAir = false;

        if (Stop)
        {
            BackupMagic = FrameWork::Memory::ReadMemory<float>(ModelInfo + 0x2C);
            FrameWork::Memory::WriteMemory<float>(ModelInfo + 0x2C, 0.f);
        }

        if (Ped)
        {
            origInAir = Ped->HasConfigFlag(CPED_CONFIG_FLAG_IsInTheAir);
            Ped->SetConfigFlag(CPED_CONFIG_FLAG_IsInTheAir, true);
        }

        FrameWork::Memory::WriteMemory<Vector3D>(Navigation + 0x50, Position);
        FrameWork::Memory::WriteMemory<Vector3D>(Object + 0x90, VisualPosition);

        if (Stop)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            FrameWork::Memory::WriteMemory<float>(ModelInfo + 0x2C, BackupMagic);

            if (Ped && !origInAir)
                Ped->SetConfigFlag(CPED_CONFIG_FLAG_IsInTheAir, false);
        }
    }

    void FivemSDK::network_request_control_of_entity(uint64_t entity, uint64_t localplayer)
    {
        uint64_t entity_net_object = FrameWork::Memory::ReadMemory<uint64_t>(entity + 0xD0);
        uint64_t local_net_object = FrameWork::Memory::ReadMemory<uint64_t>(localplayer + 0xD0);
        set_network_id_can_migrate(entity_net_object, true);
        set_network_id_exists_on_all_machines(entity_net_object, true);
        network_request_control_of_network_id(entity_net_object, local_net_object);
    }

    uint8_t FivemSDK::get_owner_id(uint64_t net_object)
    {
        return FrameWork::Memory::ReadMemory<uint8_t>(net_object + 0x49);
    }

    bool FivemSDK::set_network_id_can_migrate(uint64_t net_object, bool toggle)
    {
        auto word_004E = FrameWork::Memory::ReadMemory<uint16_t>(net_object + 0x4E);
        if (!toggle)
            FrameWork::Memory::WriteMemory<uint16_t>(net_object + 0x4E, word_004E |= 1);
        else
            FrameWork::Memory::WriteMemory<uint16_t>(net_object + 0x4E, word_004E &= ~1);
        return true;
    }

    bool FivemSDK::set_network_id_exists_on_all_machines(uint64_t net_object, bool toggle)
    {
        auto word_004E = FrameWork::Memory::ReadMemory<uint16_t>(net_object + 0x4E);
        if (toggle)
            FrameWork::Memory::WriteMemory<uint16_t>(net_object + 0x4E, word_004E |= 8);
        else
            FrameWork::Memory::WriteMemory<uint16_t>(net_object + 0x4E, word_004E &= ~8);
        return true;
    }

    bool FivemSDK::network_request_control_of_network_id(uint64_t net_object, uint64_t local_net_object)
    {
        FrameWork::Memory::WriteMemory<uint8_t>(net_object + 0x49, get_owner_id(local_net_object));
        FrameWork::Memory::WriteMemory<uint8_t>(net_object + 0x4A, 0xFF);
        FrameWork::Memory::WriteMemory<uint8_t>(net_object + 0x4B, 0);
        return true;
    }

    uint64_t FivemSDK::SyncTreeGetter(std::string SyncName)
    {
        uint64_t SyncTree = NetSync;
        uint64_t SyncTreeOffset = 0;

        std::map<std::string, uint64_t> SyncTreeTypes =
        {
            {XorStr("CPedSyncTree"), SyncTree + 0x26},
            {XorStr("CObjectSyncTree"), SyncTree + 0x2E},
            {XorStr("CHeliSyncTree"), SyncTree + 0x36},
            {XorStr("CDoorSyncTree"), SyncTree + 0x3E},
            {XorStr("CBoatSyncTree"), SyncTree + 0x46},
            {XorStr("CBikeSyncTree"), SyncTree + 0x4E},
            {XorStr("CAutomobileSyncTree"), SyncTree + 0x56},
            {XorStr("CPickupSyncTree"), SyncTree + 0x5E},
            {XorStr("CTrainSyncTree"), SyncTree + 0x82},
            {XorStr("CPlayerSyncTree"), SyncTree + 0x8A},
            {XorStr("CSubmarineSyncTree"), SyncTree + 0x92},
            {XorStr("CPlaneSyncTree"), SyncTree + 0x9A},
            {XorStr("CPickupPlacementSyncTree"), SyncTree + 0xA2}
        };

        auto it = SyncTreeTypes.find(SyncName);
        if (it != SyncTreeTypes.end())
            SyncTreeOffset = it->second;

        if (!SyncTreeOffset)
            return 0;

        return FrameWork::Memory::ReadMemory<uint64_t>(SyncTreeOffset + FrameWork::Memory::ReadMemory<int>(SyncTreeOffset + 3) + 7);
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
        uint64_t FragInstNMGta = FrameWork::Memory::ReadMemory<uint64_t>(Ped.StaticInfo.Ped + Offsets::FragInsNmGTA);
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

        return Ped.Cordinates;
    }

    CPed* FivemSDK::GetAimingEntity()
    {
        return (CPed*)FrameWork::Memory::ReadMemory<uint64_t>(PlayerAimingAt);
    }

    bool FivemSDK::IsPlayerAiming()
    {
        if (!AimingFlagValid)
            return true;

        return FrameWork::Memory::ReadMemory<bool>(PlayerAimingAt);
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
            WorldToScreen(GetBonePosVec3(Ped, SKEL_Neck_1)),
            WorldToScreen(GetBonePosVec3(Ped, SKEL_Spine3))
        };

        ImVec2 Best = ImVec2(0, 0);
        float BestDistance = FLT_MAX;

        for (const ImVec2& Bone : Bones)
        {
            if (!IsOnScreen(Bone))
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

    bool FivemSDK::FindClosestEntity(float Fov, int MaxDistance, bool VisibleCheck, bool NPC, int PriorityMode, Entity* Output)
    {
        Entity Closest;
        float ClosestWorldDistance = FLT_MAX;
        float ClosestScreenDistance = FLT_MAX;
        float BestHealth = FLT_MAX;

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

            if (!Current.StaticInfo.VisibleCheck && VisibleCheck)
                continue;

            float WorldDistance = Current.Cordinates.DistTo(LocalPos);
            if (WorldDistance > MaxDistance)
                continue;

            ImVec2 ClosestBone = GetClosestHitBox(Current);
            if (ClosestBone.x == 0 && ClosestBone.y == 0)
                continue;

            float ScreenDistance = ClosestBone.DistTo(Center);

            if (ScreenDistance > Fov)
                continue;

            float CurrentHealth = 99999.f;
            if (Current.StaticInfo.Ped)
                CurrentHealth = Current.StaticInfo.Ped->GetHealth();

            switch (PriorityMode)
            {
            case 0:
                if (ScreenDistance < ClosestScreenDistance)
                {
                    ClosestScreenDistance = ScreenDistance;
                    Closest = Current;
                    Found = true;
                }
                break;

            case 1:
                if (WorldDistance < ClosestWorldDistance)
                {
                    ClosestWorldDistance = WorldDistance;
                    Closest = Current;
                    Found = true;
                }
                break;

            case 2:
                if (CurrentHealth > 0 && CurrentHealth < BestHealth)
                {
                    BestHealth = CurrentHealth;
                    Closest = Current;
                    Found = true;
                }
                break;
            }
        }

        if (Found)
            *Output = Closest;

        LockLists.unlock();
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

        Vector3D AimDir = (WorldPosition - CrosshairPosition).Normalized();
        if (AimDir.LengthSqr() < 1e-6f)
            return;

        Vector3D CurrentDir = ViewAngles.Normalized();
        if (CurrentDir.LengthSqr() < 1e-6f)
            CurrentDir = AimDir;

        static auto LastMove = std::chrono::steady_clock::now();
        const auto Now = std::chrono::steady_clock::now();
        float Dt = std::chrono::duration<float>(Now - LastMove).count();
        LastMove = Now;

        if (Dt <= 0.f || Dt > 0.1f)
            Dt = 0.008f;

        auto SmoothToBlend = [Dt](int smooth) -> float
        {
            if (smooth <= 0) return 1.f;
            if (smooth > 100) smooth = 100;

            const float t = static_cast<float>(smooth) / 100.f;
            const float rate = 60.f / (1.f + t * t * 80.f);

            float blend = 1.f - std::exp(-rate * Dt);
            if (blend > 1.f) blend = 1.f;
            if (blend < 0.f) blend = 0.f;
            return blend;
        };

        const float blendH = SmoothToBlend(SmoothHorizontal);
        const float blendV = SmoothToBlend(SmoothVertical);

        Vector3D FinalDir;
        FinalDir.x = CurrentDir.x + (AimDir.x - CurrentDir.x) * blendH;
        FinalDir.y = CurrentDir.y + (AimDir.y - CurrentDir.y) * blendH;
        FinalDir.z = CurrentDir.z + (AimDir.z - CurrentDir.z) * blendV;

        FinalDir = FinalDir.Normalized();

        if (FinalDir.LengthSqr() < 1e-6f || !FinalDir.IsValid())
            return;

        Vector3D ThirdPerson = FollowPedCamera->GetThirdpersonViewAngles();
        const float PitchDelta = ViewAngles.z - ThirdPerson.z;

        Vector3D ThirdPersonAngles = FinalDir;
        ThirdPersonAngles.z = FinalDir.z - PitchDelta;

        FollowPedCamera->SetThirdpersonViewAngles(ThirdPersonAngles);
        FollowPedCamera->SetViewAngles(FinalDir);
    }

    ImVec2 FivemSDK::WorldToScreen(Vector3D Pos)
    {
        if (!pViewPort)
        {
            pViewPort = FrameWork::Memory::ReadMemory<uint64_t>(ViewPort);
            if (!pViewPort)
                return ImVec2(0, 0);
        }

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

    std::string FivemSDK::GetPlayerName(uint64_t PeerAddress, int GameNetId)
    {
        struct NameCacheEntry {
            std::string Name;
            uint64_t    Tick;
            bool        IsReal;
        };
        static std::unordered_map<int, NameCacheEntry> g_NetToIdNames;

        auto it = g_NetToIdNames.find(GameNetId);
        if (it != g_NetToIdNames.end())
        {
            if (it->second.IsReal)
                return it->second.Name;
            if (GetTickCount64() - it->second.Tick < 3000)
                return it->second.Name;
        }

        if (PeerAddress <= 0x1000 && GameNetId == 0)
        {
            g_NetToIdNames[GameNetId] = { XorStr("Player ") + std::to_string(GameNetId), GetTickCount64(), false };
            return g_NetToIdNames[GameNetId].Name;
        }

        std::string Result = "";

        if (NetIdToNamesPtr > 0x1000)
        {
            uint64_t GlobalPtr = FrameWork::Memory::ReadMemory<uint64_t>(NetIdToNamesPtr);
            if (GlobalPtr > 0x1000)
            {
                uint64_t ListHead = FrameWork::Memory::ReadMemory<uint64_t>(GlobalPtr + 0x8);
                if (ListHead > 0x1000)
                {
                    uint64_t CurrentEntry = ListHead;
                    for (int i = 0; i < 0x400; i++)
                    {
                        if (!CurrentEntry || CurrentEntry >= 0xCCCCCCCCCCCCCC) break;

                        uint16_t EntryId = FrameWork::Memory::ReadMemory<uint16_t>(CurrentEntry + 0x10);
                        if (EntryId != 0 && EntryId != 0xFFFF)
                        {
                            uint64_t strBase = CurrentEntry + 0x18;
                            size_t   strLen  = FrameWork::Memory::ReadMemory<size_t>(strBase + 0x10);

                            char NameBuffer[64] = { 0 };

                            if (strLen > 0 && strLen < 63)
                            {
                                if (strLen <= 15)
                                {
                                    FrameWork::Memory::ReadProcessMemoryImpl(strBase, NameBuffer, strLen);
                                }
                                else
                                {
                                    uint64_t heapPtr = FrameWork::Memory::ReadMemory<uint64_t>(strBase);
                                    if (heapPtr > 0x1000)
                                        FrameWork::Memory::ReadProcessMemoryImpl(heapPtr, NameBuffer, strLen);
                                }
                                NameBuffer[strLen] = '\0';
                            }

                            std::string Name(NameBuffer);

                            Name.erase(std::remove_if(Name.begin(), Name.end(), [](unsigned char c) {
                                return !std::isprint(c);
                            }), Name.end());

                            if (!Name.empty())
                            {
                                auto existing = g_NetToIdNames.find((int)EntryId);
                                bool shouldUpdate = (existing == g_NetToIdNames.end())
                                    || !existing->second.IsReal
                                    || (GetTickCount64() - existing->second.Tick > 30000);

                                if (shouldUpdate)
                                    g_NetToIdNames[(int)EntryId] = { Name, GetTickCount64(), true };

                                if (EntryId == (uint16_t)GameNetId)
                                    Result = Name;
                            }
                        }

                        CurrentEntry = FrameWork::Memory::ReadMemory<uint64_t>(CurrentEntry);
                    }
                }
            }
        }

        if (Result.empty())
        {
            Result = XorStr("Player ") + std::to_string(GameNetId);
            g_NetToIdNames[GameNetId] = { Result, GetTickCount64(), false };
        }

        return Result;
    }

    bool FivemSDK::IsOnScreen(ImVec2 Pos)
    {
        if (Pos.x < 0.1f || Pos.y < 0.1f || Pos.x > ImGui::GetIO().DisplaySize.x || Pos.y > ImGui::GetIO().DisplaySize.y)
            return false;

        return true;
    }
}
