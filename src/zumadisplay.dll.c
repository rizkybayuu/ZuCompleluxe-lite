/* zumadisplay.dll.c - Display & Window Driver for ZuCompleluxe.
 *
 * Responsibilities:
 *  - Section .zres DirectDraw & Window hooks
 *  - F11 & Alt+Enter Borderless Fullscreen Driver
 *  - DirectDraw PrimarySurface::Blt Letterbox Fit & Border Clearing Hook
 *  - ScreenToClient / ClientToScreen Coordinate Remapping
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifndef TARGET_W
#define TARGET_W 800
#endif
#ifndef TARGET_H
#define TARGET_H 600
#endif

#define MIN_TARGET_W 480
#define MIN_TARGET_H 320

#define LW 640
#define LH 480
#define FC __attribute__((fastcall))
#define THISCALL __attribute__((thiscall))

static int g_W = TARGET_W, g_H = TARGET_H;

static void LogF(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    OutputDebugStringA(buf);

    char logpath[MAX_PATH];
    GetModuleFileNameA(NULL, logpath, MAX_PATH);
    char *slash = strrchr(logpath, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat(logpath, "zumatest.log");

    if (GetFileAttributesA(logpath) == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesA("zumatest.log") == INVALID_FILE_ATTRIBUTES) return;

    FILE *f = fopen(logpath, "a");
    if (!f) f = fopen("zumatest.log", "a");
    if (f) {
        if (buf[0] == '[')
            fprintf(f, "[%8lu]%s\n", (unsigned long)GetTickCount(), buf);
        else
            fprintf(f, "[%8lu][Display] %s\n", (unsigned long)GetTickCount(), buf);
        fflush(f);
        fclose(f);
    }
}

static void Offsets(int W, int H, int *xo, int *yo, int *cw, int *ch)
{
    float sw = (float)W / (float)LW;
    float sh = (float)H / (float)LH;
    float s = sw < sh ? sw : sh;
    int c_w = (int)((float)LW * s + 0.5f);
    int c_h = (int)((float)LH * s + 0.5f);
    if (cw) *cw = c_w;
    if (ch) *ch = c_h;
    if (xo) *xo = (W - c_w) / 2;
    if (yo) *yo = (H - c_h) / 2;
}

static int g_IsFullscreen = 0;
static HWND g_hGameWnd = NULL;
static WNDPROC g_pfnOrigWndProc = NULL;

typedef BOOL (WINAPI *fClientToScreen)(HWND hWnd, LPPOINT lpPoint);
typedef BOOL (WINAPI *fScreenToClient)(HWND hWnd, LPPOINT lpPoint);
static fClientToScreen g_real_ClientToScreen = NULL;
static fScreenToClient g_real_ScreenToClient = NULL;

static void GetFullscreenFit(int *ox, int *oy, float *out_scale, int *fit_w, int *fit_h) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (sw <= 0 || sh <= 0 || g_W <= 0 || g_H <= 0) {
        if (ox) *ox = 0;
        if (oy) *oy = 0;
        if (out_scale) *out_scale = 1.0f;
        if (fit_w) *fit_w = g_W;
        if (fit_h) *fit_h = g_H;
        return;
    }
    float sx = (float)sw / (float)g_W;
    float sy = (float)sh / (float)g_H;
    float s = sx < sy ? sx : sy;
    int fw = (int)((float)g_W * s + 0.5f);
    int fh = (int)((float)g_H * s + 0.5f);
    int x = (sw - fw) / 2;
    int y = (sh - fh) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (ox) *ox = x;
    if (oy) *oy = y;
    if (out_scale) *out_scale = s;
    if (fit_w) *fit_w = fw;
    if (fit_h) *fit_h = fh;
}

static void ScreenToGameCoords(int sx, int sy, int *gx, int *gy) {
    if (!g_IsFullscreen) {
        if (gx) *gx = sx;
        if (gy) *gy = sy;
        return;
    }
    int ox, oy, fw, fh;
    float scale;
    GetFullscreenFit(&ox, &oy, &scale, &fw, &fh);
    if (scale <= 0.0001f) scale = 1.0f;
    int x = (int)(((float)(sx - ox)) / scale);
    int y = (int)(((float)(sy - oy)) / scale);
    if (x < 0) x = 0;
    if (x > g_W) x = g_W;
    if (y < 0) y = 0;
    if (y > g_H) y = g_H;
    if (gx) *gx = x;
    if (gy) *gy = y;
}

static void GameToScreenCoords(int gx, int gy, int *sx, int *sy) {
    if (!g_IsFullscreen) {
        if (sx) *sx = gx;
        if (sy) *sy = gy;
        return;
    }
    int ox, oy, fw, fh;
    float scale;
    GetFullscreenFit(&ox, &oy, &scale, &fw, &fh);
    int x = ox + (int)((float)gx * scale + 0.5f);
    int y = oy + (int)((float)gy * scale + 0.5f);
    if (sx) *sx = x;
    if (sy) *sy = y;
}

static BOOL WINAPI hk_ClientToScreen(HWND hWnd, LPPOINT lpPoint) {
    if (g_IsFullscreen && lpPoint && (hWnd == g_hGameWnd || !g_hGameWnd)) {
        int sx, sy;
        GameToScreenCoords(lpPoint->x, lpPoint->y, &sx, &sy);
        lpPoint->x = sx;
        lpPoint->y = sy;
        return TRUE;
    }
    return g_real_ClientToScreen ? g_real_ClientToScreen(hWnd, lpPoint) : ClientToScreen(hWnd, lpPoint);
}

static BOOL WINAPI hk_ScreenToClient(HWND hWnd, LPPOINT lpPoint) {
    BOOL r = g_real_ScreenToClient ? g_real_ScreenToClient(hWnd, lpPoint) : ScreenToClient(hWnd, lpPoint);
    if (g_IsFullscreen && lpPoint && (hWnd == g_hGameWnd || !g_hGameWnd)) {
        int gx, gy;
        ScreenToGameCoords(lpPoint->x, lpPoint->y, &gx, &gy);
        lpPoint->x = gx;
        lpPoint->y = gy;
    }
    return r;
}

/* Primary surface stretch Blt hook for true fit-to-screen fullscreen */
typedef HRESULT (STDMETHODCALLTYPE *fSurfaceBlt)(void *this_surface, LPRECT lpDestRect, void *lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, void *lpDDBltFx);
static void *s_primary_vt_copy[64];
static fSurfaceBlt g_real_PrimaryBlt = NULL;

