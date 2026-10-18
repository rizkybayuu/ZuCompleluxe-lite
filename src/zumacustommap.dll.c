#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

/*
 * zumacustommap.dll - Gauntlet Custom Map Sidecar for Zuma Deluxe v1.0.0.1
 *
 * Implements Option 1 (Rabbit first, natural progression):
 * - Discovers custom maps parsed from levels/levels.xml that are NOT in StageProgression.
 * - Dynamically expands Gauntlet board count so carousel navigates vanilla + custom maps.
 * - Provides LevelDesc ID and DisplayName to PracticeScreen::SyncLevel.
 * - Preserves natural progression: beating 7 stages in Gauntlet unlocks Eagle, 14 Jaguar, 21 Sun God.
 */

#define MAX_CUSTOM_MAPS 256

/* Logging: writes to zumatest.log if present in current directory */
static void logf_(const char *fmt, ...)
{
    if (GetFileAttributesA("zumatest.log") == INVALID_FILE_ATTRIBUTES) return;
    FILE *f = fopen("zumatest.log", "a");
    if (!f) return;
    fprintf(f, "[%8lu][CustomMap] ", (unsigned long)GetTickCount());
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f);
    fflush(f);
    fclose(f);
}

/* VEH crash catcher for diagnostic debugging */
static LONG CALLBACK veh_(PEXCEPTION_POINTERS ep)
{
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != 0xC0000005 && code != 0xC000001D && code != 0xC0000094 && code != 0xC00000FD &&
        code != 0xC0000096 && code != 0xC0000374 && code != 0xC0000409) return EXCEPTION_CONTINUE_SEARCH;
    MEMORY_BASIC_INFORMATION mbi;
    void *addr = ep->ExceptionRecord->ExceptionAddress;
    if (!VirtualQuery(addr, &mbi, sizeof mbi)) return EXCEPTION_CONTINUE_SEARCH;
    extern IMAGE_DOS_HEADER __ImageBase;
    HMODULE self = (HMODULE)&__ImageBase, exe = GetModuleHandleA(NULL);
    if (mbi.AllocationBase != (void *)self && mbi.AllocationBase != (void *)exe) return EXCEPTION_CONTINUE_SEARCH;
    static LONG n = 0;
    if (InterlockedIncrement(&n) > 6) return EXCEPTION_CONTINUE_SEARCH;
    logf_("CRASH code=%08lX addr=%p (%s+0x%lX)", (unsigned long)code, addr,
          mbi.AllocationBase == (void *)self ? "zumacustommap.dll" : "Zuma.exe",
          (unsigned long)((uint8_t *)addr - (uint8_t *)mbi.AllocationBase));
    logf_("CRASH regs eip=%08lX esp=%08lX ebp=%08lX eax=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX",
          ep->ContextRecord->Eip, ep->ContextRecord->Esp, ep->ContextRecord->Ebp, ep->ContextRecord->Eax,
          ep->ContextRecord->Ecx, ep->ContextRecord->Edx, ep->ContextRecord->Esi, ep->ContextRecord->Edi);
    return EXCEPTION_CONTINUE_SEARCH;
}

typedef struct {
    int levelDescIndex; /* 0-based index into LevelMgr.mLevels (vector<LevelDesc>) */
} CustomMapEntry;

static int g_V = 0x7FFFFFFF; /* Initialized to 0x7FFFFFFF per architect specification */
static void** g_ppApp = (void**)0x0057EF00; /* Default pointer to ZumaApp* global */

static CustomMapEntry s_customMaps[MAX_CUSTOM_MAPS];
static int s_customMapCount = 0;
static int s_customMapsBuilt = 0;

/* Helper: read C string from MSVC 7.1 std::string (28 bytes / 0x1C) */
static const char* GetStdStringCStr(const void* pStr) {
    if (!pStr) return "";
    const char* base = (const char*)pStr;
    size_t res = *(const size_t*)(base + 0x18);
    if (res < 16) {
        return base + 0x04;
    } else {
        const char* const* ppBuf = (const char* const*)(base + 0x04);
        return ppBuf ? *ppBuf : "";
    }
}

