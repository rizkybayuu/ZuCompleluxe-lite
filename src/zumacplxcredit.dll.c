#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

/*
 * zumacplxcredit.dll - Exclusive Developer Credits & Interactive Link Sidecar for Zuma Deluxe v1.0.0.1
 *
 * Implements:
 * - Extended Credits table (42 entries total) terminating with:
 *     38: "ZuCompleluxe Project" (scaled Native Alien font, 66.67% scale)
 *     39: "Developer" (FONT_HEADER / NativeAlienExtended18)
 *     40: "Rizky Bayuu \"Lamp?x\"" (FONT_NAME / NativeAlienExtended16)
 *     41: Interactive "GitHub" dialog button (IMAGE_DIALOG_BUTTON, 120x48)
 * - Interactive Button: normal (cel 0), hover with IDC_HAND cursor (cel 1), pressed (cel 2)
 * - Click handler: opens https://github.com/rizkybayuu/ZuCompleluxe via ShellExecuteA
 * - Dynamic symbol resolution via .zdata configuration (zero hardcoded addresses)
 */

#define EXPORT __declspec(dllexport)

/* Logging to zumatest.log if present in current working directory */
static void logf_(const char *fmt, ...)
{
    if (GetFileAttributesA("zumatest.log") == INVALID_FILE_ATTRIBUTES) return;
    FILE *f = fopen("zumatest.log", "a");
    if (!f) return;
    fprintf(f, "[%8lu][ZCC] ", (unsigned long)GetTickCount());
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f);
    fflush(f);
    fclose(f);
}

/* VEH crash catcher */
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
          mbi.AllocationBase == (void *)self ? "zumacplxcredit.dll" : "Zuma.exe",
          (unsigned long)((uint8_t *)addr - (uint8_t *)mbi.AllocationBase));
    logf_("CRASH regs eip=%08lX esp=%08lX ebp=%08lX eax=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX",
          ep->ContextRecord->Eip, ep->ContextRecord->Esp, ep->ContextRecord->Ebp, ep->ContextRecord->Eax,
          ep->ContextRecord->Ecx, ep->ContextRecord->Edx, ep->ContextRecord->Esi, ep->ContextRecord->Edi);
    return EXCEPTION_CONTINUE_SEARCH;
}

/* Engine Configuration structure matching .zdata @ offset 0x400 */
#define ZEP_MAGIC 0x4350455A /* "ZEPC" */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t struct_size;
    uint32_t rva_curve_ctor;
    uint32_t rva_curvedesc_ctor;
    uint32_t rva_str_assign;
    uint32_t rva_curve_init;
    uint32_t rva_op_new;
    uint32_t rva_add_hole;
    uint32_t rva_set_percent;
    uint32_t rva_set_something;
    uint32_t sizeof_curve;
    uint32_t max_holes;
    uint32_t rva_draw_image_rect;    /* Graphics::DrawImage(rect) */
    uint32_t rva_img_dialog_button;  /* Pointer to IMAGE_DIALOG_BUTTON */
    uint32_t rva_font_title;         /* Pointer to FONT_TITLE */
    uint32_t rva_font_header;        /* Pointer to FONT_HEADER */
    uint32_t rva_font_name;          /* Pointer to FONT_NAME */
    uint32_t rva_credits_count;      /* Pointer to credits line count */
    uint32_t rva_credits_table_orig; /* Original credits table */
    uint32_t rva_zuma_app;           /* ZumaApp instance pointer */
    uint32_t rva_font_dialog;        /* Pointer to FONT_DIALOG */
    uint32_t rva_graphics_set_color; /* Graphics::SetColor */
} ZccEngineConfig;
#pragma pack(pop)

typedef void (__thiscall *DrawImageRectFn)(void *g, void *img, int x, int y, const void *srcRect);
typedef void (__thiscall *DrawImageFn)(void *g, void *img, const void *destRect, const void *srcRect);
typedef HINSTANCE (WINAPI *ShellExecuteAFn)(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT);

