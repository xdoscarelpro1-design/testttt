#include "Bypass.hpp"
#include "../../Options.hpp"
#include "../LegitBot/SilentAim.hpp"
// KernelCloak removed — EFI bootkit handles kernel ops
#include <FrameWork/FrameWork.hpp>

#include <TlHelp32.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <winioctl.h>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <chrono>
#include <winternl.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "ntdll.lib")

namespace fs = std::filesystem;

// Defined here, used by GetSelfPath/CacheRealPath inside the namespace
static std::wstring s_CachedRealPath;

// Undocumented NT structures for PEB manipulation
typedef NTSTATUS(NTAPI* fnNtSetInformationProcess)(HANDLE, ULONG, PVOID, ULONG);

namespace Cheat
{
    namespace Bypass
    {
        static bool s_TaskbarHidden = false;
        static bool s_RemoteBlocked = false;
        static bool s_StreamProof = false;
        static bool s_MemoryProtected = false;
        static bool s_ProcessSpoofed = false;

        // ═══════════════════════════════════════════════════════════════════
        // HELPERS
        // ═══════════════════════════════════════════════════════════════════

        static std::vector<std::wstring> GetSSToolProcessNames()
        {
            return {
                // Ocean
                L"ocean.exe", L"oceanss.exe", L"ocean_ss.exe", L"OceanAC.exe",
                L"OceanClient.exe", L"ocean-client.exe",
                // Echo
                L"echo.exe", L"echoss.exe", L"echo_ss.exe", L"EchoAC.exe",
                L"EchoRP.exe", L"EchoClient.exe", L"echo-rp.exe",
                // Storm
                L"storm.exe", L"stormss.exe", L"storm_ss.exe", L"StormAC.exe",
                L"StormClient.exe",
                // Napse
                L"napse.exe", L"napseac.exe", L"napse_ac.exe", L"NapseAC.exe",
                L"NapseClient.exe", L"napse-ac.exe",
                // Detect.ac
                L"detect.exe", L"detectac.exe", L"detect_ac.exe", L"DetectAC.exe",
                L"DetectClient.exe", L"detect-ac.exe",
                // Generic SS tools
                L"screenshare.exe", L"ss_tool.exe", L"sstool.exe",
                L"SSHelper.exe", L"SSClient.exe",
                // Siren / Chocolate (common in Hispanic RP servers)
                L"siren.exe", L"SirenAC.exe", L"chocolate.exe", L"ChocolateAC.exe",
                // Analysis tools that reviewers use
                L"processhacker.exe", L"procexp.exe", L"procexp64.exe",
                L"SystemInformer.exe", L"si.exe",
                L"autoruns.exe", L"autorunsc.exe",
                L"tcpview.exe", L"procmon.exe", L"Procmon64.exe",
                L"strings.exe", L"strings64.exe",
                L"sigcheck.exe", L"sigcheck64.exe",
                L"handle.exe", L"handle64.exe",
                L"listdlls.exe", L"Listdlls64.exe",
                L"vmmap.exe", L"rammap.exe",
                // NirSoft forensic tools — used in manual SS kits
                L"ExecutedProgramList.exe", L"WinPrefetchView.exe",
                L"LastActivityView.exe", L"UserAssistView.exe",
                L"BrowsingHistoryView.exe", L"BrowserDownloadsView.exe",
                L"OpenFilesView.exe", L"ShellBagsView.exe",
                L"TaskSchedulerView.exe", L"RegScanner.exe",
                L"USBDeview.exe", L"LoadedDllsView.exe",
                L"ProcessActivity.exe", L"UninstallView.exe",
                L"RecentFilesView.exe", L"DiskDrill.exe",
                // Everything (voidtools) — indexes filenames including our exe
                L"Everything.exe", L"Everything64.exe",
                // Detect FM + WinLiveInfo
                L"detectfm.exe", L"DetectFM.exe", L"detect_fm.exe",
                L"winliveinfo.exe", L"WinLiveInfo.exe",
                // Recovery tools
                L"Recuva.exe", L"recuva.exe",
                L"EaseUS Data Recovery Wizard.exe",
                L"DiskDrill.exe", L"diskdrill.exe",
                L"JournalTrace.exe", L"journaltrace.exe",
                // Luyten (Java decompiler — sometimes used to inspect .jar based tools)
                L"luyten.exe",
            };
        }

        static std::vector<std::wstring> GetRemoteAccessProcessNames()
        {
            return {
                L"AnyDesk.exe", L"anydesk.exe",
                L"TeamViewer.exe", L"TeamViewer_Service.exe",
                L"tv_w32.exe", L"tv_x64.exe",
                L"rustdesk.exe", L"RustDesk.exe",
                L"parsec.exe", L"parsecd.exe",
                L"SupremoService.exe", L"Supremo.exe",
                L"UltraVnc.exe", L"winvnc.exe",
            };
        }

        static bool KillProcessByName(const std::wstring& name)
        {
            bool killed = false;
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap == INVALID_HANDLE_VALUE) return false;

            PROCESSENTRY32W pe = { sizeof(pe) };
            if (Process32FirstW(snap, &pe))
            {
                do
                {
                    std::wstring current(pe.szExeFile);
                    std::wstring lower1 = current, lower2 = name;
                    std::transform(lower1.begin(), lower1.end(), lower1.begin(), ::towlower);
                    std::transform(lower2.begin(), lower2.end(), lower2.begin(), ::towlower);

                    if (lower1 == lower2)
                    {
                        HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                        if (proc)
                        {
                            TerminateProcess(proc, 0);
                            CloseHandle(proc);
                            killed = true;
                        }
                    }
                } while (Process32NextW(snap, &pe));
            }
            CloseHandle(snap);
            return killed;
        }

        static void SecureDeleteFile(const fs::path& path)
        {
            if (!fs::exists(path)) return;
            try
            {
                auto size = fs::file_size(path);
                if (size > 0 && size < 50 * 1024 * 1024)
                {
                    // Three-pass overwrite: zeros, ones, random
                    std::ofstream f(path, std::ios::binary | std::ios::trunc);
                    std::vector<char> buf(min((uintmax_t)4096, size));

                    // Pass 1: zeros
                    memset(buf.data(), 0x00, buf.size());
                    uintmax_t written = 0;
                    while (written < size) {
                        auto chunk = min((uintmax_t)buf.size(), size - written);
                        f.write(buf.data(), chunk);
                        written += chunk;
                    }

                    // Pass 2: 0xFF
                    f.seekp(0);
                    memset(buf.data(), 0xFF, buf.size());
                    written = 0;
                    while (written < size) {
                        auto chunk = min((uintmax_t)buf.size(), size - written);
                        f.write(buf.data(), chunk);
                        written += chunk;
                    }

                    // Pass 3: random
                    f.seekp(0);
                    for (size_t i = 0; i < buf.size(); i++)
                        buf[i] = (char)(rand() & 0xFF);
                    written = 0;
                    while (written < size) {
                        auto chunk = min((uintmax_t)buf.size(), size - written);
                        f.write(buf.data(), chunk);
                        written += chunk;
                    }
                    f.close();
                }
                fs::remove(path);
            }
            catch (...) {}
        }

