/* zumareshandler.dll.c - Resolution and UI Layout Patcher for ZuCompleluxe.
 *
 * Responsibilities:
 *  - Dynamic AOB memory scanning and layout patching across 20+ game screens
 *  - Modal Dialog Universal Centering VTable hook
 *  - Gameplay board graphics clip rects and starfield bounds expansion
 *  - Synchronized Stone Menubar HUD margin patching
 *  - Coordinate translation export forwards to zumadisplay.dll
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>

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
            fprintf(f, "[%8lu][ResHandler] %s\n", (unsigned long)GetTickCount(), buf);
        fflush(f);
        fclose(f);
    }
}


static int g_W = TARGET_W, g_H = TARGET_H;

typedef struct {
    unsigned magic;                      /* 0x00 'ZRES' = 0x53455A52 */
    void *initdx, *ddinit, *gfxctor, *fillrect, *drawscreen;   /* 0x04..0x14 */
    void *volatile slot;                 /* 0x18 -> &ZApi; 0 = DLL not yet active */
    volatile unsigned tried;             /* 0x1C */
    float negx, negy;                    /* 0x20,0x24  projectile left/top clamp bounds (-Xoff,-Yoff) */
    int   rlim, blim;                    /* 0x28,0x2C  projectile right/bottom clamp bounds (W-Xoff, H-Yoff) */
    int   dminx, dmaxx, dminy, dmaxy;    /* 0x30..0x3C dialog drag clamp bounds */
    int   bw, bh;                        /* 0x40,0x44  Board size on creation (default 640x480) */
} ZTable;

_Static_assert(sizeof(ZTable) == 0x48, "ZTable layout");
static ZTable *P = NULL;

typedef struct {
    void *hk_initdx, *hk_ddinit, *hk_rootgfx, *hk_drawscreen;  /* +0x00..0x0C */
    int dim[2];                                                /* +0x10 (H4)  */
    int off[2];                                                /* +0x18 (H6)  */
} ZApi;
static ZApi ZRES_Api;

typedef int   (FC *fInitDX)(void *app, void *edx);
typedef int   (FC *fDDInit)(void *dd, void *edx, HWND h, int win);
typedef void *(FC *fGfxCtor)(void *g, void *edx, void *img);
typedef void  (FC *fFill)(void *g, void *edx, int x, int y, int w, int h);
typedef unsigned char (FC *fDrawScreen)(void *wm, void *edx);

#define APP_W(a)   (*(int *)((char *)(a) + 0x5C))
#define APP_H(a)   (*(int *)((char *)(a) + 0x60))

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

static void SetCells(int W, int H)
{
    int xo, yo;
    Offsets(W, H, &xo, &yo, NULL, NULL);
    if (!P) return;
    P->negx  = xo ? -(float)xo : 0.0f;  P->negy = yo ? -(float)yo : 0.0f;
    P->rlim  = W - xo;       P->blim  = H - yo;
    P->dminx = -xo - 8;      P->dmaxx = W - xo + 8;
    P->dminy = -yo - 8;      P->dmaxy = H - yo + 8;
    P->bw = W;               P->bh = H;
}

static void ApplyMode(int windowed)
{
    (void)windowed;
    int W = TARGET_W < MIN_TARGET_W ? MIN_TARGET_W : TARGET_W;
    int H = TARGET_H < MIN_TARGET_H ? MIN_TARGET_H : TARGET_H;
    int xo, yo;
    g_W = W; g_H = H;
    Offsets(W, H, &xo, &yo, NULL, NULL);
    ZRES_Api.dim[0] = W;  ZRES_Api.dim[1] = H;
    ZRES_Api.off[0] = xo; ZRES_Api.off[1] = yo;
    SetCells(W, H);
}

static void InitResolutionState(void)
{
    g_W = TARGET_W < MIN_TARGET_W ? MIN_TARGET_W : TARGET_W;
    g_H = TARGET_H < MIN_TARGET_H ? MIN_TARGET_H : TARGET_H;
    ZRES_Api.dim[0] = g_W;
    ZRES_Api.dim[1] = g_H;
    Offsets(g_W, g_H, &ZRES_Api.off[0], &ZRES_Api.off[1], NULL, NULL);
}

static void *FindPattern(const char *pattern);
static void **GetAppPtr(void);

static void **GetAppPtr(void) {
    static void **s_app_ptr = NULL;
    if (s_app_ptr) return s_app_ptr;
    void *p = FindPattern("A1 ?? ?? ?? ?? 8B 88 C0 04 00 00");
    if (p) s_app_ptr = *(void ***)((char *)p + 1);
    return s_app_ptr;
}

typedef void (THISCALL *fWidgetResize)(void *this, int x, int y, int w, int h);

static fWidgetResize GetDialogResizeFn(void) {
    static fWidgetResize s_fn = NULL;
    if (s_fn) return s_fn;
    void *p = FindPattern("55 8B EC 8B 45 14 8B 55 0C 53 56 57 50 8B 45 08 8B F1 8B 4D 10 51 52 50 8B CE E8 ?? ?? ?? ?? 8B 4E 60 85 C9");
    if (p) s_fn = (fWidgetResize)p;
    return s_fn;
}

static void **GetDialogResizeSlot(void) {
    static void **s_slot = NULL;
    if (s_slot) return s_slot;
    void *p = FindPattern("8A 45 28 84 C0 C7 45 FC 00 00 00 00 C7 06");
    if (p) {
        void **vt = *(void ***)((char *)p + 14);
        if (vt) s_slot = &vt[0x34 / 4];
    }
    return s_slot;
}

static void UpdateScreenOffsets(void *wm)
{
    void **app_ptr = GetAppPtr();
    void *app = wm ? *(void **)((char *)wm + 0x0C) : (app_ptr ? *app_ptr : NULL);
    void *board = app ? *(void **)((char *)app + 0x5E4) : NULL;
    void *help = app ? *(void **)((char *)app + 0x5F4) : NULL;
    void *credits = app ? *(void **)((char *)app + 0x600) : NULL;

    int W = g_W > 0 ? g_W : TARGET_W;
    int H = g_H > 0 ? g_H : TARGET_H;

    if ((board != NULL && help == NULL) || credits != NULL) {
        /* In-game stage level: fulfill entire screen, top-left aligned at (0, 0) */
        ZRES_Api.off[0] = 0;
        ZRES_Api.off[1] = 0;
        if (P) {
            P->rlim  = W;
            P->blim  = H;
            P->negx  = 0.0f;
            P->negy  = 0.0f;
            P->dminx = -8;
            P->dmaxx = W + 8;
            P->dminy = -8;
            P->dmaxy = H + 8;
            P->bw    = W;
            P->bh    = H;
        }
    } else {
        /* Menus, Credits, or HelpScreen (Instruction Mode): 4:3 centered fit/letterbox */
        int xo = 0, yo = 0, cw = W, ch = H;
        Offsets(W, H, &xo, &yo, &cw, &ch);
        ZRES_Api.off[0] = xo;
        ZRES_Api.off[1] = yo;
        if (P) {
            P->rlim  = W - xo;
            P->blim  = H - yo;
            P->negx  = xo ? -(float)xo : 0.0f;
            P->negy  = yo ? -(float)yo : 0.0f;
            P->dminx = -xo - 8;
            P->dmaxx = W - xo + 8;
            P->dminy = -yo - 8;
            P->dmaxy = H - yo + 8;
            P->bw    = W;
            P->bh    = H;
        }
    }
}

static void FillBlack(unsigned char *g, int x, int y, int w, int h)
{
    if (w > 0 && h > 0 && P && P->fillrect) ((fFill)P->fillrect)(g, 0, x, y, w, h);
}

/* H1: InitDirectX (slot vtbl) */
static int FC hk_initdx(void *app, void *edx)
{
    int r;
    (void)edx;
    ApplyMode(1);
    APP_W(app) = g_W; APP_H(app) = g_H;
    r = ((fInitDX)P->initdx)(app, 0);
    return r;
}

#include "outscreen_data.h"

typedef void (THISCALL *fMemCtor)(void *this, void *app);
static fMemCtor GetMemoryImageCtor(void) {
    static fMemCtor s_ctor = NULL;
    if (s_ctor) return s_ctor;
    void *p = FindPattern("8B 4D 08 33 C0 56 89 45 FC C7 06 ?? ?? ?? ?? 89 4E 40");
    if (p) s_ctor = (fMemCtor)((char *)p - 0x24);
    return s_ctor;
}

static void *s_pOutscreenImg = NULL;
static void *InitOutscreenImage(void) {
    if (s_pOutscreenImg) return s_pOutscreenImg;
    void **app_ptr = GetAppPtr();
    if (!app_ptr || !*app_ptr) return NULL;
    void *app = *app_ptr;

    void *mem = malloc(0x60);
    if (!mem) return NULL;
    memset(mem, 0, 0x60);
    typedef void (THISCALL *fMemCreate)(void *this, int w, int h);
    typedef uint32_t *(THISCALL *fMemGetBits)(void *this);
    typedef void (THISCALL *fMemBitsChanged)(void *this);

    fMemCtor fnMemCtor = GetMemoryImageCtor();
    if (fnMemCtor) fnMemCtor(mem, app);
    void **vt = *(void ***)mem;
    ((fMemCreate)vt[0x54 / 4])(mem, 160, 160);
    uint32_t *bits = ((fMemGetBits)vt[0x58 / 4])(mem);
    if (!bits) {
        bits = *(uint32_t **)((char *)mem + 0x14);
    }
    if (bits) {
        memcpy(bits, s_outscreen_pixels, 160 * 160 * sizeof(uint32_t));
    }
    ((fMemBitsChanged)vt[0x40 / 4])(mem);
    *(int *)((char *)mem + 0x18) += 1;
    *(char *)((char *)mem + 0x3c) = 1;
    s_pOutscreenImg = mem;
    LogF("[ZRES] InitOutscreenImage: successfully created 160x160 MemoryImage @ %p", s_pOutscreenImg);
    return s_pOutscreenImg;
}

typedef void (THISCALL *fWMResize)(void *wm, int width, int height);
typedef void (THISCALL *fDrawImage)(void *g, void *img, int x, int y);
typedef void (THISCALL *fDrawImageStretch)(void *g, void *img, const int *destRect, const int *srcRect);
static fWMResize         g_pfnWMResize = NULL;
static fDrawImage        g_pfnDrawImage = NULL;
static fDrawImageStretch g_pfnDrawImageStretch = NULL;

static void EnsureGraphicsFns(void) {
    if (!g_pfnDrawImage) {
        g_pfnDrawImage        = (fDrawImage)FindPattern("55 8B EC 83 EC 20 53 56 8B F1 8B 5E 08 8B 56 0C");
        g_pfnDrawImageStretch = (fDrawImageStretch)FindPattern("55 8B EC 83 EC 50 8B 45 0C 53 8B 18 56 8B F1 8B 56 0C");
        g_pfnWMResize         = (fWMResize)FindPattern("55 8B EC 8B 45 08 56 8B F1 39 46 1C 8B 4D 0C");
    }
}

static void DrawOutscreenBorders(void *g, int W, int H, int xo, int yo, int cw, int ch) {
    void *img = InitOutscreenImage();
    EnsureGraphicsFns();
    if (!img || !g_pfnDrawImage) return;

    if (xo > 0) {
        for (int x = 0; x < xo; x += 160) {
            for (int y = 0; y < H; y += 160) {
                g_pfnDrawImage(g, img, x, y);
            }
        }
    }
    if (W > xo + cw) {
        for (int x = xo + cw; x < W; x += 160) {
            for (int y = 0; y < H; y += 160) {
                g_pfnDrawImage(g, img, x, y);
            }
        }
    }
    if (yo > 0) {
        for (int y = 0; y < yo; y += 160) {
            for (int x = xo; x < xo + cw; x += 160) {
                g_pfnDrawImage(g, img, x, y);
            }
        }
    }
    if (H > yo + ch) {
        for (int y = yo + ch; y < H; y += 160) {
            for (int x = xo; x < xo + cw; x += 160) {
                g_pfnDrawImage(g, img, x, y);
            }
        }
    }
}