/* Helper: read string length from MSVC 7.1 std::string */
static size_t GetStdStringSize(const void* pStr) {
    if (!pStr) return 0;
    return *(const size_t*)((const char*)pStr + 0x14);
}

/* Build list of custom maps from LevelMgr */
static void BuildCustomMapsIfNeeded(void) {
    if (s_customMapsBuilt) return;

    void* pApp = g_ppApp ? *g_ppApp : NULL;
    if (!pApp) {
        logf_("BuildCustomMaps: ZumaApp* is NULL (g_ppApp=%p)", g_ppApp);
        return;
    }

    void* pLevelMgr = *(void**)((char*)pApp + 0x608);
    if (!pLevelMgr) {
        logf_("BuildCustomMaps: LevelMgr* is NULL (pApp=%p)", pApp);
        return;
    }

    /* vector<LevelDesc> at LevelMgr + 0x2C (sizeof LevelDesc == 0x220) */
    char* pDescStart = *(char**)((char*)pLevelMgr + 0x2C);
    char* pDescEnd   = *(char**)((char*)pLevelMgr + 0x30);
    if (!pDescStart || !pDescEnd || pDescEnd <= pDescStart) {
        logf_("BuildCustomMaps: LevelDesc vector invalid (start=%p, end=%p)", pDescStart, pDescEnd);
        return;
    }

    int totalLevels = (int)((pDescEnd - pDescStart) / 0x220);

    /* vector<std::string> (StageProgression vanilla boards) at LevelMgr + 0x8C (sizeof std::string == 0x1C) */
    char* pProgStart = *(char**)((char*)pLevelMgr + 0x8C);
    char* pProgEnd   = *(char**)((char*)pLevelMgr + 0x90);
    int numVanillaBoards = 0;
    if (pProgStart && pProgEnd && pProgEnd > pProgStart) {
        numVanillaBoards = (int)((pProgEnd - pProgStart) / 0x1C);
    }

    logf_("BuildCustomMaps: scanning LevelMgr %p (totalLevels=%d, vanillaBoards=%d)",
          pLevelMgr, totalLevels, numVanillaBoards);

    s_customMapCount = 0;

    for (int i = 0; i < totalLevels; i++) {
        char* pDesc = pDescStart + i * 0x220;
        const void* pIdStr = pDesc + 0x04;
        const char* idCStr = GetStdStringCStr(pIdStr);
        size_t idLen = GetStdStringSize(pIdStr);

        if (idLen == 0 || !idCStr || !idCStr[0]) continue;

        /* Check if this ID is in StageProgression vanilla boards */
        int isVanilla = 0;
        for (int v = 0; v < numVanillaBoards; v++) {
            const void* pVanillaStr = pProgStart + v * 0x1C;
            const char* vCStr = GetStdStringCStr(pVanillaStr);
            if (strcmp(idCStr, vCStr) == 0) {
                isVanilla = 1;
                break;
            }
        }
        if (isVanilla) continue;

        /* Deduplicate: multiple <Level> elements can reference the same graphics/board id */
        int alreadyExists = 0;
        for (int c = 0; c < s_customMapCount; c++) {
            char* pExistingDesc = pDescStart + s_customMaps[c].levelDescIndex * 0x220;
            const char* existingId = GetStdStringCStr(pExistingDesc + 0x04);
            if (strcmp(idCStr, existingId) == 0) {
                alreadyExists = 1;
                break;
            }
        }
        if (alreadyExists) continue;

        /* Add to custom maps list */
        if (s_customMapCount < MAX_CUSTOM_MAPS) {
            s_customMaps[s_customMapCount].levelDescIndex = i;

            const void* pNameStr = pDesc + 0x20;
            const char* nameCStr = GetStdStringSize(pNameStr) > 0 ? GetStdStringCStr(pNameStr) : idCStr;

            logf_("Custom map #%d: id='%s', dispName='%s', levelDescIndex=%d",
                  s_customMapCount, idCStr, nameCStr, i);

            s_customMapCount++;
        }
    }

    logf_("BuildCustomMaps complete: %d custom map(s) registered", s_customMapCount);
    s_customMapsBuilt = 1;
}