static HRESULT STDMETHODCALLTYPE hk_PrimaryBlt(
    void *this_surface,
    LPRECT lpDestRect,
    void *lpDDSrcSurface,
    LPRECT lpSrcRect,
    DWORD dwFlags,
    void *lpDDBltFx
) {
    if (!g_real_PrimaryBlt) return 0;

    if (!g_IsFullscreen || !lpDDSrcSurface || (dwFlags & 0x00000400 /* DDBLT_COLORFILL */)) {
        return g_real_PrimaryBlt(this_surface, lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    }

    int ox, oy, fw, fh;
    float scale;
    GetFullscreenFit(&ox, &oy, &scale, &fw, &fh);

    RECT scaledDest;
    if (lpSrcRect) {
        scaledDest.left   = ox + (int)((float)lpSrcRect->left * scale);
        scaledDest.top    = oy + (int)((float)lpSrcRect->top * scale);
        scaledDest.right  = ox + (int)((float)lpSrcRect->right * scale + 0.5f);
        scaledDest.bottom = oy + (int)((float)lpSrcRect->bottom * scale + 0.5f);
    } else {
        scaledDest.left   = ox;
        scaledDest.top    = oy;
        scaledDest.right  = ox + fw;
        scaledDest.bottom = oy + fh;
    }

    if (scaledDest.right <= scaledDest.left) scaledDest.right = scaledDest.left + 1;
    if (scaledDest.bottom <= scaledDest.top) scaledDest.bottom = scaledDest.top + 1;

    HRESULT hr = g_real_PrimaryBlt(this_surface, &scaledDest, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    if (hr == 0x8876021C /* DDERR_WASSTILLDRAWING */) {
        hr = g_real_PrimaryBlt(this_surface, &scaledDest, lpDDSrcSurface, lpSrcRect, dwFlags | 0x01000000 /* DDBLT_WAIT */, lpDDBltFx);
    } else if (FAILED(hr)) {
        hr = g_real_PrimaryBlt(this_surface, lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    }
    return hr;
}

static void HookPrimarySurface(void *primary) {
    if (!primary) return;
    void **orig_vt = *(void ***)primary;
    if (orig_vt == s_primary_vt_copy) return;
    if (!g_real_PrimaryBlt) {
        g_real_PrimaryBlt = (fSurfaceBlt)orig_vt[5];
    }
    memcpy(s_primary_vt_copy, orig_vt, sizeof(s_primary_vt_copy));
    s_primary_vt_copy[5] = (void *)hk_PrimaryBlt;
    *(void ***)primary = s_primary_vt_copy;
    LogF("[Display] Hooked PrimarySurface Blt for Fit-To-Screen Fullscreen (orig=%p)", g_real_PrimaryBlt);
}

static void *FindPattern(const char *pattern) {
    char *base = (char *)GetModuleHandleA(NULL);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);

    unsigned char pat_bytes[128];
    char pat_mask[128];
    size_t pat_len = 0;
    const char *p = pattern;
    while (*p && pat_len < 128) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == '?') {
            pat_bytes[pat_len] = 0;
            pat_mask[pat_len] = '?';
            pat_len++;
            if (*(p + 1) == '?') p += 2;
            else p++;
        } else {
            char hex[3] = { p[0], p[1], 0 };
            pat_bytes[pat_len] = (unsigned char)strtoul(hex, NULL, 16);
            pat_mask[pat_len] = 'x';
            pat_len++;
            p += 2;
        }
    }
    if (pat_len == 0) return NULL;

    for (WORD s = 0; s < nt->FileHeader.NumberOfSections; s++) {
        if (!(sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        unsigned char *sec_data = (unsigned char *)(base + sec[s].VirtualAddress);
        DWORD sec_size = sec[s].Misc.VirtualSize;
        if (sec_size == 0) sec_size = sec[s].SizeOfRawData;
        if (sec_size < pat_len) continue;

        for (DWORD i = 0; i <= sec_size - pat_len; i++) {
            int found = 1;
            for (size_t j = 0; j < pat_len; j++) {
                if (pat_mask[j] == 'x' && sec_data[i + j] != pat_bytes[j]) {
                    found = 0;
                    break;
                }
            }
            if (found) return (void *)(sec_data + i);
        }
    }
    return NULL;
}

static void **GetAppPtr(void) {
    static void **s_app_ptr = NULL;
    if (s_app_ptr) return s_app_ptr;
    void *p = FindPattern("A1 ?? ?? ?? ?? 8B 88 C0 04 00 00");
    if (p) s_app_ptr = *(void ***)((char *)p + 1);
    return s_app_ptr;
}

/* Custom Hardware Cursor Support (GPU-accelerated, zero CPU blits, zero artifacts) */
static HCURSOR g_hCustomCursors[8] = {0};

static HCURSOR CreateHardwareCursorFromPopCapImage(void *img) {
    if (!img || IsBadReadPtr(img, 0x30)) return NULL;
    int w = *(int *)((char *)img + 4);
    int h = *(int *)((char *)img + 8);
    if (w <= 0 || h <= 0 || w > 256 || h > 256) return NULL;

    uint32_t *src_bits = NULL;
    int allocated = 0;

    /* 1. Try invoking PopCap MemoryImage::GetBits() via vftable[22] (offset +0x58) to unpack paletted images */
    void **vftable = *(void ***)img;
    if (vftable && !IsBadReadPtr(vftable, 0x60)) {
        typedef uint32_t * (THISCALL *GetBitsFn)(void *self);
        GetBitsFn fnGetBits = (GetBitsFn)vftable[22];
        if (fnGetBits && !IsBadCodePtr((FARPROC)fnGetBits)) {
            src_bits = fnGetBits(img);
        }
    }

    /* 2. Direct read of mBits if already populated */
    if (!src_bits) {
        src_bits = *(uint32_t **)((char *)img + 0x14);
    }

    /* 3. Fallback: manual unpack from paletted buffer (mColorTable @ 0x24 + mColorIndices @ 0x28) */
    if (!src_bits) {
        uint32_t *pal = *(uint32_t **)((char *)img + 0x24);
        uint8_t *idx = *(uint8_t **)((char *)img + 0x28);
        if (pal && idx && !IsBadReadPtr(pal, 64 * sizeof(uint32_t)) && !IsBadReadPtr(idx, (size_t)w * h)) {
            src_bits = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
            if (src_bits) {
                allocated = 1;
                for (int i = 0; i < w * h; i++) {
                    src_bits[i] = pal[idx[i]];
                }
            }
        }
    }

    if (!src_bits || IsBadReadPtr(src_bits, (size_t)w * (size_t)h * sizeof(uint32_t))) {
        if (allocated && src_bits) free(src_bits);
        return NULL;
    }

    BITMAPV5HEADER bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = w;
    bi.bV5Height = -h; /* top-down DIB */
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask   = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask  = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    HDC hdc = GetDC(NULL);
    void *lpBits = NULL;
    HBITMAP hColorBmp = CreateDIBSection(hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &lpBits, NULL, 0);

    /* Zero-initialize monochrome mask to guarantee proper per-pixel 32-bit alpha blending */
    int mask_stride = ((w + 15) / 16) * 2;
    size_t mask_size = (size_t)mask_stride * h;
    void *mask_bits = calloc(1, mask_size);
    HBITMAP hMaskBmp = CreateBitmap(w, h, 1, 1, mask_bits);
    if (mask_bits) free(mask_bits);

    ReleaseDC(NULL, hdc);

    if (!hColorBmp || !hMaskBmp || !lpBits) {
        if (hColorBmp) DeleteObject(hColorBmp);
        if (hMaskBmp)  DeleteObject(hMaskBmp);
        if (allocated && src_bits) free(src_bits);
        return NULL;
    }

    /* Convert to premultiplied 32-bit ARGB */
    uint32_t *dst = (uint32_t *)lpBits;
    for (int i = 0; i < w * h; i++) {
        uint32_t p = src_bits[i];
        uint32_t a = (p >> 24) & 0xFF;
        if (a == 0) {
            dst[i] = 0;
        } else if (a == 255) {
            dst[i] = p;
        } else {
            uint32_t r = ((p >> 16) & 0xFF) * a / 255;
            uint32_t g = ((p >> 8)  & 0xFF) * a / 255;
            uint32_t b = (p         & 0xFF) * a / 255;
            dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    if (allocated && src_bits) {
        free(src_bits);
    }

    ICONINFO ii;
    ZeroMemory(&ii, sizeof(ii));
    ii.fIcon = FALSE; /* FALSE = Cursor */
    ii.xHotspot = w / 2;
    ii.yHotspot = h / 2;
    ii.hbmMask = hMaskBmp;
    ii.hbmColor = hColorBmp;

    HCURSOR hCursor = (HCURSOR)CreateIconIndirect(&ii);

    DeleteObject(hColorBmp);
    DeleteObject(hMaskBmp);

    return hCursor;
}

static void __cdecl OnRehookCustomCursor(void *app, void *img) {
    if (!app) return;

    /* 1. Ensure DDInterface software cursor is completely turned OFF so it NEVER touches primary surface */
    void *dd = *(void **)((char *)app + 0x31c);
    if (dd) {
        *(void **)((char *)dd + 0xcb8) = NULL;      /* mCursorImage = NULL */
        *(unsigned char *)((char *)dd + 0xcbc) = 0; /* mCursorActive = 0 */
    }

    /* 2. Read current cursor index */
    int cursor_num = *(int *)((char *)app + 0x4bc);
    if (cursor_num < 0 || cursor_num >= 8) cursor_num = 0;

    void *real_img = img ? img : *(void **)((char *)app + 0x3bc + cursor_num * 4);

    /* 3. Create or reuse cached hardware cursor */
    if (!g_hCustomCursors[cursor_num] && real_img) {
        g_hCustomCursors[cursor_num] = CreateHardwareCursorFromPopCapImage(real_img);
        if (g_hCustomCursors[cursor_num]) {
            LogF("[Display] Created hardware HCURSOR for cursor %d (%p)", cursor_num, g_hCustomCursors[cursor_num]);
        }
    }

    /* 4. Set OS hardware cursor */
    if (g_hCustomCursors[cursor_num]) {
        SetCursor(g_hCustomCursors[cursor_num]);
    } else {
        if (cursor_num == 1 || cursor_num == 4) {
            SetCursor(LoadCursorA(NULL, IDC_HAND));
        } else {
            SetCursor(LoadCursorA(NULL, IDC_ARROW));
        }
    }
}

static void InstallCursorHooks(void) {
    /* 1. Hook SexyAppBase::RehookCursor custom-cursor branch (0x00465001) */
    void *pRehookCustom = FindPattern("51 8B 8E 1C 03 00 00 E8 ?? ?? ?? ?? 6A 00 FF 15 44 E2 4F 00");
    if (pRehookCustom) {
        DWORD old;
        if (VirtualProtect(pRehookCustom, 24, PAGE_EXECUTE_READWRITE, &old)) {
            unsigned char hook_bytes[24];
            memset(hook_bytes, 0x90, sizeof(hook_bytes));

            hook_bytes[0] = 0x51; /* push ecx (Image*) */
            hook_bytes[1] = 0x56; /* push esi (app) */
            hook_bytes[2] = 0xE8; /* call OnRehookCustomCursor */
            DWORD rel = (DWORD)OnRehookCustomCursor - ((DWORD)pRehookCustom + 7);
            memcpy(&hook_bytes[3], &rel, 4);
            hook_bytes[7] = 0x83; hook_bytes[8] = 0xC4; hook_bytes[9] = 0x08; /* add esp, 8 */
            hook_bytes[10] = 0xB3; hook_bytes[11] = 0x01; /* mov bl, 1 */
            hook_bytes[12] = 0xEB; hook_bytes[13] = 0x6B; /* jmp 0x0046507A */

            memcpy(pRehookCustom, hook_bytes, sizeof(hook_bytes));
            VirtualProtect(pRehookCustom, 24, old, &old);
            LogF("[Display] Installed Hardware Custom Cursor Hook at %p", pRehookCustom);
        }
    } else {
        LogF("[Display] WARNING: Could not find RehookCustom pattern");
    }

    /* 2. Patch DDInterface::SetCursorPos (0x00450550) with 'ret 8' (C2 08 00) to silence CursorThreadProc */
    void *pSetCursorPos = FindPattern("55 8B EC 6A FF 68 58 84 4F 00 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 51 56 57 8B F1 8D 7E 14");
    if (pSetCursorPos) {
        DWORD old;
        if (VirtualProtect(pSetCursorPos, 3, PAGE_EXECUTE_READWRITE, &old)) {
            unsigned char ret8[3] = { 0xC2, 0x08, 0x00 };
            memcpy(pSetCursorPos, ret8, 3);
            VirtualProtect(pSetCursorPos, 3, old, &old);
            LogF("[Display] Silenced DDInterface::SetCursorPos at %p with 'ret 8'", pSetCursorPos);
        }
    } else {
        LogF("[Display] WARNING: Could not find SetCursorPos pattern");
    }
}

static void CleanupCustomCursors(void) {
    for (int i = 0; i < 8; i++) {
        if (g_hCustomCursors[i]) {
            DestroyIcon(g_hCustomCursors[i]);
            g_hCustomCursors[i] = NULL;
        }
    }
}

static void ApplyWindowGeometry(HWND h, int is_fullscreen);
static void ToggleFullscreen(void);

static LRESULT CALLBACK SubclassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_SYSKEYDOWN) {
        if (wParam == VK_RETURN && (HIWORD(lParam) & KF_ALTDOWN)) {
            ToggleFullscreen();
            return 0;
        }
    } else if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_F11) {
            ToggleFullscreen();
            return 0;
        }
    }

    if (uMsg == WM_SETCURSOR) {
        if (LOWORD(lParam) == HTCLIENT) {
            void **app_ptr = GetAppPtr();
            if (app_ptr && *app_ptr) {
                void *app = *app_ptr;
                typedef void (THISCALL *RehookFn)(void *self);
                RehookFn rehook = (RehookFn)0x00464FB0;
                rehook(app);
                return TRUE;
            }
        }
    }

    if (g_IsFullscreen) {
        switch (uMsg) {
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK: {
                short mx = (short)LOWORD(lParam);
                short my = (short)HIWORD(lParam);
                int gx, gy;
                ScreenToGameCoords(mx, my, &gx, &gy);
                lParam = MAKELPARAM((WORD)gx, (WORD)gy);
                break;
            }
            case WM_ERASEBKGND: {
                HDC hdc = (HDC)wParam;
                if (hdc) {
                    int sw = GetSystemMetrics(SM_CXSCREEN);
                    int sh = GetSystemMetrics(SM_CYSCREEN);
                    RECT rc = { 0, 0, sw, sh };
                    FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
                    return 1;
                }
                break;
            }
        }
    }
    return CallWindowProcA(g_pfnOrigWndProc, hWnd, uMsg, wParam, lParam);
}

