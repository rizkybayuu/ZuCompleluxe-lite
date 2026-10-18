#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#ifndef ZUCOMPLELUXE_VERSION
#define ZUCOMPLELUXE_VERSION "v0.4.1"
#endif

#define IDC_BTN_RUN  1001
#define IDC_BTN_QUIT 1002

typedef struct {
    const char *filename;
    const char *disp_name;
    const char *desc;
    bool is_mandatory;
    bool exists;
    bool is_lite_excluded;
} ModuleDep;

static ModuleDep g_modules[] = {
    { "zumadllchecker.dll", "zumadllchecker.dll", "Core Gatekeeper & Integrity Checker", true,  false, false },
    { "zumacplxcredit.dll", "zumacplxcredit.dll", "Developer Credits Screen Mod",        false, false, false },
    { "zumacustommap.dll",  "zumacustommap.dll",  "Gauntlet Custom Map Extender",        false, false, false },
    { "zumatestingkit.dll", "zumatestingkit.dll", "Dev Testing Kit & Secret Temple 5",   false, false, false },
    { "zumawebp.dll",       "zumawebp.dll",       "Animated WebP Asset Loader",          false, false, true  },
    { "zumaextrapath.dll",  "zumaextrapath.dll",  "Multi-Path Curve Engine (4-10)",      false, false, true  },
    { "zumaextracolor.dll", "zumaextracolor.dll", "11-Ball Color Pipeline Engine",       false, false, true  }
};

#define MODULE_COUNT ((int)(sizeof(g_modules) / sizeof(g_modules[0])))

typedef struct {
    char fileName[MAX_PATH];
    char productName[128];
    char fileVersion[64];
    char companyName[128];
    char specialBuild[128];
    char fileDescription[128];
} ZumaMetadata;

static ZumaMetadata g_meta;
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

    if (g_meta.productName[0] == '\0') strcpy(g_meta.productName, "N/A");
    if (g_meta.fileVersion[0] == '\0') strcpy(g_meta.fileVersion, "N/A");
    if (g_meta.companyName[0] == '\0') strcpy(g_meta.companyName, "N/A");
    if (g_meta.specialBuild[0] == '\0') strcpy(g_meta.specialBuild, "None");
}