/* Export: AdjustCount for Hook A */
__declspec(dllexport) int __cdecl zcm_AdjustCount(int items) {
    g_V = items - 1;
    BuildCustomMapsIfNeeded();
    int newCount = items + s_customMapCount;
    logf_("zcm_AdjustCount(items=%d) -> g_V=%d, returning %d (custom=%d)",
          items, g_V, newCount, s_customMapCount);
    return newCount;
}

/* Export: GetSrcId for Hook B */
__declspec(dllexport) const void* __cdecl zcm_GetSrcId(int idx) {
    if (idx < 0 || idx >= s_customMapCount) {
        logf_("zcm_GetSrcId(idx=%d) -> OUT OF BOUNDS (count=%d)", idx, s_customMapCount);
        return NULL;
    }
    void* pApp = g_ppApp ? *g_ppApp : NULL;
    if (!pApp) return NULL;
    void* pLevelMgr = *(void**)((char*)pApp + 0x608);
    if (!pLevelMgr) return NULL;
    char* pDescStart = *(char**)((char*)pLevelMgr + 0x2C);
    if (!pDescStart) return NULL;

    char* pDesc = pDescStart + s_customMaps[idx].levelDescIndex * 0x220;
    const void* pIdStr = pDesc + 0x04;
    logf_("zcm_GetSrcId(idx=%d) -> id='%s' (pDesc=%p, &mId=%p)",
          idx, GetStdStringCStr(pIdStr), pDesc, pIdStr);
    return pIdStr;
}

/* Export: GetSrcName for Hook B */
__declspec(dllexport) const void* __cdecl zcm_GetSrcName(int idx) {
    if (idx < 0 || idx >= s_customMapCount) {
        logf_("zcm_GetSrcName(idx=%d) -> OUT OF BOUNDS (count=%d)", idx, s_customMapCount);
        return NULL;
    }
    void* pApp = g_ppApp ? *g_ppApp : NULL;
    if (!pApp) return NULL;
    void* pLevelMgr = *(void**)((char*)pApp + 0x608);
    if (!pLevelMgr) return NULL;
    char* pDescStart = *(char**)((char*)pLevelMgr + 0x2C);
    if (!pDescStart) return NULL;

    char* pDesc = pDescStart + s_customMaps[idx].levelDescIndex * 0x220;
    const void* pNameStr = pDesc + 0x20;
    const void* pRet = (GetStdStringSize(pNameStr) > 0) ? pNameStr : (pDesc + 0x04);
    logf_("zcm_GetSrcName(idx=%d) -> dispName='%s'",
          idx, GetStdStringCStr(pRet));
    return pRet;
}

/* Export: GetV for Hook B, C, D */
static int s_loggedV = -1;
__declspec(dllexport) int __cdecl zcm_GetV(void) {
    if (g_V != s_loggedV) {
        logf_("zcm_GetV() -> %d", g_V);
        s_loggedV = g_V;
    }
    return g_V;
}

/* Export: GetVPtr for backwards compatibility */
__declspec(dllexport) int* __cdecl zcm_GetVPtr(void) {
    return &g_V;
}

/* Export: Optional dynamic ZumaApp pointer setter */
__declspec(dllexport) void __cdecl zcm_SetAppPtr(void** ppApp) {
    if (ppApp) g_ppApp = ppApp;
}