static DrawImageRectFn g_fn_draw_image_rect  = NULL;
static DrawImageFn     g_fn_draw_image       = NULL;
static ShellExecuteAFn g_fn_shellexecute     = NULL;

static void           **g_p_img_dialog_button = NULL;
static void           **g_p_font_title        = NULL;
static void           **g_p_font_header       = NULL;
static void           **g_p_font_name         = NULL;
static void           **g_p_font_dialog       = NULL;
static uint32_t        *g_p_credits_count     = NULL;
static void            *g_p_credits_table_orig= NULL;
static void           **g_p_zuma_app          = NULL;

static int              g_config_valid        = 0;
static int              g_init_attempted      = 0;

static int init_config_(void)
{
    if (g_init_attempted) return g_config_valid;
    g_init_attempted = 1;

    HMODULE base = GetModuleHandleA(NULL);
    if (!base) {
        logf_("Config error: GetModuleHandleA(NULL) failed");
        return 0;
    }
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((char *)base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    const ZccEngineConfig *cfg = NULL;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (memcmp(sec[i].Name, ".zdata", 6) == 0) {
            char *sec_data = (char *)base + sec[i].VirtualAddress;
            DWORD sec_size = sec[i].Misc.VirtualSize;
            for (DWORD off = 0; off + sizeof(ZccEngineConfig) <= sec_size; off += 4) {
                const ZccEngineConfig *cand = (const ZccEngineConfig *)(sec_data + off);
                if (cand->magic == ZEP_MAGIC) {
                    cfg = cand;
                    break;
                }
            }
            break;
        }
    }

    if (!cfg) {
        logf_("Config error: engine config magic not found in .zdata");
        return 0;
    }

    uintptr_t b = (uintptr_t)base;
    if (cfg->rva_draw_image_rect) {
        g_fn_draw_image_rect = (DrawImageRectFn)(b + cfg->rva_draw_image_rect);
        uint8_t *p = (uint8_t *)g_fn_draw_image_rect + 0xE0;
        if (p[0] == 0x55 && p[1] == 0x8B && p[2] == 0xEC) {
            g_fn_draw_image = (DrawImageFn)p;
            logf_("Resolved Graphics::DrawImage (dest, src) at %p", g_fn_draw_image);
        }
    }
    if (cfg->rva_img_dialog_button)
        g_p_img_dialog_button = (void **)(b + cfg->rva_img_dialog_button);
    if (cfg->rva_font_title)
        g_p_font_title = (void **)(b + cfg->rva_font_title);
    if (cfg->rva_font_header)
        g_p_font_header = (void **)(b + cfg->rva_font_header);
    if (cfg->rva_font_name)
        g_p_font_name = (void **)(b + cfg->rva_font_name);
    if (cfg->rva_font_dialog)
        g_p_font_dialog = (void **)(b + cfg->rva_font_dialog);
    if (cfg->rva_credits_count)
        g_p_credits_count = (uint32_t *)(b + cfg->rva_credits_count);
    if (cfg->rva_credits_table_orig)
        g_p_credits_table_orig = (void *)(b + cfg->rva_credits_table_orig);
    if (cfg->rva_zuma_app)
        g_p_zuma_app = (void **)(b + cfg->rva_zuma_app);

    /* Patch CreditsScreen::Draw early line cutoff check so multi-line / tall custom elements (e.g. GitHub button)
       can smoothly scroll completely out of view without disappearing halfway */
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (memcmp(sec[i].Name, ".text", 5) == 0) {
            uint8_t *text_data = (uint8_t *)base + sec[i].VirtualAddress;
            DWORD text_size = sec[i].Misc.VirtualSize;
            for (DWORD j = 0; j + 18 <= text_size; j++) {
                if (text_data[j] == 0x03 && text_data[j+1] == 0xC7 &&
                    text_data[j+2] == 0x3B && text_data[j+3] == 0xFA &&
                    text_data[j+4] == 0x0F && text_data[j+5] == 0x8D &&
                    text_data[j+10] == 0x3B && text_data[j+11] == 0xC3 &&
                    text_data[j+12] == 0x0F && text_data[j+13] == 0x8E) {
                    DWORD old_prot;
                    if (VirtualProtect(text_data + j + 10, 8, PAGE_EXECUTE_READWRITE, &old_prot)) {
                        memset(text_data + j + 10, 0x90, 8); /* 8x NOP */
                        VirtualProtect(text_data + j + 10, 8, old_prot, &old_prot);
                        logf_("Patched CreditsScreen::Draw early cutoff check at %p", text_data + j + 10);
                    }
                    break;
                }
            }
            break;
        }
    }

    logf_("Config loaded: title_font=%p, btn_img=%p, credits_count_ptr=%p",
          g_p_font_title, g_p_img_dialog_button, g_p_credits_count);
    g_config_valid = 1;
    return 1;
}