static void ClearFullscreenBorders(void *primary_surface) {
    if (!primary_surface || !g_real_PrimaryBlt) return;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ox, oy, fw, fh;
    float scale;
    GetFullscreenFit(&ox, &oy, &scale, &fw, &fh);

    struct {
        DWORD dwSize;
        DWORD dwDDFX;
        DWORD dwFillColor;
    } fx;
    memset(&fx, 0, sizeof(fx));
    fx.dwSize = sizeof(fx);
    fx.dwFillColor = 0;

    if (ox > 0) {
        RECT rLeft  = { 0, 0, ox, sh };
        RECT rRight = { ox + fw, 0, sw, sh };
        g_real_PrimaryBlt(primary_surface, &rLeft, NULL, NULL, 0x00000400 | 0x01000000, &fx);
        g_real_PrimaryBlt(primary_surface, &rRight, NULL, NULL, 0x00000400 | 0x01000000, &fx);
    }
    if (oy > 0) {
        RECT rTop = { 0, 0, sw, oy };
        RECT rBot = { 0, oy + fh, sw, sh };
        g_real_PrimaryBlt(primary_surface, &rTop, NULL, NULL, 0x00000400 | 0x01000000, &fx);
        g_real_PrimaryBlt(primary_surface, &rBot, NULL, NULL, 0x00000400 | 0x01000000, &fx);
    }
}

