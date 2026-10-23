#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#ifndef ZUCOMPLELUXE_VERSION
#define ZUCOMPLELUXE_VERSION "v0.5.1.1"
#endif

#ifndef TARGET_W
#define TARGET_W 800
#endif
#ifndef TARGET_H
#define TARGET_H 600
#endif

#define MIN_TARGET_W 480
#define MIN_TARGET_H 320

#define IDC_BTN_RUN  1001
#define IDC_BTN_QUIT 1002

typedef struct {
    const char *filename;
    const char *disp_name;
    const char *desc;
    bool is_mandatory;
    bool exists;
    bool is_lite_excluded;
    bool enabled;
    RECT rc_checkbox;
    RECT rc_row;
    bool hover;
} ModuleDep;

static ModuleDep g_modules[] = {
    { "zumadllchecker.dll", "zumadllchecker.dll", "Core Gatekeeper & Integrity Checker", true,  false, false, true,  {0}, {0}, false },
    { "zumacplxcredit.dll", "zumacplxcredit.dll", "Developer Credits Screen Mod",        false, false, false, true,  {0}, {0}, false },
    { "zumacustommap.dll",  "zumacustommap.dll",  "Gauntlet Custom Map Extender",        false, false, false, true,  {0}, {0}, false },
    { "zumatestingkit.dll", "zumatestingkit.dll", "Dev Testing Kit & Secret Temple 5",   false, false, false, true,  {0}, {0}, false },
    { "zumawebpbg.dll",     "zumawebpbg.dll",     "Animated WebP Background Canvas",     false, false, true,  true,  {0}, {0}, false },
    { "zumawebpsprite.dll", "zumawebpsprite.dll", "Multi-Layer Animated Sprite Engine",  false, false, true,  true,  {0}, {0}, false },
    { "zumaextrapath.dll",  "zumaextrapath.dll",  "Multi-Path Curve Engine (4-10)",      false, false, true,  true,  {0}, {0}, false },
    { "zumaextracolor.dll", "zumaextracolor.dll", "11-Ball Color Pipeline Engine",       false, false, true,  true,  {0}, {0}, false },
    /* Category 3: Resolution & Display Driver Modules (8..9) */
    { "zumadisplay.dll",    "zumadisplay.dll",    "Display & Window Driver (F11/DD)",    false, false, false, true,  {0}, {0}, false },
    { "zumareshandler.dll", "zumareshandler.dll", "Resolution & UI Layout Patcher",      false, false, false, true,  {0}, {0}, false }
};

#define MODULE_COUNT           ((int)(sizeof(g_modules) / sizeof(g_modules[0])))
#define GAMEPLAY_MODULE_COUNT  8
#define RES_MODULE_START       8
#define RES_MODULE_COUNT       2

typedef struct {
    char fileName[MAX_PATH];
    char productName[128];
    char fileVersion[64];
    char companyName[128];
    char specialBuild[128];
    char fileDescription[128];
} ZumaMetadata;

typedef struct {
    int w;
    int h;
    bool has_zres;
    char ratioStr[48];
} ResMetadata;

static ZumaMetadata g_meta;
static ResMetadata g_res;
static BOOL g_isDarkMode = FALSE;
static int g_dialogResult = 0;
static HWND g_hBtnRun = NULL;
static HWND g_hBtnQuit = NULL;
static bool g_btnRunHover = false;
static bool g_btnQuitHover = false;
static RECT g_rcLink = { 0, 0, 0, 0 };
static bool g_linkHover = false;
static HCURSOR g_hCurArrow = NULL;
static HCURSOR g_hCurHand = NULL;
static HBRUSH g_hBrDlgBg = NULL;

/* Detect Windows System App Theme (0 = Dark, 1 = Light) */
static BOOL DetectSystemDarkMode(void) {
    HKEY hKey;
    DWORD value = 1;
    DWORD size = sizeof(value);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, 
        "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "AppsUseLightTheme", NULL, NULL, (LPBYTE)&value, &size);
        RegCloseKey(hKey);
    }
    return (value == 0);
}