/* Credits Table Layout */
typedef struct {
    void *font;        /* +0x00 */
    const char *text;  /* +0x04 */
    int spacing;       /* +0x08 */
} ZccCreditEntry;

#define ZCC_CREDITS_TOTAL 42
static ZccCreditEntry g_extended_credits[ZCC_CREDITS_TOTAL];
static int g_credits_inited = 0;
static void *g_credits_screen = NULL;
static RECT g_btn_rect = { 0, 0, 0, 0 };
static int g_btn_visible = 0;

/* Glyph table for 66.67% scaled NativeAlien font: "ZuCompleluxe Project" */
typedef struct {
    int sx, sy, sw, sh;
    int ox, oy;
    int adv;
} ZccGlyph;

static const ZccGlyph s_zucompleluxe_glyphs[] = {
    /* 0: 'Z' */ {1069, 0, 29, 90, -2, 0, 28},
    /* 1: 'u' */ {1870, 0, 40, 90, -2, 0, 41},
    /* 2: 'c' (C mapped to c) */ {1175, 0, 43, 90, -3, 0, 40},
    /* 3: 'o' */ {1630, 0, 52, 90, -3, 0, 49},
    /* 4: 'm' (M mapped to m) */ {1546, 0, 43, 90, -2, 0, 46},
    /* 5: 'p' */ {1683, 0, 36, 90, -2, 0, 34},
    /* 6: 'l' */ {1514, 0, 31, 90, -2, 0, 30},
    /* 7: 'e' */ {1261, 0, 34, 90, -3, 0, 33},
    /* 8: 'l' */ {1514, 0, 31, 90, -2, 0, 30},
    /* 9: 'u' */ {1870, 0, 40, 90, -2, 0, 41},
    /* 10: 'x' */ {2017, 0, 36, 90, -2, 0, 35},
    /* 11: 'e' */ {1261, 0, 34, 90, -3, 0, 33},
    /* 12: ' ' */ {0, 0, 0, 0, 0, 0, 32},
    /* 13: 'P' */ {671, 0, 35, 90, -1, 0, 34},
    /* 14: 'r' */ {1757, 0, 41, 90, -3, 0, 39},
    /* 15: 'o' */ {1630, 0, 52, 90, -3, 0, 49},
    /* 16: 'j' */ {1432, 0, 40, 90, -3, 0, 38},
    /* 17: 'e' */ {1261, 0, 34, 90, -3, 0, 33},
    /* 18: 'c' */ {1175, 0, 43, 90, -3, 0, 40},
    /* 19: 't' */ {1839, 0, 30, 90, -3, 0, 27}
};
#define ZCC_GLYPHS_COUNT 20