/* Native GCC thiscall function pointer types for LevelMgr methods */
typedef void* (__attribute__((thiscall)) *fn_GetLevelDesc_t)(void* this_ptr, const void* pIdStr, int diff);
typedef void* (__attribute__((thiscall)) *fn_PickRandom_t)(void* this_ptr, int diff, int num, void* pProf, void* pRecent);
typedef void* (__attribute__((thiscall)) *fn_PickGauntlet_t)(void* this_ptr, int diff, int num, void* pProf);

/* Helper: invoke __thiscall with 2 arguments (LevelMgr::GetLevelDesc) */
static void* CallThiscallGetLevelDesc(void* fn, void* this_ptr, const void* pIdStr, int diff) {
    fn_GetLevelDesc_t f = (fn_GetLevelDesc_t)fn;
    return f(this_ptr, pIdStr, diff);
}

/* Helper: invoke __thiscall with 4 arguments (LevelMgr::PickRandomLevel) */
static void* CallThiscallPickRandom(void* fn, void* this_ptr, int diff, int num, void* pProf, void* pRecent) {
    fn_PickRandom_t f = (fn_PickRandom_t)fn;
    return f(this_ptr, diff, num, pProf, pRecent);
}

/* Helper: invoke __thiscall with 3 arguments (LevelMgr::PickGauntletLevel) */
static void* CallThiscallPickGauntlet(void* fn, void* this_ptr, int diff, int num, void* pProf) {
    fn_PickGauntlet_t f = (fn_PickGauntlet_t)fn;
    return f(this_ptr, diff, num, pProf);
}

/* Helper: check if a board ID belongs to a registered custom map */
static int IsCustomMapId(const char* key) {
    if (!key || !key[0]) return 0;
    for (int i = 0; i < s_customMapCount; i++) {
        const void* pIdStr = zcm_GetSrcId(i);
        const char* idCStr = GetStdStringCStr(pIdStr);
        if (idCStr && strcmp(idCStr, key) == 0) return 1;
    }
    return 0;
}

/* Helper: check if a board ID is in the recent list */
static int IsInRecentList(void* pRecentList, const char* id) {
    if (!pRecentList || !id || !id[0]) return 0;
    int size = *(const int*)((const char*)pRecentList + 8);
    if (size <= 0) return 0;
    void* head = *(void**)((const char*)pRecentList + 4);
    if (!head) return 0;
    void* node = *(void**)((const char*)head + 0);
    int safety = 0;
    while (node && node != head && safety++ < 32) {
        const char* nodeStr = GetStdStringCStr((const char*)node + 8);
        if (nodeStr && strcmp(nodeStr, id) == 0) {
            return 1;
        }
        node = *(void**)((const char*)node + 0);
    }
    return 0;
}

/* In-order traversal of UserProfile+0x3c std::map to collect unlocked vanilla board IDs.
 * MSVC 7.1 std::_Tree_nod layout:
 *   +0x00: void* left
 *   +0x04: void* parent
 *   +0x08: void* right
 *   +0x0C: std::pair<const std::string, PracticeBoardStats> (first = std::string at +0x0C)
 *   +0x2C: char _Color
 *   +0x2D: char _Isnil (1 if nil/head sentinel, 0 if data node)
 */
static void CollectUnlockedVanillaBoards(void* node, void* head, const char** outKeys, int* pCount, int maxCount) {
    if (!node || node == head || *(const char*)((const char*)node + 0x2D) != 0) return;
    void* left = *(void**)((char*)node + 0x00);
    CollectUnlockedVanillaBoards(left, head, outKeys, pCount, maxCount);
    if (*pCount < maxCount) {
        const char* key = GetStdStringCStr((const char*)node + 0x0C);
        if (key && key[0] && strcmp(key, "random") != 0 && strcmp(key, "sequence") != 0 && !IsCustomMapId(key)) {
            outKeys[(*pCount)++] = key;
        }
    }
    void* right = *(void**)((char*)node + 0x08);
    CollectUnlockedVanillaBoards(right, head, outKeys, pCount, maxCount);
}