        static void RecursiveDeleteDir(const fs::path& dir)
        {
            if (!fs::exists(dir)) return;
            try
            {
                for (auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied))
                {
                    if (entry.is_regular_file())
                        SecureDeleteFile(entry.path());
                }
                fs::remove_all(dir);
            }
            catch (...) {}
        }

        // Selective registry cleaner — only deletes values whose name or data
        // contains our loader path/name. Leaves everything else untouched.
        // This avoids wiping the entire key (which looks extremely suspicious
        // to a reviewer — empty RecentDocs = obvious cleanup).
        static fs::path GetSelfPath(); // forward decl — defined below after CacheRealPath

        static void CleanRegKeyLoaderOnly(HKEY root, const std::wstring& subkey)
        {
            HKEY hKey = NULL;
            if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
                return;

            std::wstring selfName = GetSelfPath().stem().wstring();
            std::transform(selfName.begin(), selfName.end(), selfName.begin(), ::towlower);
            std::wstring selfPath = GetSelfPath().wstring();
            std::transform(selfPath.begin(), selfPath.end(), selfPath.begin(), ::towlower);
            std::wstring selfFilename = GetSelfPath().filename().wstring();
            std::transform(selfFilename.begin(), selfFilename.end(), selfFilename.begin(), ::towlower);

            std::vector<std::wstring> keywords = { selfName, selfPath, selfFilename, L"loader" };

            DWORD valueCount = 0;
            RegQueryInfoKeyW(hKey, NULL, NULL, NULL, NULL, NULL, NULL, &valueCount, NULL, NULL, NULL, NULL);

            std::vector<std::wstring> toDelete;
            for (DWORD i = 0; i < valueCount; i++)
            {
                wchar_t valueName[1024] = {};
                wchar_t valueData[8192] = {};
                DWORD nameSize = 1024;
                DWORD dataSize = sizeof(valueData);
                DWORD type = 0;
                if (RegEnumValueW(hKey, i, valueName, &nameSize, NULL, &type, (BYTE*)valueData, &dataSize) != ERROR_SUCCESS)
                    continue;

                std::wstring nameLower(valueName);
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);

                bool match = false;
                for (auto& kw : keywords)
                {
                    if (nameLower.find(kw) != std::wstring::npos)
                    { match = true; break; }
                }

                if (!match && (type == REG_SZ || type == REG_EXPAND_SZ || type == REG_MULTI_SZ))
                {
                    std::wstring dataStr(valueData);
                    auto nullPos = dataStr.find(L'\0');
                    if (nullPos != std::wstring::npos)
                        dataStr = dataStr.substr(0, nullPos);
                    std::transform(dataStr.begin(), dataStr.end(), dataStr.begin(), ::towlower);
                    for (auto& kw : keywords)
                    {
                        if (dataStr.find(kw) != std::wstring::npos)
                        { match = true; break; }
                    }
                }

                if (!match && type == REG_BINARY)
                {
                    // Binary scan: search for our loader name as wide chars
                    for (size_t j = 0; j + selfName.size() * 2 <= dataSize; j++)
                    {
                        bool found = true;
                        for (size_t k = 0; k < selfName.size() && found; k++)
                        {
                            wchar_t c;
                            memcpy(&c, (BYTE*)valueData + j + k * 2, 2);
                            if (c >= L'A' && c <= L'Z') c += 32;
                            if (c != selfName[k]) found = false;
                        }
                        if (found) { match = true; break; }
                    }
                    if (!match)
                    {
                        for (size_t j = 0; j + selfPath.size() * 2 <= dataSize; j++)
                        {
                            bool found = true;
                            for (size_t k = 0; k < selfPath.size() && found; k++)
                            {
                                wchar_t c;
                                memcpy(&c, (BYTE*)valueData + j + k * 2, 2);
                                if (c >= L'A' && c <= L'Z') c += 32;
                                if (c != selfPath[k]) found = false;
                            }
                            if (found) { match = true; break; }
                        }
                    }
                }

                if (match)
                    toDelete.push_back(valueName);
            }

            for (auto& v : toDelete)
                RegDeleteValueW(hKey, v.c_str());

            RegCloseKey(hKey);
        }

        // Cached at first call — BEFORE SpoofProcessName overwrites the PEB.
        // GetModuleFileNameW reads from the PEB, so after spoof it returns
        // "C:\Windows\System32\conhost.exe" instead of our real path.
        static fs::path GetSelfPath()
        {
            // Return the path cached at startup — only source of truth after spoof
            if (!s_CachedRealPath.empty())
                return fs::path(s_CachedRealPath);

            // Should not reach here in normal flow — populate on demand as fallback
            wchar_t buf[MAX_PATH] = {};
            GetModuleFileNameW(NULL, buf, MAX_PATH);
            s_CachedRealPath = buf;
            return fs::path(s_CachedRealPath);
        }

        void CacheRealPath()
        {
            // Call this as the VERY FIRST thing in main() before any spoof
            wchar_t buf[MAX_PATH] = {};
            GetModuleFileNameW(NULL, buf, MAX_PATH);
            s_CachedRealPath = buf;
        }

        // Forward decls — defined in the SelfDestruct section below
        static void CleanUSNJournal(const fs::path& filePath);
        static void CleanPendingRenames(const std::wstring& selfPathLower);

        // ROT13 encode/decode for UserAssist registry keys
        static std::wstring Rot13(const std::wstring& input)
        {
            std::wstring result = input;
            for (auto& c : result)
            {
                if (c >= L'A' && c <= L'Z')
                    c = L'A' + ((c - L'A' + 13) % 26);
                else if (c >= L'a' && c <= L'z')
                    c = L'a' + ((c - L'a' + 13) % 26);
            }
            return result;
        }

        static void RecursiveDeleteRegKey(HKEY root, const std::wstring& subkey)
        {
            HKEY hKey = NULL;
            if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
                return;

            // Enumerate and delete all values
            DWORD valueCount = 0;
            RegQueryInfoKeyW(hKey, NULL, NULL, NULL, NULL, NULL, NULL, &valueCount, NULL, NULL, NULL, NULL);

            std::vector<std::wstring> valuesToDelete;
            for (DWORD i = 0; i < valueCount; i++)
            {
                wchar_t valueName[512] = {};
                DWORD nameSize = 512;
                if (RegEnumValueW(hKey, i, valueName, &nameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
                {
                    valuesToDelete.push_back(valueName);
                }
            }

            for (auto& val : valuesToDelete)
                RegDeleteValueW(hKey, val.c_str());

            RegCloseKey(hKey);
        }

        // ═══════════════════════════════════════════════════════════════════
        // KILL SS TOOLS
        // ═══════════════════════════════════════════════════════════════════

        void KillSSTools()
        {
            auto tools = GetSSToolProcessNames();
            for (auto& name : tools)
                KillProcessByName(name);

            // Kill windows by known class names
            const wchar_t* classNames[] = {
                L"OceanSSClass", L"EchoSSClass", L"StormSSClass",
                L"NapseACClass", L"DetectACClass", L"SirenACClass",
                L"ChocolateACClass", L"YOURMOTHER_SS",
            };
            for (auto cls : classNames)
            {
                HWND wnd = FindWindowW(cls, NULL);
                while (wnd)
                {
                    DWORD pid = 0;
                    GetWindowThreadProcessId(wnd, &pid);
                    if (pid)
                    {
                        HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                        if (proc) { TerminateProcess(proc, 0); CloseHandle(proc); }
                    }
                    wnd = FindWindowW(cls, NULL);
                }
            }

            // Kill by window titles containing SS-related keywords
            const wchar_t* titleKeywords[] = {
                L"ScreenShare", L"Screen Share", L"SS Tool",
                L"Ocean", L"Echo AC", L"Storm AC", L"Napse",
                L"Detect AC", L"AntiCheat Scanner",
            };

            for (auto keyword : titleKeywords)
            {
                HWND wnd = FindWindowW(NULL, keyword);
                if (wnd)
                {
                    DWORD pid = 0;
                    GetWindowThreadProcessId(wnd, &pid);
                    if (pid && pid != GetCurrentProcessId())
                    {
                        HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                        if (proc) { TerminateProcess(proc, 0); CloseHandle(proc); }
                    }
                }
            }
        }

        // ═══════════════════════════════════════════════════════════════════
        // USERASSIST CLEANING (ROT13 DECODED)
        // ═══════════════════════════════════════════════════════════════════

        void CleanUserAssist()
        {
            // UserAssist stores execution history with ROT13 encoded paths
            // We need to enumerate all GUIDs under UserAssist and clean entries
            const wchar_t* uaBase = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist";

            HKEY hBase = NULL;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, uaBase, 0, KEY_ALL_ACCESS, &hBase) != ERROR_SUCCESS)
                return;

            // Enumerate GUID subkeys
            DWORD subkeyCount = 0;
            RegQueryInfoKeyW(hBase, NULL, NULL, NULL, &subkeyCount, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

            for (DWORD i = 0; i < subkeyCount; i++)
            {
                wchar_t guidName[256] = {};
                DWORD guidSize = 256;
                if (RegEnumKeyExW(hBase, i, guidName, &guidSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
                    continue;

                // Open the Count subkey where actual entries live
                std::wstring countPath = std::wstring(uaBase) + L"\\" + guidName + L"\\Count";
                HKEY hCount = NULL;
                if (RegOpenKeyExW(HKEY_CURRENT_USER, countPath.c_str(), 0, KEY_ALL_ACCESS, &hCount) != ERROR_SUCCESS)
                    continue;

                // Enumerate all values and check if they reference our exe
                DWORD valCount = 0;
                RegQueryInfoKeyW(hCount, NULL, NULL, NULL, NULL, NULL, NULL, &valCount, NULL, NULL, NULL, NULL);

                std::vector<std::wstring> toDelete;
                for (DWORD v = 0; v < valCount; v++)
                {
                    wchar_t valueName[1024] = {};
                    DWORD nameSize = 1024;
                    if (RegEnumValueW(hCount, v, valueName, &nameSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
                        continue;

                    // Decode the ROT13 value name to see what it actually references
                    std::wstring decoded = Rot13(std::wstring(valueName));
                    std::wstring decodedLower = decoded;
                    std::transform(decodedLower.begin(), decodedLower.end(), decodedLower.begin(), ::towlower);

                    // Check for our binary names
                    if (decodedLower.find(L"astralith") != std::wstring::npos ||
                        decodedLower.find(L"fivem-external") != std::wstring::npos ||
                        decodedLower.find(L"fivemexternal") != std::wstring::npos ||
                        decodedLower.find(L"cheat") != std::wstring::npos ||
                        decodedLower.find(L"bypass") != std::wstring::npos ||
                        decodedLower.find(L"hack") != std::wstring::npos ||
                        decodedLower.find(L"inject") != std::wstring::npos)
                    {
                        toDelete.push_back(valueName);
                    }

                    // Also check with the actual exe name
                    std::wstring selfName = GetSelfPath().stem().wstring();
                    std::transform(selfName.begin(), selfName.end(), selfName.begin(), ::towlower);
                    std::wstring selfNameRot13 = Rot13(selfName);
                    std::wstring valueNameLower(valueName);
                    std::transform(valueNameLower.begin(), valueNameLower.end(), valueNameLower.begin(), ::towlower);
                    if (valueNameLower.find(selfNameRot13) != std::wstring::npos)
                    {
                        toDelete.push_back(valueName);
                    }
                }

                for (auto& val : toDelete)
                    RegDeleteValueW(hCount, val.c_str());

                RegCloseKey(hCount);
            }
            RegCloseKey(hBase);
        }

        // ═══════════════════════════════════════════════════════════════════
        // SHELLBAGS CLEANING
        // ═══════════════════════════════════════════════════════════════════

        void CleanShellBags()
        {
            // ShellBags: BagMRU stores the folder navigation tree (which folders
            // were opened). Bags stores the VIEW SETTINGS (icon size, columns, etc).
            // We used to wipe BagMRU entirely but that's extremely suspicious —
            // a reviewer sees ALL your folder navigation history is gone.
            // The loader's folder being here is not suspicious; every user has
            // hundreds of folder entries. Leave BagMRU completely untouched.
        }

        // ═══════════════════════════════════════════════════════════════════
        // RECENT DOCUMENTS + JUMP LISTS
        // ═══════════════════════════════════════════════════════════════════

        void CleanRecentDocs()
        {
            // Ghost LNK scan — name-agnostic: parse every .lnk in Recent and delete
            // ones whose target no longer exists. This catches any exe name, not just
            // our hardcoded keywords. LNK binary format: target path starts at a known
            // offset but varies; easiest is to just shell-read it via IShellLink COM.
            wchar_t recentPath[MAX_PATH] = {};
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_RECENT, NULL, 0, recentPath)))
            {
                for (auto& entry : fs::directory_iterator(recentPath, fs::directory_options::skip_permission_denied))
                {
                    if (!entry.is_regular_file()) continue;
                    auto ext = entry.path().extension().wstring();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                    if (ext != L".lnk") continue;

                    // Quick binary scan for the target path — LNK stores the target
                    // as a null-terminated wide string at predictable offset ranges.
                    // Rather than full COM parsing, read the .lnk as binary and look
                    // for any path that no longer exists (ghost detection).
                    bool isGhost = false;
                    try
                    {
                        std::ifstream f(entry.path(), std::ios::binary);
                        if (f)
                        {
                            std::vector<char> buf((std::istreambuf_iterator<char>(f)),
                                                   std::istreambuf_iterator<char>());
                            // Search for wchar_t paths — walk the buffer looking for
                            // sequences that look like absolute paths (L"X:\")
                            for (size_t i = 0; i + 6 < buf.size(); i += 2)
                            {
                                wchar_t* wp = reinterpret_cast<wchar_t*>(&buf[i]);
                                if (wp[0] >= L'A' && wp[0] <= L'Z' &&
                                    wp[1] == L':' && wp[2] == L'\\')
                                {
                                    // Extract the path string
                                    std::wstring candidate;
                                    for (size_t j = 0; j < 260 && (i + j*2 + 2) < buf.size(); j++)
                                    {
                                        wchar_t c = *reinterpret_cast<wchar_t*>(&buf[i + j*2]);
                                        if (c == L'\0') break;
                                        if (c < 0x20 && c != 0) { candidate.clear(); break; }
                                        candidate += c;
                                    }
                                    if (candidate.size() > 3 && !fs::exists(candidate))
                                    {
                                        isGhost = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    catch (...) {}

                    if (isGhost)
                        SecureDeleteFile(entry.path());
                }

                // Taskbar pinned items — LNK files here also embed target paths
                wchar_t appdata[MAX_PATH] = {};
                SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata);
                fs::path tbPinned = fs::path(appdata) /
                    L"Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar";
                if (fs::exists(tbPinned))
                {
                    for (auto& entry : fs::directory_iterator(tbPinned, fs::directory_options::skip_permission_denied))
                    {
                        if (!entry.is_regular_file()) continue;
                        auto ext = entry.path().extension().wstring();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                        if (ext != L".lnk") continue;

                        bool isGhost2 = false;
                        try
                        {
                            std::ifstream f(entry.path(), std::ios::binary);
                            if (f)
                            {
                                std::vector<char> buf((std::istreambuf_iterator<char>(f)),
                                                       std::istreambuf_iterator<char>());
                                for (size_t i = 0; i + 6 < buf.size(); i += 2)
                                {
                                    wchar_t* wp = reinterpret_cast<wchar_t*>(&buf[i]);
                                    if (wp[0] >= L'A' && wp[0] <= L'Z' &&
                                        wp[1] == L':' && wp[2] == L'\\')
                                    {
                                        std::wstring candidate;
                                        for (size_t j = 0; j < 260 && (i + j*2 + 2) < buf.size(); j++)
                                        {
                                            wchar_t c = *reinterpret_cast<wchar_t*>(&buf[i + j*2]);
                                            if (c == L'\0') break;
                                            if (c < 0x20) { candidate.clear(); break; }
                                            candidate += c;
                                        }
                                        if (candidate.size() > 3 && !fs::exists(candidate))
                                        {
                                            isGhost2 = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        catch (...) {}

                        if (isGhost2)
                            SecureDeleteFile(entry.path());
                    }
                }

                // AutomaticDestinations (jump lists)
                fs::path jumpListDir = fs::path(recentPath) / L"AutomaticDestinations";
                if (fs::exists(jumpListDir))
                {
                    for (auto& entry : fs::directory_iterator(jumpListDir, fs::directory_options::skip_permission_denied))
                    {
                        try { fs::remove(entry.path()); } catch (...) {}
                    }
                }

                fs::path customJumpDir = fs::path(recentPath) / L"CustomDestinations";
                if (fs::exists(customJumpDir))
                {
                    for (auto& entry : fs::directory_iterator(customJumpDir, fs::directory_options::skip_permission_denied))
                    {
                        try { fs::remove(entry.path()); } catch (...) {}
                    }
                }
            }

            // MRU lists in registry — ONLY clean entries referencing our loader.
            // Wiping entire keys (RecentDocs, RunMRU, TypedPaths, etc.) is
            // extremely suspicious — a reviewer sees ALL your history gone.
            // Selective per-value deletion leaves the user's real activity intact.
            const wchar_t* mruPaths[] = {
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RecentDocs",
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RunMRU",
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\OpenSavePidlMRU",
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\LastVisitedPidlMRU",
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\LastVisitedMRU",
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\TypedPaths",
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\WordWheelQuery",
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Search\\RecentApps",
            };

            for (auto path : mruPaths)
                CleanRegKeyLoaderOnly(HKEY_CURRENT_USER, path);
        }

        // ═══════════════════════════════════════════════════════════════════
        // SHELL ARTIFACT DEEP CLEAN
        // MUICache, ShimCache, FeatureUsage, CapabilityAccessManager,
        // Thumbnail/Icon caches, WER, CrashDumps, VSS, Timeline, ETW, Firewall
        // ═══════════════════════════════════════════════════════════════════

        void CleanShellArtifacts()
        {
            wchar_t localAppdata[MAX_PATH] = {};
            SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppdata);

            // ── MUICache ────────────────────────────────────────────────────
            // Records exe "friendly names" keyed by full path. Ghost entries
            // (path no longer exists) are the tell — Napse/Ocean enumerate and flag them.
            // ONLY delete entries for our loader, not the whole cache.
            CleanRegKeyLoaderOnly(HKEY_CURRENT_USER,
                L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache");

            // ── ShimCache / AppCompatCache ───────────────────────────────────
            // Binary blob at shutdown — list of every PE touched: path + timestamp.
            // Distinct from AppCompatFlags\Compatibility Assistant (already cleaned).
            // Delete the value; kernel rewrites it on next shutdown.
            {
                HKEY hKey = NULL;
                if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\AppCompatCache",
                    0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
                {
                    RegDeleteValueW(hKey, L"AppCompatCache");
                    RegCloseKey(hKey);
                }
            }

            // ── FeatureUsage ─────────────────────────────────────────────────
            // Every app that appeared in taskbar / Alt+Tab: path in plain text.
            // Only clear our loader's entry, not the entire usage history.
            CleanRegKeyLoaderOnly(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FeatureUsage");

            // ── CapabilityAccessManager ──────────────────────────────────────
            // Per-exe consent for broadFileSystemAccess — keyed by path.
            CleanRegKeyLoaderOnly(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore");

            // ── Thumbnail + Icon caches ──────────────────────────────────────
            // Schedule for deletion on reboot — avoids killing Explorer (which
            // leaves the user with a black screen and no taskbar).
            // The caches rebuild automatically after reboot with no trace of us.
            {
                fs::path explorerCache = fs::path(localAppdata) / L"Microsoft\\Windows\\Explorer";
                if (fs::exists(explorerCache))
                {
                    for (auto& entry : fs::directory_iterator(explorerCache, fs::directory_options::skip_permission_denied))
                    {
                        if (!entry.is_regular_file()) continue;
                        std::wstring fname = entry.path().filename().wstring();
                        std::transform(fname.begin(), fname.end(), fname.begin(), ::towlower);
                        if (fname.find(L"thumbcache_") != std::wstring::npos ||
                            fname.find(L"iconcache_") != std::wstring::npos)
                        {
                            // Try direct delete first (may work if not locked)
                            if (!DeleteFileW(entry.path().wstring().c_str()))
                            {
                                // Locked by Explorer — schedule for next reboot
                                MoveFileExW(entry.path().wstring().c_str(),
                                    NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                            }
                        }
                    }
                }
            }

            // ── WER crash reports ────────────────────────────────────────────
            // Ocean AC explicitly checks %AppData%\Microsoft\Windows\WER.
            // A crash report contains the faulting image path — smokes us immediately.
            {
                wchar_t appdata[MAX_PATH] = {};
                SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata);

                const fs::path werPaths[] = {
                    fs::path(localAppdata) / L"Microsoft\\Windows\\WER\\ReportQueue",
                    fs::path(localAppdata) / L"Microsoft\\Windows\\WER\\ReportArchive",
                    fs::path(appdata)      / L"Microsoft\\Windows\\WER",
                    fs::path(L"C:\\ProgramData\\Microsoft\\Windows\\WER\\ReportQueue"),
                    fs::path(L"C:\\ProgramData\\Microsoft\\Windows\\WER\\ReportArchive"),
                };
                for (auto& p : werPaths)
                    RecursiveDeleteDir(p);
            }

            // ── CrashDumps ───────────────────────────────────────────────────
            // Filenames contain the exe name: "fivemexternal.exe.1234.dmp"
            {
                fs::path crashDumps = fs::path(localAppdata) / L"CrashDumps";
                RecursiveDeleteDir(crashDumps);
            }

            // ── Windows Timeline (ConnectedDevicesPlatform) ──────────────────
            // ActivitiesCache.db — SQLite DB with 30-day app launch history.
            // Napse parses this. Path has a UUID subdirectory.
            {
                fs::path cdp = fs::path(localAppdata) / L"ConnectedDevicesPlatform";
                RecursiveDeleteDir(cdp);
            }

            // ── ETW AutoLogger ETL files ─────────────────────────────────────
            // Diagtrack, ReadyBoot, WdiContextLog — persistent kernel-level trace
            // sessions that record process start/stop events independently of evtx.
            {
                wchar_t sysDir[MAX_PATH] = {};
                GetSystemDirectoryW(sysDir, MAX_PATH);

                const fs::path etlPaths[] = {
                    fs::path(sysDir) / L"WDI\\LogFiles",
                    fs::path(L"C:\\ProgramData\\Microsoft\\Diagnosis\\ETLLogs\\AutoLogger"),
                    fs::path(L"C:\\ProgramData\\Microsoft\\Diagnosis\\ETLLogs\\ShutdownLogger"),
                };
                for (auto& dir : etlPaths)
                {
                    if (!fs::exists(dir)) continue;
                    for (auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied))
                    {
                        if (!entry.is_regular_file()) continue;
                        auto ext = entry.path().extension().wstring();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                        if (ext == L".etl")
                        {
                            try { fs::remove(entry.path()); } catch (...) {
                                MoveFileExW(entry.path().wstring().c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                            }
                        }
                    }
                }
            }

            // ── Windows Firewall log ─────────────────────────────────────────
            // Only populated if FW logging is enabled; safe to truncate regardless.
            {
                wchar_t sysDir[MAX_PATH] = {};
                GetSystemDirectoryW(sysDir, MAX_PATH);
                const fs::path fwLogs[] = {
                    fs::path(sysDir) / L"LogFiles\\Firewall\\pfirewall.log",
                    fs::path(sysDir) / L"LogFiles\\Firewall\\pfirewall.log.old",
                };
                for (auto& p : fwLogs)
                {
                    if (!fs::exists(p)) continue;
                    // Truncate (overwrite with empty) rather than delete —
                    // deleting triggers its own USN entry; truncating is quieter.
                    std::ofstream f(p, std::ios::binary | std::ios::trunc);
                    f.close();
                }
            }

            // ── Volume Shadow Copies (VSS) ────────────────────────────────────
            // Launched async — no wait. The bat PS block handles this after we die.
            // Waiting 15s here would make the nuke button feel broken.
            {
                std::wstring cmd = L"vssadmin delete shadows /all /quiet";
                STARTUPINFOW si = { sizeof(si) };
                si.dwFlags = STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_HIDE;
                PROCESS_INFORMATION pi = {};
                if (CreateProcessW(NULL, cmd.data(), NULL, NULL, FALSE,
                    CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
                {
                    // Fire and forget — don't wait
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                }
            }

            // ── Windows Search index ─────────────────────────────────────────
            // If WSearch indexed our directory, ESE DB contains filename + path.
            // Stop service, delete DB, restart — index rebuilds for legit files only.
            {
                SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
                if (scm)
                {
                    SC_HANDLE svc = OpenServiceW(scm, L"WSearch", SERVICE_STOP | SERVICE_START);
                    if (svc)
                    {
                        SERVICE_STATUS st = {};
                        ControlService(svc, SERVICE_CONTROL_STOP, &st);
                        Sleep(300); // minimal wait — bat handles the full cleanup

                        const fs::path searchDbs[] = {
                            fs::path(L"C:\\ProgramData\\Microsoft\\Search\\Data\\Applications\\Windows\\Windows.edb"),
                            fs::path(L"C:\\ProgramData\\Microsoft\\Search\\Data\\Applications\\Windows\\MSS.log"),
                            fs::path(L"C:\\ProgramData\\Microsoft\\Search\\Data\\Applications\\Windows\\GatherLogs"),
                        };
                        for (auto& p : searchDbs)
                        {
                            if (fs::is_directory(p)) RecursiveDeleteDir(p);
                            else SecureDeleteFile(p);
                        }

                        StartService(svc, 0, NULL);
                        CloseServiceHandle(svc);
                    }
                    CloseServiceHandle(scm);
                }
            }
        }

        // ═══════════════════════════════════════════════════════════════════
        // EVENT LOGS
        // ═══════════════════════════════════════════════════════════════════

        void CleanEventLogs()
        {
            // ClearEventLogW writes Event ID 1102 (Security) and 104 (System) as audit trails.
            // wevtutil cl bypasses the API and wipes the .evtx file directly via the
            // EventLog service, leaving no 1102/104 self-evidence.
            const wchar_t* logNames[] = {
                L"Application",
                L"Security",
                L"System",
                L"Microsoft-Windows-TaskScheduler/Operational",
                L"Microsoft-Windows-PowerShell/Operational",
                L"Microsoft-Windows-WMI-Activity/Operational",
                L"Microsoft-Windows-Sysmon/Operational",
            };

            // Launch ALL wevtutil calls in parallel — no sequential waiting.
            // Total time = slowest single log (~3s) instead of 7 × 3s = 21s.
            std::vector<HANDLE> procs;
            for (auto logName : logNames)
            {
                std::wstring cmd = std::wstring(L"wevtutil cl \"") + logName + L"\" /bu:NUL";
                STARTUPINFOW si = { sizeof(si) };
                si.dwFlags = STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_HIDE;
                PROCESS_INFORMATION pi = {};
                std::wstring mutable_cmd = cmd;
                if (CreateProcessW(NULL, mutable_cmd.data(), NULL, NULL, FALSE,
                    CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
                {
                    procs.push_back(pi.hProcess);
                    CloseHandle(pi.hThread);
                }
            }
            // Wait for all in parallel — max 4s total
            if (!procs.empty())
                WaitForMultipleObjects((DWORD)procs.size(), procs.data(), TRUE, 4000);
            for (auto h : procs) CloseHandle(h);

            // Also wipe the .evtx files directly as fallback if wevtutil couldn't open them
            wchar_t sysDir[MAX_PATH] = {};
            GetSystemDirectoryW(sysDir, MAX_PATH);
            fs::path evtxDir = fs::path(sysDir) / L"winevt\\Logs";
            if (fs::exists(evtxDir))
            {
                const wchar_t* targets[] = {
                    L"Application.evtx", L"Security.evtx", L"System.evtx",
                    L"Microsoft-Windows-TaskScheduler%4Operational.evtx",
                    L"Microsoft-Windows-PowerShell%4Operational.evtx",
                    L"Microsoft-Windows-WMI-Activity%4Operational.evtx",
                    L"Microsoft-Windows-Sysmon%4Operational.evtx",
                };
                for (auto name : targets)
                {
                    fs::path p = evtxDir / name;
                    if (fs::exists(p))
                        MoveFileExW(p.wstring().c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                }
            }
        }

        // ═══════════════════════════════════════════════════════════════════
        // PREFETCH + SUPERFETCH + SYSTEM TRACES
        // ═══════════════════════════════════════════════════════════════════

        void CleanTraces()
        {
            wchar_t windir[MAX_PATH] = {};
            GetWindowsDirectoryW(windir, MAX_PATH);

            // Prefetch directory
            fs::path prefetchDir = fs::path(windir) / L"Prefetch";
            if (fs::exists(prefetchDir))
            {
                std::wstring selfName = GetSelfPath().stem().wstring();
                std::transform(selfName.begin(), selfName.end(), selfName.begin(), ::towlower);

                for (auto& entry : fs::directory_iterator(prefetchDir, fs::directory_options::skip_permission_denied))
                {
                    if (!entry.is_regular_file()) continue;
                    std::wstring fname = entry.path().filename().wstring();
                    std::transform(fname.begin(), fname.end(), fname.begin(), ::towlower);

                    if (fname.find(selfName) != std::wstring::npos ||
                        fname.find(L"astralith") != std::wstring::npos ||
                        fname.find(L"fivem-external") != std::wstring::npos ||
                        fname.find(L"fivemexternal") != std::wstring::npos)
                    {
                        SecureDeleteFile(entry.path());
                    }
                }
            }

            // AppCompatFlags (compatibility assistant - stores exe history)
            // Only remove our loader's entries, not all compatibility settings
            CleanRegKeyLoaderOnly(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Store");
            CleanRegKeyLoaderOnly(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers");

            // TEMP directory
            wchar_t tempPath[MAX_PATH] = {};
            GetTempPathW(MAX_PATH, tempPath);
            for (auto& entry : fs::directory_iterator(tempPath, fs::directory_options::skip_permission_denied))
            {
                if (!entry.is_regular_file()) continue;
                std::wstring fname = entry.path().filename().wstring();
                std::transform(fname.begin(), fname.end(), fname.begin(), ::towlower);
                if (fname.find(L"astralith") != std::wstring::npos ||
                    fname.find(L"fivem-external") != std::wstring::npos)
                {
                    SecureDeleteFile(entry.path());
                }
            }

            // BAM (Background Activity Moderator) — stores recent exe activity
            // BAM has per-user SID subkeys; clean each one selectively
            {
                HKEY hBam = NULL;
                if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SYSTEM\\CurrentControlSet\\Services\\bam\\State\\UserSettings",
                    0, KEY_ALL_ACCESS, &hBam) == ERROR_SUCCESS)
                {
                    DWORD subkeyCount = 0;
                    RegQueryInfoKeyW(hBam, NULL, NULL, NULL, &subkeyCount, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
                    for (DWORD i = 0; i < subkeyCount; i++)
                    {
                        wchar_t sid[128] = {};
                        DWORD sidSize = 128;
                        if (RegEnumKeyExW(hBam, i, sid, &sidSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
                        {
                            std::wstring sidPath = L"SYSTEM\\CurrentControlSet\\Services\\bam\\State\\UserSettings\\" + std::wstring(sid);
                            CleanRegKeyLoaderOnly(HKEY_LOCAL_MACHINE, sidPath);
                        }
                    }
                    RegCloseKey(hBam);
                }
            }

            // SRUM (System Resource Usage Monitor) — can't easily wipe without
            // stopping the service, but we clear what we can from the registry
            RegDeleteTreeW(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SRUM\\Extensions");

            // USN Journal — stores a record of every file create/delete/rename on NTFS.
            // MFTECmd can reconstruct deleted file names from this. Nuke the journal so
            // even professional forensic tools come up empty on our file's activity.
            CleanUSNJournal(GetSelfPath());
        }

        // ═══════════════════════════════════════════════════════════════════
        // AMCACHE CLEANING (CRITICAL for Napse)
        // ═══════════════════════════════════════════════════════════════════

        void CleanAmCache()
        {
            // AmCache.hve stores SHA1 + full path of every exe ever run.
            // It's locked by the system, so we stop the service, wipe, restart.
            // If we can't stop the service (no admin), we mark for deletion on reboot.
            fs::path amcachePath = L"C:\\Windows\\appcompat\\Programs\\Amcache.hve";
            fs::path pcaDir = L"C:\\Windows\\appcompat\\pca";

            // Try to stop the AppInfo/Application Experience service
            SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
            if (scm)
            {
                const wchar_t* services[] = { L"AeLookupSvc", L"PcaSvc" };
                for (auto svcName : services)
                {
                    SC_HANDLE svc = OpenServiceW(scm, svcName, SERVICE_STOP);
                    if (svc)
                    {
                        SERVICE_STATUS status = {};
                        ControlService(svc, SERVICE_CONTROL_STOP, &status);
                        CloseServiceHandle(svc);
                    }
                }
                CloseServiceHandle(scm);
            }
            Sleep(150);

            // Try to delete or overwrite AmCache
            if (fs::exists(amcachePath))
            {
                // Try direct delete first
                SecureDeleteFile(amcachePath);
                // If still there (locked), schedule for reboot
                if (fs::exists(amcachePath))
                    MoveFileExW(amcachePath.wstring().c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
            }

            // Also delete the .hve log files
            for (auto& entry : fs::directory_iterator(L"C:\\Windows\\appcompat\\Programs",
                fs::directory_options::skip_permission_denied))
            {
                std::wstring fname = entry.path().filename().wstring();
                std::transform(fname.begin(), fname.end(), fname.begin(), ::towlower);
                if (fname.find(L"amcache") != std::wstring::npos)
                {
                    SecureDeleteFile(entry.path());
                    if (fs::exists(entry.path()))
                        MoveFileExW(entry.path().wstring().c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                }
            }

            // Clean PCA (Program Compatibility Assistant) logs
            if (fs::exists(pcaDir))
            {
                for (auto& entry : fs::directory_iterator(pcaDir, fs::directory_options::skip_permission_denied))
                {
                    try { fs::remove(entry.path()); } catch (...) {}
                }
            }

            // Clean CIT telemetry registry
            RegDeleteTreeW(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\CIT");
        }

        // ═══════════════════════════════════════════════════════════════════
        // SRUM DATABASE (Real file, not just registry)
        // ═══════════════════════════════════════════════════════════════════

        void CleanSRUM()
        {
            // Stop DiagTrack and DPS services that lock SRUDB.dat
            SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
            if (scm)
            {
                const wchar_t* services[] = { L"DiagTrack", L"DPS" };
                for (auto svcName : services)
                {
                    SC_HANDLE svc = OpenServiceW(scm, svcName, SERVICE_STOP);
                    if (svc)
                    {
                        SERVICE_STATUS status = {};
                        ControlService(svc, SERVICE_CONTROL_STOP, &status);
                        CloseServiceHandle(svc);
                    }
                }
                CloseServiceHandle(scm);
            }
            Sleep(150);

            fs::path srumPath = L"C:\\Windows\\System32\\sru\\SRUDB.dat";
            if (fs::exists(srumPath))
            {
                SecureDeleteFile(srumPath);
                if (fs::exists(srumPath))
                    MoveFileExW(srumPath.wstring().c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
            }

            // Also wipe the SRU directory entirely
            for (auto& entry : fs::directory_iterator(L"C:\\Windows\\System32\\sru",
                fs::directory_options::skip_permission_denied))
            {
                try { fs::remove(entry.path()); } catch (...) {
                    MoveFileExW(entry.path().wstring().c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                }
            }
        }

        // ═══════════════════════════════════════════════════════════════════
        // BROWSER HISTORY CLEANING
        // ═══════════════════════════════════════════════════════════════════

        // Scrubs deleted-file rows from a Chromium History SQLite DB without touching
        // browse history. Uses PowerShell + SQLite CLI if available, otherwise
        // falls back to raw string-patch of the DB to NULL deleted-file paths.
        static void ScrubChromiumDownloads(const fs::path& historyDb)
        {
            if (!fs::exists(historyDb)) return;

            // Try PowerShell with SQLite module (available on many machines)
            // DELETE rows in downloads + downloads_url_chains where the target_path
            // no longer exists on disk OR target_path matches our loader/exe name.
            std::wstring ps = L"powershell -nop -ep bypass -w hidden -c \""
                L"$ErrorActionPreference='SilentlyContinue';"
                L"Add-Type -Path (Get-ChildItem $env:LOCALAPPDATA -Recurse -Filter 'System.Data.SQLite.dll' -EA SilentlyContinue | Select-Object -First 1).FullName 2>$null;"
                L"$db='" + historyDb.wstring() + L"';"
                L"$conn=New-Object System.Data.SQLite.SQLiteConnection(\\\"Data Source=$db;Version=3;\\\");"
                L"try{$conn.Open();"
                L"$cmd=$conn.CreateCommand();"
                L"$cmd.CommandText=\\\"DELETE FROM downloads WHERE target_path NOT IN (SELECT target_path FROM downloads WHERE target_path<>'')\\\";"
                L"$cmd.CommandText=\\\"DELETE FROM downloads WHERE NOT EXISTS (SELECT 1 FROM (VALUES(1)) t WHERE target_path GLOB '*')\\\";"
                // Just nuke rows where file no longer exists on disk
                L"$rows=$conn.CreateCommand();$rows.CommandText='SELECT id,target_path FROM downloads';"
                L"$r=$rows.ExecuteReader();"
                L"$del=@();"
                L"while($r.Read()){if($r['target_path'] -and !(Test-Path $r['target_path'])){$del+=$r['id']}};"
                L"$r.Close();"
                L"foreach($id in $del){$c=$conn.CreateCommand();$c.CommandText=\\\"DELETE FROM downloads WHERE id=$id\\\";$c.ExecuteNonQuery()|Out-Null;$c2=$conn.CreateCommand();$c2.CommandText=\\\"DELETE FROM downloads_url_chains WHERE id=$id\\\";$c2.ExecuteNonQuery()|Out-Null};"
                L"$conn.Close()}catch{}" +
                std::wstring(L"\"");

            STARTUPINFOW si = { sizeof(si) };
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi = {};
            std::wstring mutable_cmd = ps;
            if (CreateProcessW(NULL, mutable_cmd.data(), NULL, NULL, FALSE,
                CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
            {
                WaitForSingleObject(pi.hProcess, 8000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }

        void CleanBrowserHistory()
        {
            wchar_t localAppdata[MAX_PATH] = {};
            wchar_t appdata[MAX_PATH] = {};
            SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppdata);
            SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata);

            // DO NOT delete full history — would look suspicious and break the browser.
            // ONLY scrub downloads table entries for files that no longer exist on disk
            // (the "loader.exe - Deleted" entries visible in Chrome's download panel).

            // Chrome — all profiles
            fs::path chromeProfiles = fs::path(localAppdata) / L"Google\\Chrome\\User Data";
            if (fs::exists(chromeProfiles))
            {
                for (auto& entry : fs::directory_iterator(chromeProfiles, fs::directory_options::skip_permission_denied))
                {
                    if (!entry.is_directory()) continue;
                    fs::path histDb = entry.path() / L"History";
                    if (fs::exists(histDb))
                        ScrubChromiumDownloads(histDb);
                }
            }

            // Edge — all profiles
            fs::path edgeProfiles = fs::path(localAppdata) / L"Microsoft\\Edge\\User Data";
            if (fs::exists(edgeProfiles))
            {
                for (auto& entry : fs::directory_iterator(edgeProfiles, fs::directory_options::skip_permission_denied))
                {
                    if (!entry.is_directory()) continue;
                    fs::path histDb = entry.path() / L"History";
                    if (fs::exists(histDb))
                        ScrubChromiumDownloads(histDb);
                }
            }

            // Firefox — clean downloads from places.sqlite (moz_annos table stores download state)
            fs::path ffProfiles = fs::path(appdata) / L"Mozilla\\Firefox\\Profiles";
            if (fs::exists(ffProfiles))
            {
                for (auto& profile : fs::directory_iterator(ffProfiles, fs::directory_options::skip_permission_denied))
                {
                    if (!profile.is_directory()) continue;
                    fs::path placesDb = profile.path() / L"places.sqlite";
                    if (!fs::exists(placesDb)) continue;

                    // Remove download annotations for files that no longer exist
                    std::wstring ps = L"powershell -nop -ep bypass -w hidden -c \""
                        L"$ErrorActionPreference='SilentlyContinue';"
                        L"$db='" + placesDb.wstring() + L"';"
                        L"$conn=New-Object System.Data.SQLite.SQLiteConnection(\\\"Data Source=$db;Version=3;\\\");"
                        L"try{$conn.Open();"
                        L"$c=$conn.CreateCommand();"
                        L"$c.CommandText=\\\"DELETE FROM moz_annos WHERE anno_attribute_id IN (SELECT id FROM moz_anno_attributes WHERE name='downloads/destinationFileURI') AND content NOT IN (SELECT content FROM moz_annos WHERE 1=1)\\\";"
                        // Simpler: just delete annos for files that don't exist
                        L"$c.CommandText=\\\"DELETE FROM moz_annos WHERE anno_attribute_id=(SELECT id FROM moz_anno_attributes WHERE name='downloads/destinationFileURI') AND NOT EXISTS (SELECT 1 FROM (VALUES(1)) t)\\\";"
                        L"$conn.Close()}catch{}" +
                        std::wstring(L"\"");
                    STARTUPINFOW si2 = { sizeof(si2) };
                    si2.dwFlags = STARTF_USESHOWWINDOW;
                    si2.wShowWindow = SW_HIDE;
                    PROCESS_INFORMATION pi2 = {};
                    std::wstring cmd2 = ps;
                    if (CreateProcessW(NULL, cmd2.data(), NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si2, &pi2))
                    {
                        WaitForSingleObject(pi2.hProcess, 8000);
                        CloseHandle(pi2.hProcess);
                        CloseHandle(pi2.hThread);
                    }
                }
            }

            // Clean Zone.Identifier ADS on ALL files in Downloads folder —
            // this strips the "mark of the web" that shows where each file came from.
            wchar_t profilePath[MAX_PATH] = {};
            if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, profilePath)))
            {
                fs::path downloads = fs::path(profilePath) / L"Downloads";
                if (fs::exists(downloads))
                {
                    for (auto& entry : fs::recursive_directory_iterator(downloads, fs::directory_options::skip_permission_denied))
                    {
                        if (!entry.is_regular_file()) continue;
                        std::wstring adsPath = entry.path().wstring() + L":Zone.Identifier";
                        DeleteFileW(adsPath.c_str());
                    }
                }
            }
        }

        // ═══════════════════════════════════════════════════════════════════
        // RECYCLE BIN CLEANING
        // ═══════════════════════════════════════════════════════════════════

        void CleanRecycleBin()
        {
            // Empty the recycle bin silently with no confirmation
            SHEmptyRecycleBinW(NULL, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
        }

        // ═══════════════════════════════════════════════════════════════════
        // WINDOWS DEFENDER HISTORY
        // ═══════════════════════════════════════════════════════════════════

        void CleanDefenderHistory()
        {
            // Detection history
            fs::path defHistory = L"C:\\ProgramData\\Microsoft\\Windows Defender\\Scans\\History\\Service\\DetectionHistory";
            if (fs::exists(defHistory))
                RecursiveDeleteDir(defHistory);

            // Quarantine folder
            fs::path quarantine = L"C:\\ProgramData\\Microsoft\\Windows Defender\\Quarantine";
            if (fs::exists(quarantine))
                RecursiveDeleteDir(quarantine);

            // Scan history
            fs::path scanHistory = L"C:\\ProgramData\\Microsoft\\Windows Defender\\Scans\\History";
            if (fs::exists(scanHistory))
            {
                for (auto& entry : fs::recursive_directory_iterator(scanHistory, fs::directory_options::skip_permission_denied))
                {
                    if (entry.is_regular_file())
                    {
                        try { fs::remove(entry.path()); } catch (...) {}
                    }
                }
            }

            // Registry threat entries
            RegDeleteTreeW(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Microsoft\\Windows Defender\\Threats");

            // Defender operational event log — use wevtutil to avoid self-logging
            {
                std::wstring cmd = L"wevtutil cl \"Microsoft-Windows-Windows Defender/Operational\" /bu:NUL";
                STARTUPINFOW si = { sizeof(si) };
                si.dwFlags = STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_HIDE;
                PROCESS_INFORMATION pi = {};
                if (CreateProcessW(NULL, cmd.data(), NULL, NULL, FALSE,
                    CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
                {
                    WaitForSingleObject(pi.hProcess, 3000);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                }
            }
        }

        // ═══════════════════════════════════════════════════════════════════
        // IN-MEMORY STRING WIPE
        // ═══════════════════════════════════════════════════════════════════

        void WipeStrings()
        {
            HMODULE hMod = GetModuleHandleW(NULL);
            if (!hMod) return;

            IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)hMod;
            IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((BYTE*)hMod + dos->e_lfanew);
            IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);

            for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
            {
                std::string secName((char*)sec[i].Name, 8);
                if (secName.find(".rdata") != std::string::npos ||
                    secName.find(".data") != std::string::npos)
                {
                    BYTE* base = (BYTE*)hMod + sec[i].VirtualAddress;
                    DWORD size = sec[i].Misc.VirtualSize;
                    DWORD oldProt = 0;
                    if (VirtualProtect(base, size, PAGE_READWRITE, &oldProt))
                    {
                        // Multi-pass wipe
                        SecureZeroMemory(base, size);
                        memset(base, 0xCC, size); // INT3 fill
                        SecureZeroMemory(base, size);
                        VirtualProtect(base, size, oldProt, &oldProt);
                    }
                }
            }
        }

        // ═══════════════════════════════════════════════════════════════════
        // CONFIG FILE DELETION
        // ═══════════════════════════════════════════════════════════════════

        void DeleteConfigs()
        {
            wchar_t appdata[MAX_PATH] = {};
            SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata);
            wchar_t localAppdata[MAX_PATH] = {};
            SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppdata);

            // Config files next to our exe
            fs::path selfDir = GetSelfPath().parent_path();
            for (auto& entry : fs::directory_iterator(selfDir, fs::directory_options::skip_permission_denied))
            {
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().wstring();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                if (ext == L".json" || ext == L".cfg" || ext == L".ini" ||
                    ext == L".config" || ext == L".log" || ext == L".bak" ||
                    ext == L".png" || ext == L".bmp" || ext == L".jpg")
                {
                    SecureDeleteFile(entry.path());
                }
            }

            std::vector<fs::path> configDirs = {
                fs::path(appdata) / L"Astralith",
                fs::path(localAppdata) / L"Astralith",
                fs::path(appdata) / L"FivemExternal",
                fs::path(localAppdata) / L"FivemExternal",
            };

            for (auto& dir : configDirs)
                RecursiveDeleteDir(dir);

            // Truncate firewall log proactively — captures sellauth.com connections
            wchar_t sysDir[MAX_PATH] = {};
            GetSystemDirectoryW(sysDir, MAX_PATH);
            const fs::path fwLog = fs::path(sysDir) / L"LogFiles\\Firewall\\pfirewall.log";
            if (fs::exists(fwLog))
            {
                std::ofstream f(fwLog, std::ios::binary | std::ios::trunc);
                f.close();
            }
        }

        // ═══════════════════════════════════════════════════════════════════
        // MEMORY PROTECTION (ANTI-SCANNER)
        // ═══════════════════════════════════════════════════════════════════

        void ProtectMemoryRegions()
        {
            // Set our PE headers to PAGE_NOACCESS so memory scanners can't read
            // the MZ/PE signature or section table from our process
            HMODULE hMod = GetModuleHandleW(NULL);
            if (!hMod) return;

            IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)hMod;
            IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((BYTE*)hMod + dos->e_lfanew);

            // Erase PE header from memory (first page)
            DWORD oldProt = 0;
            if (VirtualProtect(hMod, 0x1000, PAGE_READWRITE, &oldProt))
            {
                SecureZeroMemory(hMod, 0x1000);
                VirtualProtect(hMod, 0x1000, PAGE_NOACCESS, &oldProt);
            }

            s_MemoryProtected = true;
        }

        // ═══════════════════════════════════════════════════════════════════
        // PROCESS NAME SPOOFING (PEB MANIPULATION)
        // ═══════════════════════════════════════════════════════════════════

        void SpoofProcessName()
        {
            // Rename our process image path in the PEB to look like a legit process
            // This fools tools that read the PEB->ProcessParameters->ImagePathName
            typedef struct _MY_PEB {
                BYTE Reserved1[2];
                BYTE BeingDebugged;
                BYTE Reserved2[1];
                PVOID Reserved3[2];
                PVOID Ldr;
                PVOID ProcessParameters; // RTL_USER_PROCESS_PARAMETERS*
            } MY_PEB;

            typedef struct _MY_RTL_USER_PROCESS_PARAMETERS {
                BYTE Reserved1[16];
                PVOID Reserved2[10];
                UNICODE_STRING ImagePathName;
                UNICODE_STRING CommandLine;
            } MY_RTL_USER_PROCESS_PARAMETERS;

            // Get our PEB
#ifdef _WIN64
            MY_PEB* peb = (MY_PEB*)__readgsqword(0x60);
#else
            MY_PEB* peb = (MY_PEB*)__readfsdword(0x30);
#endif
            if (!peb || !peb->ProcessParameters) return;

            MY_RTL_USER_PROCESS_PARAMETERS* params = (MY_RTL_USER_PROCESS_PARAMETERS*)peb->ProcessParameters;

            // Overwrite the image path with something innocent
            // "conhost.exe" is always running and won't raise suspicion
            const wchar_t* fakePath = L"C:\\Windows\\System32\\RuntimeBroker.exe";
            const wchar_t* fakeCmd = L"RuntimeBroker.exe -ServerName:DefaultBrowserServer";
            size_t fakePathLen = wcslen(fakePath) * sizeof(wchar_t);
            size_t fakeCmdLen = wcslen(fakeCmd) * sizeof(wchar_t);

            // Only overwrite if our buffer is large enough
            if (params->ImagePathName.MaximumLength >= fakePathLen)
            {
                memset(params->ImagePathName.Buffer, 0, params->ImagePathName.MaximumLength);
                memcpy(params->ImagePathName.Buffer, fakePath, fakePathLen);
                params->ImagePathName.Length = (USHORT)fakePathLen;
            }

            if (params->CommandLine.MaximumLength >= fakeCmdLen)
            {
                memset(params->CommandLine.Buffer, 0, params->CommandLine.MaximumLength);
                memcpy(params->CommandLine.Buffer, fakeCmd, fakeCmdLen);
                params->CommandLine.Length = (USHORT)fakeCmdLen;
            }

            // Clear BeingDebugged flag
            peb->BeingDebugged = 0;

            // Also spoof the module name in the LDR linked list.
            // Task Manager reads BaseDllName from here — not from the PEB path.
            typedef struct _MY_LDR_DATA_TABLE_ENTRY {
                LIST_ENTRY InLoadOrderLinks;
                LIST_ENTRY InMemoryOrderLinks;
                LIST_ENTRY InInitializationOrderLinks;
                PVOID DllBase;
                PVOID EntryPoint;
                ULONG SizeOfImage;
                UNICODE_STRING FullDllName;
                UNICODE_STRING BaseDllName;
            } MY_LDR_DATA_TABLE_ENTRY;

            typedef struct _MY_PEB_LDR_DATA {
                ULONG Length;
                BOOLEAN Initialized;
                PVOID SsHandle;
                LIST_ENTRY InLoadOrderModuleList;
            } MY_PEB_LDR_DATA;

            MY_PEB_LDR_DATA* ldr = (MY_PEB_LDR_DATA*)peb->Ldr;
            if (ldr)
            {
                MY_LDR_DATA_TABLE_ENTRY* entry = (MY_LDR_DATA_TABLE_ENTRY*)ldr->InLoadOrderModuleList.Flink;
                if (entry)
                {
                    // First entry in the list is our own module
                    const wchar_t* fakeBase = L"RuntimeBroker.exe";
                    size_t fakeBaseLen = wcslen(fakeBase) * sizeof(wchar_t);

                    if (entry->BaseDllName.MaximumLength >= fakeBaseLen)
                    {
                        memset(entry->BaseDllName.Buffer, 0, entry->BaseDllName.MaximumLength);
                        memcpy(entry->BaseDllName.Buffer, fakeBase, fakeBaseLen);
                        entry->BaseDllName.Length = (USHORT)fakeBaseLen;
                    }

                    if (entry->FullDllName.MaximumLength >= fakePathLen)
                    {
                        memset(entry->FullDllName.Buffer, 0, entry->FullDllName.MaximumLength);
                        memcpy(entry->FullDllName.Buffer, fakePath, fakePathLen);
                        entry->FullDllName.Length = (USHORT)fakePathLen;
                    }
                }
            }

            s_ProcessSpoofed = true;
        }

        // ═══════════════════════════════════════════════════════════════════
        // SCANNER ACCESS BLOCKING
        // ═══════════════════════════════════════════════════════════════════

        void BlockScannerAccess()
        {
            // Set a restrictive DACL on our process so external tools can't
            // OpenProcess() us to read memory
            HANDLE hProcess = GetCurrentProcess();

            // Create empty DACL (denies all access to everyone except SYSTEM)
            SECURITY_DESCRIPTOR sd;
            InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);

            ACL emptyDacl;
            InitializeAcl(&emptyDacl, sizeof(ACL), ACL_REVISION);
            SetSecurityDescriptorDacl(&sd, TRUE, &emptyDacl, FALSE);

            // Apply — this will prevent Process Hacker, System Informer, etc
            // from reading our memory. We still run fine because we're SYSTEM-owned
            // threads internally.
            SetKernelObjectSecurity(hProcess, DACL_SECURITY_INFORMATION, &sd);
        }

        // ═══════════════════════════════════════════════════════════════════
        // TASKBAR HIDING
        // ═══════════════════════════════════════════════════════════════════

        void HideFromTaskbar(HWND overlayWnd)
        {
            if (!overlayWnd) return;

            LONG_PTR exStyle = GetWindowLongPtr(overlayWnd, GWL_EXSTYLE);
            exStyle |= WS_EX_TOOLWINDOW;
            exStyle &= ~WS_EX_APPWINDOW;
            SetWindowLongPtr(overlayWnd, GWL_EXSTYLE, exStyle);

            ShowWindow(overlayWnd, SW_HIDE);
            SetWindowLongPtr(overlayWnd, GWL_EXSTYLE, GetWindowLongPtr(overlayWnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
            ShowWindow(overlayWnd, SW_SHOW);

            // Set a window title that looks like a legit Windows component
            // SS tools enumerate windows and flag suspicious titles (empty, "overlay", "cheat", etc)
            SetWindowTextW(overlayWnd, L"MSCTFIME UI");

            // Also change the window class name would be ideal but can't after creation
            // However we can set the window property to look like an IME window
            SetPropW(overlayWnd, L"Microsoft.Windows.InputMethod", (HANDLE)1);

            s_TaskbarHidden = true;
        }

        // ═══════════════════════════════════════════════════════════════════
        // REMOTE ACCESS BLOCKING
        // ═══════════════════════════════════════════════════════════════════

        void BlockRemoteAccess()
        {
            // Only KILL the processes — do NOT disable services.
            // Napse flags disabled services as evidence of SS evasion.
            // A stopped process looks normal (user closed it), but a disabled
            // service screams "someone is hiding something".
            auto remoteProcs = GetRemoteAccessProcessNames();
            for (auto& name : remoteProcs)
                KillProcessByName(name);

            // Also stop the services (but leave them ENABLED so it looks normal)
            SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
            if (scm)
            {
                const wchar_t* services[] = {
                    L"AnyDesk", L"TeamViewer", L"AnyDeskMSI", L"TeamViewerS",
                    L"RustDesk", L"SupremoService",
                };

                for (auto svcName : services)
                {
                    SC_HANDLE svc = OpenServiceW(scm, svcName, SERVICE_STOP);
                    if (svc)
                    {
                        SERVICE_STATUS status = {};
                        ControlService(svc, SERVICE_CONTROL_STOP, &status);
                        CloseServiceHandle(svc);
                    }
                }
                CloseServiceHandle(scm);
            }

            s_RemoteBlocked = true;
        }

        // ═══════════════════════════════════════════════════════════════════
        // STREAM PROOF (CAPTURE BYPASS)
        // ═══════════════════════════════════════════════════════════════════

        void EnforceStreamProof(HWND overlayWnd)
        {
            if (!overlayWnd) return;
            SafeCall(SetWindowDisplayAffinity)(overlayWnd, WDA_EXCLUDEFROMCAPTURE);
            s_StreamProof = true;
        }

        void DisableStreamProof(HWND overlayWnd)
        {
            if (!overlayWnd) return;
            SafeCall(SetWindowDisplayAffinity)(overlayWnd, WDA_NONE);
            s_StreamProof = false;
        }

        // ═══════════════════════════════════════════════════════════════════
        // ADDITIONAL ANTI-SS MEASURES
        // ═══════════════════════════════════════════════════════════════════

        void CleanClipboard()
        {
            if (OpenClipboard(NULL))
            {
                EmptyClipboard();
                CloseClipboard();
            }
        }

        void DisableWindowGhosting()
        {
            // Prevents Windows from creating a "ghost" window when our process
            // appears hung — ghost windows have readable titles that SS tools scan
            DisableProcessWindowsGhosting();
        }

        void HideConsoleWindow()
        {
            // Kill any attached console window (some SS tools look for hidden consoles)
            HWND con = GetConsoleWindow();
            if (con)
            {
                ShowWindow(con, SW_HIDE);
                FreeConsole();
            }
        }

        void CleanDNSCache()
        {
            // Some SS tools check the DNS resolver cache for known cheat domains
            typedef BOOL(WINAPI* pDnsFlushResolverCache)(void);
            HMODULE hDns = LoadLibraryW(L"dnsapi.dll");
            if (hDns)
            {
                auto flush = (pDnsFlushResolverCache)GetProcAddress(hDns, "DnsFlushResolverCache");
                if (flush) flush();
                FreeLibrary(hDns);
            }
        }

        void CleanNetworkTraces()
        {
            // Clear ARP cache — can reveal communication with known cheat servers
            typedef DWORD(WINAPI* pFlushIpNetTable)(DWORD);
            HMODULE hIp = LoadLibraryW(L"iphlpapi.dll");
            if (hIp)
            {
                auto flush = (pFlushIpNetTable)GetProcAddress(hIp, "FlushIpNetTable");
                if (flush) flush(0);
                FreeLibrary(hIp);
            }
        }

        void AntiScreenCapture(HWND overlayWnd)
        {
            if (!overlayWnd) return;

            // Layer 1: WDA_EXCLUDEFROMCAPTURE (already used by stream proof)
            SetWindowDisplayAffinity(overlayWnd, WDA_EXCLUDEFROMCAPTURE);

            // Layer 2: SetWindowLong to remove from print-screen behavior
            LONG_PTR exStyle = GetWindowLongPtr(overlayWnd, GWL_EXSTYLE);
            exStyle |= WS_EX_TOOLWINDOW;   // not in Alt+Tab
            exStyle |= WS_EX_NOACTIVATE;   // doesn't activate when clicked
            SetWindowLongPtr(overlayWnd, GWL_EXSTYLE, exStyle);
        }

        void MonitorSSTools()
        {
            auto tools = GetSSToolProcessNames();
            for (auto& name : tools)
                KillProcessByName(name);
        }

        // Unlinks our module from all four PEB LDR lists so tools using
        // CreateToolhelp32Snapshot(TH32CS_SNAPMODULE) or direct LDR enumeration
        // cannot find our module. Note: kernel EPROCESS list is unaffected —
        // hiding from NtQuerySystemInformation(SystemProcessInformation) requires
        // a kernel driver (DKOM), which is out of scope for a usermode tool.
        void UnlinkFromProcessList()
        {
            typedef struct _MY_PEB2 {
                BYTE Reserved1[2];
                BYTE BeingDebugged;
                BYTE Reserved2[1];
                PVOID Reserved3[2];
                PVOID Ldr;
            } MY_PEB2;

            // Only the three ordered lists — no HashLinks (offset varies by build)
            typedef struct _MY_LDR_ENTRY {
                LIST_ENTRY InLoadOrderLinks;
                LIST_ENTRY InMemoryOrderLinks;
                LIST_ENTRY InInitializationOrderLinks;
            } MY_LDR_ENTRY;

            typedef struct _MY_LDR2 {
                ULONG Length;
                BOOLEAN Initialized;
                PVOID SsHandle;
                LIST_ENTRY InLoadOrderModuleList;
                LIST_ENTRY InMemoryOrderModuleList;
                LIST_ENTRY InInitializationOrderModuleList;
            } MY_LDR2;

#ifdef _WIN64
            MY_PEB2* peb = (MY_PEB2*)__readgsqword(0x60);
#else
            MY_PEB2* peb = (MY_PEB2*)__readfsdword(0x30);
#endif
            if (!peb || !peb->Ldr) return;

            MY_LDR2* ldr = (MY_LDR2*)peb->Ldr;
            MY_LDR_ENTRY* entry = (MY_LDR_ENTRY*)ldr->InLoadOrderModuleList.Flink;
            if (!entry) return;

            // Validate pointers before touching — bad pointer = instant crash
            if (!entry->InLoadOrderLinks.Flink || !entry->InLoadOrderLinks.Blink) return;
            if (!entry->InMemoryOrderLinks.Flink || !entry->InMemoryOrderLinks.Blink) return;
            if (!entry->InInitializationOrderLinks.Flink || !entry->InInitializationOrderLinks.Blink) return;

            entry->InLoadOrderLinks.Blink->Flink            = entry->InLoadOrderLinks.Flink;
            entry->InLoadOrderLinks.Flink->Blink             = entry->InLoadOrderLinks.Blink;

            entry->InMemoryOrderLinks.Blink->Flink           = entry->InMemoryOrderLinks.Flink;
            entry->InMemoryOrderLinks.Flink->Blink            = entry->InMemoryOrderLinks.Blink;

            entry->InInitializationOrderLinks.Blink->Flink   = entry->InInitializationOrderLinks.Flink;
            entry->InInitializationOrderLinks.Flink->Blink    = entry->InInitializationOrderLinks.Blink;

            // Poison — point at self so accidental re-walk is a no-op
            entry->InLoadOrderLinks.Flink          = &entry->InLoadOrderLinks;
            entry->InLoadOrderLinks.Blink          = &entry->InLoadOrderLinks;
            entry->InMemoryOrderLinks.Flink        = &entry->InMemoryOrderLinks;
            entry->InMemoryOrderLinks.Blink        = &entry->InMemoryOrderLinks;
            entry->InInitializationOrderLinks.Flink = &entry->InInitializationOrderLinks;
            entry->InInitializationOrderLinks.Blink = &entry->InInitializationOrderLinks;
        }

        // Spoofs the window to look like it belongs to the FiveM process
        // by associating it with the game's thread. SS tools check which
        // process owns each visible window.
        void SpoofWindowOwnership(HWND overlayWnd, DWORD targetPid)
        {
            if (!overlayWnd || !targetPid) return;
            // Can't truly reparent to another process externally, but we can
            // make the window description match what you'd expect from the game
            SetWindowTextW(overlayWnd, L"MSCTFIME UI");
        }

        // ═══════════════════════════════════════════════════════════════════
        // REGISTRY TRANSACTION LOG CLEAN
        // NTUSER.DAT.LOG1/LOG2 are NTFS transaction logs for the user hive.
        // Registry Explorer / RegistryExplorer by Zimmermann can recover
        // deleted keys directly from these logs even after RegDeleteTree.
        // We can't open them directly (locked by the kernel) so we:
        //   1. Flush the hive to commit all pending transactions (closes the log)
        //   2. Schedule them for deletion on next reboot via MoveFileEx
        //   3. Also try RegSaveKey + RegReplaceKey to rebuild the hive clean
        // ═══════════════════════════════════════════════════════════════════

        void CleanRegistryTransactionLogs()
        {
            // Flush HKCU to commit all pending transactions — after this,
            // LOG1/LOG2 should be in a clean/minimal state
            RegFlushKey(HKEY_CURRENT_USER);
            RegFlushKey(HKEY_LOCAL_MACHINE);

            wchar_t profile[MAX_PATH] = {};
            SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, profile);

            // Schedule LOG files for deletion on reboot — they rebuild clean
            const fs::path logFiles[] = {
                fs::path(profile) / L"NTUSER.DAT.LOG1",
                fs::path(profile) / L"NTUSER.DAT.LOG2",
                // System hives in config — cover SYSTEM and SOFTWARE logs too
                fs::path(L"C:\\Windows\\System32\\config\\SYSTEM.LOG1"),
                fs::path(L"C:\\Windows\\System32\\config\\SYSTEM.LOG2"),
                fs::path(L"C:\\Windows\\System32\\config\\SOFTWARE.LOG1"),
                fs::path(L"C:\\Windows\\System32\\config\\SOFTWARE.LOG2"),
            };

            for (auto& p : logFiles)
            {
                if (fs::exists(p))
                    MoveFileExW(p.wstring().c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
            }

            // Also clean the PendingFileRenameOperations entry WE just wrote
            // so reviewers don't see evidence of scheduled deletions
            CleanPendingRenames(GetSelfPath().stem().wstring());
        }

        // ═══════════════════════════════════════════════════════════════════
        // SELF-DESTRUCT
        // ═══════════════════════════════════════════════════════════════════

        // Timestomp a directory to look untouched
        void TimestompDirectory(const fs::path& dir)
        {
            HANDLE hDir = CreateFileW(dir.wstring().c_str(), FILE_WRITE_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS, NULL);
            if (hDir == INVALID_HANDLE_VALUE) return;

            // Set timestamps to something old and innocuous (Jan 1 2024 00:00:00)
            SYSTEMTIME st = {};
            st.wYear = 2024; st.wMonth = 1; st.wDay = 1;
            st.wHour = 0; st.wMinute = 0; st.wSecond = 0;
            FILETIME ft;
            SystemTimeToFileTime(&st, &ft);
            SetFileTime(hDir, &ft, &ft, &ft);
            CloseHandle(hDir);
        }

        // Remove our PendingFileRenameOperations entry if we used MoveFileEx
        void CleanPendingRenames(const std::wstring& selfPathLower)
        {
            HKEY hKey = NULL;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                L"SYSTEM\\CurrentControlSet\\Control\\Session Manager",
                0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
                return;

            DWORD dataSize = 0;
            if (RegQueryValueExW(hKey, L"PendingFileRenameOperations", NULL, NULL, NULL, &dataSize) != ERROR_SUCCESS || dataSize == 0)
            {
                RegCloseKey(hKey);
                return;
            }

            std::vector<wchar_t> data(dataSize / sizeof(wchar_t) + 1, 0);
            DWORD type = 0;
            if (RegQueryValueExW(hKey, L"PendingFileRenameOperations", NULL, &type,
                (BYTE*)data.data(), &dataSize) != ERROR_SUCCESS)
            {
                RegCloseKey(hKey);
                return;
            }

            // Multi-string: pairs of (source, dest) separated by nulls, double-null at end
            // Rebuild without entries that reference our path
            std::vector<wchar_t> cleaned;
            const wchar_t* ptr = data.data();
            const wchar_t* end = data.data() + data.size();

            while (ptr < end && *ptr)
            {
                const wchar_t* src = ptr;
                size_t srcLen = wcslen(src);
                ptr += srcLen + 1;

                const wchar_t* dst = ptr;
                size_t dstLen = wcslen(dst);
                ptr += dstLen + 1;

                // Check if this entry references our file
                std::wstring srcLower(src);
                std::transform(srcLower.begin(), srcLower.end(), srcLower.begin(), ::towlower);

                if (srcLower.find(selfPathLower) != std::wstring::npos)
                    continue; // skip our entry

                // Keep this entry
                cleaned.insert(cleaned.end(), src, src + srcLen + 1);
                cleaned.insert(cleaned.end(), dst, dst + dstLen + 1);
            }
            cleaned.push_back(0); // double null terminator

            if (cleaned.size() <= 2)
            {
                // Nothing left — delete the value entirely
                RegDeleteValueW(hKey, L"PendingFileRenameOperations");
            }
            else
            {
                RegSetValueExW(hKey, L"PendingFileRenameOperations", 0, REG_MULTI_SZ,
                    (BYTE*)cleaned.data(), (DWORD)(cleaned.size() * sizeof(wchar_t)));
            }
            RegCloseKey(hKey);
        }

        // Wipe NTFS USN Journal entries for our file (requires admin)
        void CleanUSNJournal(const fs::path& filePath)
        {
            // Get the volume root (e.g. "C:\\")
            std::wstring volume = filePath.root_name().wstring() + L"\\";
            std::wstring volumePath = L"\\\\.\\" + filePath.root_name().wstring();

            HANDLE hVol = CreateFileW(volumePath.c_str(), GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            if (hVol == INVALID_HANDLE_VALUE) return;

            // Delete and recreate the USN journal — this wipes ALL entries on the volume
            // which is nuclear but leaves zero trace of any specific file
            USN_JOURNAL_DATA_V0 journalData = {};
            DWORD bytesReturned = 0;
            if (DeviceIoControl(hVol, FSCTL_QUERY_USN_JOURNAL, NULL, 0,
                &journalData, sizeof(journalData), &bytesReturned, NULL))
            {
                DELETE_USN_JOURNAL_DATA deleteData = {};
                deleteData.UsnJournalID = journalData.UsnJournalID;
                deleteData.DeleteFlags = USN_DELETE_FLAG_DELETE | USN_DELETE_FLAG_NOTIFY;
                DeviceIoControl(hVol, FSCTL_DELETE_USN_JOURNAL, &deleteData,
                    sizeof(deleteData), NULL, 0, &bytesReturned, NULL);
            }
            CloseHandle(hVol);
        }

        // ═══════════════════════════════════════════════════════════════════
        // SYSTEM INFO CLEAN
        // Covers what a reviewer sees in msinfo32 / systeminfo cmd:
        //   - Startup entries (Run keys + Startup folders)
        //   - Driver service entries left by BYOVD
        //   - C:\Windows\Temp installer/system logs
        //   - Any .sys dropped in System32\drivers
        // ═══════════════════════════════════════════════════════════════════

        void CleanSystemInfo()
        {
            // ── Startup registry keys ────────────────────────────────────────
            // msinfo32 "Startup Programs" reads from these — wipe any entry
            // whose image path references our exe or known cheat names
            auto CleanRunKey = [](HKEY root, const wchar_t* subkey)
            {
                HKEY hKey = NULL;
                if (RegOpenKeyExW(root, subkey, 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
                    return;

                DWORD valueCount = 0;
                RegQueryInfoKeyW(hKey, NULL,NULL,NULL,NULL,NULL,NULL,&valueCount,NULL,NULL,NULL,NULL);

                std::vector<std::wstring> toDelete;
                for (DWORD i = 0; i < valueCount; i++)
                {
                    wchar_t name[512] = {};
                    wchar_t data[1024] = {};
                    DWORD nameSz = 512, dataSz = sizeof(data), type = 0;
                    if (RegEnumValueW(hKey, i, name, &nameSz, NULL, &type,
                        (BYTE*)data, &dataSz) != ERROR_SUCCESS) continue;

                    std::wstring dataLower(data);
                    std::transform(dataLower.begin(), dataLower.end(), dataLower.begin(), ::towlower);

                    if (dataLower.find(L"loader") != std::wstring::npos ||
                        dataLower.find(L"astralith") != std::wstring::npos ||
                        dataLower.find(L"fivem-external") != std::wstring::npos ||
                        dataLower.find(L"clemer") != std::wstring::npos)
                    {
                        toDelete.push_back(name);
                    }
                }
                for (auto& v : toDelete) RegDeleteValueW(hKey, v.c_str());
                RegCloseKey(hKey);
            };

            CleanRunKey(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run");
            CleanRunKey(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce");
            CleanRunKey(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
            CleanRunKey(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce");

            // ── Startup folders ──────────────────────────────────────────────
            wchar_t appdata[MAX_PATH] = {};
            SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata);

            const fs::path startupDirs[] = {
                fs::path(appdata) / L"Microsoft\\Windows\\Start Menu\\Programs\\Startup",
                fs::path(L"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\StartUp"),
            };
            for (auto& dir : startupDirs)
            {
                if (!fs::exists(dir)) continue;
                for (auto& entry : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied))
                {
                    if (!entry.is_regular_file()) continue;
                    std::wstring fname = entry.path().filename().wstring();
                    std::transform(fname.begin(), fname.end(), fname.begin(), ::towlower);
                    if (fname.find(L"loader") != std::wstring::npos ||
                        fname.find(L"astralith") != std::wstring::npos ||
                        fname.find(L"clemer") != std::wstring::npos)
                        SecureDeleteFile(entry.path());
                }
            }

            // ── BYOVD driver service entry cleanup ───────────────────────────
            // Any service we registered (RTCore64 or our random-named copy)
            // Clean the known name + any service whose ImagePath points to our exe dir
            fs::path selfDir = GetSelfPath().parent_path();
            std::wstring selfDirLower = selfDir.wstring();
            std::transform(selfDirLower.begin(), selfDirLower.end(), selfDirLower.begin(), ::towlower);

            {
                HKEY hServices = NULL;
                if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SYSTEM\\CurrentControlSet\\Services",
                    0, KEY_READ, &hServices) == ERROR_SUCCESS)
                {
                    DWORD subkeyCount = 0;
                    RegQueryInfoKeyW(hServices,NULL,NULL,NULL,&subkeyCount,NULL,NULL,NULL,NULL,NULL,NULL,NULL);

                    std::vector<std::wstring> toNuke;
                    for (DWORD i = 0; i < subkeyCount; i++)
                    {
                        wchar_t svcName[256] = {};
                        DWORD svcNameSz = 256;
                        if (RegEnumKeyExW(hServices, i, svcName, &svcNameSz,
                            NULL,NULL,NULL,NULL) != ERROR_SUCCESS) continue;

                        // Check ImagePath
                        std::wstring imgPath = std::wstring(L"SYSTEM\\CurrentControlSet\\Services\\")
                            + svcName + L"\\ImagePath";
                        // Actually read via subkey
                        std::wstring subPath = std::wstring(L"SYSTEM\\CurrentControlSet\\Services\\") + svcName;
                        HKEY hSvc = NULL;
                        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subPath.c_str(), 0, KEY_READ, &hSvc) != ERROR_SUCCESS)
                            continue;

                        wchar_t imgVal[MAX_PATH * 2] = {};
                        DWORD sz = sizeof(imgVal), type = 0;
                        RegQueryValueExW(hSvc, L"ImagePath", NULL, &type, (BYTE*)imgVal, &sz);
                        RegCloseKey(hSvc);

                        std::wstring imgLower(imgVal);
                        std::transform(imgLower.begin(), imgLower.end(), imgLower.begin(), ::towlower);

                        // Flag: points to our dir, or matches known driver names
                        if ((!selfDirLower.empty() && imgLower.find(selfDirLower) != std::wstring::npos) ||
                            imgLower.find(L"rtcore") != std::wstring::npos ||
                            imgLower.find(L"rtc64") != std::wstring::npos)
                        {
                            toNuke.push_back(svcName);
                        }
                    }
                    RegCloseKey(hServices);

                    for (auto& svc : toNuke)
                        RegDeleteTreeW(HKEY_LOCAL_MACHINE,
                            (std::wstring(L"SYSTEM\\CurrentControlSet\\Services\\") + svc).c_str());
                }
            }

            // ── C:\Windows\Temp — system-wide installer/MSI logs ─────────────
            // Reviewers check this for MSI*.log, CBS.log, etc. that reference our exe
            {
                wchar_t windir[MAX_PATH] = {};
                GetWindowsDirectoryW(windir, MAX_PATH);
                fs::path winTemp = fs::path(windir) / L"Temp";

                if (fs::exists(winTemp))
                {
                    for (auto& entry : fs::directory_iterator(winTemp, fs::directory_options::skip_permission_denied))
                    {
                        if (!entry.is_regular_file()) continue;
                        std::wstring fname = entry.path().filename().wstring();
                        std::transform(fname.begin(), fname.end(), fname.begin(), ::towlower);
                        if (fname.find(L"loader") != std::wstring::npos ||
                            fname.find(L"astralith") != std::wstring::npos ||
                            fname.find(L"rtcore") != std::wstring::npos ||
                            fname.find(L"rtc64") != std::wstring::npos)
                        {
                            SecureDeleteFile(entry.path());
                        }
                    }
                }
            }

            // ── System32\drivers — any .sys we dropped ───────────────────────
            {
                wchar_t sysDir[MAX_PATH] = {};
                GetSystemDirectoryW(sysDir, MAX_PATH);
                fs::path driversDir = fs::path(sysDir) / L"drivers";

                if (fs::exists(driversDir))
                {
                    for (auto& entry : fs::directory_iterator(driversDir, fs::directory_options::skip_permission_denied))
                    {
                        if (!entry.is_regular_file()) continue;
                        auto ext = entry.path().extension().wstring();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                        if (ext != L".sys") continue;

                        std::wstring fname = entry.path().filename().wstring();
                        std::transform(fname.begin(), fname.end(), fname.begin(), ::towlower);

                        if (fname.find(L"rtcore") != std::wstring::npos ||
                            fname.find(L"rtc64") != std::wstring::npos)
                        {
                            SecureDeleteFile(entry.path());
                            if (fs::exists(entry.path()))
                                MoveFileExW(entry.path().wstring().c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                        }
                    }
                }
            }

            // ── msinfo32 network connections — terminate our sockets ─────────
            {
                typedef int (__stdcall* pfnWSACancel)();
                HMODULE hWs2 = LoadLibraryW(L"ws2_32.dll");
                if (hWs2)
                {
                    pfnWSACancel fn = (pfnWSACancel)GetProcAddress(hWs2, "WSACancelBlockingCall");
                    if (fn) fn();
                    FreeLibrary(hWs2);
                }
            }
        }

        void SelfDestruct()
        {
            fs::path selfPath = GetSelfPath();
            fs::path selfDir = selfPath.parent_path();
            std::wstring selfName = selfPath.stem().wstring();
            DWORD myPid = GetCurrentProcessId();

            // ── Step 0: exclude from Defender so it releases its handle ─────────
            // Defender holds a read handle on every running exe for real-time scan.
            // Without exclusion, DeleteFileW returns ERROR_ACCESS_DENIED even as admin.
            {
                std::wstring psExclude = L"powershell -nop -ep bypass -w hidden -c \""
                    L"Add-MpPreference -ExclusionPath '" + selfDir.wstring() + L"' 2>$null;"
                    L"Add-MpPreference -ExclusionProcess '" + selfPath.wstring() + L"' 2>$null\"";
                STARTUPINFOW si = { sizeof(si) };
                si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
                PROCESS_INFORMATION pi = {};
                if (CreateProcessW(NULL, psExclude.data(), NULL, NULL, FALSE,
                    CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
                {
                    WaitForSingleObject(pi.hProcess, 3000);
                    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
                }
            }

            // ── Step 1: strip read-only/system attributes so delete can succeed ─
            SetFileAttributesW(selfPath.wstring().c_str(), FILE_ATTRIBUTE_NORMAL);

            // ── Step 2: rename immediately so the name vanishes from directory ─
            fs::path renamedPath = selfDir / L"~$tmpupd.tmp";
            DeleteFileW(renamedPath.wstring().c_str());
            BOOL renamed = MoveFileW(selfPath.wstring().c_str(), renamedPath.wstring().c_str());
            if (renamed)
                SetFileAttributesW(renamedPath.wstring().c_str(),
                    FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

            std::wstring target = renamed ? renamedPath.wstring() : selfPath.wstring();

            // ── Step 3: try direct delete now (works if no AV lock) ───────────
            if (DeleteFileW(target.c_str()))
            {
                // Deleted immediately — best case, nothing left on disk
                target = L"";
            }
            else
            {
                // ── Step 4: NtSetInformationFile — marks for deletion on handle close
                // When TerminateProcess kills us, all handles close and OS deletes it.
                typedef struct { union { NTSTATUS Status; PVOID Pointer; }; ULONG_PTR Info; } MY_IOSB;
                typedef struct { BOOLEAN Del; } MY_DISP;
                typedef NTSTATUS(NTAPI* fnNSIF)(HANDLE, MY_IOSB*, PVOID, ULONG, ULONG);
                auto NtSIF = (fnNSIF)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                    "NtSetInformationFile");
                if (NtSIF)
                {
                    HANDLE hF = CreateFileW(target.c_str(), DELETE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hF != INVALID_HANDLE_VALUE)
                    {
                        MY_IOSB iosb = {}; MY_DISP disp = { TRUE };
                        NtSIF(hF, &iosb, &disp, sizeof(disp), 13);
                        CloseHandle(hF);
                    }
                }

                // ── Step 5: reboot fallback — survives even if NtSIF fails ────
                MoveFileExW(target.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                // Also schedule original path in case rename failed
                MoveFileExW(selfPath.wstring().c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
            }

            // Pure PowerShell script — written as UTF-16 LE which PS natively reads.
            // No cmd.exe involved. PS has reliable Remove-Item + [IO.File]::Delete.
            wchar_t tempDir[MAX_PATH] = {};
            GetTempPathW(MAX_PATH, tempDir);
            std::wstring ps1Path = std::wstring(tempDir) + L"~upd.ps1";
            std::wstring batPath = ps1Path; // alias so rest of code compiles

            // Kill any leftover script
            SetFileAttributesW(ps1Path.c_str(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(ps1Path.c_str());

            // Build PS1 content as wstring — written as UTF-16 LE via WriteFile
            std::wstring ps1;
            auto bat_add = [&](const std::wstring& s) { ps1 += s; };

            // Dummy struct so existing bat << L"..." lines still compile
            struct PS1Writer {
                std::wstring& ps1;
                PS1Writer& operator<<(const std::wstring& s) { ps1 += s; return *this; }
                PS1Writer& operator<<(const wchar_t* s) { ps1 += s; return *this; }
                PS1Writer& operator<<(DWORD d) { ps1 += std::to_wstring(d); return *this; }
            } bat{ps1};
            // Pure PS1 — everything in one script, no cmd.exe
            bat << L"$ErrorActionPreference='SilentlyContinue'\n";
            // Wait for OS to release exe file handle after TerminateProcess
            bat << L"Start-Sleep -Seconds 6\n";
            bat << L"$files = @('" << selfPath.wstring() << L"','" << renamedPath.wstring() << L"')\n";
            bat << L"foreach ($f in $files) {\n";
            bat << L"  $sw = [Diagnostics.Stopwatch]::StartNew()\n";
            bat << L"  while ($sw.Elapsed.TotalSeconds -lt 30) {\n";
            bat << L"    try { [IO.File]::Delete($f); if (!(Test-Path $f)) { break } } catch {}\n";
            bat << L"    try { Remove-Item $f -Force -EA Stop; break } catch {}\n";
            bat << L"    Start-Sleep -Milliseconds 500\n";
            bat << L"  }\n";
            bat << L"  if (Test-Path $f) {\n";
            bat << L"    Add-Type -Name K -Namespace W -MemberDefinition '[DllImport(\"kernel32.dll\")]public static extern bool MoveFileEx(string a,string b,int f);' -EA SilentlyContinue\n";
            bat << L"    try { [W.K]::MoveFileEx($f,$null,4) } catch {}\n";
            bat << L"  }\n";
            bat << L"}\n";
            // ONLY delete our exe — never wipe selfDir (could be Desktop or any user folder)
            bat << L"# exe-only cleanup — no directory wipe\n";
            // Logo files alongside the exe (logo.png, logo.bmp, etc.)
            bat << L"@('" << selfDir.wstring() << L"\\logo.png','" << selfDir.wstring() << L"\\logo.bmp','" << selfDir.wstring() << L"\\logo.jpg') | ForEach-Object { try{[IO.File]::Delete($_)}catch{} }\n";
            // Prefetch
            bat << L"Get-ChildItem \"$env:SystemRoot\\Prefetch\" -Filter 'LOADER*.pf' -EA SilentlyContinue | Remove-Item -Force -EA SilentlyContinue\n";
            bat << L"Get-ChildItem \"$env:SystemRoot\\Prefetch\" -Filter '" << selfName << L"*.pf' -EA SilentlyContinue | Remove-Item -Force -EA SilentlyContinue\n";
            // ── ASYNC CLEANUP (runs after we die) — pure PowerShell, no cmd.exe ──
            bat << L"\n$e='SilentlyContinue'\n";
            // Timestomp our directory
            bat << L"$d=Get-Item -LiteralPath '" << selfDir.wstring() << L"' -Force -EA $e\n";
            bat << L"if($d){$t=[DateTime]'2024-01-15 08:30:00';$d.CreationTime=$t;$d.LastWriteTime=$t;$d.LastAccessTime=$t}\n";
            // Registry cleanup (wevtutil now runs at END of script to capture all post-death events)
            // BAM, MuiCache, FeatureUsage, TypedPaths, WordWheelQuery: handled
            // selectively by the C++ process via CleanRegKeyLoaderOnly — no
            // whole-key nukes. Only wipe the system-level AppCompatCache value.
            bat << L"reg delete 'HKLM\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\AppCompatCache' /v AppCompatCache /f 2>$null\n";
            // NOTE: PendingFileRenameOperations is cleaned AFTER MoveFileEx calls at end of script
            // Registry selective cleanup (value-level, not key-level) —
            // only wipe entries referencing our loader, not user's entire history.
            // The C++ process already does this before SelfDestruct via
            // CleanRegKeyLoaderOnly; the PS side is a belt-and-suspenders pass.
            // Removing entire keys (MuiCache, FeatureUsage, BagMRU, etc.) was
            // extremely suspicious — empty registry = obvious cleanup.
            bat << L"Clear-RecycleBin -Force -EA $e\n";
            // AmCache + SRUM — async
            bat << L"Start-Job { Stop-Service AeLookupSvc,PcaSvc -Force -EA SilentlyContinue; Start-Sleep 1; Remove-Item 'C:\\Windows\\appcompat\\Programs\\Amcache.hve*','C:\\Windows\\appcompat\\pca\\*' -Force -EA SilentlyContinue; Start-Service AeLookupSvc,PcaSvc -EA SilentlyContinue } | Out-Null\n";
            bat << L"Start-Job { Stop-Service DiagTrack,DPS -Force -EA SilentlyContinue; Start-Sleep 1; Remove-Item 'C:\\Windows\\System32\\sru\\*' -Force -EA SilentlyContinue; Start-Service DiagTrack,DPS -EA SilentlyContinue } | Out-Null\n";
            // Defender
            bat << L"Remove-Item 'C:\\ProgramData\\Microsoft\\Windows Defender\\Scans\\History\\*' -Recurse -Force -EA $e\n";
            bat << L"Remove-Item 'C:\\ProgramData\\Microsoft\\Windows Defender\\Quarantine\\*' -Recurse -Force -EA $e\n";
            bat << L"reg delete 'HKLM\\SOFTWARE\\Microsoft\\Windows Defender\\Threats' /f 2>$null\n";
            // Downloads ADS strip
            bat << L"Get-ChildItem \"$env:USERPROFILE\\Downloads\" -Recurse -File -EA $e | ForEach-Object { Remove-Item ($_.FullName+':Zone.Identifier') -Force -EA $e }\n";
            // Chrome/Edge downloads scrub via Python
            bat << L"if(Get-Command python -EA $e){\n";
            bat << L"  Get-ChildItem \"$env:LOCALAPPDATA\\Google\\Chrome\\User Data\" -Recurse -Filter History -EA $e | ForEach-Object { python -c \"import sqlite3,os;c=sqlite3.connect(r'$($_.FullName)');c.execute('DELETE FROM downloads WHERE target_path!=chr(0) AND NOT(SELECT 1)');c.commit();c.close()\" 2>$null }\n";
            bat << L"}\n";
            // Everything (voidtools) — indexes every file including loader.exe
            // DB at %LocalAppData%\Everything\Everything.db stores filename history
            bat << L"Stop-Process -Name Everything -Force -EA $e\n";
            bat << L"Remove-Item \"$env:LOCALAPPDATA\\Everything\" -Recurse -Force -EA $e\n";
            bat << L"Remove-Item \"$env:APPDATA\\Everything\" -Recurse -Force -EA $e\n";
            // LastActivityView reads from multiple sources — kill the process
            bat << L"Stop-Process -Name LastActivityView -Force -EA $e\n";
            // WER + CrashDumps + Timeline
            bat << L"Remove-Item \"$env:LOCALAPPDATA\\Microsoft\\Windows\\WER\" -Recurse -Force -EA $e\n";
            bat << L"Remove-Item \"$env:APPDATA\\Microsoft\\Windows\\WER\" -Recurse -Force -EA $e\n";
            bat << L"Remove-Item 'C:\\ProgramData\\Microsoft\\Windows\\WER\\ReportQueue' -Recurse -Force -EA $e\n";
            bat << L"Remove-Item \"$env:LOCALAPPDATA\\CrashDumps\" -Recurse -Force -EA $e\n";
            bat << L"Remove-Item \"$env:LOCALAPPDATA\\ConnectedDevicesPlatform\" -Recurse -Force -EA $e\n";
            // ETW
            bat << L"Remove-Item 'C:\\Windows\\System32\\WDI\\LogFiles\\*.etl' -Force -EA $e\n";
            bat << L"Remove-Item 'C:\\ProgramData\\Microsoft\\Diagnosis\\ETLLogs\\AutoLogger\\*.etl' -Force -EA $e\n";
            // Firewall log
            bat << L"Clear-Content 'C:\\Windows\\System32\\LogFiles\\Firewall\\pfirewall.log' -Force -EA $e\n";
            // VSS + WSearch — run async so they don't block the script
            bat << L"Start-Job { vssadmin delete shadows /all /quiet 2>$null } | Out-Null\n";
            bat << L"Start-Job { Stop-Service WSearch -Force -EA SilentlyContinue; Start-Sleep 2; Remove-Item 'C:\\ProgramData\\Microsoft\\Search\\Data\\Applications\\Windows\\Windows.edb' -Force -EA SilentlyContinue; Start-Service WSearch -EA SilentlyContinue } | Out-Null\n";
            // Ghost LNKs
            bat << L"$sh=New-Object -ComObject WScript.Shell\n";
            bat << L"Get-ChildItem \"$env:APPDATA\\Microsoft\\Windows\\Recent\\*.lnk\" -EA $e | ForEach-Object { try{$t=$sh.CreateShortcut($_.FullName).TargetPath;if($t -and !(Test-Path $t -EA $e)){Remove-Item $_.FullName -Force -EA $e}}catch{} }\n";
            bat << L"Get-ChildItem \"$env:APPDATA\\Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar\\*.lnk\" -EA $e | ForEach-Object { try{$t=$sh.CreateShortcut($_.FullName).TargetPath;if($t -and !(Test-Path $t -EA $e)){Remove-Item $_.FullName -Force -EA $e}}catch{} }\n";
            // PS history + transcripts
            bat << L"Remove-Item (Get-PSReadLineOption).HistorySavePath -Force -EA $e\n";
            bat << L"Remove-Item \"$env:USERPROFILE\\Documents\\PowerShell_transcript*\" -Recurse -Force -EA $e\n";
            bat << L"Remove-Item 'C:\\Transcripts' -Recurse -Force -EA $e\n";

            // ── Stop active transcript BEFORE trying to delete it ─────────────
            // If GPO enables transcription, the running PS engine holds the file
            // open. Stop-Transcript releases the handle so subsequent Remove-Item works.
            bat << L"try { Stop-Transcript } catch {}\n";
            bat << L"Start-Sleep -Milliseconds 200\n";
            bat << L"Remove-Item \"$env:USERPROFILE\\Documents\\PowerShell_transcript*\" -Force -EA $e\n";

            // ── POWERSHELL.EXE prefetch — stores our PS1 path as a referenced file ──
            bat << L"Remove-Item \"$env:SystemRoot\\Prefetch\\POWERSHELL*\" -Force -EA $e\n";
            bat << L"Remove-Item \"$env:SystemRoot\\Prefetch\\WSCRIPT*\" -Force -EA $e\n";

            // ── Defender ExclusionProcess — remove BOTH exclusions ────────────
            bat << L"Remove-MpPreference -ExclusionPath '" << selfDir.wstring() << L"' -EA $e\n";
            bat << L"Remove-MpPreference -ExclusionProcess '" << selfPath.wstring() << L"' -EA $e\n";
            // Also nuke the registry key directly as belt-and-suspenders
            bat << L"Remove-Item 'HKLM:\\SOFTWARE\\Microsoft\\Windows Defender\\Exclusions\\Processes' -Recurse -Force -EA $e\n";
            bat << L"Remove-Item 'HKLM:\\SOFTWARE\\Microsoft\\Windows Defender\\Exclusions\\Paths' -Recurse -Force -EA $e\n";

            // ── Registry transaction logs ─────────────────────────────────────
            // Schedule NTUSER.DAT.LOG for reboot deletion FIRST
            bat << L"Add-Type -Name KM -Namespace WM -MemberDefinition '[DllImport(\"kernel32.dll\")]public static extern bool MoveFileEx(string a,string b,int f);' -EA $e\n";
            bat << L"$hkuLog = \"$env:USERPROFILE\\NTUSER.DAT.LOG\"\n";
            bat << L"1,2 | ForEach-Object { $lf=\"$hkuLog$_\"; if(Test-Path $lf){ try{[WM.KM]::MoveFileEx($lf,$null,4)}catch{} } }\n";
            // THEN clean PendingFileRenameOperations — captures the NTUSER entries
            // we just added plus any leftover entries from the main process
            bat << L"reg delete 'HKLM\\SYSTEM\\CurrentControlSet\\Control\\Session Manager' /v PendingFileRenameOperations /f 2>$null\n";

            // ── VSS operational log + process creation events written AFTER earlier wevtutil ──
            // Run wevtutil LAST — this is the final action before self-delete so it
            // captures everything this script itself generated (process creation, reg changes,
            // VSS events, PowerShell operational blocks).
            bat << L"Start-Sleep -Milliseconds 500\n";
            bat << L"wevtutil el 2>$null | ForEach-Object { wevtutil cl $_ /bu:NUL 2>$null }\n";

            // USN wipe handled by main process via CleanUSNJournal() — no need here

            // Self-delete — PS reads script into memory before executing so the
            // file handle is released and [IO.File]::Delete works cleanly.
            bat << L"try { [IO.File]::Delete($PSCommandPath) } catch {}\n";

            // Write PS1 as UTF-16 LE with BOM — what PowerShell natively expects
            {
                HANDLE hPs = CreateFileW(ps1Path.c_str(), GENERIC_WRITE, 0, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM, NULL);
                if (hPs != INVALID_HANDLE_VALUE)
                {
                    // UTF-16 LE BOM
                    const uint8_t bom[] = { 0xFF, 0xFE };
                    DWORD written = 0;
                    WriteFile(hPs, bom, sizeof(bom), &written, NULL);
                    WriteFile(hPs, ps1.c_str(), (DWORD)(ps1.size() * sizeof(wchar_t)), &written, NULL);
                    CloseHandle(hPs);
                }
            }

            STARTUPINFOW si = { sizeof(si) };
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi = {};
            std::wstring cmdLine = L"powershell.exe -nop -ep bypass -w hidden -f \"" + ps1Path + L"\"";
            CreateProcessW(NULL, cmdLine.data(), NULL, NULL, FALSE,
                CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
            if (pi.hProcess) CloseHandle(pi.hProcess);
            if (pi.hThread) CloseHandle(pi.hThread);

            // Schedule PS1 for reboot deletion AFTER launching it — file must
            // not be open when MoveFileExW is called or it silently fails.
            // PS opens the file in its own process so we schedule from here.
            SetFileAttributesW(ps1Path.c_str(), FILE_ATTRIBUTE_NORMAL);
            MoveFileExW(ps1Path.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

            TimestompDirectory(selfDir);

            // Wipe NTFS USN Journal — Napse parses this for recently deleted filenames
            CleanUSNJournal(selfPath);
        }

        // ═══════════════════════════════════════════════════════════════════
        // PRE-NUKE CLEANUP
        // Removes all evidence from the FiveM process BEFORE we die.
        // Must run before ActivateAndNuke so the hook is gone by the time
        // any SS tool scans the process.
        // ═══════════════════════════════════════════════════════════════════

        void PreNukeCleanup()
        {
            // ── 1. Restore HandleBullet hook + free cave memory ──────────────
            SilentAim::RestoreSilent();

            // ── 2. Kernel cloak cleanup (no-op with EFI bootkit) ──────────
        }

        // ═══════════════════════════════════════════════════════════════════
        // MASTER NUKE
        // ═══════════════════════════════════════════════════════════════════

        void ActivateAndNuke()
        {
            // ══════════════════════════════════════════════════════════════════
            // TARGET: dead + clean in under 8 seconds.
            //
            // PreNukeCleanup runs FIRST to wipe in-memory evidence from the
            // FiveM process before we touch any system-level traces.
            // ══════════════════════════════════════════════════════════════════

            PreNukeCleanup();
            //
            // RULE: Only run sync what CANNOT be done from an external process.
            //   - Registry writes: must do here (HKCU only accessible as our user)
            //   - File deletes that need our token: do here
            //   - Anything that spawns and WAITS on a child process: goes to bat
            //
            // The bat runs AFTER we die and handles the slow stuff (VSS, wevtutil,
            // WSearch, AmCache service stops, browser DB scrub, etc.)
            // ══════════════════════════════════════════════════════════════════

            HWND overlay = FrameWork::Overlay::GetOverlayWindow();

            // ── 0ms: kill screen presence ─────────────────────────────────────
            if (overlay)
            {
                ShowWindow(overlay, SW_HIDE);
                SetWindowDisplayAffinity(overlay, WDA_EXCLUDEFROMCAPTURE);
                DestroyWindow(overlay);
            }

            // ── ~0ms: kill SS tools before they can screenshot ───────────────
            KillSSTools();

            // ── FAST registry cleanups (~200ms total, pure reg writes) ────────
            // These MUST run in our process — HKCU is only writable as us.
            CleanClipboard();
            DeleteConfigs();

            // UserAssist, RecentDocs, TypedPaths, WordWheelQuery, MUICache,
            // ShimCache, FeatureUsage, CapabilityAccessManager — all reg deletes
            CleanUserAssist();
            CleanRecentDocs();

            // Selective registry cleaning — ONLY loader entries, never full keys
            CleanRegKeyLoaderOnly(HKEY_CURRENT_USER,
                L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache");
            {
                HKEY hKey = NULL;
                if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\AppCompatCache",
                    0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
                {
                    RegDeleteValueW(hKey, L"AppCompatCache");
                    RegCloseKey(hKey);
                }
            }
            CleanRegKeyLoaderOnly(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FeatureUsage");
            CleanRegKeyLoaderOnly(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore");
            CleanRegKeyLoaderOnly(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\TypedPaths");
            CleanRegKeyLoaderOnly(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\WordWheelQuery");
            CleanRegKeyLoaderOnly(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Search\\RecentApps");
            // BAM — per-user SID subkeys
            {
                HKEY hBam = NULL;
                if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SYSTEM\\CurrentControlSet\\Services\\bam\\State\\UserSettings",
                    0, KEY_ALL_ACCESS, &hBam) == ERROR_SUCCESS)
                {
                    DWORD subkeyCount = 0;
                    RegQueryInfoKeyW(hBam, NULL, NULL, NULL, &subkeyCount, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
                    for (DWORD i = 0; i < subkeyCount; i++)
                    {
                        wchar_t sid[128] = {};
                        DWORD sidSize = 128;
                        if (RegEnumKeyExW(hBam, i, sid, &sidSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
                        {
                            std::wstring sidPath = L"SYSTEM\\CurrentControlSet\\Services\\bam\\State\\UserSettings\\" + std::wstring(sid);
                            CleanRegKeyLoaderOnly(HKEY_LOCAL_MACHINE, sidPath);
                        }
                    }
                    RegCloseKey(hBam);
                }
            }

            // ── FAST file cleanups (no child processes) ───────────────────────
            // Prefetch — direct file delete, instant
            {
                wchar_t windir[MAX_PATH] = {};
                GetWindowsDirectoryW(windir, MAX_PATH);
                fs::path pre = fs::path(windir) / L"Prefetch";
                std::wstring selfName = GetSelfPath().stem().wstring();
                std::transform(selfName.begin(), selfName.end(), selfName.begin(), ::towlower);
                if (fs::exists(pre))
                {
                    for (auto& e : fs::directory_iterator(pre, fs::directory_options::skip_permission_denied))
                    {
                        if (!e.is_regular_file()) continue;
                        std::wstring fn = e.path().filename().wstring();
                        std::transform(fn.begin(), fn.end(), fn.begin(), ::towlower);
                        if (fn.find(selfName) != std::wstring::npos ||
                            fn.find(L"loader") != std::wstring::npos)
                            SecureDeleteFile(e.path());
                    }
                }
            }

            // WER crash reports (fast fs delete)
            {
                wchar_t la[MAX_PATH] = {}, ap[MAX_PATH] = {};
                SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, la);
                SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, ap);
                RecursiveDeleteDir(fs::path(la) / L"Microsoft\\Windows\\WER\\ReportQueue");
                RecursiveDeleteDir(fs::path(la) / L"Microsoft\\Windows\\WER\\ReportArchive");
                RecursiveDeleteDir(fs::path(ap) / L"Microsoft\\Windows\\WER");
                RecursiveDeleteDir(fs::path(la) / L"CrashDumps");
                RecursiveDeleteDir(fs::path(la) / L"ConnectedDevicesPlatform");
            }

            // Downloads ADS strip (fast)
            {
                wchar_t profile[MAX_PATH] = {};
                SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, profile);
                fs::path dl = fs::path(profile) / L"Downloads";
                if (fs::exists(dl))
                {
                    for (auto& e : fs::recursive_directory_iterator(dl, fs::directory_options::skip_permission_denied))
                    {
                        if (!e.is_regular_file()) continue;
                        std::wstring ads = e.path().wstring() + L":Zone.Identifier";
                        DeleteFileW(ads.c_str());
                    }
                }
            }

            // Thumbcache/iconcache — try direct delete, fallback reboot-schedule
            {
                wchar_t la[MAX_PATH] = {};
                SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, la);
                fs::path ec = fs::path(la) / L"Microsoft\\Windows\\Explorer";
                if (fs::exists(ec))
                {
                    for (auto& e : fs::directory_iterator(ec, fs::directory_options::skip_permission_denied))
                    {
                        if (!e.is_regular_file()) continue;
                        std::wstring fn = e.path().filename().wstring();
                        std::transform(fn.begin(), fn.end(), fn.begin(), ::towlower);
                        if (fn.find(L"thumbcache_") != std::wstring::npos ||
                            fn.find(L"iconcache_") != std::wstring::npos)
                        {
                            if (!DeleteFileW(e.path().wstring().c_str()))
                                MoveFileExW(e.path().wstring().c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
                        }
                    }
                }
            }

            // DNS + network (instant)
            CleanDNSCache();
            CleanNetworkTraces();
            CleanRecycleBin();

            // System info artifacts — Run keys, startup folders, driver services,
            // C:\Windows\Temp, System32\drivers, active sockets
            CleanSystemInfo();

            // Registry transaction log flush
            RegFlushKey(HKEY_CURRENT_USER);
            RegFlushKey(HKEY_LOCAL_MACHINE);

            // ── SelfDestruct FIRST — writes PS1 and launches it ──────────────────
            // MUST run before WipeStrings — WipeStrings zeroes .rdata (all L"..."
            // string literals) and .data (globals). SelfDestruct uses those strings
            // to build the PS1 path and content. If WipeStrings runs first, every
            // string is zeroed → SelfDestruct crashes silently → PS1 never created.
            SelfDestruct();

            // ── In-memory wipe AFTER SelfDestruct ─────────────────────────────────
            WipeStrings();

            TerminateProcess(GetCurrentProcess(), 0);
        }

        // ═══════════════════════════════════════════════════════════════════
        // STATUS
        // ═══════════════════════════════════════════════════════════════════

        bool IsTaskbarHidden() { return s_TaskbarHidden; }
        bool IsRemoteAccessBlocked() { return s_RemoteBlocked; }
        bool IsStreamProofActive() { return s_StreamProof; }
        bool IsMemoryProtected() { return s_MemoryProtected; }
        bool IsProcessSpoofed() { return s_ProcessSpoofed; }
    }
}
