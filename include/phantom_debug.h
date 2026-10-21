#ifndef _PHANTOM_DEBUG_H_
#define _PHANTOM_DEBUG_H_

/*
 * PHANTOM Debug Protocol — shared between kernel payload and usermode debug app.
 *
 * The kernel payload maintains a status struct and circular debug log.
 * The usermode app queries it via the SSDT comm channel (NtQueryDebugFilterState).
 */

#define PHANTOM_DBG_MAGIC   0x50484E54  /* "PHNT" */
#define PHANTOM_DBG_VERSION 1

#define DBG_LOG_MAX_ENTRIES 48
#define DBG_LOG_MSG_LEN     96

/* stage completion flags */
#define STAGE_ENTRY_RAN         (1 << 0)
#define STAGE_API_RESOLVED      (1 << 1)
#define STAGE_DSE_PATCHED       (1 << 2)
#define STAGE_SSDT_HOOKED       (1 << 3)
#define STAGE_MEMORY_HIDDEN     (1 << 4)
#define STAGE_MONITOR_STARTED   (1 << 5)
#define STAGE_TARGET_FOUND      (1 << 6)
#define STAGE_CHEAT_INJECTED    (1 << 7)

/* log severity levels */
#define DBG_INFO    0
#define DBG_OK      1
#define DBG_WARN    2
#define DBG_ERR     3

typedef struct _DBG_LOG_ENTRY {
    unsigned char   Level;
    unsigned char   _pad[3];
    unsigned int    Tick;
    char            Msg[DBG_LOG_MSG_LEN];
} DBG_LOG_ENTRY;

typedef struct _PHANTOM_STATUS {
    unsigned int        Magic;          /* PHANTOM_DBG_MAGIC */
    unsigned int        Version;        /* PHANTOM_DBG_VERSION */

    /* stage bitmask */
    unsigned int        Stages;

    /* counters */
    unsigned int        ApiResolved;
    unsigned int        ApiFailed;
    volatile unsigned int SyscallCount;
    volatile unsigned int ScanCount;

    /* resolved addresses (for display) */
    unsigned long long  NtoskrnlBase;
    unsigned long long  NtoskrnlSize;
    unsigned long long  PayloadBase;
    unsigned long long  PayloadSize;
    unsigned long long  SsdtBase;
    unsigned long long  CodeCaveAddr;
    unsigned long long  PfnDatabase;

    /* injection state */
    unsigned long long  TargetPid;
    unsigned long long  MappedBase;
    unsigned int        CheatDllSize;
    unsigned int        _pad2;

    /* debug log (circular buffer) */
    unsigned int        LogCount;       /* total entries written */
    unsigned int        LogHead;        /* next write index (mod MAX) */
    DBG_LOG_ENTRY       Log[DBG_LOG_MAX_ENTRIES];
} PHANTOM_STATUS;

#endif /* _PHANTOM_DEBUG_H_ */