/* Enable DWM Immersive Dark Mode for window title bar */
static void ApplyDwmTitleBarTheme(HWND hwnd, BOOL isDark) {
    HMODULE hDwm = LoadLibraryA("dwmapi.dll");
    if (hDwm) {
        typedef HRESULT (WINAPI *pfnDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
        pfnDwmSetWindowAttribute set_attr = (pfnDwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (set_attr) {
            BOOL dark = isDark;
            set_attr(hwnd, 20, &dark, sizeof(dark));
            set_attr(hwnd, 19, &dark, sizeof(dark));
        }
        FreeLibrary(hDwm);
    }
}

/* Linear color blending for precise alpha opacity */
static inline COLORREF BlendColor(COLORREF fg, COLORREF bg, float alpha) {
    int r = (int)(GetRValue(fg) * alpha + GetRValue(bg) * (1.0f - alpha) + 0.5f);
    int g = (int)(GetGValue(fg) * alpha + GetGValue(bg) * (1.0f - alpha) + 0.5f);
    int b = (int)(GetBValue(fg) * alpha + GetBValue(bg) * (1.0f - alpha) + 0.5f);
    return RGB(r, g, b);
}

/* Detect target resolution and aspect ratio directly from PE section .zres and build configuration */
static void DetectResolutionInfo(void) {
    int w = 640;
    int h = 480;
    bool has_zres = false;

    HMODULE hExe = GetModuleHandleA(NULL);
    if (hExe) {
        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hExe;
        if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
            PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)hExe + dos->e_lfanew);
            if (nt->Signature == IMAGE_NT_SIGNATURE) {
                PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
                for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
                    if (strncmp((char*)sec[i].Name, ".zres", 5) == 0) {
                        has_zres = true;
                        BYTE *secData = (BYTE*)hExe + sec[i].VirtualAddress;
                        DWORD magic = *(DWORD*)secData;
                        if (magic == 0x53455A52) { /* 'ZRES' */
                            DWORD stored_w = *(DWORD*)(secData + 0x48);
                            DWORD stored_h = *(DWORD*)(secData + 0x4C);
                            if (stored_w >= MIN_TARGET_W && stored_h >= MIN_TARGET_H) {
                                w = (int)stored_w;
                                h = (int)stored_h;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

#ifdef TARGET_W
    if (!has_zres || w == 640) {
        w = TARGET_W;
    }
#endif
#ifdef TARGET_H
    if (!has_zres || h == 480) {
        h = TARGET_H;
    }
#endif

/* In Lite Edition, custom resolution engine is fully supported */

    /* Strictly enforce minimum limit 480x320 */
    if (has_zres) {
        if (w < MIN_TARGET_W) w = MIN_TARGET_W;
        if (h < MIN_TARGET_H) h = MIN_TARGET_H;
    }

    g_res.w = w;
    g_res.h = h;
    g_res.has_zres = has_zres;

    /* Calculate Aspect Ratio description */
    if (w * 3 == h * 4) {
        if (w == 640 && h == 480) strcpy(g_res.ratioStr, "4:3 (VGA Standard)");
        else if (w == 800 && h == 600) strcpy(g_res.ratioStr, "4:3 (SVGA Standard)");
        else snprintf(g_res.ratioStr, sizeof(g_res.ratioStr), "4:3 Standard");
    } else if (w * 9 == h * 16) {
        snprintf(g_res.ratioStr, sizeof(g_res.ratioStr), "16:9 (Widescreen)");
    } else if (w * 10 == h * 16) {
        snprintf(g_res.ratioStr, sizeof(g_res.ratioStr), "16:10 (Wide)");
    } else if (w * 4 == h * 5) {
        snprintf(g_res.ratioStr, sizeof(g_res.ratioStr), "5:4 (Standard)");
    } else if (w * 9 == h * 21) {
        snprintf(g_res.ratioStr, sizeof(g_res.ratioStr), "21:9 (Ultrawide)");
    } else {
        float r = (float)w / (float)h;
        snprintf(g_res.ratioStr, sizeof(g_res.ratioStr), "%.2f:1 (Custom)", r);
    }
}

/* Read Zuma.exe metadata strictly from PE Version Information */
static void LoadZumaMetadata(void) {
    memset(&g_meta, 0, sizeof(g_meta));
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    const char *pSlash = strrchr(exePath, '\\');
    if (!pSlash) pSlash = strrchr(exePath, '/');
    if (pSlash) {
        strncpy(g_meta.fileName, pSlash + 1, sizeof(g_meta.fileName) - 1);
    } else {
        strncpy(g_meta.fileName, exePath, sizeof(g_meta.fileName) - 1);
    }

    DWORD dwDummy = 0;
    DWORD dwSize = GetFileVersionInfoSizeA(exePath, &dwDummy);
    if (dwSize > 0) {
        BYTE *pData = (BYTE*)malloc(dwSize);
        if (pData) {
            if (GetFileVersionInfoA(exePath, 0, dwSize, pData)) {
                /* 1. Extract binary version from VS_FIXEDFILEINFO */
                VS_FIXEDFILEINFO *pFixed = NULL;
                UINT fixedLen = 0;
                if (VerQueryValueA(pData, "\\", (LPVOID*)&pFixed, &fixedLen) && pFixed && fixedLen >= sizeof(VS_FIXEDFILEINFO)) {
                    snprintf(g_meta.fileVersion, sizeof(g_meta.fileVersion), "%u.%u.%u.%u",
                             HIWORD(pFixed->dwFileVersionMS), LOWORD(pFixed->dwFileVersionMS),
                             HIWORD(pFixed->dwFileVersionLS), LOWORD(pFixed->dwFileVersionLS));
                }

                /* 2. Extract StringFileInfo from Translation block */
                struct LANGANDCODEPAGE {
                    WORD wLanguage;
                    WORD wCodePage;
                } *lpTranslate = NULL;
                UINT cbTranslate = 0;

                if (VerQueryValueA(pData, "\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &cbTranslate) && cbTranslate >= sizeof(struct LANGANDCODEPAGE)) {
                    int numTranslations = cbTranslate / sizeof(struct LANGANDCODEPAGE);
                    for (int t = 0; t < numTranslations; t++) {
                        char subBlock[128];
                        char *strVal = NULL;
                        UINT strLen = 0;

                        #define QUERY_META(key, dst) do { \
                            if (dst[0] == '\0') { \
                                snprintf(subBlock, sizeof(subBlock), "\\StringFileInfo\\%04x%04x\\" key, lpTranslate[t].wLanguage, lpTranslate[t].wCodePage); \
                                if (VerQueryValueA(pData, subBlock, (LPVOID*)&strVal, &strLen) && strVal && strLen > 0) { \
                                    strncpy(dst, strVal, sizeof(dst) - 1); \
                                } \
                            } \
                        } while(0)

                        QUERY_META("ProductName", g_meta.productName);
                        QUERY_META("FileVersion", g_meta.fileVersion);
                        QUERY_META("CompanyName", g_meta.companyName);
                        QUERY_META("SpecialBuild", g_meta.specialBuild);
                        QUERY_META("FileDescription", g_meta.fileDescription);
                    }
                }
            }
            free(pData);
        }
    }

    /* Fallback resolution if metadata is omitted from PE Version Information */
    if (g_meta.productName[0] == '\0') strcpy(g_meta.productName, "Zuma Deluxe");
    if (g_meta.fileVersion[0] == '\0') strcpy(g_meta.fileVersion, "1.0.0.1");

    /* Version string normalization: convert "1, 0, 0, 1" to "1.0.0.1" */
    for (char *p = g_meta.fileVersion; *p; p++) {
        if (*p == ',') {
            *p = '.';
        } else if (*p == ' ') {
            memmove(p, p + 1, strlen(p));
            p--;
        }
    }

    /* 1. Publisher resolution with honest detection tag */
    if (g_meta.companyName[0] == '\0' || strcmp(g_meta.companyName, "N/A") == 0) {
        strcpy(g_meta.companyName, "PopCap Games, Inc. (detected)");
    }

    /* 2. Special Build / Distributor resolution (Strict hierarchy & source tagging) */
    if (g_meta.specialBuild[0] == '\0' || strcmp(g_meta.specialBuild, "None") == 0) {
        char launchBuf[512] = {0};
        FILE *fp = fopen("launch.ini", "rb");
        if (fp) {
            fread(launchBuf, 1, sizeof(launchBuf) - 1, fp);
            fclose(fp);
        }

        char partnerBuf[1024] = {0};
        fp = fopen("properties\\partner.xml", "rb");
        if (fp) {
            fread(partnerBuf, 1, sizeof(partnerBuf) - 1, fp);
            fclose(fp);
        }

        if (strstr(launchBuf, "RealArcade=Yes")) {
            strcpy(g_meta.specialBuild, "Presented by RealArcade (launch.ini)");
        } else if (strstr(partnerBuf, "real.com")) {
            strcpy(g_meta.specialBuild, "Presented by RealArcade (partner.xml)");
        } else if (strstr(partnerBuf, "gamehouse.com")) {
            strcpy(g_meta.specialBuild, "Presented by GameHouse (partner.xml)");
        } else if (strstr(partnerBuf, "zone.msn.com") || strstr(partnerBuf, "msn.com")) {
            strcpy(g_meta.specialBuild, "Presented by MSN Games (partner.xml)");
        } else if (strstr(partnerBuf, "shockwave.com")) {
            strcpy(g_meta.specialBuild, "Presented by Shockwave (partner.xml)");
        } else if (strstr(partnerBuf, "bigfish")) {
            strcpy(g_meta.specialBuild, "Presented by Big Fish Games (partner.xml)");
        } else {
            strcpy(g_meta.specialBuild, "None detected");
        }
    }
}

/* Check existence of all sidecar DLLs in modules/ */
static void CheckModules(void) {
    for (int i = 0; i < MODULE_COUNT; i++) {
#ifdef ZUCOMPLELUXE_LITE
        if (g_modules[i].is_lite_excluded) {
            /* In Lite Edition, excluded sidecars are always disabled regardless of files on disk */
            g_modules[i].exists = false;
            g_modules[i].enabled = false;
            continue;
        }
#endif
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "modules\\%s", g_modules[i].filename);
        DWORD attr = GetFileAttributesA(path);
        g_modules[i].exists = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
        g_modules[i].enabled = g_modules[i].exists;
    }
}

/* Draw a classic Windows GroupBox with seamless background-cut title */
static void DrawClassicGroupBox(HDC hdc, RECT *rc, const char *title, COLORREF borderCol, COLORREF bgCol, COLORREF textCol, HFONT hFont) {
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

    SIZE sz;
    GetTextExtentPoint32A(hdc, title, (int)strlen(title), &sz);

    int textX = rc->left + 12;
    int topY = rc->top + (sz.cy / 2);

    HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    MoveToEx(hdc, rc->left, topY, NULL);
    LineTo(hdc, textX - 4, topY);

    MoveToEx(hdc, textX + sz.cx + 4, topY, NULL);
    LineTo(hdc, rc->right, topY);
    LineTo(hdc, rc->right, rc->bottom);
    LineTo(hdc, rc->left, rc->bottom);
    LineTo(hdc, rc->left, topY);

    SelectObject(hdc, oldPen);
    DeleteObject(hPen);

    SetTextColor(hdc, textCol);
    SetBkColor(hdc, bgCol);
    SetBkMode(hdc, OPAQUE);
    TextOutA(hdc, textX, rc->top, title, (int)strlen(title));

    SelectObject(hdc, oldFont);
}

/* Draw modern checkbox with strict 50% opacity when unusable/unclickable */
static void DrawModernCheckbox(HDC hdc, RECT *rc, bool isChecked, bool isMandatory, bool isMissing, bool isLiteExcluded, bool isHover, BOOL isDark, COLORREF dlgBg) {
    int boxSize = 14;
    int boxX = rc->left + ((rc->right - rc->left) - boxSize) / 2;
    int boxY = rc->top + ((rc->bottom - rc->top) - boxSize) / 2;

    bool isUnusable = isMandatory || isMissing || isLiteExcluded;

    COLORREF bgCol, borderCol, checkCol;
    if (isDark) {
        if (isLiteExcluded) {
            bgCol = RGB(36, 36, 40);
            borderCol = RGB(85, 85, 95);
            checkCol = RGB(150, 150, 160);
        } else if (isMissing) {
            bgCol = RGB(45, 25, 25);
            borderCol = RGB(160, 45, 45);
            checkCol = RGB(239, 68, 68);
        } else if (isMandatory) {
            bgCol = RGB(16, 38, 58);
            borderCol = RGB(56, 189, 248);
            checkCol = RGB(56, 189, 248);
        } else if (isChecked) {
            bgCol = isHover ? RGB(26, 48, 36) : RGB(20, 38, 28);
            borderCol = isHover ? RGB(74, 222, 128) : RGB(46, 160, 90);
            checkCol = RGB(74, 222, 128);
        } else {
            bgCol = isHover ? RGB(45, 45, 52) : RGB(35, 35, 40);
            borderCol = isHover ? RGB(100, 100, 115) : RGB(70, 70, 80);
            checkCol = RGB(0, 0, 0);
        }
    } else {
        if (isLiteExcluded) {
            bgCol = RGB(242, 242, 245);
            borderCol = RGB(175, 175, 182);
            checkCol = RGB(130, 130, 138);
        } else if (isMissing) {
            bgCol = RGB(254, 242, 242);
            borderCol = RGB(239, 68, 68);
            checkCol = RGB(220, 38, 38);
        } else if (isMandatory) {
            bgCol = RGB(224, 242, 254);
            borderCol = RGB(2, 132, 199);
            checkCol = RGB(2, 132, 199);
        } else if (isChecked) {
            bgCol = isHover ? RGB(220, 252, 231) : RGB(240, 253, 244);
            borderCol = isHover ? RGB(22, 163, 74) : RGB(34, 197, 94);
            checkCol = RGB(22, 128, 61);
        } else {
            bgCol = isHover ? RGB(240, 240, 245) : RGB(255, 255, 255);
            borderCol = isHover ? RGB(120, 120, 130) : RGB(170, 170, 175);
            checkCol = RGB(0, 0, 0);
        }
    }

    /* Apply strict 50% opacity (faded/disabled) when module cannot be toggled */
    if (isUnusable) {
        bgCol     = BlendColor(bgCol, dlgBg, 0.50f);
        borderCol = BlendColor(borderCol, dlgBg, 0.50f);
        checkCol  = BlendColor(checkCol, dlgBg, 0.50f);
    }

    HBRUSH hBr = CreateSolidBrush(bgCol);
    HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, boxX, boxY, boxX + boxSize, boxY + boxSize, 3, 3);

    if (isChecked) {
        HPEN hCheckPen = CreatePen(PS_SOLID, 2, checkCol);
        SelectObject(hdc, hCheckPen);
        MoveToEx(hdc, boxX + 3, boxY + 7, NULL);
        LineTo(hdc, boxX + 6, boxY + 10);
        LineTo(hdc, boxX + 11, boxY + 4);
        DeleteObject(hCheckPen);
    } else if (isLiteExcluded) {
        /* Omitted sidecar in Lite Edition: subtle horizontal grey dash */
        HPEN hDashPen = CreatePen(PS_SOLID, 2, checkCol);
        SelectObject(hdc, hDashPen);
        MoveToEx(hdc, boxX + 4, boxY + 7, NULL);
        LineTo(hdc, boxX + 11, boxY + 7);
        DeleteObject(hDashPen);
    } else if (isMissing) {
        HPEN hXPen = CreatePen(PS_SOLID, 1, checkCol);
        SelectObject(hdc, hXPen);
        MoveToEx(hdc, boxX + 4, boxY + 4, NULL);
        LineTo(hdc, boxX + 10, boxY + 10);
        MoveToEx(hdc, boxX + 10, boxY + 4, NULL);
        LineTo(hdc, boxX + 4, boxY + 10);
        DeleteObject(hXPen);
    }

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);
}

/* Draw classic button with pixel-perfect background clear (no bright corner dots) */
static void DrawClassicButton(DRAWITEMSTRUCT *dis, const char *label, bool isDefault, bool isHover, BOOL isDark, HFONT hFont) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool isPressed = (dis->itemState & ODS_SELECTED) != 0;

    /* 1. Pre-fill entire button DC with exact dialog background color to prevent light corner artifacts */
    COLORREF dlgBg = isDark ? RGB(32, 32, 36) : GetSysColor(COLOR_BTNFACE);
    HBRUSH hBrDlg = CreateSolidBrush(dlgBg);
    FillRect(hdc, &rc, hBrDlg);
    DeleteObject(hBrDlg);

    COLORREF bgCol, borderCol, textCol;
    if (isDark) {
        if (isPressed) {
            bgCol = RGB(35, 35, 38);
            borderCol = isDefault ? RGB(0, 120, 215) : RGB(90, 90, 95);
        } else if (isHover) {
            bgCol = RGB(55, 55, 60);
            borderCol = isDefault ? RGB(30, 144, 255) : RGB(100, 100, 108);
        } else {
            bgCol = RGB(45, 45, 48);
            borderCol = isDefault ? RGB(0, 120, 215) : RGB(75, 75, 82);
        }
        textCol = RGB(240, 240, 245);
    } else {
        if (isPressed) {
            bgCol = RGB(204, 206, 212);
            borderCol = isDefault ? RGB(0, 102, 204) : RGB(140, 140, 145);
        } else if (isHover) {
            bgCol = RGB(229, 231, 236);
            borderCol = isDefault ? RGB(0, 102, 204) : RGB(160, 160, 165);
        } else {
            bgCol = RGB(240, 240, 243);
            borderCol = isDefault ? RGB(0, 102, 204) : RGB(180, 180, 185);
        }
        textCol = RGB(20, 20, 25);
    }

    HBRUSH hBr = CreateSolidBrush(bgCol);
    HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 4, 4);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);

    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textCol);

    DrawTextA(hdc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
}