/* H1b: DDInterface::Init (3 call sites) */
static int FC hk_ddinit(void *dd, void *edx, HWND h, int win)
{
    void *app = *(void **)((char *)dd + 4);
    int r = 0;
    (void)edx;

    ApplyMode(1);
    APP_W(app) = g_W; APP_H(app) = g_H;

    r = ((fDDInit)P->ddinit)(dd, 0, h, 1);

    HMODULE hDisp = GetModuleHandleA("zumadisplay.dll");
    if (!hDisp) hDisp = LoadLibraryA("modules\\zumadisplay.dll");
    if (hDisp) {
        typedef void (__cdecl *fZDISP_OnDDInit)(void *dd, HWND hwnd, int win, int w, int h);
        fZDISP_OnDDInit fn = (fZDISP_OnDDInit)GetProcAddress(hDisp, "ZDISP_OnDDInit");
        if (fn) fn(dd, h, win, g_W, g_H);
    } else if (h) {
        RECT rc = { 0, 0, g_W, g_H };
        AdjustWindowRect(&rc, (DWORD)GetWindowLongA(h, GWL_STYLE), FALSE);
        int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
        int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
        int wx = (sw - ww) / 2; if (wx < 0) wx = 0;
        int wy = (sh - wh) / 2; if (wy < 0) wy = 0;
        SetWindowPos(h, HWND_NOTOPMOST, wx, wy, ww, wh, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    return r;
}

/* H5: ctor Graphics root in DrawScreen */
static void *FC hk_rootgfx(void *g, void *edx, void *img)
{
    (void)edx;
    ((fGfxCtor)P->gfxctor)(g, 0, img);
    UpdateScreenOffsets(NULL);
    int W = ZRES_Api.dim[0], H = ZRES_Api.dim[1];
    int xo = ZRES_Api.off[0], yo = ZRES_Api.off[1];
    int cw = W - 2 * xo, ch = H - 2 * yo;

    if (g) {
        *(int *)((char *)g + 0x20) = 0;
        *(int *)((char *)g + 0x24) = 0;
        *(int *)((char *)g + 0x28) = 0;
        *(int *)((char *)g + 0x2C) = 255;
        FillBlack((unsigned char *)g, 0, 0, W, H);
    }

    if (xo > 0 || yo > 0) {
        DrawOutscreenBorders(g, W, H, xo, yo, cw, ch);
        *(int *)((char *)g + 0x08) += xo;
        *(int *)((char *)g + 0x0C) += yo;
        *(int *)((char *)g + 0x10) = xo;
        *(int *)((char *)g + 0x14) = yo;
        *(int *)((char *)g + 0x18) = cw;
        *(int *)((char *)g + 0x1C) = ch;
    } else if (g) {
        /* Fullscreen board span without offset: ensure clip rect spans full W x H */
        *(int *)((char *)g + 0x08) = 0;
        *(int *)((char *)g + 0x0C) = 0;
        *(int *)((char *)g + 0x10) = 0;
        *(int *)((char *)g + 0x14) = 0;
        *(int *)((char *)g + 0x18) = W;
        *(int *)((char *)g + 0x1C) = H;
    }
    return g;
}

typedef struct { char *w; int off; char old; } Saved;

static int Unclip(void *wm, Saved *sv, int max)
{
    unsigned *sent = *(unsigned **)((char *)wm + 0x34), *nd;
    int n = 0, guard = 0;
    if (!sent) return 0;
    for (nd = (unsigned *)sent[0]; nd && nd != sent && guard < 64; nd = (unsigned *)nd[0], guard++) {
        char *w = *(char **)(nd + 2);
        if (w && w[0x1C] && n < max) { sv[n].w = w; sv[n].off = 0x1C; sv[n].old = w[0x1C]; w[0x1C] = 0; n++; }
    }
    return n;
}



/* H7: unclip, lalu DrawScreen asli, lalu restore flag */
static unsigned char FC hk_drawscreen(void *wm, void *edx)
{
    if (!g_pfnWMResize) {
        g_pfnWMResize = (fWMResize)FindPattern("55 8B EC 8B 45 08 56 8B F1 39 46 1C 8B 4D 0C");
    }
    if (wm && g_pfnWMResize) {
        int cur_w = *(int *)((char *)wm + 0x1C);
        int cur_h = *(int *)((char *)wm + 0x20);
        if (cur_w != g_W || cur_h != g_H) {
            g_pfnWMResize(wm, g_W, g_H);
        }
    }

    UpdateScreenOffsets(wm);

    void **app_ptr = GetAppPtr();
    if (app_ptr && *app_ptr) {
        void *app = *app_ptr;
        void *board = *(void **)((char *)app + 0x5E4);
        if (board) {
            void **bvt = *(void ***)board;
            if (bvt && bvt[0xAC / 4]) {
                typedef void (THISCALL *fMarkDirty)(void *this);
                ((fMarkDirty)bvt[0xAC / 4])(board);
            }
        }
        HMODULE hDisp = GetModuleHandleA("zumadisplay.dll");
        if (hDisp) {
            typedef void (__cdecl *fZDISP_OnDrawScreen)(void *app);
            fZDISP_OnDrawScreen fn = (fZDISP_OnDrawScreen)GetProcAddress(hDisp, "ZDISP_OnDrawScreen");
            if (fn) fn(app);
        }
    }

    Saved sv[16];
    int n = 0, i;
    unsigned char r;
    (void)edx;
    n = Unclip(wm, sv, 16);
    r = ((fDrawScreen)P->drawscreen)(wm, 0);
    for (i = 0; i < n; i++) sv[i].w[sv[i].off] = sv[i].old;
    return r;
}

static int LoadTable(void)
{
    char *m = (char *)GetModuleHandleA(NULL);
    IMAGE_DOS_HEADER *d = (IMAGE_DOS_HEADER *)m;
    IMAGE_NT_HEADERS *n = (IMAGE_NT_HEADERS *)(m + d->e_lfanew);
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(n);
    WORD i;
    for (i = 0; i < n->FileHeader.NumberOfSections; i++)
        if (!memcmp(s[i].Name, ".zres\0\0\0", 8)) {
            P = (ZTable *)(m + s[i].VirtualAddress);
            if (P->magic != 0x53455A52) { P = 0; return 0; }
            return 1;
        }
    return 0;
}

static void THISCALL hk_DrawHappyEnding(void *g, void *img, int x, int y)
{
    (void)x; (void)y;
    if (!g || !img) return;
    int W = g_W > 0 ? g_W : TARGET_W;
    int H = g_H > 0 ? g_H : TARGET_H;

    float sw = (float)W / 640.0f;
    float sh = (float)H / 480.0f;
    float scale = sw < sh ? sw : sh;
    int content_w = (int)(640.0f * scale + 0.5f);
    int content_h = (int)(480.0f * scale + 0.5f);
    int xo = (W - content_w) / 2;
    int yo = (H - content_h) / 2;

    FillBlack((unsigned char *)g, 0, 0, W, H);
    DrawOutscreenBorders(g, W, H, xo, yo, content_w, content_h);

    int img_w = *(int *)((char *)img + 4);
    int img_h = *(int *)((char *)img + 8);
    if (img_w <= 0 || img_h <= 0) return;

    if (g_pfnDrawImageStretch) {
        int dest[4] = { xo, yo, content_w, content_h };
        int src[4]  = { 0, 0, img_w, img_h };
        g_pfnDrawImageStretch(g, img, dest, src);
    } else if (g_pfnDrawImage) {
        g_pfnDrawImage(g, img, xo, yo);
    }
}

static int VerifyAndPatchDword(void *addr, DWORD expected_old, DWORD new_val, const char *name) {
    DWORD current = *(DWORD *)addr;
    if (current != expected_old) {
        LogF("[ZRES] VERIFY FAILED @ %p for %s: expected 0x%08X (%u), got 0x%08X (%u)", addr, name, expected_old, expected_old, current, current);
        return 0;
    }
    DWORD old_prot;
    VirtualProtect(addr, 4, PAGE_EXECUTE_READWRITE, &old_prot);
    *(DWORD *)addr = new_val;
    VirtualProtect(addr, 4, old_prot, &old_prot);
    LogF("[ZRES] Patched %s @ %p: %u -> %u", name, addr, expected_old, new_val);
    return 1;
}

static int VerifyAndPatchByte(void *addr, BYTE expected_old, BYTE new_val, const char *name) {
    BYTE current = *(BYTE *)addr;
    if (current != expected_old) {
        LogF("[ZRES] VERIFY FAILED @ %p for %s: expected 0x%02X (%u), got 0x%02X (%u)", addr, name, expected_old, expected_old, current, current);
        return 0;
    }
    DWORD old_prot;
    VirtualProtect(addr, 1, PAGE_EXECUTE_READWRITE, &old_prot);
    *(BYTE *)addr = new_val;
    VirtualProtect(addr, 1, old_prot, &old_prot);
    LogF("[ZRES] Patched %s @ %p: %u -> %u", name, addr, expected_old, new_val);
    return 1;
}

static int VerifyAndPatchBytes(void *addr, const unsigned char *expected, const unsigned char *replacement, size_t len, const char *name) {
    if (!addr) return 0;
    if (expected && memcmp(addr, expected, len) != 0) {
        LogF("[ZRES] VERIFY FAILED @ %p for %s (bytes mismatch)", addr, name);
        return 0;
    }
    DWORD old_prot;
    VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &old_prot);
    memcpy(addr, replacement, len);
    VirtualProtect(addr, len, old_prot, &old_prot);
    LogF("[ZRES] Patched bytes for %s @ %p (len=%u)", name, addr, (unsigned)len);
    return 1;
}

static void *VerifyAndPatchCall(void *site, void *expected_target, void *new_target, const char *name) {
    if (!site) return NULL;
    BYTE op = *(BYTE *)site;
    if (op != 0xE8) {
        LogF("[ZRES] VERIFY FAILED @ %p for %s: opcode is 0x%02X (expected 0xE8)", site, name, op);
        return NULL;
    }
    DWORD rel = *(DWORD *)((char *)site + 1);
    void *current_tgt = (char *)site + 5 + (int)rel;
    if (expected_target && current_tgt != expected_target) {
        LogF("[ZRES] VERIFY FAILED @ %p for %s: target is %p (expected %p)", site, name, current_tgt, expected_target);
        return NULL;
    }
    DWORD old_prot;
    VirtualProtect((char *)site + 1, 4, PAGE_EXECUTE_READWRITE, &old_prot);
    *(DWORD *)((char *)site + 1) = (DWORD)((char *)new_target - ((char *)site + 5));
    VirtualProtect((char *)site + 1, 4, old_prot, &old_prot);
    LogF("[ZRES] Patched call %s @ %p -> %p (orig %p)", name, site, new_target, current_tgt);
    return current_tgt;
}

static inline int SDIM_VAL(int v) {
    float sw = (float)TARGET_W / 640.0f;
    float sh = (float)TARGET_H / 480.0f;
    float scale = sw < sh ? sw : sh;
    return (int)((float)v * scale + 0.5f);
}

/* Identical to 4:3 scaling behavior: Pure scaled coordinates without manual +xo/+yo */
static inline int SX_VAL(int v) {
    return SDIM_VAL(v);
}

static inline int SY_VAL(int v) {
    return SDIM_VAL(v);
}

static inline int PARENT_LOCAL_X(int v) {
    return SDIM_VAL(v);
}

static inline int PARENT_LOCAL_Y(int v) {
    return SDIM_VAL(v);
}

#define THISCALL __attribute__((thiscall))
typedef void (THISCALL *fWidgetResize)(void *w, int x, int y, int width, int height);
typedef void (THISCALL *fDrawDoors)(void *this, void *g, int start, int end);
typedef void (THISCALL *fSetDrawMode)(void *g, int mode);
typedef void *(THISCALL *fColorCtor)(void *color, int r, int g, int b, int a);
typedef void (THISCALL *fSetColor)(void *g, void *color);
typedef void (THISCALL *fTranslate)(void *g, int dx, int dy);

static fDrawDoors        g_pfnDrawDoors = NULL;
static fSetDrawMode      g_pfnSetDrawMode = NULL;
static fSetColor         g_pfnSetColor = NULL;
static fColorCtor        g_pfnColorCtor = NULL;
static fTranslate        g_pfnTranslate = NULL;
static fWidgetResize     g_pfnWidgetResize = NULL;

static void **g_pImgAdvTemple1  = NULL;
static void **g_pImgAdvTemple2  = NULL;
static void **g_pImgAdvTemple2V = NULL;
static void **g_pImgAdvTemple3  = NULL;
static void **g_pImgAdvTemple3V = NULL;
static void **g_pImgAdvTemple4  = NULL;
static void **g_pImgAdvMiddle   = NULL;

#define DrawImg(g, img, x, y)                do { if (g_pfnDrawImage) g_pfnDrawImage((g), (img), (x), (y)); } while(0)
#define DrawDoors(this, g, s, e)             do { if (g_pfnDrawDoors) g_pfnDrawDoors((this), (g), (s), (e)); } while(0)
#define SetDrawMode(g, m)                    do { if (g_pfnSetDrawMode) g_pfnSetDrawMode((g), (m)); } while(0)
#define SetColor(g, c)                       do { if (g_pfnSetColor) g_pfnSetColor((g), (c)); } while(0)
#define ColorCtor(c, r, g, b, a)             (g_pfnColorCtor ? g_pfnColorCtor((c), (r), (g), (b), (a)) : (c))
#define TranslateG(g, dx, dy)                do { if (g_pfnTranslate) g_pfnTranslate((g), (dx), (dy)); } while(0)
#define WidgetResize(w, x, y, width, height) do { if (g_pfnWidgetResize) g_pfnWidgetResize((w), (x), (y), (width), (height)); } while(0)

#define IMG_ADVTEMPLE1  (g_pImgAdvTemple1  ? *g_pImgAdvTemple1  : NULL)
#define IMG_ADVTEMPLE2  (g_pImgAdvTemple2  ? *g_pImgAdvTemple2  : NULL)
#define IMG_ADVTEMPLE2V (g_pImgAdvTemple2V ? *g_pImgAdvTemple2V : NULL)
#define IMG_ADVTEMPLE3  (g_pImgAdvTemple3  ? *g_pImgAdvTemple3  : NULL)
#define IMG_ADVTEMPLE3V (g_pImgAdvTemple3V ? *g_pImgAdvTemple3V : NULL)
#define IMG_ADVTEMPLE4  (g_pImgAdvTemple4  ? *g_pImgAdvTemple4  : NULL)
#define IMG_ADVMIDDLE   (g_pImgAdvMiddle   ? *g_pImgAdvMiddle   : NULL)

struct AdventureDoor {
    void *mImage;    // +0x00
    int   mX1;       // +0x04
    int   mY1;       // +0x08
    int   mCelW;     // +0x0C
    int   mCelH;     // +0x10
    int   mX2;       // +0x14
    int   mY2;       // +0x18
};

static void PopulateAdventureDoors(void *this) {
    if (!this) return;
    struct AdventureDoor *d = (struct AdventureDoor *)((char *)this + 0x68);

    d[0].mX1  = PARENT_LOCAL_X(226); d[0].mY1  = PARENT_LOCAL_Y(300); d[0].mX2  = PARENT_LOCAL_X(256); d[0].mY2  = PARENT_LOCAL_Y(361);
    d[1].mX1  = PARENT_LOCAL_X(296); d[1].mY1  = PARENT_LOCAL_Y(270); d[1].mX2  = PARENT_LOCAL_X(327); d[1].mY2  = PARENT_LOCAL_Y(364);
    d[2].mX1  = PARENT_LOCAL_X(366); d[2].mY1  = PARENT_LOCAL_Y(300); d[2].mX2  = PARENT_LOCAL_X(398); d[2].mY2  = PARENT_LOCAL_Y(360);
    d[3].mX1  = PARENT_LOCAL_X(0);   d[3].mY1  = PARENT_LOCAL_Y(164); d[3].mX2  = PARENT_LOCAL_X(56);  d[3].mY2  = PARENT_LOCAL_Y(218);
    d[4].mX1  = PARENT_LOCAL_X(69);  d[4].mY1  = PARENT_LOCAL_Y(159); d[4].mX2  = PARENT_LOCAL_X(116); d[4].mY2  = PARENT_LOCAL_Y(201);
    d[5].mX1  = PARENT_LOCAL_X(120); d[5].mY1  = PARENT_LOCAL_Y(155); d[5].mX2  = PARENT_LOCAL_X(167); d[5].mY2  = PARENT_LOCAL_Y(188);
    d[6].mX1  = PARENT_LOCAL_X(452); d[6].mY1  = PARENT_LOCAL_Y(154); d[6].mX2  = PARENT_LOCAL_X(462); d[6].mY2  = PARENT_LOCAL_Y(188);
    d[7].mX1  = PARENT_LOCAL_X(509); d[7].mY1  = PARENT_LOCAL_Y(154); d[7].mX2  = PARENT_LOCAL_X(521); d[7].mY2  = PARENT_LOCAL_Y(192);
    d[8].mX1  = PARENT_LOCAL_X(568); d[8].mY1  = PARENT_LOCAL_Y(154); d[8].mX2  = PARENT_LOCAL_X(584); d[8].mY2  = PARENT_LOCAL_Y(196);
    d[9].mX1  = PARENT_LOCAL_X(220); d[9].mY1  = PARENT_LOCAL_Y(35);  d[9].mX2  = PARENT_LOCAL_X(248); d[9].mY2  = PARENT_LOCAL_Y(86);
    d[10].mX1 = PARENT_LOCAL_X(291); d[10].mY1 = PARENT_LOCAL_Y(35);  d[10].mX2 = PARENT_LOCAL_X(324); d[10].mY2 = PARENT_LOCAL_Y(86);
    d[11].mX1 = PARENT_LOCAL_X(369); d[11].mY1 = PARENT_LOCAL_Y(35);  d[11].mX2 = PARENT_LOCAL_X(408); d[11].mY2 = PARENT_LOCAL_Y(85);
}

struct GauntletDoor {
    void *mImage;    // +0x00
    int   mX;        // +0x04
    int   mY;        // +0x08
    int   mCelW;     // +0x0C
    int   mCelH;     // +0x10
};

static void PopulateGauntletDoors(void *this) {
    if (!this) return;
    struct GauntletDoor *gd = (struct GauntletDoor *)((char *)this + 0x9C);
    gd[0].mX = PARENT_LOCAL_X(209); gd[0].mY = PARENT_LOCAL_Y(179);
    gd[1].mX = PARENT_LOCAL_X(232); gd[1].mY = PARENT_LOCAL_Y(129);
    gd[2].mX = PARENT_LOCAL_X(249); gd[2].mY = PARENT_LOCAL_Y(82);
    gd[3].mX = PARENT_LOCAL_X(265); gd[3].mY = PARENT_LOCAL_Y(28);
}

/* Temple Draw Sidecar Hooks */
static void THISCALL hk_adv_draw_temple1(void *this, void *g) {
    PopulateAdventureDoors(this);
    if (IMG_ADVTEMPLE1)
        DrawImg(g, IMG_ADVTEMPLE1, PARENT_LOCAL_X(0), PARENT_LOCAL_Y(188));
    DrawDoors(this, g, 0, 3);
}

static void THISCALL hk_adv_draw_temple2(void *this, void *g) {
    PopulateAdventureDoors(this);
    int unlock = *(int *)((char *)this + 0x1D4);
    int stage  = *(int *)((char *)this + 0x64);
    if (unlock != 2) {
        if (stage < 3) {
            if (IMG_ADVTEMPLE2V) DrawImg(g, IMG_ADVTEMPLE2V, PARENT_LOCAL_X(0), PARENT_LOCAL_Y(112));
            return;
        }
        if (IMG_ADVTEMPLE2) DrawImg(g, IMG_ADVTEMPLE2, PARENT_LOCAL_X(0), PARENT_LOCAL_Y(112));
        DrawDoors(this, g, 3, 6);
        return;
    }
    int trans = *(int *)((char *)this + 0x1DC);
    int alpha = (trans * 255) / 100;
    if (alpha < 0) alpha = 0; if (alpha > 255) alpha = 255;
    SetDrawMode(g, 1);
    int col[4];
    ColorCtor(col, 255, 255, 255, 255 - alpha);
    SetColor(g, col);
    if (IMG_ADVTEMPLE2V) DrawImg(g, IMG_ADVTEMPLE2V, PARENT_LOCAL_X(0), PARENT_LOCAL_Y(112));
    ColorCtor(col, 255, 255, 255, alpha);
    SetColor(g, col);
    if (IMG_ADVTEMPLE2) DrawImg(g, IMG_ADVTEMPLE2, PARENT_LOCAL_X(0), PARENT_LOCAL_Y(112));
    DrawDoors(this, g, 3, 6);
    SetDrawMode(g, 0);
}

static void THISCALL hk_adv_draw_temple3(void *this, void *g) {
    PopulateAdventureDoors(this);
    int unlock = *(int *)((char *)this + 0x1D4);
    int stage  = *(int *)((char *)this + 0x64);
    if (unlock != 3) {
        if (stage < 6) {
            if (IMG_ADVTEMPLE3V) DrawImg(g, IMG_ADVTEMPLE3V, PARENT_LOCAL_X(395), PARENT_LOCAL_Y(125));
            return;
        }
        if (IMG_ADVTEMPLE3) DrawImg(g, IMG_ADVTEMPLE3, PARENT_LOCAL_X(395), PARENT_LOCAL_Y(125));
        DrawDoors(this, g, 6, 9);
        return;
    }
    int trans = *(int *)((char *)this + 0x1DC);
    int alpha = (trans * 255) / 100;
    if (alpha < 0) alpha = 0; if (alpha > 255) alpha = 255;
    SetDrawMode(g, 1);
    int col[4];
    ColorCtor(col, 255, 255, 255, 255 - alpha);
    SetColor(g, col);
    if (IMG_ADVTEMPLE3V) DrawImg(g, IMG_ADVTEMPLE3V, PARENT_LOCAL_X(395), PARENT_LOCAL_Y(125));
    ColorCtor(col, 255, 255, 255, alpha);
    SetColor(g, col);
    if (IMG_ADVTEMPLE3) DrawImg(g, IMG_ADVTEMPLE3, PARENT_LOCAL_X(395), PARENT_LOCAL_Y(125));
    DrawDoors(this, g, 6, 9);
    SetDrawMode(g, 0);
}

static void THISCALL hk_adv_draw_temple4(void *this, void *g) {
    PopulateAdventureDoors(this);
    int unlock = *(int *)((char *)this + 0x1D4);
    int stage  = *(int *)((char *)this + 0x64);
    if (unlock != 4) {
        if (stage < 9) return;
        if (IMG_ADVTEMPLE4) DrawImg(g, IMG_ADVTEMPLE4, PARENT_LOCAL_X(174), PARENT_LOCAL_Y(1));
        DrawDoors(this, g, 9, 12);
        if (IMG_ADVMIDDLE) DrawImg(g, IMG_ADVMIDDLE, PARENT_LOCAL_X(0), PARENT_LOCAL_Y(72));
        return;
    }
    int trans = *(int *)((char *)this + 0x1DC);
    if (trans < 0) {
        if (IMG_ADVMIDDLE) DrawImg(g, IMG_ADVMIDDLE, PARENT_LOCAL_X(0), PARENT_LOCAL_Y(72));
        return;
    }
    int diff = 300 - trans;
    int shift = (int)((((unsigned long long)(diff * diff)) * 0x1B4E81B5ULL) >> 37);
    int transX = *(int *)((char *)this + 0x1D0);
    TranslateG(g, transX, shift);
    if (IMG_ADVTEMPLE4) DrawImg(g, IMG_ADVTEMPLE4, PARENT_LOCAL_X(174), PARENT_LOCAL_Y(1));
    DrawDoors(this, g, 9, 12);
    TranslateG(g, -transX, -shift);
    if (IMG_ADVMIDDLE) DrawImg(g, IMG_ADVMIDDLE, PARENT_LOCAL_X(0), PARENT_LOCAL_Y(72));
}

static void THISCALL hk_moregames_resize(void *this, int x, int y, int w, int h) {
    WidgetResize(this, x, y, w, h);

    void *t0 = *(void **)((char *)this + 0x5C);
    void *t1 = *(void **)((char *)this + 0x60);
    void *t2 = *(void **)((char *)this + 0x64);
    void *t3 = *(void **)((char *)this + 0x68);
    void *btn = *(void **)((char *)this + 0x6C);

    if (t0) (*(fWidgetResize *)(*(void ***)t0 + 0x34/4))(t0, PARENT_LOCAL_X(58), PARENT_LOCAL_Y(107), SDIM_VAL(250), SDIM_VAL(150));
    if (t1) (*(fWidgetResize *)(*(void ***)t1 + 0x34/4))(t1, PARENT_LOCAL_X(330), PARENT_LOCAL_Y(107), SDIM_VAL(250), SDIM_VAL(150));
    if (t2) (*(fWidgetResize *)(*(void ***)t2 + 0x34/4))(t2, PARENT_LOCAL_X(58), PARENT_LOCAL_Y(277), SDIM_VAL(250), SDIM_VAL(150));
    if (t3) (*(fWidgetResize *)(*(void ***)t3 + 0x34/4))(t3, PARENT_LOCAL_X(330), PARENT_LOCAL_Y(277), SDIM_VAL(250), SDIM_VAL(150));
    if (btn) {
        int bw = *(int *)((char *)btn + 0x10);
        int bh = *(int *)((char *)btn + 0x14);
        (*(fWidgetResize *)(*(void ***)btn + 0x34/4))(btn, PARENT_LOCAL_X(164), PARENT_LOCAL_Y(445), bw, bh);
    }
}

static void THISCALL hk_draw_gauntlet_thumbnail(void *g, void *img, int x, int y) {
    if (!g || !img) return;
    int img_w = *(int *)((char *)img + 4);
    int img_h = *(int *)((char *)img + 8);
    if (img_w <= 0 || img_h <= 0) return;

    if (g_pfnDrawImageStretch) {
        int dest[4] = { x, y, SDIM_VAL(320), SDIM_VAL(240) };
        int src[4]  = { 0, 0, img_w, img_h };
        int dest_w  = dest[2];
        int dest_h  = dest[3];
        if (dest_w > 0 && dest_h > 0) {
            long long lhs = (long long)img_w * dest_h;
            long long rhs = (long long)dest_w * img_h;
            if (lhs > rhs) {
                /* Source image is wider than target frame: crop width symmetrically (aspect-fill) */
                int scw = (int)(((long long)img_h * dest_w) / dest_h);
                src[0] = (img_w - scw) / 2;
                src[1] = 0;
                src[2] = scw;
                src[3] = img_h;
            } else if (lhs < rhs) {
                /* Source image is taller than target frame: crop height symmetrically (aspect-fill) */
                int sch = (int)(((long long)img_w * dest_h) / dest_w);
                src[0] = 0;
                src[1] = (img_h - sch) / 2;
                src[2] = img_w;
                src[3] = sch;
            }
        }
        g_pfnDrawImageStretch(g, img, dest, src);
    } else {
        DrawImg(g, img, x, y);
    }
}

static void THISCALL hk_cached_thumbnail_scale(void *g, void *img, const int *destRect, const int *srcRect) {
    if (!g || !img || !g_pfnDrawImageStretch) return;
    int img_w = *(int *)((char *)img + 4);
    int img_h = *(int *)((char *)img + 8);
    int crop_src[4] = { 0, 0, img_w, img_h };
    if (srcRect) {
        crop_src[0] = srcRect[0];
        crop_src[1] = srcRect[1];
        crop_src[2] = srcRect[2];
        crop_src[3] = srcRect[3];
    }
    if (destRect && img_w > 0 && img_h > 0 && destRect[2] > 0 && destRect[3] > 0) {
        int dest_w = destRect[2];
        int dest_h = destRect[3];
        long long lhs = (long long)img_w * dest_h;
        long long rhs = (long long)dest_w * img_h;
        if (lhs > rhs) {
            /* Source canvas is wider than thumbnail target (e.g. 16:9 -> 4:3) */
            int scw = (int)(((long long)img_h * dest_w) / dest_h);
            crop_src[0] = (img_w - scw) / 2;
            crop_src[1] = 0;
            crop_src[2] = scw;
            crop_src[3] = img_h;
        } else if (lhs < rhs) {
            /* Source canvas is taller than thumbnail target */
            int sch = (int)(((long long)img_w * dest_h) / dest_w);
            crop_src[0] = 0;
            crop_src[1] = (img_h - sch) / 2;
            crop_src[2] = img_w;
            crop_src[3] = sch;
        }
    }
    g_pfnDrawImageStretch(g, img, destRect, crop_src);
}

static void THISCALL hk_DrawHappyEnding(void *g, void *img, int x, int y);

static void THISCALL hk_gauntlet_resize(void *this, int x, int y, int w, int h) {
    WidgetResize(this, x, y, w, h);

    PopulateGauntletDoors(this);

    void *btn_practice = *(void **)((char *)this + 0x60);
    void *btn_survival = *(void **)((char *)this + 0x64);
    void *btn_back     = *(void **)((char *)this + 0x5C);
    void *btn_mainmenu = *(void **)((char *)this + 0x68);
    void *btn_next     = *(void **)((char *)this + 0x70);
    void *btn_play     = *(void **)((char *)this + 0x6C);

    char vis = (x == 0);
    if (btn_practice) {
        (*(fWidgetResize *)(*(void ***)btn_practice + 0x34/4))(btn_practice, x + PARENT_LOCAL_X(177), y + PARENT_LOCAL_Y(444), *(int*)((char*)btn_practice+16), *(int*)((char*)btn_practice+20));
        *(char *)((char *)btn_practice + 0x18) = vis;
        *(char *)((char *)btn_practice + 0x19) = vis;
    }
    if (btn_survival) {
        (*(fWidgetResize *)(*(void ***)btn_survival + 0x34/4))(btn_survival, x + PARENT_LOCAL_X(277), y + PARENT_LOCAL_Y(444), *(int*)((char*)btn_survival+16), *(int*)((char*)btn_survival+20));
        *(char *)((char *)btn_survival + 0x18) = vis;
        *(char *)((char *)btn_survival + 0x19) = vis;
    }
    if (btn_back) {
        (*(fWidgetResize *)(*(void ***)btn_back + 0x34/4))(btn_back, x + PARENT_LOCAL_X(390), y + PARENT_LOCAL_Y(444), *(int*)((char*)btn_back+16), *(int*)((char*)btn_back+20));
        *(char *)((char *)btn_back + 0x18) = vis;
        *(char *)((char *)btn_back + 0x19) = vis;
    }
    if (btn_mainmenu) {
        (*(fWidgetResize *)(*(void ***)btn_mainmenu + 0x34/4))(btn_mainmenu, x + PARENT_LOCAL_X(10), y + PARENT_LOCAL_Y(422), *(int*)((char*)btn_mainmenu+16), *(int*)((char*)btn_mainmenu+20));
        *(char *)((char *)btn_mainmenu + 0x18) = vis;
        *(char *)((char *)btn_mainmenu + 0x19) = vis;
    }
    if (btn_next) {
        (*(fWidgetResize *)(*(void ***)btn_next + 0x34/4))(btn_next, x + PARENT_LOCAL_X(506), y + PARENT_LOCAL_Y(413), *(int*)((char*)btn_next+16), *(int*)((char*)btn_next+20));
        *(char *)((char *)btn_next + 0x18) = vis;
        *(char *)((char *)btn_next + 0x19) = vis;
    }
    if (btn_play) {
        (*(fWidgetResize *)(*(void ***)btn_play + 0x34/4))(btn_play, x + PARENT_LOCAL_X(505), y + PARENT_LOCAL_Y(446), *(int*)((char*)btn_play+16), *(int*)((char*)btn_play+20));
        *(char *)((char *)btn_play + 0x18) = vis;
        *(char *)((char *)btn_play + 0x19) = vis;
    }
}

static void THISCALL hk_adventure_resize(void *this, int x, int y, int w, int h) {
    WidgetResize(this, x, y, w, h);

    PopulateAdventureDoors(this);

    void *btn_mainmenu = *(void **)((char *)this + 0x5C);
    void *btn_play     = *(void **)((char *)this + 0x60);

    char vis = (x == 0);
    if (btn_mainmenu) {
        (*(fWidgetResize *)(*(void ***)btn_mainmenu + 0x34/4))(btn_mainmenu, x + PARENT_LOCAL_X(0), y + PARENT_LOCAL_Y(438), *(int*)((char*)btn_mainmenu+16), *(int*)((char*)btn_mainmenu+20));
        *(char *)((char *)btn_mainmenu + 0x18) = vis;
        *(char *)((char *)btn_mainmenu + 0x19) = vis;
    }
    if (btn_play) {
        (*(fWidgetResize *)(*(void ***)btn_play + 0x34/4))(btn_play, x + PARENT_LOCAL_X(542), y + PARENT_LOCAL_Y(441), *(int*)((char*)btn_play+16), *(int*)((char*)btn_play+20));
        *(char *)((char *)btn_play + 0x18) = vis;
        *(char *)((char *)btn_play + 0x19) = vis;
    }
}

static fWidgetResize g_orig_DialogVirtualResize = NULL;
static void THISCALL hk_dialog_virtual_resize(void *this, int x, int y, int w, int h) {
    if (this && *(int *)((char *)this + 0x100) == 999) {
        int W = g_W > 0 ? g_W : TARGET_W;
        int H = g_H > 0 ? g_H : TARGET_H;
        void **app_ptr = GetAppPtr();
        void *app = (app_ptr && *app_ptr) ? *app_ptr : NULL;
        void *board = app ? *(void **)((char *)app + 0x5E4) : NULL;

        if (board) {
            x = (W - w) / 2;
            y = (H - h) / 2;
        } else {
            int xo = 0, yo = 0, cw = W, ch = H;
            Offsets(W, H, &xo, &yo, &cw, &ch);
            x = (cw - w) / 2;
            y = (ch - h) / 2;
        }
        if (y < 10) y = 10;
    }
    if (g_orig_DialogVirtualResize) {
        g_orig_DialogVirtualResize(this, x, y, w, h);
    } else {
        { fWidgetResize fn = GetDialogResizeFn(); if (fn) fn(this, x, y, w, h); }
    }
}

static void THISCALL hk_setup_dialog_resize(void *this, int x, int y, int w, int h) {
    (void)x; (void)y;
    int W = g_W > 0 ? g_W : TARGET_W;
    int H = g_H > 0 ? g_H : TARGET_H;
    void **app_ptr = GetAppPtr();
    void *app = (app_ptr && *app_ptr) ? *app_ptr : NULL;
    void *board = app ? *(void **)((char *)app + 0x5E4) : NULL;

    if (board) {
        if (w > 0 && h > 0) {
            x = (W - w) / 2;
            y = (H - h) / 2;
            if (y < 10) y = 10;
        }
    } else {
        int xo = 0, yo = 0, cw = W, ch = H;
        Offsets(W, H, &xo, &yo, &cw, &ch);
        if (w > 0 && h > 0) {
            x = (cw - w) / 2;
            y = (ch - h) / 2;
            if (y < 10) y = 10;
        }
    }
    void **vt = *(void ***)this;
    fWidgetResize resize = (fWidgetResize)vt[0x34 / 4];
    resize(this, x, y, w, h);
}

static fWidgetResize g_orig_LoadingScreenResize = NULL;
static void THISCALL hk_loading_screen_resize(void *this, int x, int y, int w, int h) {
    (void)x; (void)y; (void)w; (void)h;
    int W = g_W > 0 ? g_W : TARGET_W;
    int H = g_H > 0 ? g_H : TARGET_H;
    int xo = 0, yo = 0, cw = W, ch = H;
    Offsets(W, H, &xo, &yo, &cw, &ch);

    if (g_orig_LoadingScreenResize) {
        g_orig_LoadingScreenResize(this, 0, 0, cw, ch);
    }
}

static fWidgetResize g_orig_HelpResize = NULL;

typedef void (*fDrawCheckbox)(void *g, void *text, void *cb);
static fDrawCheckbox g_orig_DrawCheckbox = NULL;

static void hk_draw_checkbox(void *g, void *text, void *cb) {
    if (!cb || !g_orig_DrawCheckbox) return;
    int xo = ZRES_Api.off[0];
    int yo = ZRES_Api.off[1];
    if (xo == 0 && yo == 0) {
        g_orig_DrawCheckbox(g, text, cb);
        return;
    }

    int *px = (int *)((char *)cb + 8);
    int *py = (int *)((char *)cb + 12);
    *px += xo;
    *py += yo;

    g_orig_DrawCheckbox(g, text, cb);

    *px -= xo;
    *py -= yo;
}

static void THISCALL hk_help_resize(void *this, int x, int y, int w, int h) {
    (void)x; (void)y; (void)w; (void)h;
    int W = g_W > 0 ? g_W : TARGET_W;
    int H = g_H > 0 ? g_H : TARGET_H;

    int xo = 0, yo = 0, cw = W, ch = H;
    Offsets(W, H, &xo, &yo, &cw, &ch);

    if (g_orig_HelpResize) {
        g_orig_HelpResize(this, 0, 0, cw, ch);
    }
    if (cw == 640 && ch == 480) return;

    void *w1 = *(void **)((char *)this + 0x60);
    void *w2 = *(void **)((char *)this + 0x64);
    void *w3 = *(void **)((char *)this + 0x68);
    void *w4 = *(void **)((char *)this + 0x6C);
    void *mCheckbox = *(void **)((char *)this + 0x70);
    void *mDoneBtn = *(void **)((char *)this + 0x74);

    /* w4 is the background stone backing widget (0x4343e0).
       Resize it to (0, 0, cw, ch) so it seamlessly covers the entire 4:3 canvas without gaps. */
    if (w4) {
        void **vt = *(void ***)w4;
        ((fWidgetResize)vt[0x34 / 4])(w4, 0, 0, cw, ch);
    }

    /* Proportional Quadrant Resizing */
    int q_x1 = SDIM_VAL(11);
    int q_y1 = SDIM_VAL(25);
    int q_gap = SDIM_VAL(10);
    int q_w = (cw - 2 * q_x1 - q_gap) / 2;
    int q_h1 = SDIM_VAL(220);

    int q_x2 = q_x1 + q_w + q_gap;
    int q_y2 = q_y1;

    int q_x3 = q_x1;
    int q_y3 = q_y1 + q_h1 + SDIM_VAL(10);
    int q_w3 = (q_x2 + q_w) - q_x3;
    int q_h3 = SDIM_VAL(160);

    if (w1) { void **vt = *(void ***)w1; ((fWidgetResize)vt[0x34 / 4])(w1, q_x1, q_y1, q_w, q_h1); }
    if (w2) { void **vt = *(void ***)w2; ((fWidgetResize)vt[0x34 / 4])(w2, q_x2, q_y2, q_w, q_h1); }
    if (w3) { void **vt = *(void ***)w3; ((fWidgetResize)vt[0x34 / 4])(w3, q_x3, q_y3, q_w3, q_h3); }

    /* Resize OK Button and Checkbox anchored adaptively to bottom right and bottom left */
    int btn_w = SDIM_VAL(150);
    int btn_h = mDoneBtn ? *(int *)((char *)mDoneBtn + 0x14) : SDIM_VAL(48);
    int btn_x = cw - btn_w - SDIM_VAL(10);
    int btn_y = ch - SDIM_VAL(43);
    if (mDoneBtn) {
        void **vt = *(void ***)mDoneBtn;
        ((fWidgetResize)vt[0x34 / 4])(mDoneBtn, btn_x, btn_y, btn_w, btn_h);
    }

    int cb_x = SDIM_VAL(10);
    int cb_y = ch - SDIM_VAL(43);
    int cb_w = 50; /* Native unscaled frame width of checkbutton.gif */
    int cb_h = 46; /* Native unscaled frame height of checkbutton.gif */
    if (mCheckbox) {
        void **vt = *(void ***)mCheckbox;
        ((fWidgetResize)vt[0x34 / 4])(mCheckbox, cb_x, cb_y, cb_w, cb_h);
    }
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

static void *FindVTableSlot(void *func_ptr) {
    char *base = (char *)GetModuleHandleA(NULL);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    unsigned char *rdata_data = NULL;
    DWORD rdata_size = 0;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (memcmp(sec[i].Name, ".rdata", 6) == 0) {
            rdata_data = (unsigned char *)(base + sec[i].VirtualAddress);
            rdata_size = sec[i].Misc.VirtualSize;
            break;
        }
    }
    if (!rdata_data || rdata_size < 4) return NULL;

    DWORD target = (DWORD)func_ptr;
    for (DWORD i = 0; i <= rdata_size - 4; i += 4) {
        if (*(DWORD *)(rdata_data + i) == target) {
            return (void *)(rdata_data + i);
        }
    }
    return NULL;
}

static float s_zero = 0.0f;
static float s_star_max_y = 0.0f;

static unsigned char s_trampoline_FillRect[16];
typedef void (THISCALL *fFillRect)(void *g, int x, int y, int w, int h);
static fFillRect g_orig_FillRect = (fFillRect)s_trampoline_FillRect;

static void THISCALL hk_FillRect(void *g, int x, int y, int w, int h) {
    int W = TARGET_W;
    int H = TARGET_H;

    /* Detect any fullscreen / near-fullscreen fill (covers vanilla screen 640x480 or content box) */
    if (x <= 0 && y <= 0 && w >= 500 && h >= 380) {
        if (g) {
            int trans_x = *(int *)((char *)g + 0x08);
            int trans_y = *(int *)((char *)g + 0x0C);
            int clip_x  = *(int *)((char *)g + 0x10);
            int clip_y  = *(int *)((char *)g + 0x14);
            int clip_w  = *(int *)((char *)g + 0x18);
            int clip_h  = *(int *)((char *)g + 0x1C);

            /* Fill whole window without translation and clipping restrictions */
            *(int *)((char *)g + 0x08) = 0;
            *(int *)((char *)g + 0x0C) = 0;
            *(int *)((char *)g + 0x10) = 0;
            *(int *)((char *)g + 0x14) = 0;
            *(int *)((char *)g + 0x18) = W;
            *(int *)((char *)g + 0x1C) = H;

            g_orig_FillRect(g, 0, 0, W, H);

            /* Restore translation and clip */
            *(int *)((char *)g + 0x08) = trans_x;
            *(int *)((char *)g + 0x0C) = trans_y;
            *(int *)((char *)g + 0x10) = clip_x;
            *(int *)((char *)g + 0x14) = clip_y;
            *(int *)((char *)g + 0x18) = clip_w;
            *(int *)((char *)g + 0x1C) = clip_h;
            return;
        }
    }
    g_orig_FillRect(g, x, y, w, h);
}

static void InstallFillRectHook(void) {
    static int s_done = 0;
    if (s_done) return;
    s_done = 1;
    void *target = FindPattern("55 8B EC 83 EC 20 56 8B F1 8B 46 2C 85 C0 74 44");
    if (!target) return;
    DWORD old_prot;
    VirtualProtect(s_trampoline_FillRect, sizeof(s_trampoline_FillRect), PAGE_EXECUTE_READWRITE, &old_prot);
    memcpy(s_trampoline_FillRect, target, 6);
    s_trampoline_FillRect[6] = 0xE9;
    *(int *)(s_trampoline_FillRect + 7) = (int)((uintptr_t)target + 6 - ((uintptr_t)s_trampoline_FillRect + 11));

    VirtualProtect(target, 6, PAGE_EXECUTE_READWRITE, &old_prot);
    unsigned char *p = (unsigned char *)target;
    p[0] = 0xE9; /* jmp hk_FillRect */
    *(int *)(p + 1) = (int)((uintptr_t)hk_FillRect - ((uintptr_t)target + 5));
    p[5] = 0x90; /* nop */
    VirtualProtect(target, 6, old_prot, &old_prot);
    LogF("[ZRES] Hooked Graphics::FillRect @ %p -> %p", target, hk_FillRect);
}

typedef void (THISCALL *fGfxCtorSingle)(void *g, void *img);
static fGfxCtorSingle g_orig_LevelBgGfxCtor = NULL;

static void THISCALL hk_LevelBgGraphics(void *g, void *img) {
    if (g_orig_LevelBgGfxCtor) {
        g_orig_LevelBgGfxCtor(g, img);
    }
    if (g && g_orig_FillRect) {
        int w = img ? *(int *)((char *)img + 4) : (g_W > 0 ? g_W : TARGET_W);
        int h = img ? *(int *)((char *)img + 8) : (g_H > 0 ? g_H : TARGET_H);
        if (w <= 0) w = TARGET_W;
        if (h <= 0) h = TARGET_H;

        /* Force pure opaque black color on graphics context */
        *(int *)((char *)g + 0x20) = 0;   /* Red */
        *(int *)((char *)g + 0x24) = 0;   /* Green */
        *(int *)((char *)g + 0x28) = 0;   /* Blue */
        *(int *)((char *)g + 0x2C) = 255; /* Alpha */

        g_orig_FillRect(g, 0, 0, w, h);
        LogF("[ZRES] hk_LevelBgGraphics: Pre-cleared mBackground (%p) to opaque black (%dx%d)", img, w, h);
    }
}

static void THISCALL hk_DrawCongratsTitle(void *board, void *g, int y, void *str) {
    int W = TARGET_W;
    int delta_x = (W - 640) / 2;
    if (g) *(int *)((char *)g + 0x08) -= delta_x;
    void **vt = *(void ***)board;
    typedef void (THISCALL *fDrawCentered)(void *b, void *gfx, int yy, void *s);
    fDrawCentered orig = (fDrawCentered)vt[0x80 / 4];
    orig(board, g, y, str);
    if (g) *(int *)((char *)g + 0x08) += delta_x;
}

static void (THISCALL *g_orig_DrawCongratulations)(void *this, void *g) = NULL;
static void THISCALL hk_DrawCongratulations(void *this, void *g) {
    int W = TARGET_W;
    int H = TARGET_H;
    int delta_x = (W - 640) / 2;
    int delta_y = (H - 480) / 2;

    if (g) {
        *(int *)((char *)g + 0x08) += delta_x;
        *(int *)((char *)g + 0x0C) += delta_y;
    }

    if (g_orig_DrawCongratulations) g_orig_DrawCongratulations(this, g);

    if (g) {
        *(int *)((char *)g + 0x08) -= delta_x;
        *(int *)((char *)g + 0x0C) -= delta_y;
    }
}


/* ========================================================
   ZumaTestingKit Repositioning Wrapper (AdvTemple 5 / Stage 13)
   Compensates for +xo / +yo in ZTK when custom resolution is active.
   Zero modifications to zumatestingkit.dll.
   ======================================================== */
typedef void (__cdecl *fZTK_DrawTemple5)(void *mapScreen, void *graphics);
typedef int  (__cdecl *fZTK_CheckDoor5Hit)(void *mapScreen, int mouseX, int mouseY, int vanilla_hit);
typedef int  (__cdecl *fZTK_CheckDoor5Click)(void *mapScreen, int mouseX, int mouseY);
typedef void (__cdecl *fZTK_DrawStage13Banner)(void *mapScreen, void *graphics);

static fZTK_DrawTemple5        g_orig_ZTK_DrawTemple5 = NULL;
static fZTK_CheckDoor5Hit      g_orig_ZTK_CheckDoor5Hit = NULL;
static fZTK_CheckDoor5Click    g_orig_ZTK_CheckDoor5Click = NULL;
static fZTK_DrawStage13Banner  g_orig_ZTK_DrawStage13Banner = NULL;

static void __cdecl hk_ZTK_DrawTemple5(void *mapScreen, void *graphics) {
    if (!g_orig_ZTK_DrawTemple5) return;
    int xo = ZRES_Api.off[0];
    int yo = ZRES_Api.off[1];
    if (graphics && (xo > 0 || yo > 0)) {
        *(int *)((char *)graphics + 0x08) -= xo;
        *(int *)((char *)graphics + 0x0C) -= yo;
        g_orig_ZTK_DrawTemple5(mapScreen, graphics);
        *(int *)((char *)graphics + 0x08) += xo;
        *(int *)((char *)graphics + 0x0C) += yo;
    } else {
        g_orig_ZTK_DrawTemple5(mapScreen, graphics);
    }
}

static void __cdecl hk_ZTK_DrawStage13Banner(void *mapScreen, void *graphics) {
    if (!g_orig_ZTK_DrawStage13Banner) return;
    int xo = ZRES_Api.off[0];
    int yo = ZRES_Api.off[1];
    if (graphics && (xo > 0 || yo > 0)) {
        *(int *)((char *)graphics + 0x08) -= xo;
        *(int *)((char *)graphics + 0x0C) -= yo;
        g_orig_ZTK_DrawStage13Banner(mapScreen, graphics);
        *(int *)((char *)graphics + 0x08) += xo;
        *(int *)((char *)graphics + 0x0C) += yo;
    } else {
        g_orig_ZTK_DrawStage13Banner(mapScreen, graphics);
    }
}

static int __cdecl hk_ZTK_CheckDoor5Hit(void *mapScreen, int mouseX, int mouseY, int vanilla_hit) {
    if (!g_orig_ZTK_CheckDoor5Hit) return vanilla_hit;
    int xo = ZRES_Api.off[0];
    int yo = ZRES_Api.off[1];
    return g_orig_ZTK_CheckDoor5Hit(mapScreen, mouseX + xo, mouseY + yo, vanilla_hit);
}

static int __cdecl hk_ZTK_CheckDoor5Click(void *mapScreen, int mouseX, int mouseY) {
    if (!g_orig_ZTK_CheckDoor5Click) return 0;
    int xo = ZRES_Api.off[0];
    int yo = ZRES_Api.off[1];
    return g_orig_ZTK_CheckDoor5Click(mapScreen, mouseX + xo, mouseY + yo);
}

static void HookZTKIAT(void) {
    static int s_done = 0;
    if (s_done) return;
    HMODULE hZTK = GetModuleHandleA("zumatestingkit.dll");
    if (!hZTK) return;

    FARPROC pDrawTemple5   = GetProcAddress(hZTK, "ZTK_DrawTemple5");
    FARPROC pCheckDoor5Hit = GetProcAddress(hZTK, "ZTK_CheckDoor5Hit");
    FARPROC pCheckDoor5Clk = GetProcAddress(hZTK, "ZTK_CheckDoor5Click");
    FARPROC pDrawBanner    = GetProcAddress(hZTK, "ZTK_DrawStage13Banner");

    if (!pDrawTemple5 && !pCheckDoor5Hit && !pCheckDoor5Clk && !pDrawBanner) return;

    char *base = (char *)GetModuleHandleA(NULL);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt  = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    int hooked_count = 0;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (memcmp(sec->Name, ".zdata\0\0", 8) == 0) {
            uintptr_t *ptr = (uintptr_t *)(base + sec->VirtualAddress);
            size_t count = sec->Misc.VirtualSize / sizeof(uintptr_t);
            for (size_t j = 0; j < count; j++) {
                DWORD old;
                if (ptr[j] == (uintptr_t)pDrawTemple5 && pDrawTemple5) {
                    VirtualProtect(&ptr[j], sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &old);
                    g_orig_ZTK_DrawTemple5 = (fZTK_DrawTemple5)ptr[j];
                    ptr[j] = (uintptr_t)hk_ZTK_DrawTemple5;
                    VirtualProtect(&ptr[j], sizeof(uintptr_t), old, &old);
                    LogF("[ZRES] Hooked ZTK_DrawTemple5 in IAT @ %p -> %p", &ptr[j], hk_ZTK_DrawTemple5);
                    hooked_count++;
                }
                else if (ptr[j] == (uintptr_t)pCheckDoor5Hit && pCheckDoor5Hit) {
                    VirtualProtect(&ptr[j], sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &old);
                    g_orig_ZTK_CheckDoor5Hit = (fZTK_CheckDoor5Hit)ptr[j];
                    ptr[j] = (uintptr_t)hk_ZTK_CheckDoor5Hit;
                    VirtualProtect(&ptr[j], sizeof(uintptr_t), old, &old);
                    LogF("[ZRES] Hooked ZTK_CheckDoor5Hit in IAT @ %p -> %p", &ptr[j], hk_ZTK_CheckDoor5Hit);
                    hooked_count++;
                }
                else if (ptr[j] == (uintptr_t)pCheckDoor5Clk && pCheckDoor5Clk) {
                    VirtualProtect(&ptr[j], sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &old);
                    g_orig_ZTK_CheckDoor5Click = (fZTK_CheckDoor5Click)ptr[j];
                    ptr[j] = (uintptr_t)hk_ZTK_CheckDoor5Click;
                    VirtualProtect(&ptr[j], sizeof(uintptr_t), old, &old);
                    LogF("[ZRES] Hooked ZTK_CheckDoor5Click in IAT @ %p -> %p", &ptr[j], hk_ZTK_CheckDoor5Click);
                    hooked_count++;
                }
                else if (ptr[j] == (uintptr_t)pDrawBanner && pDrawBanner) {
                    VirtualProtect(&ptr[j], sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &old);
                    g_orig_ZTK_DrawStage13Banner = (fZTK_DrawStage13Banner)ptr[j];
                    ptr[j] = (uintptr_t)hk_ZTK_DrawStage13Banner;
                    VirtualProtect(&ptr[j], sizeof(uintptr_t), old, &old);
                    LogF("[ZRES] Hooked ZTK_DrawStage13Banner in IAT @ %p -> %p", &ptr[j], hk_ZTK_DrawStage13Banner);
                    hooked_count++;
                }
            }
            if (hooked_count > 0) s_done = 1;
            break;
        }
    }
}