/* Build full candidate pool of std::string pointers for Gauntlet board selection */
static int BuildGauntletCandidates(
    void* pLevelMgr,
    void* pProfile,
    const void** outCandidates,
    int maxCandidates,
    int* pNumVanilla
) {
    if (!pLevelMgr || !outCandidates || maxCandidates <= 0) return 0;

    char* pProgStart = *(char**)((char*)pLevelMgr + 0x8C);
    char* pProgEnd   = *(char**)((char*)pLevelMgr + 0x90);
    int numProg = (pProgStart && pProgEnd && pProgEnd > pProgStart) ? (int)((pProgEnd - pProgStart) / 0x1C) : 0;

    int count = 0;

    /* Check if profile has unlocked vanilla boards */
    const char* unlockedKeys[64];
    int numUnlocked = 0;
    if (pProfile) {
        void* pMap = (char*)pProfile + 0x3c;
        void* head = *(void**)((char*)pMap + 4);
        if (head) {
            void* root = *(void**)((char*)head + 4);
            CollectUnlockedVanillaBoards(root, head, unlockedKeys, &numUnlocked, 64);
        }
    }

    if (numUnlocked > 0) {
        /* User has specific unlocked vanilla boards in profile */
        for (int u = 0; u < numUnlocked && count < maxCandidates; u++) {
            const char* key = unlockedKeys[u];
            /* Find matching std::string in StageProgression */
            for (int v = 0; v < numProg; v++) {
                const void* pVStr = pProgStart + v * 0x1C;
                const char* vCStr = GetStdStringCStr(pVStr);
                if (vCStr && strcmp(vCStr, key) == 0) {
                    outCandidates[count++] = pVStr;
                    break;
                }
            }
        }
    } else {
        /* DevTester (or fresh profile fallback where all boards unlocked): add all 21 vanilla Gauntlet boards */
        int limit = numProg < 21 ? numProg : 21;
        for (int v = 0; v < limit && count < maxCandidates; v++) {
            const void* pVStr = pProgStart + v * 0x1C;
            const char* vCStr = GetStdStringCStr(pVStr);
            if (vCStr && vCStr[0] && strcmp(vCStr, "space") != 0) {
                outCandidates[count++] = pVStr;
            }
        }
    }

    if (pNumVanilla) *pNumVanilla = count;

    /* Add custom maps */
    for (int c = 0; c < s_customMapCount && count < maxCandidates; c++) {
        const void* pCustomId = zcm_GetSrcId(c);
        if (pCustomId) {
            outCandidates[count++] = pCustomId;
        }
    }

    return count;
}

static void EnsureRandomSeeded(void) {
    static int s_seeded = 0;
    if (!s_seeded) {
        LARGE_INTEGER qpc;
        QueryPerformanceCounter(&qpc);
        srand((unsigned int)(GetTickCount() ^ qpc.LowPart ^ (GetCurrentProcessId() << 8)));
        s_seeded = 1;
    }
}