/* Render a single module table row (checkbox, name, description, status) */
static void DrawModuleRow(HDC hdc, int i, int rowY, COLORREF colTextPri, COLORREF colTextSec, COLORREF colSuccess, COLORREF colError, COLORREF colDisabled, COLORREF colBadgeBg, COLORREF colBadgeBorder, COLORREF colBadgeText, COLORREF dlgBg, HFONT hFontNormal, HFONT hFontSemibold, HFONT hFontBold) {
#ifdef ZUCOMPLELUXE_LITE
    bool isExcluded = g_modules[i].is_lite_excluded;
#else
    bool isExcluded = false;
#endif
    bool isUnusable = g_modules[i].is_mandatory || !g_modules[i].exists || isExcluded;

    /* Checkbox clickable rect */
    g_modules[i].rc_checkbox.left   = 34;
    g_modules[i].rc_checkbox.top    = rowY - 1;
    g_modules[i].rc_checkbox.right  = 54;
    g_modules[i].rc_checkbox.bottom = rowY + 17;

    /* Entire row clickable rect */
    g_modules[i].rc_row.left   = 28;
    g_modules[i].rc_row.top    = rowY - 2;
    g_modules[i].rc_row.right  = 620;
    g_modules[i].rc_row.bottom = rowY + 18;

    /* Draw Checkbox with strict 50% opacity when unusable (required, missing, or excluded) */
    DrawModernCheckbox(hdc, &g_modules[i].rc_checkbox,
                       g_modules[i].enabled,
                       g_modules[i].is_mandatory,
                       !g_modules[i].exists,
                       isExcluded,
                       isUnusable ? false : g_modules[i].hover,
                       g_isDarkMode,
                       dlgBg);

    /* Column 2: Module name */
    SelectObject(hdc, hFontNormal);
    COLORREF nameCol = isUnusable ? BlendColor(colTextPri, dlgBg, 0.70f) : (g_modules[i].enabled ? colTextPri : colDisabled);
    SetTextColor(hdc, nameCol);
    TextOutA(hdc, 88, rowY, g_modules[i].disp_name, (int)strlen(g_modules[i].disp_name));

    /* Column 3: Description */
    COLORREF descCol = isUnusable ? BlendColor(colTextSec, dlgBg, 0.70f) : (g_modules[i].enabled ? colTextSec : colDisabled);
    SetTextColor(hdc, descCol);
    TextOutA(hdc, 232, rowY, g_modules[i].desc, (int)strlen(g_modules[i].desc));

    /* Column 4: Status */
    if (g_modules[i].is_mandatory) {
        SelectObject(hdc, hFontBold);
        const char *reqText = "[OK] Required";
        SIZE sz;
        GetTextExtentPoint32A(hdc, reqText, (int)strlen(reqText), &sz);

        RECT rcBadge = { 491, rowY - 2, 491 + sz.cx + 8, rowY + sz.cy + 1 };
        HBRUSH hBr = CreateSolidBrush(colBadgeBg);
        HPEN hPen = CreatePen(PS_SOLID, 1, colBadgeBorder);
        HBRUSH oldBr = (HBRUSH)SelectObject(hdc, hBr);
        HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
        RoundRect(hdc, rcBadge.left, rcBadge.top, rcBadge.right, rcBadge.bottom, 4, 4);
        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPen);
        DeleteObject(hBr);
        DeleteObject(hPen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, colBadgeText);
        TextOutA(hdc, 495, rowY, reqText, (int)strlen(reqText));
    } else {
        SelectObject(hdc, hFontSemibold);
#ifdef ZUCOMPLELUXE_LITE
        if (g_modules[i].is_lite_excluded) {
            SetTextColor(hdc, colDisabled);
            TextOutA(hdc, 495, rowY, "[OMITTED]", 9);
        } else
#endif
        if (!g_modules[i].exists) {
            SetTextColor(hdc, colError);
            TextOutA(hdc, 495, rowY, "[MISSING]", 9);
        } else if (!g_modules[i].enabled) {
            COLORREF colOff = g_isDarkMode ? RGB(251, 146, 60) : RGB(194, 65, 12);
            SetTextColor(hdc, colOff);
            TextOutA(hdc, 495, rowY, "[OFF] Disabled", 14);
        } else {
            SetTextColor(hdc, colSuccess);
            TextOutA(hdc, 495, rowY, "[OK] Present", 12);
        }
    }
}