static void *get_native_alien_image_(void)
{
    static void *s_img = NULL;
    if (s_img) return s_img;
    if (!g_p_font_title || !*g_p_font_title) return NULL;

    uint8_t *font = (uint8_t *)(*g_p_font_title);
    void *head = *(void **)(font + 0x34);
    if (head && (uintptr_t)head >= 0x10000 && !IsBadReadPtr(head, 4)) {
        void *node = *(void **)head;
        if (node && (uintptr_t)node >= 0x10000 && node != head && !IsBadReadPtr(node, 0x30)) {
            uint32_t *dwords = (uint32_t *)node;
            for (int i = 2; i < 32; i++) {
                uintptr_t ptr = (uintptr_t)dwords[i];
                if (ptr >= 0x10000 && (ptr & 3) == 0 && !IsBadReadPtr((void *)ptr, 16)) {
                    int w = *(int *)(ptr + 4);
                    int h = *(int *)(ptr + 8);
                    if (w == 2911 && h == 90) {
                        s_img = (void *)ptr;
                        logf_("Located NativeAlien48 Image* at node offset 0x%X = %p (%dx%d)", i * 4, s_img, w, h);
                        return s_img;
                    }
                }
            }
        }
    }

    /* Fallback: scan font structure */
    uint32_t *font_dwords = (uint32_t *)font;
    for (int i = 0; i < 64; i++) {
        uintptr_t ptr = (uintptr_t)font_dwords[i];
        if (ptr >= 0x10000 && (ptr & 3) == 0 && !IsBadReadPtr((void *)ptr, 16)) {
            int w = *(int *)(ptr + 4);
            int h = *(int *)(ptr + 8);
            if (w == 2911 && h == 90) {
                s_img = (void *)ptr;
                logf_("Located NativeAlien48 Image* at font offset 0x%X = %p (%dx%d)", i * 4, s_img, w, h);
                return s_img;
            }
        }
    }

    logf_("Warning: Could not locate NativeAlien48 Image* (2911x90)");
    return NULL;
}

static void draw_scaled_zucompleluxe_project_(void *creditsScreen, void *g, int lineY)
{
    (void)creditsScreen;
    if (!g || !g_fn_draw_image) return;

    void *img = get_native_alien_image_();
    if (!img) return;

    int screenW = (g_p_zuma_app && *g_p_zuma_app) ? *(int *)((uint8_t *)(*g_p_zuma_app) + 0x5c) : 640;
    int screenH = (g_p_zuma_app && *g_p_zuma_app) ? *(int *)((uint8_t *)(*g_p_zuma_app) + 0x60) : 480;
    if (screenW <= 0) screenW = 640;
    if (screenH <= 0) screenH = 480;

    /* Total scaled advance is 489 px; dynamically centered across full widescreen */
    int curX = (screenW - 489) / 2;
    float scale = 2.0f / 3.0f;

    for (int i = 0; i < ZCC_GLYPHS_COUNT; i++) {
        const ZccGlyph *gl = &s_zucompleluxe_glyphs[i];
        int adv = (int)(gl->adv * scale + 0.5f);

        if (gl->sw > 0 && gl->sh > 0) {
            struct { int x, y, w, h; } srcRect;
            srcRect.x = gl->sx;
            srcRect.y = gl->sy;
            srcRect.w = gl->sw;
            srcRect.h = gl->sh;

            struct { int x, y, w, h; } destRect;
            destRect.x = curX + (int)(gl->ox * scale);
            destRect.y = lineY + (int)(gl->oy * scale);
            destRect.w = (int)(gl->sw * scale + 0.5f);
            destRect.h = (int)(gl->sh * scale + 0.5f);

            g_fn_draw_image(g, img, &destRect, &srcRect);
        }

        curX += adv;
    }
}

static void open_github_url_(void)
{
    HWND hwnd = NULL;
    if (g_p_zuma_app && *g_p_zuma_app) {
        hwnd = *(HWND *)((uint8_t *)(*g_p_zuma_app) + 0x2fc);
    }
    if (!hwnd) hwnd = GetActiveWindow();

    if (!g_fn_shellexecute) {
        HMODULE hShell = LoadLibraryA("shell32.dll");
        if (hShell) {
            g_fn_shellexecute = (ShellExecuteAFn)GetProcAddress(hShell, "ShellExecuteA");
        }
    }
    if (g_fn_shellexecute) {
        logf_("Opening ZuCompleluxe GitHub repository in default browser...");
        g_fn_shellexecute(hwnd, "open", "https://github.com/rizkybayuu/ZuCompleluxe", NULL, NULL, SW_SHOWNORMAL);
        if (hwnd) {
            ShowWindow(hwnd, SW_MINIMIZE);
        }
    } else {
        logf_("Error: Failed to resolve ShellExecuteA from shell32.dll");
    }
}