static void *g_pRedrawResume = NULL;
static void *g_pRedrawAbort  = NULL;

__attribute__((naked)) static void hk_safe_redraw_asm(void) {
    __asm__ volatile (
        ".intel_syntax noprefix\n\t"
        "cmp dword ptr [ebp - 0x24], 2\n\t"
        "je 1f\n\t"
        "mov eax, [esi + 0x2dc]\n\t"
        "test eax, eax\n\t"
        "jz 2f\n\t"
        "mov dword ptr [eax + 0x10], 0\n\t"
        "2:\n\t"
        "mov edx, [esi]\n\t"
        "mov ecx, esi\n\t"
        "call dword ptr [edx + 0x94]\n\t"
        "jmp dword ptr [%0]\n\t"
        "1:\n\t"
        "jmp dword ptr [%1]\n\t"
        ".att_syntax prefix\n\t"
        :
        : "m"(g_pRedrawResume), "m"(g_pRedrawAbort)
    );
}

static void ApplyUIPatches(void) {
    static int s_ui_patched = 0;
    if (s_ui_patched) return;
    s_ui_patched = 1;

    InstallFillRectHook();

    void *pCrashLoc = FindPattern("83 7D DC 02 0F 84 C0 00 00 00 8B 86 DC 02 00 00 C7 40 10 00 00 00 00");
    if (pCrashLoc) {
        g_pRedrawResume = (void *)((char *)pCrashLoc + 33);
        g_pRedrawAbort  = (void *)((char *)pCrashLoc + 0xCA);
        DWORD old;
        if (VirtualProtect(pCrashLoc, 33, PAGE_EXECUTE_READWRITE, &old)) {
            unsigned char patch[33];
            memset(patch, 0x90, 33);
            patch[0] = 0xE9;
            *(DWORD *)(patch + 1) = (DWORD)((char *)hk_safe_redraw_asm - ((char *)pCrashLoc + 5));
            memcpy(pCrashLoc, patch, 33);
            VirtualProtect(pCrashLoc, 33, old, &old);
            FlushInstructionCache(GetCurrentProcess(), pCrashLoc, 33);
            LogF("[ResHandler] Hooked safe Redraw guard @ %p -> %p (resume=%p, abort=%p)",
                 pCrashLoc, hk_safe_redraw_asm, g_pRedrawResume, g_pRedrawAbort);
        }
    }

    float sw = (float)TARGET_W / 640.0f;
    float sh = (float)TARGET_H / 480.0f;
    float scale = sw < sh ? sw : sh;
    int content_w = (int)(640.0f * scale + 0.5f);
    int content_h = (int)(480.0f * scale + 0.5f);

    /* Dynamic engine function resolution (Zero hardcoded addresses) */
    g_pfnDrawImage        = (fDrawImage)FindPattern("55 8B EC 83 EC 20 53 56 8B F1 8B 5E 08 8B 56 0C");
    g_pfnDrawDoors        = (fDrawDoors)FindPattern("55 8B EC 8B 45 0C 53 8B D9 3B 45 10 57 8B F8 7D");
    g_pfnSetDrawMode      = (fSetDrawMode)FindPattern("55 8B EC 8A 45 08 88 41 48 5D C2 04 00");
    g_pfnSetColor         = (fSetColor)FindPattern("55 8B EC 8B 45 08 8B 10 83 C1 20 89 11 8B 50 04");
    g_pfnColorCtor        = (fColorCtor)FindPattern("55 8B EC 8B 55 0C 8B C1 8B 4D 08 89 08 8B 4D 10 89 50 04 8B");
    g_pfnTranslate        = (fTranslate)FindPattern("55 8B EC 8B 45 08 8B 51 08 03 D0 8B 41 0C 89 51");
    g_pfnWidgetResize     = (fWidgetResize)FindPattern("55 8B EC 53 8B 5D 14 56 8B F1 8B 46 08 57 8B 7D");
    g_pfnWMResize         = (fWMResize)FindPattern("55 8B EC 8B 45 08 56 8B F1 39 46 1C 8B 4D 0C");
    g_pfnDrawImageStretch = (fDrawImageStretch)FindPattern("55 8B EC 83 EC 50 8B 45 0C 53 8B 18 56 8B F1 8B 56 0C");

    LogF("[ZRES] Engine funcs: DrawImg=%p DrawDoors=%p SetDrawMode=%p SetColor=%p ColorCtor=%p Translate=%p WidgetResize=%p WMResize=%p DrawImgStretch=%p",
         g_pfnDrawImage, g_pfnDrawDoors, g_pfnSetDrawMode, g_pfnSetColor, g_pfnColorCtor, g_pfnTranslate, g_pfnWidgetResize, g_pfnWMResize, g_pfnDrawImageStretch);

    /* 1. LoadingScreen (loaderbar & "Click Here To Play!" button) - Pure Local Coordinates */
    void *pLoadBarY = FindPattern("5D 01 00 00 68 80");
    if (pLoadBarY) VerifyAndPatchDword(pLoadBarY, 349, PARENT_LOCAL_Y(349), "LoadingBarY");

    void *pLoadBarX = FindPattern("80 00 00 00 89 4D E0 89 4D");
    if (pLoadBarX) VerifyAndPatchDword(pLoadBarX, 128, PARENT_LOCAL_X(128), "LoadingBarX");

    void *pClickPlay = FindPattern("AE 01 00 00 03");
    if (pClickPlay) VerifyAndPatchDword(pClickPlay, 430, PARENT_LOCAL_Y(430), "ClickToPlayY");

    void *fnLoadResize = FindPattern("55 8B EC 8B 45 14 8B 55 0C 53 56 57 8B 7D 08 8B F1 8B 4D 10");
    if (fnLoadResize) {
        void *slot = FindVTableSlot(fnLoadResize);
        if (slot) {
            g_orig_LoadingScreenResize = (fWidgetResize)fnLoadResize;
            DWORD old_prot;
            VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &old_prot);
            *(DWORD *)slot = (DWORD)hk_loading_screen_resize;
            VirtualProtect(slot, 4, old_prot, &old_prot);
            LogF("[ZRES] Hooked LoadingScreen VTable Resize @ %p -> %p (orig=%p)", slot, hk_loading_screen_resize, g_orig_LoadingScreenResize);
        }
    }

    /* 2. MainMenu (5 buttons) - Pure Local Coordinates relative to Parent (0,0) */
    void *pBtn1Y = FindPattern("40 68 C4 01 00 00");
    if (pBtn1Y) VerifyAndPatchByte(pBtn1Y, 64, (BYTE)PARENT_LOCAL_Y(64), "MainMenuBtn1Y");

    void *pBtn1X = FindPattern("C4 01 00 00 56");
    if (pBtn1X) VerifyAndPatchDword(pBtn1X, 452, PARENT_LOCAL_X(452), "MainMenuBtn1X");

    void *pBtn2Y = FindPattern("99 00 00 00 68 B4");
    if (pBtn2Y) VerifyAndPatchDword(pBtn2Y, 153, PARENT_LOCAL_Y(153), "MainMenuBtn2Y");

    void *pBtn2X = FindPattern("B4 01 00 00 56");
    if (pBtn2X) VerifyAndPatchDword(pBtn2X, 436, PARENT_LOCAL_X(436), "MainMenuBtn2X");

    void *pBtn3Y = FindPattern("EC 00 00 00 68 A2");
    if (pBtn3Y) VerifyAndPatchDword(pBtn3Y, 236, PARENT_LOCAL_Y(236), "MainMenuBtn3Y");

    void *pBtn3X = FindPattern("A2 01 00 00 56");
    if (pBtn3X) VerifyAndPatchDword(pBtn3X, 418, PARENT_LOCAL_X(418), "MainMenuBtn3X");

    void *pBtn4Y = FindPattern("31 01 00 00 68 8A 01 00 00");
    if (pBtn4Y) VerifyAndPatchDword(pBtn4Y, 305, PARENT_LOCAL_Y(305), "MainMenuBtn4Y");

    void *pBtn4X = FindPattern("8A 01 00 00 56");
    if (pBtn4X) VerifyAndPatchDword(pBtn4X, 394, PARENT_LOCAL_X(394), "MainMenuBtn4X");

    void *pBtn5Y = FindPattern("3B 01 00 00 68 F0 01 00 00");
    if (pBtn5Y) VerifyAndPatchDword(pBtn5Y, 315, PARENT_LOCAL_Y(315), "MainMenuBtn5Y");

    void *pBtn5X = FindPattern("F0 01 00 00 56 6A");
    if (pBtn5X) VerifyAndPatchDword(pBtn5X, 496, PARENT_LOCAL_X(496), "MainMenuBtn5X");

    /* 3. MainMenu Frog Head & Sun Offsets - Pure Local Coordinates */
    void *pEyesInit = FindPattern("BF 00 00 00 B9 4B");
    if (pEyesInit) {
        VerifyAndPatchDword(pEyesInit, 191, PARENT_LOCAL_X(191), "MainMenuEyesX");
        VerifyAndPatchDword((char *)pEyesInit + 5, 331, PARENT_LOCAL_Y(331), "MainMenuEyesY");
    }

    void *pSun1X = FindPattern("C1 6A BB 50");
    if (pSun1X) {
        VerifyAndPatchByte(pSun1X, 0xC1, (BYTE)(int)(-63.0f * scale + 0.5f), "MainMenuSun1X");
        VerifyAndPatchByte((char *)pSun1X + 2, 0xBB, (BYTE)(int)(-69.0f * scale + 0.5f), "MainMenuSun1Y");
    }

    void *pSun2X = FindPattern("C1 6A BB 51");
    if (pSun2X) {
        VerifyAndPatchByte(pSun2X, 0xC1, (BYTE)(int)(-63.0f * scale + 0.5f), "MainMenuSun2X");
        VerifyAndPatchByte((char *)pSun2X + 2, 0xBB, (BYTE)(int)(-69.0f * scale + 0.5f), "MainMenuSun2Y");
    }

    /* 4. MainMenu Frog Eyes: Idle coordinates, Raycast starts, Eyelid Blink */
    void *pEyeIdle = FindPattern("C7 45 DC 00 60 6B 40 8B 55 DC 89 50 04");
    if (pEyeIdle) {
        double sc_eye1_x = 219.0 * scale;
        double sc_eye1_y = 368.0 * scale;
        double sc_eye2_x = 276.0 * scale;
        double sc_eye2_y = 355.0 * scale;
        DWORD hi_eye1_x = *(DWORD*)((char*)&sc_eye1_x + 4);
        DWORD hi_eye1_y = *(DWORD*)((char*)&sc_eye1_y + 4);
        DWORD hi_eye2_x = *(DWORD*)((char*)&sc_eye2_x + 4);
        DWORD hi_eye2_y = *(DWORD*)((char*)&sc_eye2_y + 4);
        VerifyAndPatchDword((char*)pEyeIdle + 3,    0x406B6000, hi_eye1_x, "EyeIdle1X");
        VerifyAndPatchDword((char*)pEyeIdle + 0x1D, 0x40770000, hi_eye1_y, "EyeIdle1Y");
        VerifyAndPatchDword((char*)pEyeIdle + 0x3C, 0x40714000, hi_eye2_x, "EyeIdle2X");
        VerifyAndPatchDword((char*)pEyeIdle + 0x53, 0x40763000, hi_eye2_y, "EyeIdle2Y");
    }

    void *pEyeRay = FindPattern("C7 45 F0 1D 00 00 00 C7 45 F4 19 00 00 00 C7 45 E0 57 00 00 00 C7 45 E4 10 00 00 00");
    if (pEyeRay) {
        VerifyAndPatchDword((char*)pEyeRay + 3,  29, (DWORD)(int)(29.0f * scale + 0.5f), "EyeRay1X");
        VerifyAndPatchDword((char*)pEyeRay + 10, 25, (DWORD)(int)(25.0f * scale + 0.5f), "EyeRay1Y");
        VerifyAndPatchDword((char*)pEyeRay + 17, 87, (DWORD)(int)(87.0f * scale + 0.5f), "EyeRay2X");
        VerifyAndPatchDword((char*)pEyeRay + 24, 16, (DWORD)(int)(16.0f * scale + 0.5f), "EyeRay2Y");
    }

    void *pBlink = FindPattern("F7 00 00 00 68 BC 00 00 00 51 8B CE");
    if (pBlink) {
        VerifyAndPatchDword(pBlink, 247, PARENT_LOCAL_Y(247), "EyeBlinkY");
        VerifyAndPatchDword((char*)pBlink + 5, 188, PARENT_LOCAL_X(188), "EyeBlinkX");
    }

    /* 5. MainMenu User and Welcome Centering */
    void *pUserCenter = FindPattern("80 02 00 00 2B C1 99 51 8B 4E 70 2B");
    if (pUserCenter) VerifyAndPatchDword(pUserCenter, 640, content_w, "MainMenuUserCenter");

    void *pWelcomeCenter = FindPattern("80 02 00 00 2B C8 8B C1 99");
    if (pWelcomeCenter) VerifyAndPatchDword(pWelcomeCenter, 640, content_w, "MainMenuWelcomeCenter");

    /* 6. AdventureScreen: VTable Resize hook + 4 Temple Call hooks + Draw Text & Plate */
    void *fnAdvResize = FindPattern("55 8B EC 8B 45 14 53 8B 5D 08 56 57 8B 7D 0C 8B F1 8B 4D 10 50 51 57 53 8B CE E8 ?? ?? ?? ?? 8B 46 24");
    if (fnAdvResize) {
        void *slot = FindVTableSlot(fnAdvResize);
        if (slot) {
            DWORD old_prot;
            VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &old_prot);
            *(DWORD *)slot = (DWORD)hk_adventure_resize;
            VirtualProtect(slot, 4, old_prot, &old_prot);
            LogF("[ZRES] Hooked AdventureScreen VTable Resize @ %p -> %p", slot, hk_adventure_resize);
        }
    }

    void *pCallT1 = FindPattern("E8 ?? ?? ?? ?? 8B 87 B8 01 00 00");
    if (pCallT1) {
        void *orig = VerifyAndPatchCall(pCallT1, NULL, (void *)hk_adv_draw_temple1, "AdvTemple1");
        if (orig && *(unsigned char *)orig == 0x55) {
            g_pImgAdvTemple1 = *(void ***)((char *)orig + 4);
        }
    }

    void *pCallT2 = FindPattern("E8 ?? ?? ?? ?? 8B 87 C8 01 00 00");
    if (pCallT2) {
        void *orig = VerifyAndPatchCall(pCallT2, NULL, (void *)hk_adv_draw_temple2, "AdvTemple2");
        if (orig && *(unsigned char *)orig == 0x55) {
            g_pImgAdvTemple2V = *(void ***)((char *)orig + 0x1E);
            g_pImgAdvTemple2  = *(void ***)((char *)orig + 0x35);
        }
    }

    void *pCallT3 = FindPattern("E8 ?? ?? ?? ?? 8B 8F CC 01 00 00");
    if (pCallT3) {
        void *orig = VerifyAndPatchCall(pCallT3, NULL, (void *)hk_adv_draw_temple3, "AdvTemple3");
        if (orig && *(unsigned char *)orig == 0x55) {
            g_pImgAdvTemple3V = *(void ***)((char *)orig + 0x21);
            g_pImgAdvTemple3  = *(void ***)((char *)orig + 0x38);
        }
    }

    void *pCallT4 = FindPattern("E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 90 6A 00");
    if (pCallT4) {
        void *orig = VerifyAndPatchCall(pCallT4, NULL, (void *)hk_adv_draw_temple4, "AdvTemple4");
        if (orig && *(unsigned char *)orig == 0x55) {
            g_pImgAdvTemple4  = *(void ***)((char *)orig + 0x1F);
            g_pImgAdvMiddle   = *(void ***)((char *)orig + 0x43);
        }
    }

    void *pAdvHsTitle = FindPattern("C8 01 00 00 52");
    if (pAdvHsTitle) VerifyAndPatchDword(pAdvHsTitle, 456, PARENT_LOCAL_X(456), "AdvHighscoreTitleX");

    void *pAdvBottomBoxY = FindPattern("7E 01 00 00 68 21 01 00 00");
    if (pAdvBottomBoxY) {
        VerifyAndPatchDword(pAdvBottomBoxY, 382, PARENT_LOCAL_Y(382), "AdvBottomBoxY");
        VerifyAndPatchDword((char*)pAdvBottomBoxY + 5, 289, PARENT_LOCAL_X(289), "AdvBottomBoxX");
    }

    void *pAdvStageNum = FindPattern("68 8C 01 00 00 68 58 01 00 00 8D 45");
    if (pAdvStageNum) {
        VerifyAndPatchDword((char*)pAdvStageNum + 1, 396, PARENT_LOCAL_Y(396), "AdvStageNumY");
        VerifyAndPatchDword((char*)pAdvStageNum + 6, 344, PARENT_LOCAL_X(344), "AdvStageNumX");
    }

    void *pAdvStageTextY = FindPattern("A6 01 00 00 8D 85 58 FF FF");
    if (pAdvStageTextY) VerifyAndPatchDword(pAdvStageTextY, 422, PARENT_LOCAL_Y(422), "AdvStageTextY");

    void *pAdvStageCenterX = FindPattern("46 01 00 00 2B C8 51 8D 55 CC");
    if (pAdvStageCenterX) VerifyAndPatchDword(pAdvStageCenterX, 326, PARENT_LOCAL_X(326), "AdvStageCenterX");

    void *pAdvSubtitleY = FindPattern("C8 01 00 00 B9 46 01 00 00");
    if (pAdvSubtitleY) VerifyAndPatchDword(pAdvSubtitleY, 456, PARENT_LOCAL_Y(456), "AdvSubtitleY");

    void *pAdvSubtitleCenterX = FindPattern("46 01 00 00 2B C8 51 8D 55 A0 52 8B CE E8");
    if (pAdvSubtitleCenterX) VerifyAndPatchDword(pAdvSubtitleCenterX, 326, PARENT_LOCAL_X(326), "AdvSubtitleCenterX");

    void *pAdvHsScoresX = FindPattern("3F 02 00 00 53 8D 8D 00");
    if (pAdvHsScoresX) VerifyAndPatchDword(pAdvHsScoresX, 575, PARENT_LOCAL_X(575), "AdvHighscoreScoresX");

    void *pAdvHsDateX = FindPattern("37 02 00 00 6A 00 6A 00 8D 8D 00 FF FF FF");
    if (pAdvHsDateX) VerifyAndPatchDword(pAdvHsDateX, 567, PARENT_LOCAL_X(567), "AdvHighscoreDateX");

    void *pAdvHsNamesX = FindPattern("D6 01 00 00 8D 43 0C 50 8D");
    if (pAdvHsNamesX) VerifyAndPatchDword(pAdvHsNamesX, 470, PARENT_LOCAL_X(470), "AdvHighscoreNamesX");

    void *pAdvInfoBox = FindPattern("C7 45 90 14 00 00 00 C7 45 94 3C 00 00 00 C7 45 98 96 00 00 00 C7 45 9C 64 00 00 00 E8 ?? ?? ?? ?? 68 4C 53 28 00");
    if (pAdvInfoBox) {
        VerifyAndPatchDword((char*)pAdvInfoBox + 3,  20,  PARENT_LOCAL_X(20),   "AdvInfoBoxX");
        VerifyAndPatchDword((char*)pAdvInfoBox + 10, 60,  PARENT_LOCAL_Y(60),   "AdvInfoBoxY");
        VerifyAndPatchDword((char*)pAdvInfoBox + 17, 150, SDIM_VAL(150),        "AdvInfoBoxW");
        VerifyAndPatchDword((char*)pAdvInfoBox + 24, 100, SDIM_VAL(100),        "AdvInfoBoxH");
    }

    /* 7. GauntletScreen: VTable Resize hook + Draw coords + preview box */
    void *fnGauntResize = FindPattern("55 8B EC 8B 45 14 53 8B 5D 0C 56 57 8B 7D 08 8B F1 8B 4D 10 50 51 53 57 8B CE E8 ?? ?? ?? ?? 8B 4E 60");
    if (fnGauntResize) {
        void *slot = FindVTableSlot(fnGauntResize);
        if (slot) {
            DWORD old_prot;
            VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &old_prot);
            *(DWORD *)slot = (DWORD)hk_gauntlet_resize;
            VirtualProtect(slot, 4, old_prot, &old_prot);
            LogF("[ZRES] Hooked GauntletScreen VTable Resize @ %p -> %p", slot, hk_gauntlet_resize);
        }
    }

    void *pGauntDoorFrameY = FindPattern("DC 00 00 00 68 AA 00 00 00");
    if (pGauntDoorFrameY) {
        VerifyAndPatchDword(pGauntDoorFrameY, 220, PARENT_LOCAL_Y(220), "GauntDoorFrameY");
        VerifyAndPatchDword((char*)pGauntDoorFrameY + 5, 170, PARENT_LOCAL_X(170), "GauntDoorFrameX");

        unsigned char *call_site = (unsigned char *)pGauntDoorFrameY + 12;
        if (*call_site == 0xE8) {
            DWORD old_prot;
            VirtualProtect(call_site, 5, PAGE_EXECUTE_READWRITE, &old_prot);
            *(int *)(call_site + 1) = (int)((uintptr_t)hk_draw_gauntlet_thumbnail - ((uintptr_t)call_site + 5));
            VirtualProtect(call_site, 5, old_prot, &old_prot);
            LogF("[ZRES] Hooked GauntletScreen preview DrawImage @ %p -> %p", call_site, hk_draw_gauntlet_thumbnail);
        }
    }

    void *pGauntHsTitleX = FindPattern("B2 01 00 00 50");
    if (pGauntHsTitleX) VerifyAndPatchDword(pGauntHsTitleX, 434, PARENT_LOCAL_X(434), "GauntHighscoreTitleX");

    void *pGauntDoorNameY = FindPattern("AB 01 00 00 8D 45 BC 50 8B");
    if (pGauntDoorNameY) VerifyAndPatchDword(pGauntDoorNameY, 427, PARENT_LOCAL_Y(427), "GauntDoorNameY");

    void *pGauntDoorNameX = FindPattern("AA 00 00 00 50 8D");
    if (pGauntDoorNameX) VerifyAndPatchDword(pGauntDoorNameX, 170, PARENT_LOCAL_X(170), "GauntDoorNameX");

    void *pGauntDoorNameW = FindPattern("8B 4F 74 8B 51 04 2B D0 8B C2 99");
    if (pGauntDoorNameW) {
        unsigned char expected[6] = { 0x8B, 0x4F, 0x74, 0x8B, 0x51, 0x04 };
        unsigned char repl[6]     = { 0xBA, 0, 0, 0, 0, 0x90 };
        *(DWORD *)(repl + 1) = SDIM_VAL(320);
        VerifyAndPatchBytes(pGauntDoorNameW, expected, repl, 6, "GauntDoorNameW");
    }

    void *pGauntPracticeY = FindPattern("04 01 00 00 8D 45 BC");
    if (pGauntPracticeY) VerifyAndPatchDword(pGauntPracticeY, 260, PARENT_LOCAL_Y(260), "GauntPracticeTextY");

    void *pGauntPracticeX = FindPattern("96 00 00 00 52 8D");
    if (pGauntPracticeX) VerifyAndPatchDword(pGauntPracticeX, 150, PARENT_LOCAL_X(150), "GauntPracticeTextX");

    void *pGauntPracticeW = FindPattern("8B 4F 74 8B 51 04 2B D0 81 C2");
    if (pGauntPracticeW) {
        unsigned char expected[6] = { 0x8B, 0x4F, 0x74, 0x8B, 0x51, 0x04 };
        unsigned char repl[6]     = { 0xBA, 0, 0, 0, 0, 0x90 };
        *(DWORD *)(repl + 1) = SDIM_VAL(320);
        VerifyAndPatchBytes(pGauntPracticeW, expected, repl, 6, "GauntPracticeTextW");
    }

    void *pGauntCustomMap = FindPattern("68 04 01 00 00 68 ?? ?? ?? ?? 8D 45 BC 50 89 F1");
    if (pGauntCustomMap) {
        DWORD old_x = *(DWORD *)((char *)pGauntCustomMap + 6);
        VerifyAndPatchDword((char *)pGauntCustomMap + 1, 260, (DWORD)PARENT_LOCAL_Y(260), "GauntCustomMapTagY");
        VerifyAndPatchDword((char *)pGauntCustomMap + 6, old_x, (DWORD)PARENT_LOCAL_X(old_x), "GauntCustomMapTagX");
    }

    void *pGauntGemY = FindPattern("DC 00 00 00 2B C3");
    if (pGauntGemY) VerifyAndPatchDword(pGauntGemY, 220, PARENT_LOCAL_Y(220), "GauntGemY");

    void *pGauntGemX = FindPattern("30 02 00 00 2B D0");
    if (pGauntGemX) VerifyAndPatchDword(pGauntGemX, 560, PARENT_LOCAL_X(560), "GauntGemX");

    void *pGauntGem3X = FindPattern("BB 30 02 00 00 2B D8");
    if (pGauntGem3X) VerifyAndPatchDword((char*)pGauntGem3X + 1, 560, PARENT_LOCAL_X(560), "GauntGem3X");

    void *pGauntGem3Y = FindPattern("B8 DC 00 00 00 2B C1");
    if (pGauntGem3Y) VerifyAndPatchDword((char*)pGauntGem3Y + 1, 220, PARENT_LOCAL_Y(220), "GauntGem3Y");

    void *pGauntGem2Y = FindPattern("BA DC 00 00 00 2B D3");
    if (pGauntGem2Y) VerifyAndPatchDword((char*)pGauntGem2Y + 1, 220, PARENT_LOCAL_Y(220), "GauntGem2Y");

    void *pGauntHsScoresX = FindPattern("3F 02 00 00 53 8D 8D 38");
    if (pGauntHsScoresX) VerifyAndPatchDword(pGauntHsScoresX, 575, PARENT_LOCAL_X(575), "GauntHighscoreScoresX");

    void *pGauntHsNamesX = FindPattern("BD 01 00 00 8D");
    if (pGauntHsNamesX) VerifyAndPatchDword(pGauntHsNamesX, 445, PARENT_LOCAL_X(445), "GauntHighscoreNamesX");

    void *pGauntHsClipX = FindPattern("37 02 00 00 6A 00 6A 00 8D 8D 38 FF FF FF");
    if (pGauntHsClipX) {
        VerifyAndPatchDword((char*)pGauntHsClipX - 5, 480, (DWORD)content_h, "GauntHighscoreClipH");
        VerifyAndPatchDword(pGauntHsClipX, 567, PARENT_LOCAL_X(567), "GauntHighscoreClipW");
    }

    void *pGauntInfoBox = FindPattern("C7 45 90 14 00 00 00 C7 45 94 3C 00 00 00 C7 45 98 96 00 00 00 C7 45 9C 64 00 00 00 E8 ?? ?? ?? ?? 68 0A 01 5B 00");
    if (pGauntInfoBox) {
        VerifyAndPatchDword((char*)pGauntInfoBox + 3,  20,  PARENT_LOCAL_X(20),   "GauntInfoBoxX");
        VerifyAndPatchDword((char*)pGauntInfoBox + 10, 60,  PARENT_LOCAL_Y(60),   "GauntInfoBoxY");
        VerifyAndPatchDword((char*)pGauntInfoBox + 17, 150, SDIM_VAL(150),        "GauntInfoBoxW");
        VerifyAndPatchDword((char*)pGauntInfoBox + 24, 100, SDIM_VAL(100),        "GauntInfoBoxH");
    }

    /* 8. MoreGamesScreen: VTable Resize hook */
    void *fnMoreGamesResize = FindPattern("55 8B EC 8B 45 14 53 8B 5D 08 56 57 8B 7D 0C 8B F1 8B 4D 10 50 51 57 53 8B CE E8 ?? ?? ?? ?? 8B 4E 5C 8B 11 68 96 00 00 00 68 FA 00 00 00");
    if (fnMoreGamesResize) {
        void *slot = FindVTableSlot(fnMoreGamesResize);
        if (slot) {
            DWORD old_prot;
            VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &old_prot);
            *(DWORD *)slot = (DWORD)hk_moregames_resize;
            VirtualProtect(slot, 4, old_prot, &old_prot);
            LogF("[ZRES] Hooked MoreGamesScreen VTable Resize @ %p -> %p", slot, hk_moregames_resize);
        }
    }

    /* 9. In-Game & Overlay Fill / Buffer Patches */
    /* 9a. Board pause darkening layer: FillRect(g, 0, 0, 640, 480) */
    void *pBoardPauseFill = FindPattern("68 E0 01 00 00 68 80 02 00 00 6A 00 6A 00 8B CB");
    if (pBoardPauseFill) {
        VerifyAndPatchDword((char*)pBoardPauseFill + 1, 480, TARGET_H, "BoardPauseFillH");
        VerifyAndPatchDword((char*)pBoardPauseFill + 6, 640, TARGET_W, "BoardPauseFillW");
    }

    /* 9a2. Fix pause screen turning black: clamp mPauseFadeCount and keep board marked dirty */
    void *pPauseFade = FindPattern("8B 86 D8 00 00 00 83 F8 32 7F");
    if (pPauseFade) {
        char *pJump = (char *)pPauseFade + 9;
        DWORD old_prot;
        VirtualProtect(pJump, 2, PAGE_EXECUTE_READWRITE, &old_prot);
        pJump[0] = (char)0x7D; /* jge */
        pJump[1] = (char)0x07; /* +7 to 0x421D8E (calls MarkDirty every frame) */
        VirtualProtect(pJump, 2, old_prot, &old_prot);
        LogF("[ZRES] Patched Board::Update pause fade loop @ %p (7F AC -> 7D 07)", pJump);
    }

    /* 9b. CurveDrawer buffer: Image::Create(640, 480) */
    void *pCurveDrawerBuf = FindPattern("68 E0 01 00 00 89 44 BE 20 8B 10 68 80 02 00 00");
    if (pCurveDrawerBuf) {
        VerifyAndPatchDword((char*)pCurveDrawerBuf + 1, 480, TARGET_H, "CurveDrawerBufferH");
        VerifyAndPatchDword((char*)pCurveDrawerBuf + 12, 640, TARGET_W, "CurveDrawerBufferW");
    }

    /* 9c. Level background image buffer: Image::Create(640, 480) */
    void *pLevelDefaultBg = FindPattern("68 E0 01 00 00 89 46 4C 8B 10 68 80 02 00 00");
    if (pLevelDefaultBg) {
        VerifyAndPatchDword((char*)pLevelDefaultBg + 1, 480, TARGET_H, "LevelDefaultBgH");
        VerifyAndPatchDword((char*)pLevelDefaultBg + 11, 640, TARGET_W, "LevelDefaultBgW");
    }

    /* 9c-2. Level background image buffer pre-clear: hook Graphics ctor in Level::Load */
    void *pLevelBgGfx = FindPattern("8B 43 4C 50 8D 8D 58 FF FF FF E8");
    if (pLevelBgGfx) {
        g_orig_LevelBgGfxCtor = (fGfxCtorSingle)VerifyAndPatchCall(
            (char *)pLevelBgGfx + 10, NULL, (void *)hk_LevelBgGraphics, "LevelBgGraphicsCtor");
    }

    /* 9d. Sky fill background: FillRect(g, 0, 0, content_w, content_h) */
    void *pSkyFill = FindPattern("8D 55 F0 52 8B CE E8 ?? ?? ?? ?? 68 E0 01 00 00 68 80 02 00 00 6A 00 6A 00 8B CE");
    if (pSkyFill) {
        VerifyAndPatchDword((char*)pSkyFill + 12, 480, (DWORD)content_h, "SkyFillH");
        VerifyAndPatchDword((char*)pSkyFill + 17, 640, (DWORD)content_w, "SkyFillW");
    }

    /* 9e. MainMenuOverlay resize (0, 0, content_w, content_h) & draw fill - Parent Local Box */
    void *pMMOverlayResize = FindPattern("68 E0 01 00 00 68 80 02 00 00 51 89 46 58");
    if (pMMOverlayResize) {
        VerifyAndPatchDword((char*)pMMOverlayResize + 1, 480, (DWORD)content_h, "MainMenuOverlayResizeH");
        VerifyAndPatchDword((char*)pMMOverlayResize + 6, 640, (DWORD)content_w, "MainMenuOverlayResizeW");
    }

    void *pMMOverlayFill = FindPattern("68 E0 01 00 00 68 80 02 00 00 6A 00 6A 00 8B CF");
    if (pMMOverlayFill) {
        VerifyAndPatchDword((char*)pMMOverlayFill + 1, 480, (DWORD)content_h, "MainMenuOverlayFillH");
        VerifyAndPatchDword((char*)pMMOverlayFill + 6, 640, (DWORD)content_w, "MainMenuOverlayFillW");
    }

    /* 9f. ScreenFade fill & LoadingScreen fill */
    void *pScreenFadeFill = FindPattern("8D 4D F0 E8 ?? ?? ?? ?? 8B 75 08 50 8B CE E8 ?? ?? ?? ?? 68 E0 01 00 00 68 80 02 00 00 6A 00 6A 00");
    if (pScreenFadeFill) {
        VerifyAndPatchDword((char*)pScreenFadeFill + 20, 480, (DWORD)TARGET_H, "ScreenFadeFillH");
        VerifyAndPatchDword((char*)pScreenFadeFill + 25, 640, (DWORD)TARGET_W, "ScreenFadeFillW");
    }

    void *pLoadingScreenFill = FindPattern("8D 45 F0 50 8B CE E8 ?? ?? ?? ?? 68 E0 01 00 00 68 80 02 00 00");
    if (pLoadingScreenFill) {
        VerifyAndPatchDword((char*)pLoadingScreenFill + 12, 480, (DWORD)TARGET_H, "LoadingScreenFillH");
        VerifyAndPatchDword((char*)pLoadingScreenFill + 17, 640, (DWORD)TARGET_W, "LoadingScreenFillW");
    }

    /* 9g. Floating Text & Floating Score Clamp (Match / Middle-Click / Combos) */
    void *pFloatingTextClamp = FindPattern("81 FA 58 02 00 00 89 75 0C 7E 0A B8 58 02 00 00");
    if (pFloatingTextClamp) {
        VerifyAndPatchDword((char*)pFloatingTextClamp + 2,  600, TARGET_W - 40, "FloatingTextClampMaxX_1");
        VerifyAndPatchDword((char*)pFloatingTextClamp + 12, 600, TARGET_W - 40, "FloatingTextClampMaxX_2");
        VerifyAndPatchDword((char*)pFloatingTextClamp + 25, 440, TARGET_H - 40, "FloatingTextClampMaxY_1");
        VerifyAndPatchDword((char*)pFloatingTextClamp + 32, 440, TARGET_H - 40, "FloatingTextClampMaxY_2");
    }

    /* 9h. Floating Text Particle Movement Boundary Clamp */
    void *pFloatingTextUpdate = FindPattern("81 F9 76 02 00 00 7C 10 8B 56 0C");
    if (pFloatingTextUpdate) {
        VerifyAndPatchDword((char*)pFloatingTextUpdate + 2,  630, TARGET_W - 10, "FloatingTextBoundX_1");
        VerifyAndPatchDword((char*)pFloatingTextUpdate + 15, 640, TARGET_W,      "FloatingTextBoundX_2");
        VerifyAndPatchDword((char*)pFloatingTextUpdate + 46, 470, TARGET_H - 10, "FloatingTextBoundY_1");
        VerifyAndPatchDword((char*)pFloatingTextUpdate + 59, 480, TARGET_H,      "FloatingTextBoundY_2");
    }

    /* 9i. Board::Draw Graphics Clip Rects (Expanding in-game drawing bounds) */
    void *pBoardClip1 = FindPattern("68 E0 01 00 00 68 80 02 00 00 53 53 8B CE E8 ?? ?? ?? ?? 68 00 FF FF 00");
    if (pBoardClip1) {
        VerifyAndPatchDword((char*)pBoardClip1 + 1, 480, TARGET_H, "BoardClipRect1H");
        VerifyAndPatchDword((char*)pBoardClip1 + 6, 640, TARGET_W, "BoardClipRect1W");
    }

    void *pBoardClip2 = FindPattern("8B 75 08 8D 45 F0 50 8B CE E8 ?? ?? ?? ?? 68 E0 01 00 00 68 80 02 00 00 6A 00 6A 00 8B CE");
    if (pBoardClip2) {
        VerifyAndPatchDword((char*)pBoardClip2 + 15, 480, TARGET_H, "BoardClipRect2H");
        VerifyAndPatchDword((char*)pBoardClip2 + 20, 640, TARGET_W, "BoardClipRect2W");
    }

    void *pBoardClip3 = FindPattern("68 E0 01 00 00 68 80 02 00 00 6A 00 8B CE 6A 00 E8 ?? ?? ?? ?? 5B 8B 4F 04");
    if (pBoardClip3) {
        VerifyAndPatchDword((char*)pBoardClip3 + 1, 480, TARGET_H, "BoardClipRect3H");
        VerifyAndPatchDword((char*)pBoardClip3 + 6, 640, TARGET_W, "BoardClipRect3W");
    }

    void *pBoardClipReset = FindPattern("68 E0 01 00 00 68 80 02 00 00 6A 00 6A 00 8B CE E8 ?? ?? ?? ?? 5E 8B E5 5D");
    if (pBoardClipReset) {
        VerifyAndPatchDword((char*)pBoardClipReset + 1, 480, TARGET_H, "BoardClipResetH");
        VerifyAndPatchDword((char*)pBoardClipReset + 6, 640, TARGET_W, "BoardClipResetW");
    }

    /* 9j. Gauntlet Mode Level Banner (bottom-right text e.g. 'RABBIT 1') */
    void *pGauntLevelBanner = FindPattern("68 B8 01 00 00 57 FF 50 1C B9 58 02 00 00 2B C8");
    if (pGauntLevelBanner) {
        VerifyAndPatchDword((char*)pGauntLevelBanner + 1,  440, TARGET_H - 40, "GauntLevelBannerY");
        VerifyAndPatchDword((char*)pGauntLevelBanner + 10, 600, TARGET_W - 40, "GauntLevelBannerX");
    }

    /* 9k. Stage Complete Sparkles / Particle Spread (600 / 640) */
    void *pStageCompleteSparkles = FindPattern("B9 58 02 00 00 F7 F9 51 83 EA 78 89 55 EC");
    if (pStageCompleteSparkles) {
        VerifyAndPatchDword((char*)pStageCompleteSparkles + 1,  600, TARGET_H + 120, "StageCompleteSparklesH");
        VerifyAndPatchDword((char*)pStageCompleteSparkles + 27, 640, TARGET_W,       "StageCompleteSparklesW");
    }

    /* 9l. MainMenuOverlay Second ClipRect */
    void *pMMOverlayClip = FindPattern("68 E0 01 00 00 68 80 02 00 00 53 53 8B CF E8");
    if (pMMOverlayClip) {
        VerifyAndPatchDword((char*)pMMOverlayClip + 1, 480, (DWORD)content_h, "MainMenuOverlayClipH");
        VerifyAndPatchDword((char*)pMMOverlayClip + 6, 640, (DWORD)content_w, "MainMenuOverlayClipW");
    }

    /* 9m. Universal Dialog Box Dynamic Centering */
    void *pSetupDialogResize = FindPattern("8B C1 8B CE FF 50 34 8B 56 60");
    if (pSetupDialogResize) {
        DWORD old_prot;
        VirtualProtect(pSetupDialogResize, 7, PAGE_EXECUTE_READWRITE, &old_prot);
        unsigned char *p = (unsigned char *)pSetupDialogResize;
        p[0] = 0x8B; /* mov ecx, esi */
        p[1] = 0xCE;
        p[2] = 0xE8; /* call hk_setup_dialog_resize */
        *(int *)(p + 3) = (int)((uintptr_t)hk_setup_dialog_resize - ((uintptr_t)p + 7));
        VirtualProtect(pSetupDialogResize, 7, old_prot, &old_prot);
        LogF("[ResHandler] Hooked SetupDialog Resize @ %p -> %p", pSetupDialogResize, hk_setup_dialog_resize);
    }

    /* 9m-ztk. TestingKit Dialog 999 Centering via Dialog VTable slot 0x34 */
    void **pDialogVTableSlot = GetDialogResizeSlot();
    if (pDialogVTableSlot && *pDialogVTableSlot) {
        DWORD old_prot;
        VirtualProtect(pDialogVTableSlot, sizeof(void *), PAGE_EXECUTE_READWRITE, &old_prot);
        g_orig_DialogVirtualResize = (fWidgetResize)*pDialogVTableSlot;
        *pDialogVTableSlot = (void *)hk_dialog_virtual_resize;
        VirtualProtect(pDialogVTableSlot, sizeof(void *), old_prot, &old_prot);
        LogF("[ResHandler] Hooked Dialog VTable Slot @ %p -> %p (targeting ZTK Dialog 999)", pDialogVTableSlot, hk_dialog_virtual_resize);
    }

    /* 9m2. Checkbox & Slider Track Text Alignment Hook (OptionsDialog & HelpScreen Checkboxes) */
    void *pDrawCheckboxEntry = FindPattern("55 8B EC 83 EC 14 8B 45 10 8B 50 08");
    if (pDrawCheckboxEntry) {
        g_orig_DrawCheckbox = (fDrawCheckbox)pDrawCheckboxEntry;

        void *pCallCbHelp = FindPattern("C8 51 56 C7 45 FC 00 00 00 00 E8 ?? ?? ?? ?? 8B 45 E0 83 C4 0C");
        if (pCallCbHelp) VerifyAndPatchCall((char *)pCallCbHelp + 10, (void *)pDrawCheckboxEntry, (void *)hk_draw_checkbox, "DrawCheckboxHelp");

        void *pCallCbFull = FindPattern("D8 51 57 C7 45 FC 02 00 00 00 E8 ?? ?? ?? ?? 8B 45 F0 83 C4 0C");
        if (pCallCbFull) VerifyAndPatchCall((char *)pCallCbFull + 10, (void *)pDrawCheckboxEntry, (void *)hk_draw_checkbox, "DrawCheckboxFullscreen");

        void *pCallCbCursor = FindPattern("D8 51 57 C7 45 FC 03 00 00 00 E8 ?? ?? ?? ?? 8B 45 F0 89 5D FC");
        if (pCallCbCursor) VerifyAndPatchCall((char *)pCallCbCursor + 10, (void *)pDrawCheckboxEntry, (void *)hk_draw_checkbox, "DrawCheckboxCustomCursor");

        void *pCallCb3D = FindPattern("BC 51 57 C7 45 FC 04 00 00 00 E8 ?? ?? ?? ?? 8B 45 D4 83 C4 0C");
        if (pCallCb3D) VerifyAndPatchCall((char *)pCallCb3D + 10, (void *)pDrawCheckboxEntry, (void *)hk_draw_checkbox, "DrawCheckbox3DAccel");
    }

    /* 9n. HelpScreen (Instruction Screen & Dev Mode Instructions) Centering */
    void *pHelpResizeBody = FindPattern("8B 4E 60 68 DC 00 00 00 03 D8 03 FA 83 C0 0A 68 31 01 00 00 83 C2 19 89 45 14 8B 01 52 8B 55 14");
    if (pHelpResizeBody) {
        void *fnHelpResize = (char *)pHelpResizeBody - 0x2B;
        void *slot = FindVTableSlot(fnHelpResize);
        if (slot) {
            g_orig_HelpResize = (fWidgetResize)fnHelpResize;
            DWORD old_prot;
            VirtualProtect(slot, 4, PAGE_EXECUTE_READWRITE, &old_prot);
            *(DWORD *)slot = (DWORD)hk_help_resize;
            VirtualProtect(slot, 4, old_prot, &old_prot);
            LogF("[ZRES] Hooked HelpScreen VTable Resize @ %p -> %p (orig=%p)", slot, hk_help_resize, g_orig_HelpResize);
        }
    }

    /* 9o. HelpScreen Background Image Draw Position (IMAGE_HELP_BACK @ 0x434440) */
    void *pHelpBack = FindPattern("8B 4D 08 6A 33 6A 0B 50");
    if (pHelpBack) {
        BYTE new_y = (BYTE)(SY_VAL(51) > 127 ? 127 : SY_VAL(51));
        BYTE new_x = (BYTE)(SX_VAL(11) > 127 ? 127 : SX_VAL(11));
        VerifyAndPatchByte((char *)pHelpBack + 4, 0x33, new_y, "HelpBackY");
        VerifyAndPatchByte((char *)pHelpBack + 6, 0x0B, new_x, "HelpBackX");
    }

    /* 9p. HelpScreen Lower Compartments & Tutorial Ball Dynamic Positions (0x437780 & 0x436030) */
    float h_sx = scale;
    float h_sy = scale;

    /* Skull Pos @ 0x4377D5 & 0x4377E7 (6A 14 6A 41) */
    void *pSkull1 = FindPattern("6A 14 6A 41 51 8B CE E8");
    if (pSkull1) {
        VerifyAndPatchByte((char *)pSkull1 + 1, 0x14, (BYTE)((float)0x14 * h_sy + 0.5f), "HelpSkullY");
        VerifyAndPatchByte((char *)pSkull1 + 3, 0x41, (BYTE)((float)0x41 * h_sx + 0.5f), "HelpSkullX");
    }
    void *pSkullCover = FindPattern("6A 00 6A 14 6A 41 52 8B CE E8");
    if (pSkullCover) {
        VerifyAndPatchByte((char *)pSkullCover + 3, 0x14, (BYTE)((float)0x14 * h_sy + 0.5f), "HelpSkullCoverY");
        VerifyAndPatchByte((char *)pSkullCover + 5, 0x41, (BYTE)((float)0x41 * h_sx + 0.5f), "HelpSkullCoverX");
    }
    /* Skull Text @ 0x43780E (C7 45 E4 1E 00 00 00) */
    void *pSkullText = FindPattern("C7 45 E4 1E 00 00 00 89 5D E8 C7 45 EC A0 00 00 00");
    if (pSkullText) {
        VerifyAndPatchDword((char *)pSkullText + 3, 30, (DWORD)(30.0f * h_sx + 0.5f), "HelpSkullTextX");
        VerifyAndPatchDword((char *)pSkullText + 13, 160, (DWORD)(160.0f * h_sx + 0.5f), "HelpSkullTextW");
    }

    /* Coin Pos @ 0x4378B5 (6A 2D 68 1A 01 00 00) */
    void *pCoinPos = FindPattern("6A 2D 68 1A 01 00 00 52 E8");
    if (pCoinPos) {
        VerifyAndPatchByte((char *)pCoinPos + 1, 0x2D, (BYTE)((float)0x2D * h_sy + 0.5f), "HelpCoinY");
        VerifyAndPatchDword((char *)pCoinPos + 3, 282, (DWORD)(282.0f * h_sx + 0.5f), "HelpCoinX");
    }
    /* Coin Text @ 0x4378CA (C7 45 D4 DC 00 00 00) */
    void *pCoinText = FindPattern("C7 45 D4 DC 00 00 00 89 5D D8 C7 45 DC A0 00 00 00");
    if (pCoinText) {
        VerifyAndPatchDword((char *)pCoinText + 3, 220, (DWORD)(220.0f * h_sx + 0.5f), "HelpCoinTextX");
        VerifyAndPatchDword((char *)pCoinText + 13, 160, (DWORD)(160.0f * h_sx + 0.5f), "HelpCoinTextW");
    }

    /* Swap Frog Pos @ 0x437961 (6A 41 68 F4 01 00 00) */
    void *pFrogPos = FindPattern("6A 41 68 F4 01 00 00 E8 ?? ?? ?? ?? 8B 4F 58 56");
    if (pFrogPos) {
        VerifyAndPatchByte((char *)pFrogPos + 1, 0x41, (BYTE)((float)0x41 * h_sy + 0.5f), "HelpSwapFrogY");
        VerifyAndPatchDword((char *)pFrogPos + 3, 500, (DWORD)(500.0f * h_sx + 0.5f), "HelpSwapFrogX");
    }
    /* Swap Text @ 0x437984 (C7 45 C4 AE 01 00 00) */
    void *pSwapText = FindPattern("C7 45 C4 AE 01 00 00 C7 45 CC AA 00 00 00");
    if (pSwapText) {
        VerifyAndPatchDword((char *)pSwapText + 3, 430, (DWORD)(430.0f * h_sx + 0.5f), "HelpSwapTextX");
        VerifyAndPatchDword((char *)pSwapText + 10, 170, (DWORD)(170.0f * h_sx + 0.5f), "HelpSwapTextW");
    }
    /* Swap Click Text @ 0x437A07 (6A 37 68 30 02 00 00) */
    void *pSwapClick = FindPattern("6A 37 68 30 02 00 00 8D 55 A8 52 8B CE");
    if (pSwapClick) {
        VerifyAndPatchByte((char *)pSwapClick + 1, 0x37, (BYTE)((float)0x37 * h_sy + 0.5f), "HelpSwapClickY");
        VerifyAndPatchDword((char *)pSwapClick + 3, 560, (DWORD)(560.0f * h_sx + 0.5f), "HelpSwapClickX");
    }
    /* Swap Mouse Icon @ 0x437A50 (6A 3C 68 44 02 00 00) */
    void *pSwapMouse = FindPattern("6A 3C 68 44 02 00 00 52 8B CE");
    if (pSwapMouse) {
        VerifyAndPatchByte((char *)pSwapMouse + 1, 0x3C, (BYTE)((float)0x3C * h_sy + 0.5f), "HelpSwapMouseY");
        VerifyAndPatchDword((char *)pSwapMouse + 3, 580, (DWORD)(580.0f * h_sx + 0.5f), "HelpSwapMouseX");
    }

    /* Widget1 Yellow Ball Target @ 0x4360F9 (C7 46 60 A0 00 00 00) */
    void *pW1BallTarget = FindPattern("C7 46 60 A0 00 00 00 8B 4E 60 C7 46 5C F0 00 00 00");
    if (pW1BallTarget) {
        VerifyAndPatchDword((char *)pW1BallTarget + 3, 160, (DWORD)(160.0f * h_sy + 0.5f), "HelpW1BallTargetY");
        VerifyAndPatchDword((char *)pW1BallTarget + 13, 240, (DWORD)(240.0f * h_sx + 0.5f), "HelpW1BallTargetX");
    }

    /* HelpScreen w1 & w2 Top Instruction Text Y Repositioning (Uncovering from Stone Border) */
    void *pW1TopText = FindPattern("C7 45 D4 1E 00 00 00 C7 45 D8 28 00 00 00");
    if (pW1TopText) {
        VerifyAndPatchDword((char *)pW1TopText + 3,  30, (DWORD)(30.0f * h_sx + 0.5f), "HelpW1TopTextX");
        VerifyAndPatchDword((char *)pW1TopText + 10, 40, (DWORD)(40.0f * h_sy + 0.5f), "HelpW1TopTextY");
    }

    void *pW2TopText = FindPattern("C7 45 E0 08 00 00 00 C7 45 E4 28 00 00 00");
    if (pW2TopText) {
        VerifyAndPatchDword((char *)pW2TopText + 3,  8,  (DWORD)(8.0f * h_sx + 0.5f),  "HelpW2TopTextX");
        VerifyAndPatchDword((char *)pW2TopText + 10, 40, (DWORD)(40.0f * h_sy + 0.5f), "HelpW2TopTextY");
    }

    /* HelpScreen w1 Frog Tutorial Pos */
    void *pW1FrogPos = FindPattern("2B C2 D1 F8 83 C0 3C 50 8B 41 24 99 2B C2 D1 F8 83 C0 0A 50 E8");
    if (pW1FrogPos) {
        BYTE new_frog_y = (BYTE)(60.0f * h_sy + 0.5f > 127 ? 127 : 60.0f * h_sy + 0.5f);
        VerifyAndPatchByte((char *)pW1FrogPos + 6, 0x3C, new_frog_y, "HelpW1FrogPosY");
    }

    /* 9q. Stage Complete Cutscene Centering (Stage Text, Sparkles, Frog Waypoints, Complete Text) */
    float scale_w = (float)TARGET_W / 640.0f;
    DWORD stage_complete_cx = (DWORD)(TARGET_W / 2);
    int delta_y = (TARGET_H - 480) / 2;
    DWORD stage_complete_cy = (DWORD)(250 + delta_y);
    DWORD stage_text_y      = (DWORD)(150 + delta_y);
    float f_complete_cy     = (float)stage_complete_cy;

    DWORD frog_waypoint_b_x = stage_complete_cx - (DWORD)(170.0f * scale_w + 0.5f);
    DWORD frog_waypoint_c_x = stage_complete_cx + (DWORD)(180.0f * scale_w + 0.5f);
    DWORD frog_waypoint_d_x = stage_complete_cx;

    /* "STAGE %d" Text Center X @ 0x428FA3 */
    void *pStageTextCX = FindPattern("D1 F8 BF 40 01 00 00 2B F8");
    if (pStageTextCX) {
        VerifyAndPatchDword((char *)pStageTextCX + 3, 320, stage_complete_cx, "StageCompleteStageTextCX");
    }

    /* "STAGE %d" Text Sparkles Y @ 0x429088 */
    void *pStageSparkleY = FindPattern("68 00 FF FF 00 68 96 00 00 00 57 52 8B CE");
    if (pStageSparkleY) {
        VerifyAndPatchDword((char *)pStageSparkleY + 6, 150, stage_text_y, "StageCompleteStageSparkleY");
    }

    /* "STAGE %d" Text Char Draw Y @ 0x4290D2 */
    void *pStageCharDrawY = FindPattern("8B 4A 7C 51 68 96 00 00 00 57 50");
    if (pStageCharDrawY) {
        VerifyAndPatchDword((char *)pStageCharDrawY + 5, 150, stage_text_y, "StageCompleteStageCharDrawY");
    }

    /* "STAGE %d" Text Init Waypoints Y @ 0x429009 & 0x42902D */
    void *pStageTextY1 = FindPattern("8D 81 FA 00 00 00 99 89 4D C8");
    if (pStageTextY1) {
        VerifyAndPatchDword((char *)pStageTextY1 + 2, 250, stage_complete_cy, "StageCompleteStageTextY1");
    }
    void *pStageTextY2 = FindPattern("C7 45 C0 FA 00 00 00 E8 ?? ?? ?? ?? 8B 4D DC");
    if (pStageTextY2) {
        VerifyAndPatchDword((char *)pStageTextY2 + 3, 250, stage_complete_cy, "StageCompleteStageTextY2");
    }
    void *pStageTextY3 = FindPattern("89 7D EC C7 45 E4 FA 00 00 00 FF 52 20");
    if (pStageTextY3) {
        VerifyAndPatchDword((char *)pStageTextY3 + 6, 250, stage_complete_cy, "StageCompleteStageTextY3");
    }

    /* Frog Waypoints 3-step trajectory (X) @ 0x429199 */
    void *pFrogWaypointsX = FindPattern("C7 45 CC 96 00 00 00 C7 45 E0 F4 01 00 00 C7 45 C4 40 01 00 00");
    if (pFrogWaypointsX) {
        VerifyAndPatchDword((char *)pFrogWaypointsX + 3,  150, frog_waypoint_b_x, "StageCompleteFrogWaypointBX");
        VerifyAndPatchDword((char *)pFrogWaypointsX + 10, 500, frog_waypoint_c_x, "StageCompleteFrogWaypointCX");
        VerifyAndPatchDword((char *)pFrogWaypointsX + 17, 320, frog_waypoint_d_x, "StageCompleteFrogWaypointDX");
    }

    /* Frog Waypoints Baseline Y @ 0x42917A */
    void *pFrogWaypointY = FindPattern("B8 FA 00 00 00 03 C8 6A 14 8D 55 CC");
    if (pFrogWaypointY) {
        VerifyAndPatchDword((char *)pFrogWaypointY + 1, 250, stage_complete_cy, "StageCompleteFrogWaypointY");
    }

    /* "COMPLETE" Sparkle X Center @ 0x429214 */
    void *pSparkleCX = FindPattern("D1 F9 B8 40 01 00 00 2B C1 33 FF 89 45 EC");
    if (pSparkleCX) {
        VerifyAndPatchDword((char *)pSparkleCX + 3, 320, stage_complete_cx, "StageCompleteSparkleCX");
    }

    /* "COMPLETE" Sparkle Float Y @ 0x4292A1 */
    void *pCompleteSparkleFloatY = FindPattern("03 D1 68 00 00 7A 43 51 8B 4E 04");
    if (pCompleteSparkleFloatY) {
        DWORD dw_float_y = *(DWORD *)&f_complete_cy;
        VerifyAndPatchDword((char *)pCompleteSparkleFloatY + 3, 0x437A0000, dw_float_y, "StageCompleteSparkleFloatY");
    }

    /* "COMPLETE" Text Draw Center X & Y @ 0x426DBE & 0x426DC3 */
    void *pCompleteTextCX = FindPattern("68 FA 00 00 00 BA 40 01 00 00 2B D0 52");
    if (pCompleteTextCX) {
        VerifyAndPatchDword((char *)pCompleteTextCX + 1, 250, stage_complete_cy, "StageCompleteCompleteTextY");
        VerifyAndPatchDword((char *)pCompleteTextCX + 6, 320, stage_complete_cx, "StageCompleteCompleteTextCX");
    }

    /* Cutscene Overlay Widget Resize (Bounds expansion) @ 0x4189ED */
    void *pCutsceneWidgetResize = FindPattern("68 E0 01 00 00 68 80 02 00 00 53 53 FF 50 34");
    if (pCutsceneWidgetResize) {
        VerifyAndPatchDword((char *)pCutsceneWidgetResize + 1, 480, (DWORD)TARGET_H, "CutsceneWidgetResizeH");
        VerifyAndPatchDword((char *)pCutsceneWidgetResize + 6, 640, (DWORD)TARGET_W, "CutsceneWidgetResizeW");
    }

    /* Frog Scale Perspective Desync Fix (Eliminate 50px/100px shift during letter writing) */
    void *pFrogScaleOffsets = FindPattern("D9 45 0C D8 25 ?? ?? ?? ?? 83 EC 08 D9 05");
    if (pFrogScaleOffsets) {
        DWORD old_y = *(DWORD *)((char *)pFrogScaleOffsets + 14);
        DWORD old_x = *(DWORD *)((char *)pFrogScaleOffsets + 37);
        VerifyAndPatchDword((char *)pFrogScaleOffsets + 14, old_y, (DWORD)(uintptr_t)&s_zero, "FrogScaleOffsetY");
        VerifyAndPatchDword((char *)pFrogScaleOffsets + 37, old_x, (DWORD)(uintptr_t)&s_zero, "FrogScaleOffsetX");
    }

    /* Stage 13 Frog Zoom-out-to-0 Destination Centering @ 0x427712 */
    void *pFrogZoomTarget = FindPattern("B9 40 01 00 00 BB C8 00 00 00 89 4D F8 89 5D FC");
    if (pFrogZoomTarget) {
        VerifyAndPatchDword((char *)pFrogZoomTarget + 1, 320, stage_complete_cx, "Stage13FrogZoomTargetX");
        VerifyAndPatchDword((char *)pFrogZoomTarget + 6, 200, (DWORD)(200 + delta_y), "Stage13FrogZoomTargetY");
    }

    /* 9r. Temple Cleared / Congratulations Dialog Centering Hook (Dead Center: Middle-Center) */
    int delta_x = (TARGET_W - 640) / 2;

    /* CONTINUE Button Position X & Y @ 0x4189D5 & 0x4189DD */
    void *pContinueBtn = FindPattern("57 68 9A 01 00 00 FF 52 34");
    if (pContinueBtn) {
        VerifyAndPatchDword((char *)pContinueBtn - 6, 450, (DWORD)(450 + delta_y), "TempleContinueBtnY");
        VerifyAndPatchDword((char *)pContinueBtn + 2, 410, (DWORD)(410 + delta_x), "TempleContinueBtnX");
    }

    /* Hook "CONGRATULATIONS!" title call @ 0x426F3D so it centers on screen without doubling delta_x */
    void *pCongratsTitle = FindPattern("6A 78 56 89 5D FC FF 92 80 00 00 00");
    if (pCongratsTitle) {
        unsigned char *call_site = (unsigned char *)pCongratsTitle + 6;
        DWORD old_prot;
        VirtualProtect(call_site, 6, PAGE_EXECUTE_READWRITE, &old_prot);
        call_site[0] = 0xE8; /* relative call */
        *(int *)(call_site + 1) = (int)((uintptr_t)hk_DrawCongratsTitle - ((uintptr_t)call_site + 5));
        call_site[5] = 0x90; /* NOP */
        VirtualProtect(call_site, 6, old_prot, &old_prot);
        LogF("[ZRES] Hooked Congrats Title Draw @ %p -> %p", call_site, hk_DrawCongratsTitle);
    }

    /* Hook CutsceneMgr::Draw Congratulations Case -> hk_DrawCongratulations */
    void *pCutsceneSwitch = FindPattern("55 8B EC 8B 41 60 48 83 F8 05 77");
    if (pCutsceneSwitch) {
        void *pCongratsCaseJmp = (char *)pCutsceneSwitch + 0x20;
        if (*(unsigned char *)pCongratsCaseJmp == 0xE9) {
            int rel = *(int *)((char *)pCongratsCaseJmp + 1);
            g_orig_DrawCongratulations = (void (THISCALL *)(void *, void *))((char *)pCongratsCaseJmp + 5 + rel);
            DWORD old_prot;
            VirtualProtect(pCongratsCaseJmp, 5, PAGE_EXECUTE_READWRITE, &old_prot);
            *(unsigned char *)pCongratsCaseJmp = 0xE9;
            *(int *)((char *)pCongratsCaseJmp + 1) = (int)((uintptr_t)hk_DrawCongratulations - ((uintptr_t)pCongratsCaseJmp + 5));
            VirtualProtect(pCongratsCaseJmp, 5, old_prot, &old_prot);
            LogF("[ZRES] Hooked Congrats Case JMP @ %p -> %p (orig=%p)", pCongratsCaseJmp, hk_DrawCongratulations, g_orig_DrawCongratulations);
        }
    }
    /* 9s. Credits Screen Layout (Whatever Button, Credits Text Center X, Scroll Start Y, HappyEnding Fit) */
    void *pWhateverBtn = FindPattern("BB E0 01 00 00 2B D8 53 68 FE 01 00 00");
    if (pWhateverBtn) {
        VerifyAndPatchDword((char *)pWhateverBtn + 1, 480, (DWORD)TARGET_H, "CreditsWhateverBtnH");
        VerifyAndPatchDword((char *)pWhateverBtn + 9, 510, (DWORD)(TARGET_W - 130), "CreditsWhateverBtnX");
    }

    void *pCreditsTextCX = FindPattern("FF 50 1C 8B 4D 08 99 2B C2 D1 F8 BA 40 01 00 00");
    if (pCreditsTextCX) {
        VerifyAndPatchDword((char *)pCreditsTextCX + 12, 320, (DWORD)(TARGET_W / 2), "CreditsTextCenterX");
    }

    void *pCreditsScrollStartY = FindPattern("C6 45 FC 01 C7 46 5C E0 01 00 00");
    if (pCreditsScrollStartY) {
        VerifyAndPatchDword((char *)pCreditsScrollStartY + 7, 480, (DWORD)(TARGET_H + 20), "CreditsScrollStartY");
    }

    void *pCreditsScrollResetY = FindPattern("83 FF 9C 7D 0A 8B 4D ?? C7 41 5C 08 02 00 00");
    if (pCreditsScrollResetY) {
        VerifyAndPatchDword((char *)pCreditsScrollResetY + 11, 520, (DWORD)(TARGET_H + 20), "CreditsScrollResetY");
    }

    void *pHappyEndingCall = FindPattern("8B 47 74 33 DB 3B C3 74 0C 53 53 50 8B CE E8");
    if (pHappyEndingCall) {
        char *call_site = (char *)pHappyEndingCall + 14;
        DWORD old_prot;
        VirtualProtect(call_site, 5, PAGE_EXECUTE_READWRITE, &old_prot);
        *(int *)(call_site + 1) = (int)((uintptr_t)hk_DrawHappyEnding - ((uintptr_t)call_site + 5));
        VirtualProtect(call_site, 5, old_prot, &old_prot);
        LogF("[ZRES] Hooked CreditsScreen HappyEnding DrawImage @ %p -> %p", call_site, hk_DrawHappyEnding);
    }

    /* 9t. Space Level (13-1) & In-Game Stone Menubar HUD Framing */
    s_star_max_y = (float)TARGET_H;

    /* Star Kill Threshold Y in Background::Update @ 0x41220a (Prevents stars from being deleted at Y=480) */
    void *pStarKillThreshold = FindPattern("8B 38 3B 7E 60 74 49 D9 47 10 D8 47 0C D9 57 0C D8 1D ?? ?? ?? ?? DF E0");
    if (pStarKillThreshold) {
        DWORD old_ptr = *(DWORD *)((char *)pStarKillThreshold + 18);
        VerifyAndPatchDword((char *)pStarKillThreshold + 18, old_ptr, (DWORD)&s_star_max_y, "StarKillMaxYPtr");
    }

    /* Board::Draw Background ClipRect @ 0x40ccbe (Prevents starfield from being clipped to 640x480) */
    void *pBoardBgClip = FindPattern("68 E0 01 00 00 68 80 02 00 00 8D 8D 00 FF FF FF C6 45 FC 02");
    if (pBoardBgClip) {
        VerifyAndPatchDword((char *)pBoardBgClip + 1, 480, (DWORD)TARGET_H, "BoardBgClipH");
        VerifyAndPatchDword((char *)pBoardBgClip + 6, 640, (DWORD)TARGET_W, "BoardBgClipW");
    }

    /* Background Pre-render ClipRect @ 0x4125e1 */
    void *pBgInitClip = FindPattern("68 E0 01 00 00 68 80 02 00 00 8D 4D A8 89 5D FC");
    if (pBgInitClip) {
        VerifyAndPatchDword((char *)pBgInitClip + 1, 480, (DWORD)TARGET_H, "BgInitClipH");
        VerifyAndPatchDword((char *)pBgInitClip + 6, 640, (DWORD)TARGET_W, "BgInitClipW");
    }

    void *pStarSpawnX = FindPattern("B9 80 02 00 00 F7 F9 89 55 08");
    if (pStarSpawnX) {
        VerifyAndPatchDword((char *)pStarSpawnX + 1, 640, (DWORD)TARGET_W, "StarSpawnX");
    }

    void *pStarSpawnY1 = FindPattern("E8 ?? ?? ?? ?? 99 B9 E0 01 00 00 F7 F9 8B CE 52 E8 EB FE FF FF");
    if (pStarSpawnY1) {
        VerifyAndPatchDword((char *)pStarSpawnY1 + 7, 480, (DWORD)TARGET_H, "StarSpawnY1");
    }

    void *pStarSpawnY2 = FindPattern("BF E8 03 00 00 E8 ?? ?? ?? ?? 99 B9 E0 01 00 00 F7 F9 8B CE 52 E8");
    if (pStarSpawnY2) {
        VerifyAndPatchDword((char *)pStarSpawnY2 + 12, 480, (DWORD)TARGET_H, "StarSpawnY2");
    }

    void *pStarWrapX = FindPattern("81 F9 76 02 00 00 7C 10 8B 56 0C 03 D0 81 FA 80 02 00 00");
    if (pStarWrapX) {
        VerifyAndPatchDword((char *)pStarWrapX + 2, 630, (DWORD)(TARGET_W - 10), "StarWrapX_Threshold");
        VerifyAndPatchDword((char *)pStarWrapX + 15, 640, (DWORD)TARGET_W, "StarWrapX_Max");
    }

    void *pStarWrapY = FindPattern("81 F9 D6 01 00 00 7C 10 8B 56 10 03 D0 81 FA E0 01 00 00");
    if (pStarWrapY) {
        VerifyAndPatchDword((char *)pStarWrapY + 2, 470, (DWORD)(TARGET_H - 10), "StarWrapY_Threshold");
        VerifyAndPatchDword((char *)pStarWrapY + 15, 480, (DWORD)TARGET_H, "StarWrapY_Max");
    }

    void *pDrawStarsFill = FindPattern("8D 55 F0 52 8B CE E8 ?? ?? ?? ?? 68 E0 01 00 00 68 80 02 00 00");
    if (pDrawStarsFill) {
        VerifyAndPatchDword((char *)pDrawStarsFill + 12, 480, (DWORD)TARGET_H, "DrawStarsFillRectH");
        VerifyAndPatchDword((char *)pDrawStarsFill + 17, 640, (DWORD)TARGET_W, "DrawStarsFillRectW");
    }

    void *pBoardDrawFill = FindPattern("8B CB E8 ?? ?? ?? ?? 68 E0 01 00 00 68 80 02 00 00");
    if (pBoardDrawFill) {
        VerifyAndPatchDword((char *)pBoardDrawFill + 8, 480, (DWORD)TARGET_H, "BoardDrawFillRectH");
        VerifyAndPatchDword((char *)pBoardDrawFill + 13, 640, (DWORD)TARGET_W, "BoardDrawFillRectW");
    }

    void *pMenubarMargins = FindPattern("8B 15 C0 7E 54 00 6A 64 6A 32 68 96 00 00 00 68 C8 00 00 00");
    if (pMenubarMargins) {
        VerifyAndPatchByte((char *)pMenubarMargins + 7, 100, (BYTE)(100.0f * scale + 0.5f), "MenubarMarginBottom");
        VerifyAndPatchByte((char *)pMenubarMargins + 9, 50, (BYTE)(50.0f * scale + 0.5f), "MenubarMarginTop");
        VerifyAndPatchDword((char *)pMenubarMargins + 11, 150, (DWORD)(150.0f * scale + 0.5f), "MenubarMarginRight");
        VerifyAndPatchDword((char *)pMenubarMargins + 16, 200, (DWORD)(200.0f * scale + 0.5f), "MenubarMarginLeft");
    }

    /* Synchronized Menubar HUD Element Repositioning (TARGET_W >= 480) */
    int hud_w = TARGET_W < 480 ? 480 : TARGET_W;
    int lives_x = 28;
    int stage_cx = 200;
    int score_rx = 360;
    int zuma_x = 409;
    int menu_x = 540;

    if (TARGET_W == 640 && TARGET_H == 480) {
        /* Exact vanilla 640x480 HUD element coordinates */
        lives_x  = 28;
        stage_cx = 200;
        score_rx = 360;
        zuma_x   = 409;
        menu_x   = 540;
    } else if (hud_w >= 540) {
        int extra_w = hud_w - 800;
        int ba, bb, bc, bd;
        if (extra_w >= 0) {
            ba = (int)((float)extra_w * 0.15f + 0.5f);
            bb = (int)((float)extra_w * 0.35f + 0.5f);
            bc = (int)((float)extra_w * 0.35f + 0.5f);
            bd = extra_w - ba - bb - bc;
            ba += 30; bb += 77; bc += 57; bd += 45;
        } else {
            int s = -extra_w;
            int sha = (int)((float)s * (30.0f / 209.0f) + 0.5f);
            int shb = (int)((float)s * (77.0f / 209.0f) + 0.5f);
            int shc = (int)((float)s * (57.0f / 209.0f) + 0.5f);
            int shd = s - sha - shb - shc;
            ba = (30 - sha > 0) ? (30 - sha) : 0;
            bb = (77 - shb > 0) ? (77 - shb) : 0;
            bc = (57 - shc > 0) ? (57 - shc) : 0;
            bd = (45 - shd > 0) ? (45 - shd) : 0;
        }

        int c1_w = 134;
        int c2_x = c1_w + ba;
        int c2_w = 109;
        int c3_x = c2_x + c2_w + bb;
        int c3_w = 113;
        int c4_x = c3_x + c3_w + bc;
        int c4_w = 113;
        int c5_x = c4_x + c4_w + bd;

        lives_x = 28;
        /* Red bar visual oval center is at c2_x + 66 (vanilla: 134+66=200, 800x600: 164+66=230) */
        stage_cx = c2_x + 66;
        score_rx = c3_x + 90;
        zuma_x = c4_x + 19;
        menu_x = c5_x + 22;
    } else {
        /* 480 <= hud_w < 540: Minimal bridge with smoothstep blend */
        float t = (float)(hud_w - 480) / 60.0f;
        int stage_cx_480 = 153, stage_cx_540 = 200;
        int score_rx_480 = 278, score_rx_540 = 333;
        int zuma_x_480   = 312, zuma_x_540   = 375;
        int menu_x_480   = 381, menu_x_540   = 491;

        lives_x = (hud_w <= 480) ? 26 : 28;
        stage_cx = (int)(stage_cx_480 + t * (stage_cx_540 - stage_cx_480) + 0.5f);
        score_rx = (int)(score_rx_480 + t * (score_rx_540 - score_rx_480) + 0.5f);
        zuma_x   = (int)(zuma_x_480   + t * (zuma_x_540   - zuma_x_480)   + 0.5f);
        menu_x   = (int)(menu_x_480   + t * (menu_x_540   - menu_x_480)   + 0.5f);
    }

    void *pInGameMenuBtnX = FindPattern("6A 03 68 1C 02 00 00");
    if (pInGameMenuBtnX) {
        VerifyAndPatchDword((char *)pInGameMenuBtnX + 3, 540, (DWORD)menu_x, "InGameMenuBtnX");
    }

    void *pInGameZumaBarX = FindPattern("6A 02 68 99 01 00 00 33 DB");
    if (pInGameZumaBarX) {
        VerifyAndPatchDword((char *)pInGameZumaBarX + 3, 409, (DWORD)zuma_x, "InGameZumaBarDrawX");
    }

    void *pInGameZumaBarResizeX = FindPattern("BF C2 01 00 00 2B F8 57 68 9A 01 00 00");
    if (pInGameZumaBarResizeX) {
        VerifyAndPatchDword((char *)pInGameZumaBarResizeX + 9, 410, (DWORD)(zuma_x + 1), "InGameZumaBarResizeX");
    }

    void *pInGameStageTextCX = FindPattern("B9 C8 00 00 00 2B C8 51");
    if (pInGameStageTextCX) {
        VerifyAndPatchDword((char *)pInGameStageTextCX + 1, 200, (DWORD)stage_cx, "InGameStageTextCX");
    }

    void *pInGameScoreTextRX = FindPattern("B9 68 01 00 00 2B C8 51");
    if (pInGameScoreTextRX) {
        VerifyAndPatchDword((char *)pInGameScoreTextRX + 1, 360, (DWORD)score_rx, "InGameScoreTextRX");
    }

    void *pInGameLivesStartX = FindPattern("C7 45 EC 1C 00 00 00");
    if (pInGameLivesStartX) {
        VerifyAndPatchDword((char *)pInGameLivesStartX + 3, 28, (DWORD)lives_x, "InGameLivesStartX");
    }

    /* Adventure Mode Level Banner Position Clamp (LEVEL %d-%d) @ 0x4177f8 */
    void *pAdvLevelBanner = FindPattern("81 FA 58 02 00 00 89 75 0C 7E 0A B8 58 02 00 00 2B C1 89 45 0C 8D 04 3B 3D B8 01 00 00 7E 07 BF B8 01 00 00");
    if (pAdvLevelBanner) {
        VerifyAndPatchDword((char *)pAdvLevelBanner + 2,  600, (DWORD)(TARGET_W - 40), "AdvLevelBannerMaxX_1");
        VerifyAndPatchDword((char *)pAdvLevelBanner + 12, 600, (DWORD)(TARGET_W - 40), "AdvLevelBannerMaxX_2");
        VerifyAndPatchDword((char *)pAdvLevelBanner + 25, 440, (DWORD)(TARGET_H - 40), "AdvLevelBannerMaxY_1");
        VerifyAndPatchDword((char *)pAdvLevelBanner + 32, 440, (DWORD)(TARGET_H - 40), "AdvLevelBannerMaxY_2");
    }

    /* 9u. In-Game Lives Banner ("%d LIVES LEFT", "LAST LIFE!", "GAME OVER") Centering */
    void *pLivesBanner = FindPattern("68 F0 00 00 00 8D 4D B0 51 8B CF FF 50 1C 99 2B C2 D1 F8 BA 40 01 00 00 2B D0 52");
    if (pLivesBanner) {
        VerifyAndPatchDword((char *)pLivesBanner + 1,  240, (DWORD)(TARGET_H / 2), "LivesBannerCenterY");
        VerifyAndPatchDword((char *)pLivesBanner + 20, 320, (DWORD)(TARGET_W / 2), "LivesBannerCenterX");
    }

    /* 9v. Cached Thumbnail Generation Aspect-Fill Blit Hook */
    void *pCachedThumbScale = FindPattern("8D 85 00 FF FF FF 50 8D 8D A8 FE FF FF 89 5D 98 89 5D 9C 89 5D 88 89 5D 8C E8");
    if (pCachedThumbScale) {
        VerifyAndPatchCall((char *)pCachedThumbScale + 25, (void *)g_pfnDrawImageStretch, (void *)hk_cached_thumbnail_scale, "CachedThumbnailScale");
    }

    LogF("[ZRES] Applied all UI layout patches dynamically for %dx%d (scale=%.2f)", TARGET_W, TARGET_H, scale);
    HookZTKIAT();
}