/* Window procedure for Classic Dialog Window */
static LRESULT CALLBACK CheckerWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        ApplyDwmTitleBarTheme(hwnd, g_isDarkMode);

        g_hBtnQuit = CreateWindowExA(0, "BUTTON", "Quit",
                                     WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                     484, 456, 68, 25, hwnd, (HMENU)IDC_BTN_QUIT, GetModuleHandle(NULL), NULL);

        g_hBtnRun = CreateWindowExA(0, "BUTTON", "Run",
                                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                    562, 456, 68, 25, hwnd, (HMENU)IDC_BTN_RUN, GetModuleHandle(NULL), NULL);
        return 0;
    }

    case WM_CTLCOLORBTN: {
        HDC hdcBtn = (HDC)wParam;
        COLORREF colBg = g_isDarkMode ? RGB(32, 32, 36) : GetSysColor(COLOR_BTNFACE);
        SetBkColor(hdcBtn, colBg);
        if (g_hBrDlgBg) DeleteObject(g_hBrDlgBg);
        g_hBrDlgBg = CreateSolidBrush(colBg);
        return (LRESULT)g_hBrDlgBg;
    }

    case WM_SETTINGCHANGE: {
        BOOL newTheme = DetectSystemDarkMode();
        if (newTheme != g_isDarkMode) {
            g_isDarkMode = newTheme;
            ApplyDwmTitleBarTheme(hwnd, g_isDarkMode);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT*)lParam;
        HFONT hBtnFont = CreateFontA(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        if (dis->CtlID == IDC_BTN_RUN) {
            DrawClassicButton(dis, "Run", true, g_btnRunHover, g_isDarkMode, hBtnFont);
            DeleteObject(hBtnFont);
            return TRUE;
        } else if (dis->CtlID == IDC_BTN_QUIT) {
            DrawClassicButton(dis, "Quit", false, g_btnQuitHover, g_isDarkMode, hBtnFont);
            DeleteObject(hBtnFont);
            return TRUE;
        }
        DeleteObject(hBtnFont);
        break;
    }

    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        if (id == IDC_BTN_RUN || id == IDOK) {
            g_dialogResult = 1;
            DestroyWindow(hwnd);
            return 0;
        } else if (id == IDC_BTN_QUIT || id == IDCANCEL) {
            g_dialogResult = 0;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdcWindow = BeginPaint(hwnd, &ps);

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);
        int w = clientRc.right - clientRc.left;
        int h = clientRc.bottom - clientRc.top;

        HDC hdc = CreateCompatibleDC(hdcWindow);
        HBITMAP hbm = CreateCompatibleBitmap(hdcWindow, w, h);
        HBITMAP oldBm = (HBITMAP)SelectObject(hdc, hbm);

        COLORREF colBg          = g_isDarkMode ? RGB(32, 32, 36)   : GetSysColor(COLOR_BTNFACE);
        COLORREF colBorder      = g_isDarkMode ? RGB(65, 65, 72)   : RGB(180, 180, 185);
        COLORREF colTextPri     = g_isDarkMode ? RGB(235, 235, 240): RGB(20, 20, 20);
        COLORREF colTextSec     = g_isDarkMode ? RGB(160, 160, 168): RGB(105, 105, 112);
        COLORREF colSuccess     = g_isDarkMode ? RGB(74, 222, 128) : RGB(22, 128, 61);
        COLORREF colError       = g_isDarkMode ? RGB(248, 113, 113): RGB(220, 38, 38);
        COLORREF colDisabled    = g_isDarkMode ? RGB(130, 130, 138): RGB(130, 130, 135);
        COLORREF colWarning     = g_isDarkMode ? RGB(250, 204, 21) : RGB(180, 83, 9);
        COLORREF colBadgeBg     = g_isDarkMode ? RGB(12, 45, 72)   : RGB(224, 242, 254);
        COLORREF colBadgeBorder = g_isDarkMode ? RGB(56, 189, 248) : RGB(2, 132, 199);
        COLORREF colBadgeText   = g_isDarkMode ? RGB(56, 189, 248) : RGB(2, 132, 199);

        HBRUSH hBrBg = CreateSolidBrush(colBg);
        FillRect(hdc, &clientRc, hBrBg);
        DeleteObject(hBrBg);

        /* Comfortable standard font sizes */
        HFONT hFontBold = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT hFontSemibold = CreateFontA(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT hFontNormal = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT hFontNote = CreateFontA(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        /* --- 1. Game & Patch Information GroupBox --- */
        RECT rcGroup1 = { 16, 10, 630, 112 };
        DrawClassicGroupBox(hdc, &rcGroup1, " Game & Patch Information ", colBorder, colBg, colTextPri, hFontSemibold);

        SelectObject(hdc, hFontNormal);
        SetBkMode(hdc, TRANSPARENT);

        /* Left column: Vanilla Game Info */
        SetTextColor(hdc, colTextSec);
        TextOutA(hdc, 32, 28, "Product:", 8);
        TextOutA(hdc, 32, 48, "Publisher:", 10);
        TextOutA(hdc, 32, 68, "Version:", 8);
        TextOutA(hdc, 32, 88, "Special Build:", 14);

        SetTextColor(hdc, colTextPri);
        TextOutA(hdc, 132, 28, g_meta.productName, (int)strlen(g_meta.productName));
        TextOutA(hdc, 132, 48, g_meta.companyName, (int)strlen(g_meta.companyName));
        TextOutA(hdc, 132, 68, g_meta.fileVersion, (int)strlen(g_meta.fileVersion));
        TextOutA(hdc, 132, 88, g_meta.specialBuild, (int)strlen(g_meta.specialBuild));

        /* Right column: ZuCompleluxe Project & Author Info */
        SetTextColor(hdc, colTextSec);
        TextOutA(hdc, 350, 28, "Patch:", 6);
        TextOutA(hdc, 350, 48, "Developer:", 10);
        TextOutA(hdc, 350, 68, "License:", 8);
        TextOutA(hdc, 350, 88, "Target File:", 12);

        SetTextColor(hdc, colTextPri);
        char patchBuf[64];
#ifdef ZUCOMPLELUXE_LITE
        snprintf(patchBuf, sizeof(patchBuf), "ZuCompleluxe %s Lite", ZUCOMPLELUXE_VERSION);
#else
        snprintf(patchBuf, sizeof(patchBuf), "ZuCompleluxe %s", ZUCOMPLELUXE_VERSION);
#endif
        TextOutA(hdc, 432, 28, patchBuf, (int)strlen(patchBuf));

        /* Interactive Hyperlink: Rizky Bayuu */
        HFONT hFontLink = CreateFontA(15, 0, 0, 0, FW_SEMIBOLD, FALSE, TRUE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        SelectObject(hdc, hFontLink);

        SIZE szLink;
        GetTextExtentPoint32A(hdc, "Rizky Bayuu", 11, &szLink);
        g_rcLink.left = 432;
        g_rcLink.top = 48;
        g_rcLink.right = 432 + szLink.cx;
        g_rcLink.bottom = 48 + szLink.cy;

        COLORREF colLink = g_isDarkMode 
            ? (g_linkHover ? RGB(121, 192, 255) : RGB(88, 166, 255))
            : (g_linkHover ? RGB(5, 80, 174)    : RGB(9, 105, 218));
        SetTextColor(hdc, colLink);
        TextOutA(hdc, 432, 48, "Rizky Bayuu", 11);
        DeleteObject(hFontLink);

        SelectObject(hdc, hFontNormal);
        SetTextColor(hdc, colTextPri);
        TextOutA(hdc, 432, 68, "MIT License", 11);
        TextOutA(hdc, 432, 88, g_meta.fileName, (int)strlen(g_meta.fileName));

        /* --- 2. Sidecar Module Dependencies GroupBox (Gameplay & Content: 8 Modules) --- */
        RECT rcGroup2 = { 16, 118, 630, 314 };
#ifdef ZUCOMPLELUXE_LITE
        DrawClassicGroupBox(hdc, &rcGroup2, " Sidecar Module Dependencies (modules\\) [Lite Edition] ", colBorder, colBg, colTextPri, hFontSemibold);
#else
        DrawClassicGroupBox(hdc, &rcGroup2, " Sidecar Module Dependencies (modules\\) ", colBorder, colBg, colTextPri, hFontSemibold);
#endif

        /* Table 1 Column Headers */
        SelectObject(hdc, hFontSemibold);
        SetTextColor(hdc, colTextSec);
        TextOutA(hdc, 30, 135, "Enable", 6);
        TextOutA(hdc, 88, 135, "Module", 6);
        TextOutA(hdc, 232, 135, "Description", 11);
        TextOutA(hdc, 495, 135, "Status", 6);

        /* Separator line under table 1 header */
        HPEN hPenSep = CreatePen(PS_SOLID, 1, colBorder);
        HPEN oldPen = (HPEN)SelectObject(hdc, hPenSep);
        MoveToEx(hdc, 26, 152, NULL);
        LineTo(hdc, 620, 152);
        SelectObject(hdc, oldPen);
        DeleteObject(hPenSep);

        /* Render 8 Gameplay & Content Sidecars (indices 0..7) */
        int rowY = 158;
        int rowGap = 19;

        for (int i = 0; i < GAMEPLAY_MODULE_COUNT; i++) {
            DrawModuleRow(hdc, i, rowY, colTextPri, colTextSec, colSuccess, colError, colDisabled,
                          colBadgeBg, colBadgeBorder, colBadgeText, colBg, hFontNormal, hFontSemibold, hFontBold);
            rowY += rowGap;
        }

        /* --- 3. Display & Custom Resolution System GroupBox --- */
        RECT rcGroup3 = { 16, 322, 630, 446 };
#ifdef ZUCOMPLELUXE_LITE
        DrawClassicGroupBox(hdc, &rcGroup3, " Display & Custom Resolution System (.zres) [Lite Edition] ", colBorder, colBg, colTextPri, hFontSemibold);
#else
        DrawClassicGroupBox(hdc, &rcGroup3, " Display & Custom Resolution System (.zres) ", colBorder, colBg, colTextPri, hFontSemibold);
#endif

        /* Line 1: Resolution Summary Row */
        SelectObject(hdc, hFontNormal);
        SetTextColor(hdc, colTextSec);
        TextOutA(hdc, 30, 338, "Resolution:", 11);

        char resStr[48];
        snprintf(resStr, sizeof(resStr), "%d x %d", g_res.w, g_res.h);
        SelectObject(hdc, hFontBold);
        SetTextColor(hdc, colTextPri);
        TextOutA(hdc, 108, 338, resStr, (int)strlen(resStr));

        SelectObject(hdc, hFontNormal);
        SetTextColor(hdc, colTextSec);
        TextOutA(hdc, 194, 338, "Aspect Ratio:", 13);
        SelectObject(hdc, hFontSemibold);
        SetTextColor(hdc, colTextPri);
        TextOutA(hdc, 282, 338, g_res.ratioStr, (int)strlen(g_res.ratioStr));

        SelectObject(hdc, hFontNormal);
        SetTextColor(hdc, colTextSec);
        TextOutA(hdc, 480, 338, "Limit:", 6);
        SelectObject(hdc, hFontSemibold);
        SetTextColor(hdc, colWarning);
        TextOutA(hdc, 524, 338, "480 x 320", 9);

        /* Line 2: Asset Mode & imagescustomres/ explanation */
        SelectObject(hdc, hFontNormal);
        SetTextColor(hdc, colTextSec);
        TextOutA(hdc, 30, 356, "Asset Mode:", 11);

        if (g_res.has_zres && (g_res.w != 640 || g_res.h != 480)) {
            SelectObject(hdc, hFontSemibold);
            SetTextColor(hdc, colTextPri);
            TextOutA(hdc, 114, 356, "imagescustomres/", 16);
            SelectObject(hdc, hFontNote);
            SetTextColor(hdc, colTextSec);
            TextOutA(hdc, 236, 358, "(Auto-loads scaled UI graphics; fallback to images/)", 52);
        } else {
            SelectObject(hdc, hFontSemibold);
            SetTextColor(hdc, colTextPri);
            TextOutA(hdc, 114, 356, "images/", 7);
            SelectObject(hdc, hFontNote);
            SetTextColor(hdc, colTextSec);
            TextOutA(hdc, 170, 358, "(Vanilla 640x480 standard assets)", 33);
        }

        /* Separator line between resolution info and table 2 */
        HPEN hPenSep2 = CreatePen(PS_SOLID, 1, colBorder);
        HPEN oldPen2 = (HPEN)SelectObject(hdc, hPenSep2);
        MoveToEx(hdc, 26, 374, NULL);
        LineTo(hdc, 620, 374);
        SelectObject(hdc, oldPen2);
        DeleteObject(hPenSep2);

        /* Table 2 Column Headers for Resolution Modules */
        SelectObject(hdc, hFontSemibold);
        SetTextColor(hdc, colTextSec);
        TextOutA(hdc, 30, 380, "Enable", 6);
        TextOutA(hdc, 88, 380, "Module", 6);
        TextOutA(hdc, 232, 380, "Description", 11);
        TextOutA(hdc, 495, 380, "Status", 6);

        /* Separator line under table 2 header */
        HPEN hPenSep3 = CreatePen(PS_SOLID, 1, colBorder);
        HPEN oldPen3 = (HPEN)SelectObject(hdc, hPenSep3);
        MoveToEx(hdc, 26, 398, NULL);
        LineTo(hdc, 620, 398);
        SelectObject(hdc, oldPen3);
        DeleteObject(hPenSep3);

        /* Render 2 Resolution Sidecars (indices 8..9: zumadisplay.dll, zumareshandler.dll) */
        int rowY2 = 404;
        for (int i = RES_MODULE_START; i < RES_MODULE_START + RES_MODULE_COUNT; i++) {
            DrawModuleRow(hdc, i, rowY2, colTextPri, colTextSec, colSuccess, colError, colDisabled,
                          colBadgeBg, colBadgeBorder, colBadgeText, colBg, hFontNormal, hFontSemibold, hFontBold);
            rowY2 += rowGap;
        }

        /* --- 4. Bottom Footer Note --- */
        SelectObject(hdc, hFontNote);
        SetTextColor(hdc, colTextSec);
#ifdef ZUCOMPLELUXE_LITE
        TextOutA(hdc, 20, 456, "Lite Edition: WebP, Multi-Path & 11-Colors omitted. Press F11 for borderless fullscreen.", 88);
        TextOutA(hdc, 20, 472, "Press Run to start Zuma Deluxe, or Quit to exit.", 48);
#else
        TextOutA(hdc, 20, 456, "Tip: Click checkboxes to toggle optional sidecars. Press F11 for borderless fullscreen.", 87);
        TextOutA(hdc, 20, 472, "Press Run to start Zuma Deluxe, or Quit to exit.", 48);
#endif

        DeleteObject(hFontBold);
        DeleteObject(hFontSemibold);
        DeleteObject(hFontNormal);
        DeleteObject(hFontNote);

        BitBlt(hdcWindow, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

        SelectObject(hdc, oldBm);
        DeleteObject(hbm);
        DeleteDC(hdc);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT rcRun, rcQuit;
        GetWindowRect(g_hBtnRun, &rcRun);
        GetWindowRect(g_hBtnQuit, &rcQuit);
        MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&rcRun, 2);
        MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&rcQuit, 2);

        bool newRunHover = PtInRect(&rcRun, pt);
        bool newQuitHover = PtInRect(&rcQuit, pt);
        bool newLinkHover = PtInRect(&g_rcLink, pt);

        if (newRunHover != g_btnRunHover || newQuitHover != g_btnQuitHover) {
            g_btnRunHover = newRunHover;
            g_btnQuitHover = newQuitHover;
            InvalidateRect(g_hBtnRun, NULL, TRUE);
            InvalidateRect(g_hBtnQuit, NULL, TRUE);
        }

        if (newLinkHover != g_linkHover) {
            g_linkHover = newLinkHover;
            InvalidateRect(hwnd, &g_rcLink, TRUE);
        }

        bool overClickable = newLinkHover;
        for (int i = 0; i < MODULE_COUNT; i++) {
#ifdef ZUCOMPLELUXE_LITE
            bool isExcluded = g_modules[i].is_lite_excluded;
#else
            bool isExcluded = false;
#endif
            bool canToggle = !g_modules[i].is_mandatory && g_modules[i].exists && !isExcluded;
            bool isOverRow = PtInRect(&g_modules[i].rc_row, pt);
            if (canToggle) {
                if (isOverRow != g_modules[i].hover) {
                    g_modules[i].hover = isOverRow;
                    InvalidateRect(hwnd, &g_modules[i].rc_row, FALSE);
                }
                if (isOverRow) {
                    overClickable = true;
                }
            } else {
                g_modules[i].hover = false;
            }
        }

        SetCursor(overClickable ? g_hCurHand : g_hCurArrow);

        TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE: {
        if (g_btnRunHover || g_btnQuitHover) {
            g_btnRunHover = false;
            g_btnQuitHover = false;
            InvalidateRect(g_hBtnRun, NULL, TRUE);
            InvalidateRect(g_hBtnQuit, NULL, TRUE);
        }
        if (g_linkHover) {
            g_linkHover = false;
            InvalidateRect(hwnd, &g_rcLink, TRUE);
        }
        for (int i = 0; i < MODULE_COUNT; i++) {
            if (g_modules[i].hover) {
                g_modules[i].hover = false;
                InvalidateRect(hwnd, &g_modules[i].rc_row, FALSE);
            }
        }
        SetCursor(g_hCurArrow);
        return 0;
    }

    case WM_SETCURSOR: {
        if (LOWORD(lParam) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            bool overClickable = PtInRect(&g_rcLink, pt);
            if (!overClickable) {
                for (int i = 0; i < MODULE_COUNT; i++) {
#ifdef ZUCOMPLELUXE_LITE
                    bool isExcluded = g_modules[i].is_lite_excluded;
#else
                    bool isExcluded = false;
#endif
                    bool canToggle = !g_modules[i].is_mandatory && g_modules[i].exists && !isExcluded;
                    if (canToggle && PtInRect(&g_modules[i].rc_row, pt)) {
                        overClickable = true;
                        break;
                    }
                }
            }
            SetCursor(overClickable ? g_hCurHand : g_hCurArrow);
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PtInRect(&g_rcLink, pt)) {
            ShellExecuteA(hwnd, "open", "https://github.com/rizkybayuu", NULL, NULL, SW_SHOWNORMAL);
            return 0;
        }

        /* Check module row click / checkbox toggle */
        for (int i = 0; i < MODULE_COUNT; i++) {
#ifdef ZUCOMPLELUXE_LITE
            bool isExcluded = g_modules[i].is_lite_excluded;
#else
            bool isExcluded = false;
#endif
            bool canToggle = !g_modules[i].is_mandatory && g_modules[i].exists && !isExcluded;
            if (canToggle && PtInRect(&g_modules[i].rc_row, pt)) {
                g_modules[i].enabled = !g_modules[i].enabled;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
        }
        break;
    }

    case WM_CLOSE: {
        g_dialogResult = 0;
        DestroyWindow(hwnd);
        return 0;
    }

    case WM_DESTROY: {
        if (g_hBrDlgBg) {
            DeleteObject(g_hBrDlgBg);
            g_hBrDlgBg = NULL;
        }
        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

/* Neutralize disabled sidecar DLL string references inside the host executable memory */
static void ApplyModuleStates(void) {
    HMODULE hExe = GetModuleHandleA(NULL);
    if (!hExe) return;

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hExe;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)hExe + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;

    for (int i = 0; i < MODULE_COUNT; i++) {
        if (!g_modules[i].enabled && !g_modules[i].is_mandatory) {
            char target[64];
            snprintf(target, sizeof(target), "modules\\%s", g_modules[i].filename);
            size_t tlen = strlen(target);

            PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
            for (WORD s = 0; s < nt->FileHeader.NumberOfSections; s++, sec++) {
                DWORD secSize = sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData;
                BYTE *secStart = (BYTE*)hExe + sec->VirtualAddress;

                if (secSize < tlen) continue;
                for (DWORD j = 0; j <= secSize - tlen; j++) {
                    if (memcmp(secStart + j, target, tlen) == 0) {
                        DWORD oldProt;
                        if (VirtualProtect(secStart + j, 4, PAGE_READWRITE, &oldProt)) {
                            secStart[j] = '\0'; /* Neutralize string so LoadLibraryA("") returns NULL safely */
                            VirtualProtect(secStart + j, 4, oldProt, &oldProt);
                        }
                    }
                }
            }
        }
    }
}

/* Exported main entry point called by Zuma's module loader (.zldr) */
__declspec(dllexport) int __cdecl ZDC_RunChecker(void) {
    if (getenv("ZUMA_NO_CHECKER") || strstr(GetCommandLineA(), "--no-checker") || strstr(GetCommandLineA(), "-fast")) {
        ApplyModuleStates();
        return 1;
    }

    g_dialogResult = 0;

    LoadZumaMetadata();
    DetectResolutionInfo();
    CheckModules();
    g_isDarkMode = DetectSystemDarkMode();

    HINSTANCE hInstance = GetModuleHandle(NULL);
    g_hCurArrow = LoadCursor(NULL, IDC_ARROW);
    g_hCurHand = LoadCursor(NULL, IDC_HAND);

    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = CheckerWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ZuCompleluxe_DLLChecker_Classic";
    wc.hCursor = g_hCurArrow;
    wc.hbrBackground = NULL;
    RegisterClassExA(&wc);

    RECT rc = { 0, 0, 646, 498 };
    DWORD dwStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    DWORD dwExStyle = WS_EX_DLGMODALFRAME;
    AdjustWindowRectEx(&rc, dwStyle, FALSE, dwExStyle);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - winW) / 2;
    int posY = (screenH - winH) / 2;

    HWND hwnd = CreateWindowExA(
        dwExStyle,
        wc.lpszClassName,
#ifdef ZUCOMPLELUXE_LITE
        "ZuCompleluxe " ZUCOMPLELUXE_VERSION " Lite - Module & Dependency Checker",
#else
        "ZuCompleluxe " ZUCOMPLELUXE_VERSION " - Module & Dependency Checker",
#endif
        dwStyle | WS_VISIBLE,
        posX, posY, winW, winH,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        return 0;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_RETURN) {
                g_dialogResult = 1;
                DestroyWindow(hwnd);
            } else if (msg.wParam == VK_ESCAPE) {
                g_dialogResult = 0;
                DestroyWindow(hwnd);
            }
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    UnregisterClassA(wc.lpszClassName, hInstance);

    if (g_dialogResult == 0) {
        ExitProcess(0);
    }

    ApplyModuleStates();

    return g_dialogResult;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpvReserved;
    return TRUE;
}
