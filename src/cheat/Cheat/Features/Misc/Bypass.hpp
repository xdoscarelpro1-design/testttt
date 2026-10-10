#pragma once

#include <Windows.h>
#include <string>
#include <vector>

namespace Cheat
{
    namespace Bypass
    {
        // ─── Init (call before SpoofProcessName) ─────────────────────────────
        void CacheRealPath();

        // ─── Pre-nuke: restore hooks, unload driver, undo memory patching ───
        void PreNukeCleanup();

        // ─── Full nuke: kills SS tools, cleans everything, self-destructs ───
        void ActivateAndNuke();

        // ─── SS Tool Killer ─────────────────────────────────────────────────
        void KillSSTools();

        // ─── Forensic Trace Cleaning ────────────────────────────────────────
        void CleanTraces();            // Prefetch, Superfetch, BAM, TEMP
        void CleanUserAssist();        // ROT13 decoded registry wipe
        void CleanShellBags();         // Explorer folder history
        void CleanRecentDocs();        // Recent documents + jump lists
        void CleanEventLogs();         // Windows event log entries (wevtutil, no 1102)
        void CleanAmCache();           // AmCache.hve (SHA1 + path of every exe run)
        void CleanSRUM();              // SRUDB.dat (app execution & network history)
        void CleanBrowserHistory();    // Chrome/Edge downloads table scrub + ADS clean
        void CleanRecycleBin();        // Empty recycle bin
        void CleanDefenderHistory();   // Defender detections, quarantine, scan history
        void CleanRegistryTransactionLogs(); // NTUSER.DAT.LOG1/LOG2 — defeats Registry Explorer
        void CleanSystemInfo();              // Run keys, startup folders, driver services, C:\Windows\Temp
        void CleanShellArtifacts();    // MUICache, ShimCache, FeatureUsage, thumbcache,
                                       // iconcache, WER, CrashDumps, VSS, Timeline,
                                       // ETW ETLs, Firewall log, Search index, ghost LNKs
        void WipeStrings();            // Zero .rdata/.data in our process
        void DeleteConfigs();          // Remove all config files

        // ─── Anti-Scanner / Memory Protection ───────────────────────────────
        void ProtectMemoryRegions();   // PAGE_NOACCESS on our sections
        void SpoofProcessName();      // Rename our process in PEB + LDR
        void BlockScannerAccess();    // Deny handle access to our process
        void UnlinkFromProcessList(); // Hide from EnumProcesses/toolhelp

        // ─── Additional Anti-SS ──────────────────────────────────────────────
        void CleanClipboard();
        void DisableWindowGhosting();
        void HideConsoleWindow();
        void CleanDNSCache();
        void CleanNetworkTraces();
        void AntiScreenCapture(HWND overlayWnd);
        void MonitorSSTools();

        // ─── Stealth Features ───────────────────────────────────────────────
        void HideFromTaskbar(HWND overlayWnd);
        void BlockRemoteAccess();
        void EnforceStreamProof(HWND overlayWnd);
        void DisableStreamProof(HWND overlayWnd);
        void SelfDestruct();

        // ─── Status ─────────────────────────────────────────────────────────
        bool IsTaskbarHidden();
        bool IsRemoteAccessBlocked();
        bool IsStreamProofActive();
        bool IsMemoryProtected();
        bool IsProcessSpoofed();
    }
}