/* Exports */
__declspec(dllexport) int __cdecl ZRH_IsActive(void) { return 1; }
__declspec(dllexport) int __cdecl ZRC_IsActive(void) { return 1; }
__declspec(dllexport) int __cdecl ZCR_IsActive(void) { return 1; }

__declspec(dllexport) void __cdecl ZRH_ScreenToGameCoords(int sx, int sy, int *gx, int *gy)
{
    int ox = ZRES_Api.off[0];
    int oy = ZRES_Api.off[1];
    if (gx) *gx = sx - ox;
    if (gy) *gy = sy - oy;
}

__declspec(dllexport) void __cdecl ZRH_GameToScreenCoords(int gx, int gy, int *sx, int *sy)
{
    int ox = ZRES_Api.off[0];
    int oy = ZRES_Api.off[1];
    if (sx) *sx = gx + ox;
    if (sy) *sy = gy + oy;
}

__declspec(dllexport) int __cdecl ZRH_IsFullscreen(void)
{
    HMODULE hDisp = GetModuleHandleA("zumadisplay.dll");
    typedef int (__cdecl *fn_ISFS)(void);
    fn_ISFS pfn = hDisp ? (fn_ISFS)GetProcAddress(hDisp, "ZDISP_IsFullscreen") : NULL;
    return pfn ? pfn() : 0;
}