/* ---------------------------------------------------------------- EXPORTS */

EXPORT void * __cdecl ZCC_GetCreditsTable(void)
{
    if (!init_config_()) return NULL;

    if (!g_credits_inited) {
        if (g_p_credits_table_orig) {
            memcpy(g_extended_credits, g_p_credits_table_orig, 38 * sizeof(ZccCreditEntry));
            /* Add spacing after original credits end (Tristan Monnastes) */
            g_extended_credits[37].spacing = 40;
        }

        /* Entry 38: Title "ZuCompleluxe Project" -> scaled NativeAlien (FONT_HUGE) */
        g_extended_credits[38].font = (g_p_font_title && *g_p_font_title) ? *g_p_font_title : NULL;
        g_extended_credits[38].text = "ZuCompleluxe Project";
        g_extended_credits[38].spacing = 15;

        /* Entry 39: Role "Developer" -> NativeAlienExtended18 (FONT_HEADER / TITLE) */
        g_extended_credits[39].font = (g_p_font_header && *g_p_font_header) ? *g_p_font_header : NULL;
        g_extended_credits[39].text = "Developer";
        g_extended_credits[39].spacing = 0;

        /* Entry 40: Name "Rizky Bayuu \"Lamp?x\"" -> NativeAlienExtended16 (FONT_NAME / BROWNTITLE) */
        g_extended_credits[40].font = (g_p_font_name && *g_p_font_name) ? *g_p_font_name : NULL;
        g_extended_credits[40].text = "Rizky Bayuu \"Lamp?x\"";
        g_extended_credits[40].spacing = 35;

        /* Entry 41: GitHub Button -> FONT_DIALOG (style like "Whatever" button) */
        g_extended_credits[41].font = (g_p_font_dialog && *g_p_font_dialog) ? *g_p_font_dialog : NULL;
        g_extended_credits[41].text = "GitHub";
        g_extended_credits[41].spacing = 60; /* Added spacing so GitHub button completely scrolls off before loop reset */

        g_credits_inited = 1;
        logf_("Extended credits table initialized (total entries: %d)", ZCC_CREDITS_TOTAL);
    }

    if (g_p_credits_count) {
        *g_p_credits_count = ZCC_CREDITS_TOTAL;
    }

    return (void *)(&g_extended_credits[0].text);
}

