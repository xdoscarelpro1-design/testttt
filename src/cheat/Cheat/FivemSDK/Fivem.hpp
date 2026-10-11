#pragma once
#include <cstdint>

#include "Classes.hpp"
#include <mutex>
#include <unordered_map>
#include <vector>

#include <FrameWork/FrameWork.hpp>

namespace Cheat
{
    struct LocalPEDInfo
    {
        CPed* Ped;
        int iIndex;
        ImVec2 ScreenPos;
        Vector3D WorldPos;
    };

    struct VehicleInfo
    {
        CVehicle* Vehicle;
        std::string Name;
        uint64_t ModelInfo;
        int iIndex = -1;
    };

    struct PedStaticInfo
    {
        CPed* Ped;
        uint64_t crSkeletonData;
        std::unordered_map<unsigned int, unsigned> MaskToBoneId;
        std::string Name;

        int NetId;
        int iIndex;

        bool IsFriend = false;
        bool bIsLocalPlayer;
        bool bIsNPC;
        bool VisibleCheck;
        bool IsLookingAtYou;
    };

    struct Entity
    {
        PedStaticInfo StaticInfo;
        Vector3D Cordinates;
        bool Visible;
        ImVec2 HeadPos;
    };

    class FivemSDK
    {
    public:
        void Intialize();
        bool UpdateEntities();
        bool UpdateVehicles();

        uint64_t GetModuleBase() { return ModuleBase; };

        LocalPEDInfo GetLocalPlayerInfo() { return LocalPlayerInfo; }
        CCamGameplayDirector* GetCamGameplayDirector() { return pCamGameplayDirector; }
        std::vector<Entity> GetEntitiyList() { return EntityList; }
        std::vector<VehicleInfo> GetVehicleList() { return VehicleList; }
        uint64_t GetHandleBulletAddress() { return HandleBullet; }
        uint64_t GetBlipListAddress() { return BlipList; }

        uint64_t GetInvisibleAddress() { return Invisible; }
        uint64_t GetCanCombatRollAddress() { return CanCombatRoll; }
        uint64_t GetGameplayCamHolderAddress() { return GameplayCamHolder; }
        uint64_t GetGameplayCamTargetAddress() { return GameplayCamTarget; }

        CPed* GetAimingEntity();
        bool IsPlayerAiming();

        Vector3D GetBonePosVec3(Entity Ped, unsigned int Mask);
        bool GetPedBoneIndex(Entity Ped, unsigned int Mask, unsigned int& newIdx);

        ImVec2 GetClosestHitBox(Entity Ped);
        bool FindClosestEntity(float Fov, int MaxDistance, bool VisibleCheck, bool NPC, int PriorityMode, Entity* Output);

        void ProcessCameraMovement(Vector3D WorldPosition, int SmoothHorizontal, int SmoothVertical);
        void TeleportToObject(uintptr_t Object, uintptr_t Navigation, uintptr_t ModelInfo, Vector3D Position, Vector3D VisualPosition, bool Stop);

        void network_request_control_of_entity(uint64_t entity, uint64_t localplayer);
        uint8_t get_owner_id(uint64_t net_object);
        bool set_network_id_can_migrate(uint64_t net_object, bool toggle);
        bool set_network_id_exists_on_all_machines(uint64_t net_object, bool toggle);
        bool network_request_control_of_network_id(uint64_t net_object, uint64_t local_net_object);

        uint64_t SyncTreeGetter(std::string SyncTree);
        std::string GetPlayerName(uint64_t PeerAddress, int GameNetId);

        ImVec2 WorldToScreen(Vector3D Pos);
        bool IsOnScreen(ImVec2 Pos);

        int GetGameVersion() { return RealGameVersion ? RealGameVersion : GameVersion; }
        bool IsInitialized() { return bIsIntialized; }

        int  GetResolvedBuild()    { return ResolvedBuild; }
        bool IsWorldValid()        { return WorldValid; }
        bool IsReplayValid()       { return ReplayValid; }
        bool IsViewPortValid()     { return ViewPortValid; }
        bool IsCameraValid()       { return CameraValid; }
        bool IsHandleBulletValid() { return HandleBulletValid; }
        bool IsBlipListValid()     { return BlipListValid; }

        std::unordered_map<CPed*, PedStaticInfo> AllEntitesList;
        std::unordered_map<int, PedStaticInfo> FriendList;

    public:
        bool bIsIntialized = false;

        uint64_t World;
        uint64_t ReplayInterface;
        uint64_t Camera;
        uint64_t ViewPort;

        uint64_t Invisible;
        uint64_t CanCombatRoll;
        uint64_t NetSync;
        uint64_t NetIdToNamesPtr;
        uint64_t GameplayCamHolder;
        uint64_t GameplayCamTarget;
        uint64_t BlipList;

        uint64_t HandleBullet;
        uint64_t PlayerAimingAt;
        uint64_t OcclusionArray;

        uint64_t ModuleBase;
        uint64_t ModuleBaseSize;
        uint64_t CitizemPlayerNamesModule;

        int GameVersion;
        int RealGameVersion;

        int  ResolvedBuild = 0;
        bool WorldValid = false;
        bool ReplayValid = false;
        bool ViewPortValid = false;
        bool CameraValid = false;
        bool HandleBulletValid = false;
        bool BlipListValid = false;
        bool AimingFlagValid = false;

    public:
        CWorld* pWorld;
        CPed* pLocalPlayer;
        CReplayInterface* pReplayInterface;
        CPedInterface* pPedInterface;
        CCamGameplayDirector* pCamGameplayDirector;
        CVehicleInterface* pVehicleInterface;

        uint64_t pViewPort;

    public:
        std::mutex LockLists;
        std::mutex LockLists2;
        std::vector<Entity> EntityList;
        std::vector<VehicleInfo> VehicleList;
        LocalPEDInfo LocalPlayerInfo;
    };

    inline FivemSDK g_Fivem;
}