/* Export: Hook for Random Level selection (0x00419170 in Board::InitLevel) */
__declspec(dllexport) void* __cdecl zcm_PickRandomLevel(
    void* pLevelMgr,
    int difficulty,
    int level_num,
    void* pProfile,
    void* pRecentList,
    void* fnVanillaRandom,
    void* fnGetLevelDesc
) {
    EnsureRandomSeeded();
    BuildCustomMapsIfNeeded();

    if (!pLevelMgr) {
        return CallThiscallPickRandom(fnVanillaRandom, pLevelMgr, difficulty, level_num, pProfile, pRecentList);
    }

    const void* candidates[MAX_CUSTOM_MAPS + 64];
    int numVanilla = 0;
    int totalPool = BuildGauntletCandidates(pLevelMgr, pProfile, candidates, MAX_CUSTOM_MAPS + 64, &numVanilla);

    if (totalPool <= 0) {
        logf_("zcm_PickRandomLevel: candidate pool empty, delegating to vanilla");
        return CallThiscallPickRandom(fnVanillaRandom, pLevelMgr, difficulty, level_num, pProfile, pRecentList);
    }

    /* Pick a random board, retrying if in recent list */
    int pick = rand() % totalPool;
    if (totalPool > 1 && pRecentList) {
        for (int retry = 0; retry < 15; retry++) {
            const char* id = GetStdStringCStr(candidates[pick]);
            if (!IsInRecentList(pRecentList, id)) {
                break;
            }
            pick = rand() % totalPool;
        }
    }

    const void* pChosenIdStr = candidates[pick];
    const char* chosenId = GetStdStringCStr(pChosenIdStr);
    int isCustom = (pick >= numVanilla);

    logf_("zcm_PickRandomLevel: totalPool=%d (vanilla=%d, custom=%d), pick=%d -> id='%s' (isCustom=%d)",
          totalPool, numVanilla, s_customMapCount, pick, chosenId, isCustom);

    void* pDesc = CallThiscallGetLevelDesc(fnGetLevelDesc, pLevelMgr, pChosenIdStr, difficulty);
    if (!pDesc) {
        logf_("zcm_PickRandomLevel: WARNING GetLevelDesc returned NULL for '%s', falling back to vanilla", chosenId);
        return CallThiscallPickRandom(fnVanillaRandom, pLevelMgr, difficulty, level_num, pProfile, pRecentList);
    }

    return pDesc;
}

/* Export: Hook for Gauntlet progression level selection (0x00418F50 in Board::InitLevel) */
__declspec(dllexport) void* __cdecl zcm_PickGauntletLevel(
    void* pLevelMgr,
    int difficulty,
    int level_num,
    void* pProfile,
    void* fnVanillaGauntlet,
    void* fnGetLevelDesc
) {
    BuildCustomMapsIfNeeded();

    if (!pLevelMgr) {
        return CallThiscallPickGauntlet(fnVanillaGauntlet, pLevelMgr, difficulty, level_num, pProfile);
    }

    const void* candidates[MAX_CUSTOM_MAPS + 64];
    int numVanilla = 0;
    int totalPool = BuildGauntletCandidates(pLevelMgr, pProfile, candidates, MAX_CUSTOM_MAPS + 64, &numVanilla);

    if (totalPool <= 0) {
        logf_("zcm_PickGauntletLevel: candidate pool empty, delegating to vanilla");
        return CallThiscallPickGauntlet(fnVanillaGauntlet, pLevelMgr, difficulty, level_num, pProfile);
    }

    int cycleIdx = (level_num >= 0 ? level_num : 0) % totalPool;
    const void* pChosenIdStr = candidates[cycleIdx];
    const char* chosenId = GetStdStringCStr(pChosenIdStr);
    int isCustom = (cycleIdx >= numVanilla);

    logf_("zcm_PickGauntletLevel: level_num=%d, totalPool=%d (vanilla=%d, custom=%d) -> cycleIdx=%d ('%s', isCustom=%d)",
          level_num, totalPool, numVanilla, s_customMapCount, cycleIdx, chosenId, isCustom);

    void* pDesc = CallThiscallGetLevelDesc(fnGetLevelDesc, pLevelMgr, pChosenIdStr, difficulty);
    if (!pDesc) {
        logf_("zcm_PickGauntletLevel: WARNING GetLevelDesc returned NULL for '%s', falling back to vanilla", chosenId);
        return CallThiscallPickGauntlet(fnVanillaGauntlet, pLevelMgr, difficulty, 0, pProfile);
    }

    return pDesc;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        AddVectoredExceptionHandler(1, veh_);
        logf_("zumacustommap.dll loaded (Gauntlet Custom Map sidecar)");
    }
    return TRUE;
}