__declspec(dllexport) void __cdecl ZRH_GetDimensions(int *w, int *h)
{
    if (w) *w = TARGET_W;
    if (h) *h = TARGET_H;
}

__declspec(dllexport) void __cdecl ZRH_ApplyPatches(void)
{
    InitResolutionState();
    ApplyUIPatches();
}

/* ========================================================
   File Redirection Hook: images/ -> imagescustomres/
   ======================================================== */
typedef HANDLE (WINAPI *fCreateFileA)(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
);
static fCreateFileA g_real_CreateFileA = NULL;

static HANDLE WINAPI hk_CreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    if (lpFileName && g_real_CreateFileA) {
        char redirected[MAX_PATH];
        const char *p = NULL;
        if (strnicmp(lpFileName, "images\\", 7) == 0 || strnicmp(lpFileName, "images/", 7) == 0) {
            snprintf(redirected, sizeof(redirected), "imagescustomres\\%s", lpFileName + 7);
            p = redirected;
        } else if ((p = strstr(lpFileName, "\\images\\")) != NULL) {
            int prefix_len = (int)(p - lpFileName);
            snprintf(redirected, sizeof(redirected), "%.*s\\imagescustomres\\%s", prefix_len, lpFileName, p + 8);
            p = redirected;
        } else if ((p = strstr(lpFileName, "/images/")) != NULL) {
            int prefix_len = (int)(p - lpFileName);
            snprintf(redirected, sizeof(redirected), "%.*s/imagescustomres/%s", prefix_len, lpFileName, p + 8);
            p = redirected;
        }
        if (p) {
            DWORD attr = GetFileAttributesA(p);
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                return g_real_CreateFileA(p, dwDesiredAccess, dwShareMode,
                                         lpSecurityAttributes, dwCreationDisposition,
                                         dwFlagsAndAttributes, hTemplateFile);
            }
        }
    }
    return g_real_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
                             lpSecurityAttributes, dwCreationDisposition,
                             dwFlagsAndAttributes, hTemplateFile);
}