EXPORT void __cdecl ZCC_PreDrawCreditLine(void *creditsScreen, void *g, void *font, const char *text, int lineY)
{
    (void)font;
    if (!text) return;

    if (strcmp(text, "ZuCompleluxe Project") == 0) {
        draw_scaled_zucompleluxe_project_(creditsScreen, g, lineY);
        /* Suppress Zuma's default unscaled DrawString for this entry */
        g_extended_credits[38].text = "";
    } else {
        /* Restore entry 38 text so it is ready for subsequent frames */
        if (g_extended_credits[38].text[0] == '\0') {
            g_extended_credits[38].text = "ZuCompleluxe Project";
        }
    }

    if (strcmp(text, "GitHub") == 0) {
        g_credits_screen = creditsScreen;

        int screenW = (g_p_zuma_app && *g_p_zuma_app) ? *(int *)((uint8_t *)(*g_p_zuma_app) + 0x5c) : 640;
        int screenH = (g_p_zuma_app && *g_p_zuma_app) ? *(int *)((uint8_t *)(*g_p_zuma_app) + 0x60) : 480;
        if (screenW <= 0) screenW = 640;
        if (screenH <= 0) screenH = 480;

        int btnW = 120;
        int btnH = 48;
        int btnX = (screenW - btnW) / 2;
        int btnY = lineY - 11;

        /* Hitbox in root window client coordinates (CreditsScreen has xo=0, yo=0 fullscreen) */
        g_btn_rect.left = btnX;
        g_btn_rect.top = btnY;
        g_btn_rect.right = btnX + btnW;
        g_btn_rect.bottom = btnY + btnH;
        g_btn_visible = (btnY + btnH > 0 && btnY < screenH);

        int cel = 0; /* 0 = Normal, 1 = Hover, 2 = Pressed */
        if (g_btn_visible) {
            HWND hwnd = NULL;
            if (g_p_zuma_app && *g_p_zuma_app) {
                hwnd = *(HWND *)((uint8_t *)(*g_p_zuma_app) + 0x2fc);
            }
            if (!hwnd) hwnd = GetActiveWindow();

            void *app = (g_p_zuma_app && *g_p_zuma_app) ? *g_p_zuma_app : NULL;
            void *wm = app ? *(void **)((uint8_t *)app + 0x2dc) : NULL;
            int mx = -1, my = -1;
            if (wm) {
                mx = *(int *)((uint8_t *)wm + 0x54);
                my = *(int *)((uint8_t *)wm + 0x58);
            } else if (hwnd) {
                POINT pt;
                if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt)) {
                    mx = pt.x;
                    my = pt.y;
                    HMODULE hMod = GetModuleHandleA("zumadisplay.dll");
                    if (!hMod) hMod = GetModuleHandleA("zumareshandler.dll");
                    if (hMod) {
                        typedef void (__cdecl *fn_S2G)(int, int, int *, int *);
                        fn_S2G pfn = (fn_S2G)GetProcAddress(hMod, "ZRH_ScreenToGameCoords");
                        if (pfn) pfn(mx, my, &mx, &my);
                    }
                }
            }

            static int s_was_pressed = 0;
            int is_pressed = ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);

            if (mx >= g_btn_rect.left && mx < g_btn_rect.right &&
                my >= g_btn_rect.top && my < g_btn_rect.bottom) {
                SetCursor(LoadCursorA(NULL, IDC_HAND));
                if (is_pressed) {
                    cel = 2;
                    s_was_pressed = 1;
                } else {
                    cel = 1;
                    if (s_was_pressed) {
                        s_was_pressed = 0;
                        open_github_url_();
                    }
                }
            } else {
                if (!is_pressed) s_was_pressed = 0;
            }
        }

        if (g_fn_draw_image_rect && g_p_img_dialog_button && *g_p_img_dialog_button) {
            void *img = *g_p_img_dialog_button;
            struct { int x, y, w, h; } srcRect;
            srcRect.x = cel * 120;
            srcRect.y = 0;
            srcRect.w = 120;
            srcRect.h = 48;
            g_fn_draw_image_rect(g, img, btnX, btnY, &srcRect);
        }
    }
}

EXPORT int __cdecl ZCC_OnMouseDown(void *widget, int x, int y)
{
    (void)widget;
    if (g_btn_visible) {
        void *app = (g_p_zuma_app && *g_p_zuma_app) ? *g_p_zuma_app : NULL;
        void *wm = app ? *(void **)((uint8_t *)app + 0x2dc) : NULL;
        int mx = x, my = y;
        if (wm) {
            mx = *(int *)((uint8_t *)wm + 0x54);
            my = *(int *)((uint8_t *)wm + 0x58);
        }

        int hit = (x >= g_btn_rect.left && x < g_btn_rect.right &&
                   y >= g_btn_rect.top && y < g_btn_rect.bottom) ||
                  (mx >= g_btn_rect.left && mx < g_btn_rect.right &&
                   my >= g_btn_rect.top && my < g_btn_rect.bottom);

        if (hit) {
            open_github_url_();
            return 1; /* Handled */
        }
    }
    return 0; /* Not handled */
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID res)
{
    (void)res;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        AddVectoredExceptionHandler(0, veh_);
        init_config_();
        logf_("zumacplxcredit.dll loaded");
    }
    return TRUE;
}