/* Check existence of all sidecar DLLs in modules/ */
static void CheckModules(void) {
    for (int i = 0; i < MODULE_COUNT; i++) {
#ifdef ZUCOMPLELUXE_LITE
        if (g_modules[i].is_lite_excluded) {
            /* In Lite Edition, excluded sidecars are always disabled regardless of files on disk */
            g_modules[i].exists = false;
            continue;
        }
#endif
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "modules\\%s", g_modules[i].filename);
        DWORD attr = GetFileAttributesA(path);
        g_modules[i].exists = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
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

/* Draw classic button with proper theme adaptation */
static void DrawClassicButton(DRAWITEMSTRUCT *dis, const char *label, bool isDefault, bool isHover, BOOL isDark, HFONT hFont) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool isPressed = (dis->itemState & ODS_SELECTED) != 0;

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

/* Window procedure for Classic Dialog Window */
static LRESULT CALLBACK CheckerWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        ApplyDwmTitleBarTheme(hwnd, g_isDarkMode);

        g_hBtnQuit = CreateWindowExA(0, "BUTTON", "Quit",
                                     WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                     484, 325, 68, 24, hwnd, (HMENU)IDC_BTN_QUIT, GetModuleHandle(NULL), NULL);

        g_hBtnRun = CreateWindowExA(0, "BUTTON", "Run",
                                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                                    562, 325, 68, 24, hwnd, (HMENU)IDC_BTN_RUN, GetModuleHandle(NULL), NULL);
        return 0;
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

        COLORREF colBg        = g_isDarkMode ? RGB(32, 32, 36)   : GetSysColor(COLOR_BTNFACE);
        COLORREF colBorder    = g_isDarkMode ? RGB(65, 65, 72)   : RGB(180, 180, 185);
        COLORREF colTextPri   = g_isDarkMode ? RGB(235, 235, 240): RGB(20, 20, 20);
        COLORREF colTextSec   = g_isDarkMode ? RGB(160, 160, 168): RGB(105, 105, 112);
        COLORREF colSuccess   = g_isDarkMode ? RGB(74, 222, 128) : RGB(22, 128, 61);
        COLORREF colError     = g_isDarkMode ? RGB(248, 113, 113): RGB(220, 38, 38);

        HBRUSH hBrBg = CreateSolidBrush(colBg);
        FillRect(hdc, &clientRc, hBrBg);
        DeleteObject(hBrBg);

        /* Comfortable standard font size (Segoe UI 15px = ~11pt) */
        HFONT hFontSemibold = CreateFontA(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT hFontNormal = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT hFontNote = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
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

        /* --- 2. Sidecar Module Dependencies GroupBox --- */
        RECT rcGroup2 = { 16, 120, 630, 314 };
#ifdef ZUCOMPLELUXE_LITE
        DrawClassicGroupBox(hdc, &rcGroup2, " Sidecar Module Dependencies (modules\\) [Lite Edition] ", colBorder, colBg, colTextPri, hFontSemibold);
#else
        DrawClassicGroupBox(hdc, &rcGroup2, " Sidecar Module Dependencies (modules\\) ", colBorder, colBg, colTextPri, hFontSemibold);
#endif

        /* Table Column Headers */
        SelectObject(hdc, hFontSemibold);
        SetTextColor(hdc, colTextSec);
        TextOutA(hdc, 32, 138, "Module", 6);
        TextOutA(hdc, 195, 138, "Description", 11);
        TextOutA(hdc, 485, 138, "Status", 6);

        /* Separator line under table header */
        HPEN hPenSep = CreatePen(PS_SOLID, 1, colBorder);
        HPEN oldPen = (HPEN)SelectObject(hdc, hPenSep);
        MoveToEx(hdc, 26, 157, NULL);
        LineTo(hdc, 620, 157);
        SelectObject(hdc, oldPen);
        DeleteObject(hPenSep);

        /* Natural, compact rows (20px pitch) */
        int rowY = 166;
        int rowGap = 20;

        for (int i = 0; i < MODULE_COUNT; i++) {
            /* Column 1: Module name */
            SelectObject(hdc, hFontNormal);
            SetTextColor(hdc, colTextPri);
            TextOutA(hdc, 32, rowY, g_modules[i].disp_name, (int)strlen(g_modules[i].disp_name));

            /* Column 2: Description */
            SetTextColor(hdc, colTextSec);
            TextOutA(hdc, 195, rowY, g_modules[i].desc, (int)strlen(g_modules[i].desc));

            /* Column 3: Status */
            SelectObject(hdc, hFontSemibold);
#ifdef ZUCOMPLELUXE_LITE
            if (g_modules[i].is_lite_excluded) {
                COLORREF colDisabled = g_isDarkMode ? RGB(140, 149, 159) : RGB(110, 119, 129);
                SetTextColor(hdc, colDisabled);
                TextOutA(hdc, 485, rowY, "[DISABLED]", 10);
            } else
#endif
            if (g_modules[i].exists) {
                SetTextColor(hdc, colSuccess);
                if (g_modules[i].is_mandatory) {
                    TextOutA(hdc, 485, rowY, "[OK] Required", 13);
                } else {
                    TextOutA(hdc, 485, rowY, "[OK] Present", 12);
                }
            } else {
                SetTextColor(hdc, colError);
                TextOutA(hdc, 485, rowY, "[MISSING]", 9);
            }

            rowY += rowGap;
        }

        /* --- 3. Bottom Footer Note --- */
        SelectObject(hdc, hFontNote);
        SetTextColor(hdc, colTextSec);
#ifdef ZUCOMPLELUXE_LITE
        TextOutA(hdc, 20, 320, "Lite Edition: Core mods only. WebP, Multi-Path & 11-Colors disabled.", 68);
        TextOutA(hdc, 20, 336, "Press Run to start Zuma Deluxe, or Quit to exit.", 48);
#else
        TextOutA(hdc, 20, 329, "Press Run to start Zuma Deluxe, or Quit to exit.", 48);
#endif

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
            SetCursor(g_linkHover ? g_hCurHand : g_hCurArrow);
        }

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
        SetCursor(g_hCurArrow);
        return 0;
    }

    case WM_SETCURSOR: {
        if (LOWORD(lParam) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            SetCursor(PtInRect(&g_rcLink, pt) ? g_hCurHand : g_hCurArrow);
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
        break;
    }

    case WM_CLOSE: {
        g_dialogResult = 0;
        DestroyWindow(hwnd);
        return 0;
    }

    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }
}

#ifdef ZUCOMPLELUXE_LITE
/* Neutralize excluded sidecar DLL strings inside the loaded Zuma.exe executable.
   This guarantees that the in-executable .zldr stub cannot load them even if
   physical DLL files exist in modules\ */
static void DisableLiteExcludedModules(void) {
    HMODULE hExe = GetModuleHandleA(NULL);
    if (!hExe) return;

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hExe;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)hExe + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;

    const char *targets[] = {
        "modules\\zumawebp.dll",
        "modules\\zumaextrapath.dll",
        "modules\\zumaextracolor.dll"
    };

    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        DWORD secSize = sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData;
        BYTE *secStart = (BYTE*)hExe + sec->VirtualAddress;

        for (int t = 0; t < 3; t++) {
            size_t tlen = strlen(targets[t]);
            if (secSize < tlen) continue;
            for (DWORD j = 0; j <= secSize - tlen; j++) {
                if (memcmp(secStart + j, targets[t], tlen) == 0) {
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
#endif

/* Exported main entry point called by Zuma's module loader (.zldr) */
__declspec(dllexport) int __cdecl ZDC_RunChecker(void) {
    g_dialogResult = 0;

    LoadZumaMetadata();
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

    RECT rc = { 0, 0, 646, 360 };
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
        "ZuCompleluxe v0.4.1 Lite - Module & Dependency Checker",
#else
        "ZuCompleluxe v0.4.1 - Module & Dependency Checker",
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
                break;
            } else if (msg.wParam == VK_ESCAPE) {
                g_dialogResult = 0;
                DestroyWindow(hwnd);
                break;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    UnregisterClassA(wc.lpszClassName, hInstance);

    if (g_dialogResult == 0) {
        ExitProcess(0);
    }

#ifdef ZUCOMPLELUXE_LITE
    DisableLiteExcludedModules();
#endif

    return g_dialogResult;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpvReserved;
    return TRUE;
}
