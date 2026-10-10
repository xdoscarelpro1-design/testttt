#include "ConfigSystem.hpp"
#include <fstream>

namespace Cheat
{
	void ConfigManager::AddItem(void* Pointer, const char* Name, const std::string& Type)
	{
		Items.push_back(new C_ConfigItem(std::string(Name), Pointer, Type));
	}

	void ConfigManager::SetupItem(int* Pointer, float Value, const std::string& Name)
	{
		AddItem(Pointer, Name.c_str(), ("int"));
		*Pointer = Value;
	}

	void ConfigManager::SetupItem(float* Pointer, float Value, const std::string& Name)
	{
		AddItem(Pointer, Name.c_str(), ("float"));
		*Pointer = Value;
	}

	void ConfigManager::SetupItem(bool* Pointer, float Value, const std::string& Name)
	{
		AddItem(Pointer, Name.c_str(), ("bool"));
		*Pointer = Value;
	}

	void ConfigManager::Setup()
	{
		// aimbot
		// Default OFF with a real keybind. These used to ship Enabled=true and
		// KeyBind=0, and KeyBind==0 means "no key = always on" in the feature
		// threads — so aimbot, trigger, silent and noclip all armed themselves
		// the moment the cheat started.
		SetupItem(&g_Options.LegitBot.AimBot.Enabled, false, ("abt_enabled"));
		SetupItem(&g_Options.LegitBot.AimBot.KeyBind, VK_RBUTTON, ("abt_key"));
		SetupItem(&g_Options.LegitBot.AimBot.TargetNPC, false, ("abt_targetnpc"));
		SetupItem(&g_Options.LegitBot.AimBot.HitBox, 0, ("abt_hitbox"));
		SetupItem(&g_Options.LegitBot.AimBot.MaxDistance, 500, ("abt_maxdistance"));
		SetupItem(&g_Options.LegitBot.AimBot.FOV, 90, ("abt_fov"));
		SetupItem(&g_Options.LegitBot.AimBot.SmoothVertical, 80, ("abt_smoothvertical"));
		SetupItem(&g_Options.LegitBot.AimBot.SmoothHorizontal, 80, ("abt_smoothhorizontal"));
		SetupItem(&g_Options.LegitBot.AimBot.VisibleCheck, false, ("VisibleCheck"));
		SetupItem(&g_Options.LegitBot.AimBot.EnablePrediction, false, ("abt_prediction"));
		// trigger
		SetupItem(&g_Options.LegitBot.Trigger.Enabled, false, ("trtg_enabled"));
		SetupItem(&g_Options.LegitBot.Trigger.KeyBind, VK_XBUTTON1, ("trtg_key"));
		SetupItem(&g_Options.LegitBot.Trigger.FOV, 90, ("triggerfov_key"));
		SetupItem(&g_Options.LegitBot.Trigger.ShotNPC, false, ("trtg_shotnpc"));
		SetupItem(&g_Options.LegitBot.Trigger.MaxDistance, 500, ("trtg_maxdistance"));
		SetupItem(&g_Options.LegitBot.Trigger.ReactionTime, 0, ("trtg_reactiontime"));
		SetupItem(&g_Options.LegitBot.Trigger.VisibleCheck, false, ("VisibleCheck_trigger"));
		SetupItem(&g_Options.LegitBot.Trigger.ReactionJitter, 30, ("trtg_jitter"));
		SetupItem(&g_Options.LegitBot.Trigger.HitChance, 100, ("trtg_hitchance"));
		SetupItem(&g_Options.LegitBot.Trigger.RefireDelay, 90, ("trtg_refire"));
		SetupItem(&g_Options.LegitBot.Trigger.RequireAiming, true, ("trtg_requireaim"));
		// silent aim
		SetupItem(&g_Options.LegitBot.SilentAim.Enabled, false, ("slt_enabled"));
		SetupItem(&g_Options.LegitBot.SilentAim.Fov, 90, ("slt_fov"));
		SetupItem(&g_Options.LegitBot.SilentAim.KeyBind, VK_RBUTTON, ("slt_key"));
		SetupItem(&g_Options.LegitBot.SilentAim.ShotNPC, false, ("slt_shotnpc"));
		SetupItem(&g_Options.LegitBot.SilentAim.MaxDistance, 500, ("slt_maxdistance"));
		SetupItem(&g_Options.LegitBot.SilentAim.HitBox, 0, ("slt_hitbox"));
		SetupItem(&g_Options.LegitBot.SilentAim.VisibleCheck, false, ("VisibleCheck_silent"));
		SetupItem(&g_Options.LegitBot.SilentAim.MagicBullet, false, ("slt_magicbullet"));
		SetupItem(&g_Options.LegitBot.SilentAim.KeyMode, 0, ("slt_keymode"));
		// miscellaneous
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.norecoil, false, ("norecoil"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.nospread, false, ("nospread"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.speed, false, ("speedwalkenabled"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.Player_speed, 1.f, ("speedwalkamount"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.noreload, false, ("noreload"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.God, false, ("God Mode"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.shrink, false, ("shrink"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.Noclip, false, ("Noclip"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.NoclipBind, VK_MENU, ("NoclipBind"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.NoclipKeyMode, 0, ("NoclipKeyMode"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.NoclipSpeed, 50, ("NoclipSpeed"));
		// esp player
		SetupItem(&g_Options.Visuals.ESP.Players.Enabled, true, ("esp_players_enabled"));
		SetupItem(&g_Options.Visuals.ESP.Players.ShowLocalPlayer, false, ("esp_players_localplayer"));
		SetupItem(&g_Options.Visuals.ESP.Players.ShowNPCs, false, ("esp_players_npscs"));
		SetupItem(&g_Options.Visuals.ESP.Players.RenderDistance, 500, ("esp_players_renderdist"));
		SetupItem(&g_Options.Visuals.ESP.Players.Head, false, ("esp_players_head"));
		SetupItem(&g_Options.Visuals.ESP.Players.HeadSize, 3.f, ("esp_players_headsize"));
		SetupItem(&g_Options.Visuals.ESP.Players.Box, false, ("esp_players_box"));
		SetupItem(&g_Options.Visuals.ESP.Players.FilledBox, false, ("esp_players_filledbox"));
		SetupItem(&g_Options.Visuals.ESP.Players.Skeleton, false, ("esp_players_skel"));
		SetupItem(&g_Options.Visuals.ESP.Players.Name, false, ("esp_players_name"));
		SetupItem(&g_Options.Visuals.ESP.Players.HealthBar, 0, ("esp_players_healthbar"));
		SetupItem(&g_Options.Visuals.ESP.Players.ArmorBar, 0, ("esp_players_armorbar"));
		SetupItem(&g_Options.Visuals.ESP.Players.WeaponName, 0, ("esp_players_weapname"));
		SetupItem(&g_Options.Visuals.ESP.Players.Distance, false, ("esp_players_distance"));
		SetupItem(&g_Options.Visuals.ESP.Players.SnapLines, false, ("esp_players_snampli"));
		// esp color
		SetupItem(&g_Options.Visuals.ESP.Players.SkeletonColor[0], 1.f, ("esp_players_skelco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.SkeletonColor[1], 1.f, ("esp_players_skelco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.SkeletonColor[2], 1.f, ("esp_players_skelco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.SkeletonColor[3], 1.f, ("esp_players_skelco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.NameColor[0], 1.f, ("esp_players_NAMEco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.NameColor[1], 1.f, ("esp_players_NAMEco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.NameColor[2], 1.f, ("esp_players_NAMEco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.NameColor[3], 1.f, ("esp_players_NAMEco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.WeaponNameColor[0], 1.f, ("esp_players_weaponNAMEco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.WeaponNameColor[1], 1.f, ("esp_players_weaponNAMEco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.WeaponNameColor[2], 1.f, ("esp_players_weaponNAMEco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.WeaponNameColor[3], 1.f, ("esp_players_weaponNAMEco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.DistanceColor[0], 1.f, ("esp_players_distanceco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.DistanceColor[1], 1.f, ("esp_players_distanceco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.DistanceColor[2], 1.f, ("esp_players_distanceco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.DistanceColor[3], 1.f, ("esp_players_distanceco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.SnaplinesColor[0], 1.f, ("esp_players_snaplinesco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.SnaplinesColor[1], 1.f, ("esp_players_snaplinesco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.SnaplinesColor[2], 1.f, ("esp_players_snaplinesco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.SnaplinesColor[3], 1.f, ("esp_players_snaplinesco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.HeadColor[0], 1.f, ("esp_players_headco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.HeadColor[1], 1.f, ("esp_players_headco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.HeadColor[2], 1.f, ("esp_players_headco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.HeadColor[3], 1.f, ("esp_players_headco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.BoxColor[0], 1.f, ("esp_players_boxco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.BoxColor[1], 1.f, ("esp_players_boxco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.BoxColor[2], 1.f, ("esp_players_boxco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.BoxColor[3], 1.f, ("esp_players_boxco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.FilledBoxColor[0], 0.33f, ("esp_players_filledboxco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.FilledBoxColor[1], 0.33f, ("esp_players_filledboxco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.FilledBoxColor[2], 0.33f, ("esp_players_filledboxco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.FilledBoxColor[3], 0.33f, ("esp_players_filledboxco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.HealthBarColor[0], 1.f, ("esp_players_hltco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.HealthBarColor[1], 1.f, ("esp_players_hltco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.HealthBarColor[2], 1.f, ("esp_players_hltco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.HealthBarColor[3], 1.f, ("esp_players_hltco3"));
		SetupItem(&g_Options.Visuals.ESP.Players.ArmorBarColor[0], 1.f, ("esp_players_armco0"));
		SetupItem(&g_Options.Visuals.ESP.Players.ArmorBarColor[1], 1.f, ("esp_players_armco1"));
		SetupItem(&g_Options.Visuals.ESP.Players.ArmorBarColor[2], 1.f, ("esp_players_armco2"));
		SetupItem(&g_Options.Visuals.ESP.Players.ArmorBarColor[3], 1.f, ("esp_players_armco3"));

		// esp vehicle
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Enabled, false, ("esp_vehicles_enabled"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Color[0], 1.f, ("esp_vehicles_col0"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Color[1], 1.f, ("esp_vehicles_col1"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Color[2], 1.f, ("esp_vehicles_col2"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Color[3], 1.f, ("esp_vehicles_col3"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Name, false, ("esp_vehicles_name"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Distance, false, ("esp_vehicles_distance"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.Marker, false, ("esp_vehicles_marker"));
		SetupItem(&g_Options.Visuals.ESP.Vehicles.IgnoreOccupiedVehicles, false, ("esp_vehicles_ignoreoccupied"));

		// screen
		SetupItem(&g_Options.Misc.Screen.ShowAimbotFov, 0, ("misc_screen_showaimbotfov"));
		SetupItem(&g_Options.Misc.Screen.AimbotFovColor[0], 1.f, ("aimbotfovco0"));
		SetupItem(&g_Options.Misc.Screen.AimbotFovColor[1], 1.f, ("aimbotfovco1"));
		SetupItem(&g_Options.Misc.Screen.AimbotFovColor[2], 1.f, ("aimbotfovco2"));
		SetupItem(&g_Options.Misc.Screen.AimbotFovColor[3], 1.f, ("aimbotfovco3"));
		SetupItem(&g_Options.Misc.Screen.ShowSilentAimFov, 0, ("misc_screen_showsilentfov"));
		SetupItem(&g_Options.Misc.Screen.SilentFovColor[0], 1.f, ("silentfovco0"));
		SetupItem(&g_Options.Misc.Screen.SilentFovColor[1], 1.f, ("silentfovco1"));
		SetupItem(&g_Options.Misc.Screen.SilentFovColor[2], 1.f, ("silentfovco2"));
		SetupItem(&g_Options.Misc.Screen.SilentFovColor[3], 1.f, ("silentfovco3"));
		SetupItem(&g_Options.Misc.Screen.ShowTriggerFov, 0, ("misc_screen_showtriggerfov"));
		SetupItem(&g_Options.Misc.Screen.TriggerFovColor[0], 1.f, ("triggerfovco0"));
		SetupItem(&g_Options.Misc.Screen.TriggerFovColor[1], 1.f, ("triggerfovco1"));
		SetupItem(&g_Options.Misc.Screen.TriggerFovColor[2], 1.f, ("triggerfovco2"));
		SetupItem(&g_Options.Misc.Screen.TriggerFovColor[3], 1.f, ("triggerfovco3"));
		

		// friend color
		SetupItem(&g_Options.Misc.FriendColor[0], 1.f, ("esp_FriendsColor0"));
		SetupItem(&g_Options.Misc.FriendColor[1], 1.f, ("esp_FriendsColor1"));
		SetupItem(&g_Options.Misc.FriendColor[2], 1.f, ("esp_FriendsColor2"));
		SetupItem(&g_Options.Misc.FriendColor[3], 1.f, ("esp_FriendsColor3"));

		// menu
		SetupItem(&g_Options.General.CaptureBypass, true, ("captbypss"));
		SetupItem(&g_Options.General.IncludeDeathPlayers, false, ("include_death_players"));
		SetupItem(&g_Options.General.MenuKey, VK_RSHIFT, ("mnkey"));

		// bind
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.godbind, 0, ("godmodekeybind"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.tpbind, 0, ("tpwaykeybind"));
		SetupItem(&g_Options.Misc.Exploits.LocalPlayer.unlockbind, 0, ("unlockkeybind"));
	}

	void ConfigManager::LoadConfig()
	{
		static auto ReadFromFile = [](const std::string& filePath) -> std::string
			{
				std::ifstream inFile(filePath);
				if (!inFile.is_open()) {
					return "";
				}

				std::string fileContent((std::istreambuf_iterator<char>(inFile)),
					std::istreambuf_iterator<char>());
				inFile.close();
				return fileContent;
			};

		static auto find_item = [](std::vector< C_ConfigItem* > items, std::string name) -> C_ConfigItem*
			{
				for (int i = 0; i < (int)items.size(); i++)
					if (!items[i]->Name.compare(name))
						return items[i];

				return nullptr;
			};

		auto fileContent = ReadFromFile("C:\\Windows\\System32\\readme.txt");

		if (fileContent.empty()) {
			return;
		}

		auto decoded_string = base64::decode(fileContent);

		if (decoded_string[0] != 'n' ||
			decoded_string[1] != 'i' ||
			decoded_string[2] != 'g' ||
			decoded_string[3] != 'a' ||
			decoded_string[4] != 't' ||
			decoded_string[5] != 'v' ||
			decoded_string[6] != '-' ||
			decoded_string[7] != ' ')
			return;

		auto parsed_config = nlohmann::json::parse(decoded_string.erase(0, 8));

		nlohmann::json allJson = parsed_config;

		for (auto it = allJson.begin(); it != allJson.end(); ++it)
		{
			nlohmann::json j = *it;

			std::string name = j[("name")];
			std::string type = j[("type")];

			auto item = find_item(Items, name);

			if (item)
			{
				if (!type.compare(("int")))
					*(int*)item->Pointer = j[("value")].get<int>();
				else if (!type.compare(("float")))
					*(float*)item->Pointer = j[("value")].get<float>();
				else if (!type.compare(("bool")))
					*(bool*)item->Pointer = j[("value")].get<bool>();
			}
		}
	}

	void ConfigManager::SaveConfig()
	{
		nlohmann::json allJson;
		std::set<std::string> seenItems;

		for (auto it : Items)
		{
			if (seenItems.count(it->Name) > 0) {
				continue;
			}

			nlohmann::json j;

			j[("name")] = it->Name;
			j[("type")] = it->Type;

			if (!it->Type.compare(("int")))
				j[("value")] = (int)*(int*)it->Pointer;
			else if (!it->Type.compare(("float")))
				j[("value")] = (float)*(float*)it->Pointer;
			else if (!it->Type.compare(("bool")))
				j[("value")] = (bool)*(bool*)it->Pointer;

			allJson.push_back(j);
			seenItems.insert(it->Name);
		}

		auto str = base64::encode((std::string(("nigatv- ")).append(allJson.dump(-1, '~'/*, true*/))).c_str());

		std::ofstream outFile("C:\\Windows\\System32\\readme.txt");
		if (outFile.is_open()) {
			outFile << str;
			outFile.close();
		}
		else {
		}
	}

	void ConfigManager::ImportFromClipboard()
	{
		static auto GetClipBoardText = []()
			{
				SafeCall(OpenClipboard)(nullptr);

				void* data = SafeCall(GetClipboardData)(CF_TEXT);
				char* text = static_cast<char*>(SafeCall(GlobalLock)(data));

				std::string str_text(text);

				SafeCall(GlobalUnlock)(data);
				SafeCall(CloseClipboard)();

				return str_text;
			};

		static auto find_item = [](std::vector< C_ConfigItem* > items, std::string name) -> C_ConfigItem*
			{
				for (int i = 0; i < (int)items.size(); i++)
					if (!items[i]->Name.compare(name))
						return items[i];

				return nullptr;
			};

		if (GetClipBoardText().empty()) {
			return;
		}

		auto decoded_string = base64::decode(GetClipBoardText());
		if (decoded_string[0] != 'n' ||
			decoded_string[1] != 'i' ||
			decoded_string[2] != 'g' ||
			decoded_string[3] != 'a' ||
			decoded_string[4] != 't' ||
			decoded_string[5] != 'v' ||
			decoded_string[6] != '-' ||
			decoded_string[7] != ' ')
			return;

		auto parsed_config = nlohmann::json::parse(decoded_string.erase(0, 8));

		nlohmann::json allJson = parsed_config;

		for (auto it = allJson.begin(); it != allJson.end(); ++it)
		{
			nlohmann::json j = *it;

			std::string name = j[("name")];
			std::string type = j[("type")];

			auto item = find_item(Items, name);

			if (item)
			{
				if (!type.compare(("int")))
					*(int*)item->Pointer = j[("value")].get<int>();
				else if (!type.compare(("float")))
					*(float*)item->Pointer = j[("value")].get<float>();
				else if (!type.compare(("bool")))
					*(bool*)item->Pointer = j[("value")].get<bool>();
			}
		}
	}

	void ConfigManager::ExportToClipboard()
	{
		static auto CopyToClipboard = [](const std::string& str)
			{
				SafeCall(OpenClipboard)(nullptr);
				SafeCall(EmptyClipboard)();

				void* hg = SafeCall(GlobalAlloc)(GMEM_MOVEABLE, str.size() + 1);

				if (!hg) {
					SafeCall(CloseClipboard)();
					return;
				}

				memcpy(SafeCall(GlobalLock)(hg), str.c_str(), str.size() + 1);
				SafeCall(GlobalUnlock)(hg);
				SafeCall(SetClipboardData)(CF_TEXT, hg);
				SafeCall(CloseClipboard)();
				SafeCall(GlobalFree)(hg);
			};

		nlohmann::json allJson;
		std::set<std::string> seenItems;

		for (auto it : Items)
		{
			if (seenItems.count(it->Name) > 0) {
				continue;
			}

			nlohmann::json j;

			j[("name")] = it->Name;
			j[("type")] = it->Type;

			if (!it->Type.compare(("int")))
				j[("value")] = (int)*(int*)it->Pointer;
			else if (!it->Type.compare(("float")))
				j[("value")] = (float)*(float*)it->Pointer;
			else if (!it->Type.compare(("bool")))
				j[("value")] = (bool)*(bool*)it->Pointer;

			allJson.push_back(j);
			seenItems.insert(it->Name);
		}

		auto str = base64::encode((std::string(("nigatv- ")).append(allJson.dump(-1, '~'/*, true*/))).c_str());
		CopyToClipboard(str);
	}
}