static void InstallFileHook(void)
{
    char *base = (char *)GetModuleHandleA(NULL);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    DWORD impRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!impRVA) return;

    FARPROC pTarget = GetProcAddress(GetModuleHandleA("kernel32.dll"), "CreateFileA");
    if (!pTarget) return;

    IMAGE_IMPORT_DESCRIPTOR *imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + impRVA);
    for (; imp->Name; imp++) {
        if (stricmp(base + imp->Name, "kernel32.dll") == 0) {
            IMAGE_THUNK_DATA *iat = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
            IMAGE_THUNK_DATA *oft = imp->OriginalFirstThunk ? (IMAGE_THUNK_DATA *)(base + imp->OriginalFirstThunk) : NULL;

            for (int idx = 0; iat[idx].u1.Function; idx++) {
                int match = 0;
                if ((FARPROC)iat[idx].u1.Function == pTarget) {
                    match = 1;
                } else if (oft && !IMAGE_SNAP_BY_ORDINAL(oft[idx].u1.Ordinal)) {
                    IMAGE_IMPORT_BY_NAME *ibn = (IMAGE_IMPORT_BY_NAME *)(base + oft[idx].u1.AddressOfData);
                    if (strcmp((char *)ibn->Name, "CreateFileA") == 0) {
                        match = 1;
                    }
                }

                if (match) {
                    DWORD old;
                    VirtualProtect(&iat[idx].u1.Function, sizeof(void *), PAGE_EXECUTE_READWRITE, &old);
                    g_real_CreateFileA = (fCreateFileA)iat[idx].u1.Function;
                    iat[idx].u1.Function = (DWORD)hk_CreateFileA;
                    VirtualProtect(&iat[idx].u1.Function, sizeof(void *), old, &old);
                    LogF("[ZRES] Hooked CreateFileA in IAT: %p (orig=%p)", &iat[idx].u1.Function, g_real_CreateFileA);
                    return;
                }
            }
        }
    }
    LogF("[ZRES] Warning: CreateFileA not found in IAT");
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)h; (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        if (!LoadTable()) return TRUE;
        LogF("zumareshandler.dll loaded (Master Resolution Engine active: %dx%d)", TARGET_W, TARGET_H);
        InitResolutionState();
        if (TARGET_W != 640 || TARGET_H != 480) {
            InstallFileHook();
        }
        ApplyUIPatches();

        ZRES_Api.hk_initdx     = (void *)hk_initdx;
        ZRES_Api.hk_ddinit     = (void *)hk_ddinit;
        ZRES_Api.hk_rootgfx    = (void *)hk_rootgfx;
        ZRES_Api.hk_drawscreen = (void *)hk_drawscreen;
        ZRES_Api.dim[0] = LW; ZRES_Api.dim[1] = LH;
        ZRES_Api.off[0] = 0;  ZRES_Api.off[1] = 0;
        SetCells(LW, LH);
        P->slot = &ZRES_Api;
    } else if (reason == DLL_PROCESS_DETACH) {
        if (P) P->slot = 0;
    }
    return TRUE;
}