static void ToggleFullscreen(void) {
    g_IsFullscreen = !g_IsFullscreen;
    void **app_ptr = GetAppPtr();
    if (app_ptr && *app_ptr) {
        void *app = *app_ptr;
        *((unsigned char *)app + 0x2F2) = g_IsFullscreen ? 0 : 1;
        /* Note: dd + 0x10 is mIs3D (3D hardware acceleration flag), NOT windowed flag!
         * Never overwrite dd + 0x10 during fullscreen toggle! */
    }
    ApplyWindowGeometry(g_hGameWnd, g_IsFullscreen);
    if (g_IsFullscreen) {
        void **app_ptr2 = GetAppPtr();
        if (app_ptr2 && *app_ptr2) {
            void *app = *app_ptr2;
            void *dd = *(void **)((char *)app + 0x31c);
            if (dd) {
                void *p34 = *(void **)((char *)dd + 0x34);
                ClearFullscreenBorders(p34);
            }
        }
    }
    if (g_hGameWnd) InvalidateRect(g_hGameWnd, NULL, TRUE);
    LogF("[Display] ToggleFullscreen: New mode = %s", g_IsFullscreen ? "Fullscreen (Borderless Fit)" : "Windowed");
}

static void ApplyWindowGeometry(HWND h, int is_fullscreen) {
    if (!h) return;
    static int s_in_geometry = 0;
    if (s_in_geometry) return;
    s_in_geometry = 1;

    void **app_ptr = GetAppPtr();
    if (app_ptr && *app_ptr) {
        void *app = *app_ptr;
        if (*(void **)((char *)app + 0x2dc) == NULL) {
            s_in_geometry = 0;
            return;
        }
    }

    RECT cur_rc;
    GetWindowRect(h, &cur_rc);
    int cur_w = cur_rc.right - cur_rc.left;
    int cur_h = cur_rc.bottom - cur_rc.top;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    if (is_fullscreen) {
        if (cur_w != sw || cur_h != sh || cur_rc.left != 0 || cur_rc.top != 0) {
            SetWindowLongA(h, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowLongA(h, GWL_EXSTYLE, WS_EX_APPWINDOW);
            SetWindowPos(h, HWND_TOP, 0, 0, sw, sh, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            HDC hdc = GetDC(h);
            if (hdc) {
                RECT rc = { 0, 0, sw, sh };
                FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
                ReleaseDC(h, hdc);
            }
            LogF("[Display] Fullscreen borderless applied (%dx%d on %dx%d)", g_W, g_H, sw, sh);
        }
    } else {
        RECT rc = { 0, 0, g_W, g_H };
        AdjustWindowRect(&rc, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE, FALSE);
        int ww = rc.right - rc.left;
        int wh = rc.bottom - rc.top;
        int wx = (sw - ww) / 2;
        int wy = (sh - wh) / 2;
        if (wx < 0) wx = 0;
        if (wy < 0) wy = 0;
        if (cur_w != ww || cur_h != wh || cur_rc.left != wx || cur_rc.top != wy) {
            SetWindowLongA(h, GWL_STYLE, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE);
            SetWindowLongA(h, GWL_EXSTYLE, 0);
            SetWindowPos(h, HWND_NOTOPMOST, wx, wy, ww, wh, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            LogF("[Display] Windowed mode restored (%dx%d at %d,%d)", ww, wh, wx, wy);
        }
    }
    s_in_geometry = 0;
}

typedef HWND (WINAPI *fCreateWindowExA)(
    DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle,
    int X, int Y, int nWidth, int nHeight,
    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam
);
static fCreateWindowExA g_real_CreateWindowExA = NULL;

static HWND WINAPI hk_CreateWindowExA(
    DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle,
    int X, int Y, int nWidth, int nHeight,
    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam
) {
    if (!g_real_CreateWindowExA) return NULL;
    HWND h = g_real_CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    if (h) {
        g_hGameWnd = h;
        WNDPROC old_proc = (WNDPROC)GetWindowLongA(h, GWL_WNDPROC);
        if (old_proc && old_proc != SubclassWndProc) {
            g_pfnOrigWndProc = old_proc;
            SetWindowLongA(h, GWL_WNDPROC, (LONG)SubclassWndProc);
            LogF("[Display] Subclassed game HWND %p via CreateWindowExA hook", h);
        }
    }
    return h;
}

static void HookModuleScreenIAT(HMODULE hMod)
{
    if (!hMod) return;
    char *base = (char *)hMod;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;
    DWORD impRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!impRVA) return;

    HMODULE hUser = GetModuleHandleA("user32.dll");
    FARPROC pC2S = hUser ? GetProcAddress(hUser, "ClientToScreen") : NULL;
    FARPROC pS2C = hUser ? GetProcAddress(hUser, "ScreenToClient") : NULL;
    FARPROC pCW  = hUser ? GetProcAddress(hUser, "CreateWindowExA") : NULL;

    IMAGE_IMPORT_DESCRIPTOR *imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + impRVA);
    for (; imp->Name; imp++) {
        if (stricmp(base + imp->Name, "user32.dll") == 0) {
            IMAGE_THUNK_DATA *iat = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
            IMAGE_THUNK_DATA *oft = imp->OriginalFirstThunk ? (IMAGE_THUNK_DATA *)(base + imp->OriginalFirstThunk) : NULL;

            for (int idx = 0; iat[idx].u1.Function; idx++) {
                int is_c2s = 0, is_s2c = 0, is_cw = 0;
                if (pC2S && (FARPROC)iat[idx].u1.Function == pC2S) is_c2s = 1;
                else if (pS2C && (FARPROC)iat[idx].u1.Function == pS2C) is_s2c = 1;
                else if (pCW && (FARPROC)iat[idx].u1.Function == pCW) is_cw = 1;
                else if (oft && !IMAGE_SNAP_BY_ORDINAL(oft[idx].u1.Ordinal)) {
                    IMAGE_IMPORT_BY_NAME *ibn = (IMAGE_IMPORT_BY_NAME *)(base + oft[idx].u1.AddressOfData);
                    if (strcmp((char *)ibn->Name, "ClientToScreen") == 0) is_c2s = 1;
                    else if (strcmp((char *)ibn->Name, "ScreenToClient") == 0) is_s2c = 1;
                    else if (strcmp((char *)ibn->Name, "CreateWindowExA") == 0) is_cw = 1;
                }

                if (is_c2s && (FARPROC)iat[idx].u1.Function != (FARPROC)hk_ClientToScreen) {
                    DWORD old;
                    VirtualProtect(&iat[idx].u1.Function, sizeof(void *), PAGE_EXECUTE_READWRITE, &old);
                    if (!g_real_ClientToScreen) g_real_ClientToScreen = (fClientToScreen)iat[idx].u1.Function;
                    iat[idx].u1.Function = (DWORD)hk_ClientToScreen;
                    VirtualProtect(&iat[idx].u1.Function, sizeof(void *), old, &old);
                    LogF("[Display] Hooked ClientToScreen in IAT (%p): %p", hMod, &iat[idx].u1.Function);
                } else if (is_s2c && (FARPROC)iat[idx].u1.Function != (FARPROC)hk_ScreenToClient) {
                    DWORD old;
                    VirtualProtect(&iat[idx].u1.Function, sizeof(void *), PAGE_EXECUTE_READWRITE, &old);
                    if (!g_real_ScreenToClient) g_real_ScreenToClient = (fScreenToClient)iat[idx].u1.Function;
                    iat[idx].u1.Function = (DWORD)hk_ScreenToClient;
                    VirtualProtect(&iat[idx].u1.Function, sizeof(void *), old, &old);
                    LogF("[Display] Hooked ScreenToClient in IAT (%p): %p", hMod, &iat[idx].u1.Function);
                } else if (is_cw && (FARPROC)iat[idx].u1.Function != (FARPROC)hk_CreateWindowExA) {
                    DWORD old;
                    VirtualProtect(&iat[idx].u1.Function, sizeof(void *), PAGE_EXECUTE_READWRITE, &old);
                    if (!g_real_CreateWindowExA) g_real_CreateWindowExA = (fCreateWindowExA)iat[idx].u1.Function;
                    iat[idx].u1.Function = (DWORD)hk_CreateWindowExA;
                    VirtualProtect(&iat[idx].u1.Function, sizeof(void *), old, &old);
                    LogF("[Display] Hooked CreateWindowExA in IAT (%p): %p", hMod, &iat[idx].u1.Function);
                }
            }
            break;
        }
    }
}

static void InstallScreenHooks(void)
{
    HookModuleScreenIAT(GetModuleHandleA(NULL));
    HookModuleScreenIAT(GetModuleHandleA("zumatestingkit.dll"));
    HookModuleScreenIAT(GetModuleHandleA("zumacplxcredit.dll"));

    HWND existing = FindWindowA("MainWindow", NULL);
    if (!existing) existing = FindWindowA(NULL, "Zuma Deluxe");
    if (existing) {
        g_hGameWnd = existing;
        WNDPROC old_proc = (WNDPROC)GetWindowLongA(existing, GWL_WNDPROC);
        if (old_proc && old_proc != SubclassWndProc) {
            g_pfnOrigWndProc = old_proc;
            SetWindowLongA(existing, GWL_WNDPROC, (LONG)SubclassWndProc);
            LogF("[Display] Subclassed existing HWND %p", existing);
        }
    }
}

__declspec(dllexport) void __cdecl ZDISP_OnDDInit(void *dd, HWND hwnd, int win, int w, int h) {
    g_W = w; g_H = h;
    g_hGameWnd = hwnd;
    g_IsFullscreen = (win == 0);
    LogF("[Display] ZDISP_OnDDInit: dd=%p, hwnd=%p, win=%d, fs=%d, res=%dx%d", dd, hwnd, win, g_IsFullscreen, w, h);

    if (hwnd) {
        WNDPROC old_proc = (WNDPROC)GetWindowLongA(hwnd, GWL_WNDPROC);
        if (old_proc && old_proc != SubclassWndProc) {
            g_pfnOrigWndProc = old_proc;
            SetWindowLongA(hwnd, GWL_WNDPROC, (LONG)SubclassWndProc);
            LogF("[Display] Subclassed game HWND %p for fullscreen borderless", hwnd);
        }
    }

    if (dd) {
        void *p34 = *(void **)((char *)dd + 0x34);
        if (p34) HookPrimarySurface(p34);
    }

    ApplyWindowGeometry(hwnd, g_IsFullscreen);
}

__declspec(dllexport) void __cdecl ZDISP_OnDrawScreen(void *app) {
    if (!app) return;
    int app_is_win = *(unsigned char *)((char *)app + 0x2F2);
    int want_fs = (app_is_win == 0);
    if (g_IsFullscreen != want_fs) {
        g_IsFullscreen = want_fs;
        ApplyWindowGeometry(g_hGameWnd, g_IsFullscreen);
    }
    void *dd = *(void **)((char *)app + 0x31c);
    if (dd) {
        void *p34 = *(void **)((char *)dd + 0x34);
        if (p34) HookPrimarySurface(p34);
    }
}

/* Exports */
__declspec(dllexport) int __cdecl ZDISP_IsActive(void) { return 1; }
__declspec(dllexport) void __cdecl ZDISP_ScreenToGameCoords(int sx, int sy, int *gx, int *gy) { ScreenToGameCoords(sx, sy, gx, gy); }
__declspec(dllexport) void __cdecl ZDISP_GameToScreenCoords(int gx, int gy, int *sx, int *sy) { GameToScreenCoords(gx, gy, sx, sy); }
__declspec(dllexport) int __cdecl ZDISP_IsFullscreen(void) { return g_IsFullscreen; }
__declspec(dllexport) void __cdecl ZDISP_ToggleFullscreen(void) { ToggleFullscreen(); }
__declspec(dllexport) void __cdecl ZDISP_GetDimensions(int *w, int *h) {
    int cur_w = g_W < MIN_TARGET_W ? MIN_TARGET_W : g_W;
    int cur_h = g_H < MIN_TARGET_H ? MIN_TARGET_H : g_H;
    if (w) *w = cur_w;
    if (h) *h = cur_h;
}

/* Backwards-compatibility aliases for existing sidecars */
__declspec(dllexport) int __cdecl ZRH_IsActive(void) { return 1; }
__declspec(dllexport) int __cdecl ZRC_IsActive(void) { return 1; }
__declspec(dllexport) int __cdecl ZCR_IsActive(void) { return 1; }
__declspec(dllexport) void __cdecl ZRH_ScreenToGameCoords(int sx, int sy, int *gx, int *gy) { ScreenToGameCoords(sx, sy, gx, gy); }
__declspec(dllexport) void __cdecl ZRH_GameToScreenCoords(int gx, int gy, int *sx, int *sy) { GameToScreenCoords(gx, gy, sx, sy); }
__declspec(dllexport) int __cdecl ZRH_IsFullscreen(void) { return g_IsFullscreen; }

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)h; (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        LogF("zumadisplay.dll loaded (Display & Window Enhancements active: %dx%d)", TARGET_W, TARGET_H);
        InstallScreenHooks();
        InstallCursorHooks();
    } else if (reason == DLL_PROCESS_DETACH) {
        CleanupCustomCursors();
    }
    return TRUE;
}
