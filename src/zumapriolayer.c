/* zumapriolayer.c - ZuCompleluxe v0.6.0.4 High-Priority Layer & Earthquake Bake Engine
 *
 * Sprites & Cutouts with priority >= 10 render on top of Frog, Coins, and Bullets.
 * Supports full industry-standard blend modes (Normal, Add, Multiply, Screen, Darken,
 * Lighten, Color Dodge, Color Burn, Overlay, Difference/Negate, Exclusion, Hard Light, Soft Light).
 * During Stage Completion Earthquake (State 5), all visual elements shake and crumble together.
 * Standalone XML reader: Zero hooks on vanilla PopCap XML parser for 100% stability.
 */
#define ZPL_BUILD
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "zumapriolayer.h"

typedef void (__attribute__((thiscall)) *ListCtorFn)(void *self);
typedef void (__attribute__((thiscall)) *ListDtorFn)(void *self);
typedef void (__attribute__((thiscall)) *DrawXmlFn)(void *self, void *g, int prio);
typedef void (__attribute__((thiscall)) *DrawImgFn)(void *g, void *img, int x, int y);   /* ret 0xC */
typedef void (__attribute__((thiscall)) *SetModeFn)(void *g, int mode);                  /* ret 4   */

typedef struct Slot {
    uint32_t pad;      /* B                */
    uint32_t proxy;    /* B+4  = list+0    */
    uint32_t head;     /* B+8  = list+4    */
    uint32_t size;     /* B+12 = list+8    */
} Slot;

typedef struct Set {
    void *holemgr;
    Slot  slot[ZPL_NSLOTS];
} Set;

typedef char zpl_static_assert_slot[(sizeof(Slot) == 16) ? 1 : -1];

#define ZPL_MAX_SETS 8          /* active + staging + margin */
static Set g_sets[ZPL_MAX_SETS];
static int g_evict = 0;
static int g_current_draw_pri = 0;

static ZPL_Sites g_sites = {
    0x00408CC0u, /* list_ctor: std::list::list(ECX)            */
    0x0040ABF0u, /* list_dtor: std::list::~list(ECX)           */
    0x00409BA0u, /* list_erase: list::erase(begin, end)        */
    0x00405AF0u, /* xml_draw: HoleMgr::DrawSpritesAndCutouts    */
    0x0044D470u, /* draw_image: Graphics::DrawImage            */
    0x00447890u  /* set_draw_mode: Graphics::SetDrawMode        */
};

static int s_level_log_count = 0;

static void logf_(const char *fmt, ...)
{
    if (s_level_log_count >= 50) return;

    char logpath[MAX_PATH];
    GetModuleFileNameA(NULL, logpath, MAX_PATH);
    char *slash = strrchr(logpath, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat(logpath, "zumatest.log");

    FILE *f = fopen(logpath, "a");
    if (!f) f = fopen("zumatest.log", "a");
    if (!f) return;

    s_level_log_count++;
    if (s_level_log_count == 50) {
        fprintf(f, "[%8lu][PrioLayer] Level log line limit reached (50). Suppressing further logs for this level.\n", (unsigned long)GetTickCount());
        fclose(f);
        return;
    }

    fprintf(f, "[%8lu][PrioLayer] ", (unsigned long)GetTickCount());
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f);
    fflush(f);
    fclose(f);
}

static void logf_always(const char *fmt, ...)
{
    char logpath[MAX_PATH];
    GetModuleFileNameA(NULL, logpath, MAX_PATH);
    char *slash = strrchr(logpath, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat(logpath, "zumatest.log");

    FILE *f = fopen(logpath, "a");
    if (!f) f = fopen("zumatest.log", "a");
    if (!f) return;

    fprintf(f, "[%8lu][PrioLayer] ", (unsigned long)GetTickCount());
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f);
    fflush(f);
    fclose(f);
}

/* ------------------------------ Standalone levels.xml Reader ------------------------------ */

typedef struct SpriteBlendEntry {
    char level_id[64];
    int  pri;
    int  blend_mode;
} SpriteBlendEntry;

#define MAX_BLEND_ENTRIES 128
static SpriteBlendEntry g_blend_entries[MAX_BLEND_ENTRIES];
static int g_num_blend_entries = 0;
static int g_blend_parsed = 0;

static int parse_blend_name(const char *name)
{
    if (!name || !*name) return ZPL_BLEND_NORMAL;
    char clean[64];
    int ci = 0;
    for (int i = 0; name[i] && ci < (int)sizeof(clean) - 1; i++) {
        char ch = name[i];
        if (ch == '-' || ch == '_' || ch == ' ' || ch == '\t') continue;
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
        clean[ci++] = ch;
    }
    clean[ci] = '\0';

    /* Direct integer ID support (e.g. blend="3" -> Multiply, blend="10" -> Add) */
    if (clean[0] >= '0' && clean[0] <= '9') {
        int id = atoi(clean);
        if (id >= 0 && id <= ZPL_BLEND_MAX) return id;
    }

    /* Category 1: Normal / Standard */
    if (strcmp(clean, "normal") == 0 || strcmp(clean, "none") == 0 || strcmp(clean, "default") == 0 || strcmp(clean, "standard") == 0) return ZPL_BLEND_NORMAL;
    if (strcmp(clean, "pigment") == 0) return ZPL_BLEND_PIGMENT;

    /* Category 2: Darken */
    if (strcmp(clean, "darken") == 0 || strcmp(clean, "dark") == 0) return ZPL_BLEND_DARKEN;
    if (strcmp(clean, "multiply") == 0 || strcmp(clean, "mult") == 0) return ZPL_BLEND_MULTIPLY;
    if (strcmp(clean, "colorburn") == 0 || strcmp(clean, "burn") == 0) return ZPL_BLEND_COLOR_BURN;
    if (strcmp(clean, "linearburn") == 0) return ZPL_BLEND_LINEAR_BURN;
    if (strcmp(clean, "darkercolor") == 0 || strcmp(clean, "darker") == 0) return ZPL_BLEND_DARKER_COLOR;

    /* Category 3: Lighten */
    if (strcmp(clean, "lighten") == 0 || strcmp(clean, "light") == 0) return ZPL_BLEND_LIGHTEN;
    if (strcmp(clean, "screen") == 0 || strcmp(clean, "scr") == 0) return ZPL_BLEND_SCREEN;
    if (strcmp(clean, "colordodge") == 0 || strcmp(clean, "dodge") == 0) return ZPL_BLEND_COLOR_DODGE;
    if (strcmp(clean, "add") == 0 || strcmp(clean, "additive") == 0 || strcmp(clean, "lineardodge") == 0 || strcmp(clean, "plus") == 0) return ZPL_BLEND_ADD;
    if (strcmp(clean, "lightercolor") == 0 || strcmp(clean, "lighter") == 0) return ZPL_BLEND_LIGHTER_COLOR;

    /* Category 4: Contrast */
    if (strcmp(clean, "overlay") == 0) return ZPL_BLEND_OVERLAY;
    if (strcmp(clean, "softlight") == 0) return ZPL_BLEND_SOFT_LIGHT;
    if (strcmp(clean, "hardlight") == 0) return ZPL_BLEND_HARD_LIGHT;
    if (strcmp(clean, "vividlight") == 0) return ZPL_BLEND_VIVID_LIGHT;
    if (strcmp(clean, "linearlight") == 0) return ZPL_BLEND_LINEAR_LIGHT;
    if (strcmp(clean, "pinlight") == 0) return ZPL_BLEND_PIN_LIGHT;
    if (strcmp(clean, "hardmix") == 0) return ZPL_BLEND_HARD_MIX;

    /* Category 5: Comparative */
    if (strcmp(clean, "difference") == 0 || strcmp(clean, "diff") == 0 || strcmp(clean, "invert") == 0 || strcmp(clean, "inversion") == 0) return ZPL_BLEND_DIFFERENCE;
    if (strcmp(clean, "exclusion") == 0 || strcmp(clean, "excl") == 0) return ZPL_BLEND_EXCLUSION;
    if (strcmp(clean, "subtract") == 0 || strcmp(clean, "sub") == 0 || strcmp(clean, "minus") == 0) return ZPL_BLEND_SUBTRACT;
    if (strcmp(clean, "divide") == 0 || strcmp(clean, "div") == 0) return ZPL_BLEND_DIVIDE;

    /* Category 6: Component / HSL */
    if (strcmp(clean, "hue") == 0) return ZPL_BLEND_HUE;
    if (strcmp(clean, "saturation") == 0 || strcmp(clean, "sat") == 0) return ZPL_BLEND_SATURATION;
    if (strcmp(clean, "color") == 0 || strcmp(clean, "colour") == 0) return ZPL_BLEND_COLOR;
    if (strcmp(clean, "luminosity") == 0 || strcmp(clean, "lum") == 0 || strcmp(clean, "luminance") == 0) return ZPL_BLEND_LUMINOSITY;

    /* Category 7: Specialized */
    if (strcmp(clean, "average") == 0 || strcmp(clean, "avg") == 0 || strcmp(clean, "mean") == 0) return ZPL_BLEND_AVERAGE;
    if (strcmp(clean, "negation") == 0 || strcmp(clean, "negate") == 0) return ZPL_BLEND_NEGATION;
    if (strcmp(clean, "reflect") == 0) return ZPL_BLEND_REFLECT;
    if (strcmp(clean, "glow") == 0) return ZPL_BLEND_GLOW;
    if (strcmp(clean, "contrastnegate") == 0 || strcmp(clean, "contrastneg") == 0) return ZPL_BLEND_CONTRAST_NEGATE;

    /* Category 8: Utility / Masking */
    if (strcmp(clean, "erase") == 0 || strcmp(clean, "eraser") == 0 || strcmp(clean, "clear") == 0) return ZPL_BLEND_ERASE;
    return ZPL_BLEND_NORMAL;
}

static void *FindUniquePattern(const char *pattern, const char *name) {
    char *base = (char *)GetModuleHandleA(NULL);
    if (!base) return NULL;
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

    void *found_match = NULL;
    int match_count = 0;

    for (WORD s = 0; s < nt->FileHeader.NumberOfSections; s++) {
        if (!(sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        unsigned char *sec_data = (unsigned char *)(base + sec[s].VirtualAddress);
        DWORD sec_size = sec[s].Misc.VirtualSize;
        if (sec_size == 0) sec_size = sec[s].SizeOfRawData;
        if (sec_size < pat_len) continue;
        for (DWORD i = 0; i <= sec_size - pat_len; i++) {
            int match = 1;
            for (size_t j = 0; j < pat_len; j++) {
                if (pat_mask[j] == 'x' && sec_data[i + j] != pat_bytes[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                match_count++;
                if (match_count == 1) {
                    found_match = (void *)(sec_data + i);
                }
            }
        }
    }

    if (match_count == 1) {
        logf_("FindUniquePattern: '%s' uniquely resolved to %p (exact 1 match)", name ? name : "unknown", found_match);
        return found_match;
    }
    logf_("FindUniquePattern ERROR: '%s' expected exactly 1 match, but found %d matches! Disabling hook.",
          name ? name : "unknown", match_count);
    return NULL;
}

static void **GetAppPtr(void) {
    static void **s_app_ptr = NULL;
    static int s_searched = 0;
    if (s_searched) return s_app_ptr;
    s_searched = 1;
    void *p = FindUniquePattern("A1 ?? ?? ?? ?? 8B 88 C0 04 00 00", "AppPtr");
    if (p) {
        s_app_ptr = *(void ***)((char *)p + 1);
    } else {
        s_app_ptr = (void **)0x0057EF00;
    }
    return s_app_ptr;
}

static int read_msvc_string(const uint8_t *str_obj, char *out, size_t cap)
{
    if (!out || cap == 0) return 0;
    out[0] = 0;
    if (!str_obj || IsBadReadPtr(str_obj, 0x1C)) return 0;
    uint32_t size = *(const uint32_t *)(str_obj + 0x14);
    uint32_t res  = *(const uint32_t *)(str_obj + 0x18);
    if (size == 0 || size >= cap || size > 255 || size > res) return 0;
    const char *s = (res < 0x10) ? (const char *)(str_obj + 4) : *(const char **)(str_obj + 4);
    if (!s || IsBadReadPtr(s, size)) return 0;
    memcpy(out, s, size);
    out[size] = 0;
    return (int)size;
}

static int get_active_level_id(char *out, size_t cap)
{
    if (!out || cap == 0) return 0;
    out[0] = 0;
    void **app_ptr = GetAppPtr();
    if (!app_ptr || IsBadReadPtr(app_ptr, sizeof(void *)) || !*app_ptr) return 0;
    uint8_t *app = (uint8_t *)*app_ptr;
    if (IsBadReadPtr(app + 0x5E4, sizeof(void *))) return 0;
    uint8_t *board = *(uint8_t **)(app + 0x5E4);
    if (!board || IsBadReadPtr(board, 0x100)) return 0;
    const uint8_t *desc = *(const uint8_t **)(board + 0xF8);
    if (!desc || IsBadReadPtr(desc, 0x30)) return 0;
    return read_msvc_string(desc + 0x04, out, cap);
}

static void parse_levels_xml_blends(void)
{
    if (g_blend_parsed) return;
    g_blend_parsed = 1;
    g_num_blend_entries = 0;

    FILE *f = fopen("levels/levels.xml", "rb");
    if (!f) {
        logf_("XML-Blend: could not open levels/levels.xml");
        return;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 4 * 1024 * 1024) { fclose(f); return; }

    char *buf = (char *)malloc(sz + 1);
    if (!buf) { fclose(f); return; }
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);

    char cur_level[64] = {0};
    char *p = buf;

    while (*p) {
        char *tag = strchr(p, '<');
        if (!tag) break;
        p = tag + 1;

        if (_strnicmp(p, "Graphics ", 9) == 0 || _strnicmp(p, "Graphics\t", 9) == 0 ||
            _strnicmp(p, "Graphics\r", 9) == 0 || _strnicmp(p, "Graphics\n", 9) == 0 ||
            _strnicmp(p, "level ", 6) == 0 || _strnicmp(p, "level\t", 6) == 0 ||
            _strnicmp(p, "level\r", 6) == 0 || _strnicmp(p, "level\n", 6) == 0) {
            
            char *tag_end = strchr(p, '>');
            char *id_attr = strstr(p, "id=\"");
            char quote = '"';
            if (!id_attr || (tag_end && id_attr > tag_end)) {
                id_attr = strstr(p, "id='");
                quote = '\'';
            }
            if (id_attr && (!tag_end || id_attr < tag_end)) {
                id_attr += 4;
                char *end_id = strchr(id_attr, quote);
                if (end_id && (!tag_end || end_id <= tag_end)) {
                    size_t len = (size_t)(end_id - id_attr);
                    if (len >= sizeof(cur_level)) len = sizeof(cur_level) - 1;
                    memcpy(cur_level, id_attr, len);
                    cur_level[len] = 0;
                }
            }
        } else if (_strnicmp(p, "Sprite ", 7) == 0 || _strnicmp(p, "Sprite\t", 7) == 0 ||
                   _strnicmp(p, "Cutout ", 7) == 0 || _strnicmp(p, "Cutout\t", 7) == 0) {
            char *tag_end = strchr(p, '>');
            if (!tag_end) break;

            int pri = -1;
            char *pri_attr = strstr(p, "pri=\"");
            if (!pri_attr || pri_attr > tag_end) pri_attr = strstr(p, "pri='");
            if (pri_attr && pri_attr < tag_end) {
                pri = atoi(pri_attr + 5);
            }

            char blend_name[32] = {0};
            char *blend_attr = strstr(p, "blend=\"");
            char bquote = '"';
            if (!blend_attr || blend_attr > tag_end) {
                blend_attr = strstr(p, "blend='");
                bquote = '\'';
            }
            if (blend_attr && blend_attr < tag_end) {
                blend_attr += 7;
                char *end_b = strchr(blend_attr, bquote);
                if (end_b && end_b < tag_end) {
                    size_t blen = (size_t)(end_b - blend_attr);
                    if (blen >= sizeof(blend_name)) blen = sizeof(blend_name) - 1;
                    memcpy(blend_name, blend_attr, blen);
                    blend_name[blen] = 0;
                }
            }

            if (pri >= 0 && g_num_blend_entries < MAX_BLEND_ENTRIES) {
                SpriteBlendEntry *e = &g_blend_entries[g_num_blend_entries++];
                snprintf(e->level_id, sizeof(e->level_id), "%s", cur_level);
                e->pri = pri;
                e->blend_mode = parse_blend_name(blend_name);
                logf_("XML-Blend: level='%s' pri=%d blend='%s' -> mode=%d",
                      e->level_id, e->pri, blend_name[0] ? blend_name : "(default)", e->blend_mode);
            }
        }
    }

    free(buf);
}

static int get_blend_mode_for_pri(int pri)
{
    char active_id[64] = {0};
    get_active_level_id(active_id, sizeof(active_id));

    parse_levels_xml_blends();

    /* Match active level and pri */
    if (active_id[0]) {
        for (int i = 0; i < g_num_blend_entries; i++) {
            if (g_blend_entries[i].pri == pri && _stricmp(g_blend_entries[i].level_id, active_id) == 0) {
                return g_blend_entries[i].blend_mode;
            }
        }
    }

    /* Fallback: match by pri if no specific level matched or level unknown */
    for (int i = 0; i < g_num_blend_entries; i++) {
        if (g_blend_entries[i].pri == pri && (!g_blend_entries[i].level_id[0] || !active_id[0])) {
            return g_blend_entries[i].blend_mode;
        }
    }

    return ZPL_BLEND_NORMAL;
}

/* ------------------------------ Set & Lifecycle ------------------------------ */

static void set_create(Set *s, void *hm)
{
    int i;
    memset(s, 0, sizeof(*s));
    s->holemgr = hm;
    for (i = 0; i < ZPL_NSLOTS; i++) {
        ((ListCtorFn)g_sites.list_ctor)(&s->slot[i].proxy);   /* sentinel node via engine operator new */
    }
    logf_("SetCreate: hm=%p", hm);
}

static void set_destroy(Set *s)
{
    int i;
    if (!s || !s->holemgr) return;
    for (i = 0; i < ZPL_NSLOTS; i++) {
        ((ListDtorFn)g_sites.list_dtor)(&s->slot[i].proxy);   /* nodes + sentinel freed via engine */
    }
    s->holemgr = NULL;
}

static void set_reset(Set *s)
{
    int i;
    if (!s || !s->holemgr) return;
    for (i = 0; i < ZPL_NSLOTS; i++) {
        ((ListDtorFn)g_sites.list_dtor)(&s->slot[i].proxy);
        ((ListCtorFn)g_sites.list_ctor)(&s->slot[i].proxy);
    }
}

static Set *find_set(void *hm, int create)
{
    int i;
    if (!hm) return NULL;
    for (i = 0; i < ZPL_MAX_SETS; i++) {
        if (g_sets[i].holemgr == hm) return &g_sets[i];
    }
    if (!create) return NULL;

    for (i = 0; i < ZPL_MAX_SETS; i++) {
        if (!g_sets[i].holemgr) {
            set_create(&g_sets[i], hm);
            return &g_sets[i];
        }
    }

    /* Round-robin eviction */
    i = (g_evict++) % ZPL_MAX_SETS;
    set_destroy(&g_sets[i]);
    set_create(&g_sets[i], hm);
    return &g_sets[i];
}

static void draw_set(Set *s, void *g)
{
    int i;
    DrawXmlFn fn = (DrawXmlFn)g_sites.xml_draw;
    if (!s || !g) return;
    for (i = 0; i < ZPL_NSLOTS; i++) {
        if (s->slot[i].size == 0) continue;
        g_current_draw_pri = ZPL_PRI_MIN + i;  /* 10..19 */
        fn(&s->slot[i], g, 0);             /* this = &Slot, prio = 0 -> list = &Slot+4 */
    }
    g_current_draw_pri = 0;
}

/* ------------------------------ Software Blend Blitter ------------------------------ */

static inline uint8_t clamp255(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

static inline int lum(int r, int g, int b) {
    return (77 * r + 150 * g + 29 * b + 128) >> 8;
}

static inline int sat(int r, int g, int b) {
    int min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    int max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    return max - min;
}

static inline void clip_color(int *r, int *g, int *b) {
    int l = lum(*r, *g, *b);
    int n = *r < *g ? (*r < *b ? *r : *b) : (*g < *b ? *g : *b);
    int x = *r > *g ? (*r > *b ? *r : *b) : (*g > *b ? *g : *b);
    if (n < 0) {
        int denom = l - n;
        if (denom != 0) {
            *r = l + ((*r - l) * l) / denom;
            *g = l + ((*g - l) * l) / denom;
            *b = l + ((*b - l) * l) / denom;
        }
    }
    if (x > 255) {
        int denom = x - l;
        if (denom != 0) {
            *r = l + ((*r - l) * (255 - l)) / denom;
            *g = l + ((*g - l) * (255 - l)) / denom;
            *b = l + ((*b - l) * (255 - l)) / denom;
        }
    }
    *r = clamp255(*r);
    *g = clamp255(*g);
    *b = clamp255(*b);
}

static inline void set_lum(int *r, int *g, int *b, int l) {
    int d = l - lum(*r, *g, *b);
    *r += d;
    *g += d;
    *b += d;
    clip_color(r, g, b);
}

static inline void set_sat(int *r, int *g, int *b, int s) {
    int *cmin, *cmid, *cmax;
    if (*r <= *g) {
        if (*g <= *b)      { cmin = r; cmid = g; cmax = b; }
        else if (*r <= *b) { cmin = r; cmid = b; cmax = g; }
        else               { cmin = b; cmid = r; cmax = g; }
    } else {
        if (*r <= *b)      { cmin = g; cmid = r; cmax = b; }
        else if (*g <= *b) { cmin = g; cmid = b; cmax = r; }
        else               { cmin = b; cmid = g; cmax = r; }
    }
    if (*cmax > *cmin) {
        *cmid = ((*cmid - *cmin) * s) / (*cmax - *cmin);
        *cmax = s;
    } else {
        *cmid = *cmax = 0;
    }
    *cmin = 0;
}

static inline uint8_t blend_channel(int mode, uint8_t d, uint8_t s) {
    switch (mode) {
        case ZPL_BLEND_NORMAL:
            return s;
        case ZPL_BLEND_ADD:
            return clamp255((int)d + s);
        case ZPL_BLEND_MULTIPLY:
            return (uint8_t)(((int)d * s) / 255);
        case ZPL_BLEND_SCREEN:
            return clamp255((int)d + s - ((int)d * s) / 255);
        case ZPL_BLEND_DARKEN:
            return (d < s) ? d : s;
        case ZPL_BLEND_LIGHTEN:
            return (d > s) ? d : s;
        case ZPL_BLEND_COLOR_DODGE:
            if (s >= 255) return 255;
            return clamp255(((int)d * 255) / (255 - s));
        case ZPL_BLEND_COLOR_BURN:
            if (s <= 0) return 0;
            return (uint8_t)(255 - clamp255(((255 - (int)d) * 255) / s));
        case ZPL_BLEND_OVERLAY:
            if (d < 128) return clamp255((2 * (int)d * s) / 255);
            return clamp255(255 - (2 * (255 - (int)d) * (255 - s)) / 255);
        case ZPL_BLEND_DIFFERENCE:
            return (d > s) ? (d - s) : (s - d);
        case ZPL_BLEND_EXCLUSION:
            return clamp255((int)d + s - (2 * (int)d * s) / 255);
        case ZPL_BLEND_HARD_LIGHT:
            if (s < 128) return clamp255((2 * (int)d * s) / 255);
            return clamp255(255 - (2 * (255 - (int)d) * (255 - s)) / 255);
        case ZPL_BLEND_SOFT_LIGHT: {
            int num = (255 - 2 * s) * (int)d * d + 510 * s * (int)d + 32512;
            return clamp255(num / 65025);
        }
        case ZPL_BLEND_LINEAR_BURN:
            return clamp255((int)d + s - 255);
        case ZPL_BLEND_VIVID_LIGHT:
            if (s <= 128) {
                if (s == 0) return 0;
                return (uint8_t)(255 - clamp255(((255 - (int)d) * 255) / (2 * s)));
            } else {
                if (s >= 255) return 255;
                return clamp255(((int)d * 255) / (2 * (255 - s)));
            }
        case ZPL_BLEND_LINEAR_LIGHT:
            return clamp255((int)d + 2 * s - 255);
        case ZPL_BLEND_PIN_LIGHT:
            if (s <= 128) {
                int s2 = 2 * s;
                return (d < s2) ? d : (uint8_t)s2;
            } else {
                int s2 = 2 * (s - 128);
                return (d > s2) ? d : (uint8_t)s2;
            }
        case ZPL_BLEND_HARD_MIX:
            return ((int)d + s >= 255) ? 255 : 0;
        case ZPL_BLEND_SUBTRACT:
            return (d > s) ? (d - s) : 0;
        case ZPL_BLEND_DIVIDE:
            if (s == 0) return 255;
            return clamp255(((int)d * 255) / s);
        case ZPL_BLEND_AVERAGE:
            return (uint8_t)(((int)d + s) >> 1);
        case ZPL_BLEND_NEGATION:
            return (uint8_t)(255 - abs(255 - (int)d - s));
        case ZPL_BLEND_REFLECT:
            if (s >= 255) return 255;
            return clamp255(((int)d * d) / (255 - s));
        case ZPL_BLEND_GLOW:
            if (d >= 255) return 255;
            return clamp255(((int)s * s) / (255 - d));
        case ZPL_BLEND_CONTRAST_NEGATE:
            if ((s > 128 && d < 128) || (s <= 128 && d >= 128)) return (uint8_t)(255 - d);
            return d;
        case ZPL_BLEND_PIGMENT: {
            int inv_d = 255 - d;
            int inv_s = 255 - s;
            int mix = (int)sqrt((inv_d * inv_d + inv_s * inv_s) / 2.0);
            return (uint8_t)(255 - clamp255(mix));
        }
        default:
            return s;
    }
}

static inline uint32_t blend_pixel(int mode, uint32_t dst_px, uint32_t src_px) {
    uint8_t sa = (uint8_t)(src_px >> 24);
    if (sa == 0) return dst_px; /* Transparent pixel: skip */

    uint8_t sr = (uint8_t)(src_px >> 16);
    uint8_t sg = (uint8_t)(src_px >> 8);
    uint8_t sb = (uint8_t)(src_px);

    uint8_t dr = (uint8_t)(dst_px >> 16);
    uint8_t dg = (uint8_t)(dst_px >> 8);
    uint8_t db = (uint8_t)(dst_px);
    uint8_t da = (uint8_t)(dst_px >> 24);

    if (mode == ZPL_BLEND_ERASE) {
        uint8_t out_a = (uint8_t)(((int)da * (255 - sa) + 127) / 255);
        return (out_a << 24) | (dr << 16) | (dg << 8) | db;
    }

    uint8_t br, bg, bb;

    if (mode == ZPL_BLEND_DARKER_COLOR) {
        int lum_d = lum(dr, dg, db);
        int lum_s = lum(sr, sg, sb);
        if (lum_s < lum_d) { br = sr; bg = sg; bb = sb; }
        else               { br = dr; bg = dg; bb = db; }
    } else if (mode == ZPL_BLEND_LIGHTER_COLOR) {
        int lum_d = lum(dr, dg, db);
        int lum_s = lum(sr, sg, sb);
        if (lum_s > lum_d) { br = sr; bg = sg; bb = sb; }
        else               { br = dr; bg = dg; bb = db; }
    } else if (mode == ZPL_BLEND_HUE) {
        int tr = sr, tg = sg, tb = sb;
        set_sat(&tr, &tg, &tb, sat(dr, dg, db));
        set_lum(&tr, &tg, &tb, lum(dr, dg, db));
        br = (uint8_t)tr; bg = (uint8_t)tg; bb = (uint8_t)tb;
    } else if (mode == ZPL_BLEND_SATURATION) {
        int tr = dr, tg = dg, tb = db;
        set_sat(&tr, &tg, &tb, sat(sr, sg, sb));
        set_lum(&tr, &tg, &tb, lum(dr, dg, db));
        br = (uint8_t)tr; bg = (uint8_t)tg; bb = (uint8_t)tb;
    } else if (mode == ZPL_BLEND_COLOR) {
        int tr = sr, tg = sg, tb = sb;
        set_lum(&tr, &tg, &tb, lum(dr, dg, db));
        br = (uint8_t)tr; bg = (uint8_t)tg; bb = (uint8_t)tb;
    } else if (mode == ZPL_BLEND_LUMINOSITY) {
        int tr = dr, tg = dg, tb = db;
        set_lum(&tr, &tg, &tb, lum(sr, sg, sb));
        br = (uint8_t)tr; bg = (uint8_t)tg; bb = (uint8_t)tb;
    } else {
        br = blend_channel(mode, dr, sr);
        bg = blend_channel(mode, dg, sg);
        bb = blend_channel(mode, db, sb);
    }

    if (sa == 255) {
        return (da << 24) | (br << 16) | (bg << 8) | bb;
    }

    /* Standard linear alpha interpolation with half-up rounding: C = (B * sa + D * (255 - sa) + 127) / 255 */
    uint8_t inv_a = 255 - sa;
    uint8_t out_r = (uint8_t)(((int)br * sa + (int)dr * inv_a + 127) / 255);
    uint8_t out_g = (uint8_t)(((int)bg * sa + (int)dg * inv_a + 127) / 255);
    uint8_t out_b = (uint8_t)(((int)bb * sa + (int)db * inv_a + 127) / 255);

    return (da << 24) | (out_r << 16) | (out_g << 8) | out_b;
}

/* ------------------------------ Exports ------------------------------ */

void ZPL_API ZPL_Init(const ZPL_Sites *sites)
{
    if (!sites) return;
    if (sites->list_ctor)     g_sites.list_ctor     = sites->list_ctor;
    if (sites->list_dtor)     g_sites.list_dtor     = sites->list_dtor;
    if (sites->list_erase)    g_sites.list_erase    = sites->list_erase;
    if (sites->xml_draw)      g_sites.xml_draw      = sites->xml_draw;
    if (sites->draw_image)    g_sites.draw_image    = sites->draw_image;
    if (sites->set_draw_mode) g_sites.set_draw_mode = sites->set_draw_mode;
}

static uint32_t s_logged_bits_fail_mask = 0;

static int s_level_blend_counts[ZPL_BLEND_MAX + 1] = {0};
static int s_level_blend_logged = 0;
static int s_logged_state_s1 = 0;
static int s_logged_state_s3 = 0;
static int s_logged_state_s4 = 0;
static int s_last_logged_state = -1;
static int s_state_change_count = 0;
static int s_draw_high_log_count[16] = {0};
static int s_draw_low_log_count[16] = {0};
static int s_draw_bake_log_count = 0;

void ZPL_API ZPL_OnStateDraw(int state)
{
    if (state != s_last_logged_state) {
        s_last_logged_state = state;
        if (s_state_change_count < 50) {
            s_state_change_count++;
            logf_("Dispatcher StateTransition -> mState=%d (change #%d)", state, s_state_change_count);
        }
    }
}

static int get_board_state(void *level)
{
    if (!level || IsBadReadPtr(level, 0x88)) return -1;
    void *board = *(void **)((uint8_t *)level + 0x84);
    if (!board || IsBadReadPtr(board, 0x64)) return -1;
    return *(int *)((uint8_t *)board + 0x60);
}

static void check_log_state(void *level)
{
    int state = get_board_state(level);
    if (state < 0) return;
    void *board = *(void **)((uint8_t *)level + 0x84);
    if (state == 1 && !s_logged_state_s1) {
        s_logged_state_s1 = 1;
        logf_("StateEnter: State 1 (Intro) Board=%p mState=%d", board, state);
    } else if (state == 3 && !s_logged_state_s3) {
        s_logged_state_s3 = 1;
        logf_("StateEnter: State 3 (Stage Complete) Board=%p mState=%d", board, state);
    } else if (state == 4 && !s_logged_state_s4) {
        s_logged_state_s4 = 1;
        logf_("StateEnter: State 4 (Pause) Board=%p mState=%d", board, state);
    }
}

void ZPL_API ZPL_OnLoadBegin(void *holemgr)
{
    s_logged_bits_fail_mask = 0;
    s_level_log_count = 0;
    s_level_blend_logged = 0;
    s_logged_state_s1 = 0;
    s_logged_state_s3 = 0;
    s_logged_state_s4 = 0;
    s_last_logged_state = -1;
    s_state_change_count = 0;
    memset(s_draw_high_log_count, 0, sizeof(s_draw_high_log_count));
    memset(s_draw_low_log_count, 0, sizeof(s_draw_low_log_count));
    s_draw_bake_log_count = 0;
    memset(s_level_blend_counts, 0, sizeof(s_level_blend_counts));
    g_blend_parsed = 0; /* re-parse XML if level changed */
    Set *s = find_set(holemgr, 0);
    if (s) {
        int freed = 0;
        for (int i = 0; i < ZPL_NSLOTS; i++) freed += s->slot[i].size;
        logf_("OnLoadBegin: reset hm=%p freed=%d", holemgr, freed);
        set_reset(s);
    }
}

void ZPL_API ZPL_OnHoleMgrDestroy(void *holemgr)
{
    Set *s = find_set(holemgr, 0);
    if (s) {
        logf_("OnHoleMgrDestroy: hm=%p", holemgr);
        set_destroy(s);
    }
}

void *ZPL_API ZPL_GetListBase(void *holemgr, int pri)
{
    Set *s;
    if (!holemgr) return NULL;
    if (pri > ZPL_PRI_MAX) pri = ZPL_PRI_MAX;
    if (pri < ZPL_PRI_MIN) pri = ZPL_PRI_MIN;
    s = find_set(holemgr, 1);
    if (!s) return NULL;
    int idx = pri - ZPL_PRI_MIN;
    return &s->slot[idx];
}

void ZPL_API ZPL_DrawHigh(void *level, void *graphics)
{
    int st = get_board_state(level);
    int st_idx = (st >= 0 && st < 16) ? st : 15;
    void *hm;

    /* Guard: Never draw high-priority layers during non-gameplay cutscenes:
     * State 4 = Congratulations Cutscene (no curve balls or prio layers)
     * State 5 = Earthquake Crumble
     * State 6 = Stage Complete End / Stats
     */
    if (st >= 3) {
        return;
    }

    if (!level) {
        if (s_draw_high_log_count[st_idx] < 3) {
            s_draw_high_log_count[st_idx]++;
            logf_("DrawHigh[State %d]: early exit (level is NULL)", st);
        }
        return;
    }
    if (!graphics) {
        if (s_draw_high_log_count[st_idx] < 3) {
            s_draw_high_log_count[st_idx]++;
            logf_("DrawHigh[State %d]: early exit (graphics is NULL)", st);
        }
        return;
    }
    check_log_state(level);
    hm = *(void **)((uint8_t *)level + 0x74);        /* mActiveHoleMgr */
    if (!hm) {
        if (s_draw_high_log_count[st_idx] < 3) {
            s_draw_high_log_count[st_idx]++;
            logf_("DrawHigh[State %d]: early exit (hm is NULL, level=%p)", st, level);
        }
        return;
    }
    Set *s = find_set(hm, 0);
    int total_nodes = 0;
    int active_slots = 0;
    if (s) {
        for (int i = 0; i < ZPL_NSLOTS; i++) {
            total_nodes += s->slot[i].size;
            if (s->slot[i].size > 0) active_slots++;
        }
    }
    if (s_draw_high_log_count[st_idx] < 3) {
        s_draw_high_log_count[st_idx]++;
        logf_("DrawHigh[State %d]: level=%p hm=%p set=%p total_nodes=%d active_slots=%d",
              st, level, hm, s, total_nodes, active_slots);
    }
    if (s) {
        draw_set(s, graphics);
    }
}

void ZPL_API ZPL_DrawLow(void *level, void *graphics)
{
    int st = get_board_state(level);
    int st_idx = (st >= 0 && st < 16) ? st : 15;
    void *hm;
    int k;
    DrawXmlFn fn = (DrawXmlFn)g_sites.xml_draw;

    /* Guard: Never draw low-priority layers during dark overlay states:
     * State 3 = Stage Complete Cutscene (layers 0..4 already drawn before dark overlay at 0x426E41)
     * State 4 = Pause Screen (darkening overlay already applied at 0x426ED7)
     * State 5 = Earthquake
     * State 6 = Stage Complete End / Stats
     */
    if (st == 3 || st == 4 || st == 5 || st == 6) {
        return;
    }

    if (!level) {
        if (s_draw_low_log_count[st_idx] < 3) {
            s_draw_low_log_count[st_idx]++;
            logf_("DrawLow[State %d]: early exit (level is NULL)", st);
        }
        return;
    }
    if (!graphics) {
        if (s_draw_low_log_count[st_idx] < 3) {
            s_draw_low_log_count[st_idx]++;
            logf_("DrawLow[State %d]: early exit (graphics is NULL)", st);
        }
        return;
    }
    if (!fn) {
        if (s_draw_low_log_count[st_idx] < 3) {
            s_draw_low_log_count[st_idx]++;
            logf_("DrawLow[State %d]: early exit (xml_draw fn is NULL)", st);
        }
        return;
    }
    check_log_state(level);
    hm = *(void **)((uint8_t *)level + 0x74);        /* mActiveHoleMgr */
    if (!hm) {
        if (s_draw_low_log_count[st_idx] < 3) {
            s_draw_low_log_count[st_idx]++;
            logf_("DrawLow[State %d]: early exit (hm is NULL, level=%p)", st, level);
        }
        return;
    }
    if (s_draw_low_log_count[st_idx] < 3) {
        s_draw_low_log_count[st_idx]++;
        logf_("DrawLow[State %d]: level=%p hm=%p calling xml_draw for k=0..4", st, level, hm);
    }
    for (k = 0; k < 5; k++) {
        fn(hm, graphics, k);                         /* vanilla layers 0..4 (all cutouts/sprites < 10) */
    }
}

void ZPL_API ZPL_DrawBake(void *holemgr, void *graphics)
{
    int k;
    DrawXmlFn fn = (DrawXmlFn)g_sites.xml_draw;
    if (!holemgr || !graphics) return;

    Set *s = find_set(holemgr, 0);
    int total_nodes = 0;
    if (s) {
        for (int i = 0; i < ZPL_NSLOTS; i++) total_nodes += s->slot[i].size;
    }
    if (s_draw_bake_log_count < 3) {
        s_draw_bake_log_count++;
        logf_("DrawBake: hm=%p g=%p set=%p total_nodes=%d (baking low prio only k=0..4, prio>=10 excluded)",
              holemgr, graphics, s, total_nodes);
    }

    if (fn) {
        for (k = 0; k < 5; k++) {
            fn(holemgr, graphics, k);                 /* vanilla cutouts & layers 0..4 only */
        }
    }
    /* Note: pri >= 10 is intentionally excluded from earthquake bake snapshot */
}

int ZPL_API ZPL_GetPriRange(int *lo, int *hi)
{
    if (lo) *lo = ZPL_PRI_MIN;
    if (hi) *hi = ZPL_PRI_MAX;
    return ZPL_NSLOTS;
}

void *ZPL_API ZPL_GetList(void *holemgr, int pri)
{
    Set *s = find_set(holemgr, 0);
    if (!s || pri < ZPL_PRI_MIN || pri > ZPL_PRI_MAX) return NULL;
    return &s->slot[pri - ZPL_PRI_MIN].proxy;
}

#define VTBL_DDIMAGE          0x005039A4u
#define VTBL_MEMIMAGE         0x005034DCu
#define VA_MEMIMG_BITSCHANGED 0x00448390u   /* MemoryImage::BitsChanged */
#define VA_DDIMG_BITSCHANGED  0x0044CD10u   /* DDImage::BitsChanged */

static inline uint32_t *img_get_bits(void *img)
{
    if (!img || IsBadReadPtr(img, 0x20)) return NULL;
    uint32_t *bits = *(uint32_t **)((uint8_t *)img + 0x14);
    if (!bits) {
        void **vt = *(void ***)img;
        if (vt && !IsBadReadPtr(vt, 0x60)) {
            typedef uint32_t *(__thiscall *GetBitsFn)(void *self);
            GetBitsFn fn = (GetBitsFn)vt[22]; /* slot 22 (offset 0x58) = GetBits */
            if (fn && !IsBadCodePtr((FARPROC)fn)) {
                bits = fn(img);
            }
        }
    }
    if (!bits) {
        bits = *(uint32_t **)((uint8_t *)img + 0x14);
    }
    return bits;
}

static inline void img_bits_changed(void *img)
{
    if (!img || IsBadReadPtr(img, 4)) return;
    uintptr_t vt = *(uintptr_t *)img;
    typedef void (__fastcall *BitsChangedFn)(void *self);
    if (vt == VTBL_DDIMAGE) {
        ((BitsChangedFn)VA_DDIMG_BITSCHANGED)(img);
    } else if (vt == VTBL_MEMIMAGE) {
        ((BitsChangedFn)VA_MEMIMG_BITSCHANGED)(img);
    } else {
        void **vtp = *(void ***)img;
        if (vtp && !IsBadReadPtr(vtp, 0x50)) {
            BitsChangedFn fn = (BitsChangedFn)vtp[16]; /* slot 16 (offset 0x40) */
            if (fn && !IsBadCodePtr((FARPROC)fn)) {
                fn(img);
            }
        }
    }
}

/* ------------------------------ Tingkat 2: D3D7 Hardware Render States ------------------------------ */

typedef HRESULT (__stdcall *fSetRenderState)(void *dev, DWORD state, DWORD val);
typedef HRESULT (__stdcall *fSetTextureStageState)(void *dev, DWORD stage, DWORD state, DWORD val);
typedef HRESULT (__stdcall *fValidateDevice)(void *dev, DWORD *passes);

static inline HRESULT rs(void *dev, DWORD state, DWORD val) {
    if (!dev) return -1;
    fSetRenderState fn = ((fSetRenderState *)(*(void ***)dev))[20];
    return fn(dev, state, val);
}

static inline HRESULT tss(void *dev, DWORD stage, DWORD state, DWORD val) {
    if (!dev) return -1;
    fSetTextureStageState fn = ((fSetTextureStageState *)(*(void ***)dev))[37];
    return fn(dev, stage, state, val);
}

static inline HRESULT validate_device(void *dev, DWORD *passes) {
    if (!dev) return -1;
    fValidateDevice fn = ((fValidateDevice *)(*(void ***)dev))[38];
    return fn(dev, passes);
}

static int s_hw_mode = 0;
__attribute__((unused)) static int s_hw_ok = -1;   /* 0 off / 2 multiply / 3 screen ; -1 = belum diuji */
static void *s_dev = NULL;                /* hanya valid di dalam SATU DrawNode */

static void hw_reset(void *dev) {
    if (!dev) return;
    tss(dev, 1, 1, 1);                /* COLOROP stage 1 = DISABLE */
    rs(dev, 19, 5); rs(dev, 20, 6);   /* D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA (normal) */
}

int ZPL_API ZPL_HwBlendHook(void *dev) {
    DWORD passes = 0;
    if (!s_hw_mode || !dev) return 0;

    DWORD src_val = (s_hw_mode == 2 ? 9 /* D3DBLEND_DESTCOLOR */ : 2 /* D3DBLEND_ONE */);
    DWORD dst_val = (s_hw_mode == 2 ? 6 /* D3DBLEND_INVSRCALPHA */ : 4 /* D3DBLEND_INVSRCCOLOR */);
    rs(dev, 19, src_val);
    rs(dev, 20, dst_val);

    static int s_logged_hw_calls = 0;
    if (s_logged_hw_calls < 20) {
        s_logged_hw_calls++;
        HRESULT hr_pure = validate_device(dev, &passes);
        logf_always("ZPL_HwBlendHook: mode=%d dev=%p rs(19,%u) rs(20,%u) -> ValidateDevice pure hr=0x%08X passes=%u",
                    s_hw_mode, dev, (unsigned)src_val, (unsigned)dst_val, (unsigned)hr_pure, (unsigned)passes);

        /* Diagnostic test: what would ValidateDevice report if Stage 1 TSS was applied */
        DWORD passes_tss = 0;
        tss(dev, 1, 1, 4); tss(dev, 1, 2, 1); tss(dev, 1, 3, 0x21);
        tss(dev, 1, 4, 2); tss(dev, 1, 5, 1);
        HRESULT hr_tss = validate_device(dev, &passes_tss);
        logf_always("ZPL_HwBlendHook: mode=%d Stage 1 TSS test -> ValidateDevice hr=0x%08X passes=%u",
                    s_hw_mode, (unsigned)hr_tss, (unsigned)passes_tss);
        tss(dev, 1, 1, 1); /* Restore Stage 1 COLOROP = DISABLE */
    }

    s_dev = dev;
    return 1;
}

__attribute__((unused)) static void hw_end(void) {
    if (s_dev) { hw_reset(s_dev); s_dev = NULL; }   /* hanya jika hook benar-benar jalan */
    s_hw_mode = 0;
}

/* ------------------------------ Tingkat 3: DirectDraw Backbuffer Lock/Unlock ------------------------------ */

#pragma pack(push, 4)
typedef struct {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwFourCC;
    DWORD dwRGBBitCount;
    DWORD dwRBitMask;
    DWORD dwGBitMask;
    DWORD dwBBitMask;
    DWORD dwRGBAlphaBitMask;
} ZPL_DDPIXELFORMAT;

typedef struct {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwHeight;
    DWORD dwWidth;
    LONG  lPitch;
    DWORD dwBackBufferCount;
    DWORD dwMipMapCount;
    DWORD dwAlphaBitDepth;
    DWORD dwReserved;
    void *lpSurface;
    DWORD dwCKDestOverlay[2];
    DWORD dwCKDestBlt[2];
    DWORD dwCKSrcOverlay[2];
    DWORD dwCKSrcBlt[2];
    ZPL_DDPIXELFORMAT ddpfPixelFormat;
    DWORD ddsCaps[4];
    DWORD dwTextureStage;
} ZPL_DDSURFACEDESC2;
#pragma pack(pop)

typedef HRESULT (__stdcall *fEndScene)(void *dev);
typedef HRESULT (__stdcall *fBeginScene)(void *dev);
typedef HRESULT (__stdcall *fGetRenderTarget)(void *dev, void **ppSurface);
typedef HRESULT (__stdcall *fSurfaceLock)(void *dds, RECT *rc, void *desc, DWORD flags, HANDLE hEvent);
typedef HRESULT (__stdcall *fSurfaceUnlock)(void *dds, RECT *rc);
typedef ULONG   (__stdcall *fRelease)(void *p);

static int blend_to_dd_surface(void *dst_img, void *img, const uint32_t *src_bits, void *g, int x, int y, int mode) {
    if (!dst_img || !img || !src_bits || !g) {
        logf_always("blend_to_dd_surface NULL arg: dst_img=%p img=%p src_bits=%p g=%p", dst_img, img, src_bits, g);
        return 0;
    }
    void *dds = *(void **)((char *)dst_img + 0x48);
    void *dev = *(void **)((char *)dst_img + 0x1C);
    if (!dev) {
        void *d3d = *(void **)((char *)dst_img + 0x44);
        if (d3d && !IsBadReadPtr(d3d, 16)) {
            void *helper = *(void **)((char *)d3d + 8);
            if (helper && !IsBadReadPtr(helper, 32)) {
                dev = *(void **)((char *)helper + 0x1C);
            }
        }
    }
    if (!dev) {
        void **p_app = (void **)0x0057EF00;
        if (p_app && !IsBadReadPtr(p_app, sizeof(void *)) && *p_app) {
            char *app = (char *)*p_app;
            if (!IsBadReadPtr(app + 0x31C, sizeof(void *))) {
                char *dd_iface = *(char **)(app + 0x31C);
                if (dd_iface && !IsBadReadPtr(dd_iface + 8, sizeof(void *))) {
                    char *helper = *(char **)(dd_iface + 8);
                    if (helper && !IsBadReadPtr(helper + 0x20, sizeof(void *))) {
                        dev = *(void **)(helper + 0x1C);
                    }
                }
            }
        }
    }
    if (!dev && s_dev) {
        dev = s_dev;
    }

    static int s_logged_dd_entry = 0;
    if (s_logged_dd_entry < 5) {
        s_logged_dd_entry++;
        logf_always("blend_to_dd_surface ENTRY: mode=%d dst_img=%p img=%p src_bits=%p dev=%p dds=%p",
                    mode, dst_img, img, src_bits, dev, dds);
    }

    void *rt = NULL;
    HRESULT hr_rt = -1;
    if (dev && !IsBadReadPtr(dev, 40)) {
        fGetRenderTarget pfnGetRenderTarget = ((fGetRenderTarget *)(*(void ***)dev))[9];
        if (pfnGetRenderTarget && !IsBadCodePtr((FARPROC)pfnGetRenderTarget)) {
            hr_rt = pfnGetRenderTarget(dev, &rt);
        }
    }
    void *target_dds = (hr_rt == 0 && rt) ? rt : dds;

    static int s_logged_dd_rt = 0;
    if (s_logged_dd_rt < 5) {
        s_logged_dd_rt++;
        logf_always("blend_to_dd_surface RT: hr_rt=0x%08X rt=%p target_dds=%p", (unsigned)hr_rt, rt, target_dds);
    }

    if (!target_dds) {
        logf_always("blend_to_dd_surface: target_dds is NULL! hr_rt=0x%08X rt=%p dds=%p", (unsigned)hr_rt, rt, dds);
        return 0;
    }

    int img_w = *(int *)((char *)img + 4);
    int img_h = *(int *)((char *)img + 8);
    int dst_w = *(int *)((char *)dst_img + 4);
    int dst_h = *(int *)((char *)dst_img + 8);

    /* Apply Graphics translation */
    int trans_x = *(int *)((char *)g + 8);
    int trans_y = *(int *)((char *)g + 0xC);
    int draw_x = x + trans_x;
    int draw_y = y + trans_y;

    /* Clip bounds */
    int clip_x = *(int *)((char *)g + 0x10);
    int clip_y = *(int *)((char *)g + 0x14);
    int clip_w = *(int *)((char *)g + 0x18);
    int clip_h = *(int *)((char *)g + 0x1C);

    int min_x = (clip_w > 0) ? clip_x : 0;
    int min_y = (clip_h > 0) ? clip_y : 0;
    int max_x = (clip_w > 0) ? (clip_x + clip_w) : dst_w;
    int max_y = (clip_h > 0) ? (clip_y + clip_h) : dst_h;
    if (max_x > dst_w) max_x = dst_w;
    if (max_y > dst_h) max_y = dst_h;

    int x1 = draw_x;
    int y1 = draw_y;
    int x2 = draw_x + img_w;
    int y2 = draw_y + img_h;

    if (x1 < min_x) x1 = min_x;
    if (y1 < min_y) y1 = min_y;
    if (x2 > max_x) x2 = max_x;
    if (y2 > max_y) y2 = max_y;

    int blit_w = x2 - x1;
    int blit_h = y2 - y1;

    static int s_logged_dd_clip = 0;
    if (s_logged_dd_clip < 5) {
        s_logged_dd_clip++;
        logf_always("blend_to_dd_surface CLIP: draw=(%d,%d) img=%dx%d dst=%dx%d clip=(%d,%d,%d,%d) -> blit=(%d,%d) rect=[%d,%d to %d,%d]",
                    draw_x, draw_y, img_w, img_h, dst_w, dst_h, clip_x, clip_y, clip_w, clip_h, blit_w, blit_h, x1, y1, x2, y2);
    }

    if (blit_w <= 0 || blit_h <= 0) {
        if (rt) ((fRelease)(*(void ***)rt)[2])(rt);
        return 1; /* Clipped out completely, success */
    }

    int src_start_x = x1 - draw_x;
    int src_start_y = y1 - draw_y;

    /* 1. Flush D3D scene: EndScene (vtable slot 6) */
    fEndScene pfnEndScene = dev ? ((fEndScene *)(*(void ***)dev))[6] : NULL;
    fBeginScene pfnBeginScene = dev ? ((fBeginScene *)(*(void ***)dev))[5] : NULL;
    if (pfnEndScene) pfnEndScene(dev);

    /* 2. Lock entire surface (RECT = NULL) per Claude Opsi A */
    ZPL_DDSURFACEDESC2 desc;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);

    fSurfaceLock pfnLock = ((fSurfaceLock *)(*(void ***)target_dds))[25];
    fSurfaceUnlock pfnUnlock = ((fSurfaceUnlock *)(*(void ***)target_dds))[32];

    HRESULT hr_lock = pfnLock(target_dds, NULL, &desc, 1 /* DDLOCK_WAIT */, NULL);
    if (hr_lock != 0 || !desc.lpSurface) {
        static int s_logged_lock_fail = 0;
        if (s_logged_lock_fail < 5) {
            s_logged_lock_fail++;
            logf_always("blend_to_dd_surface: Lock FAILED hr=0x%08X rt=%p target_dds=%p dds=%p", (unsigned)hr_lock, rt, target_dds, dds);
        }
        if (rt) ((fRelease)(*(void ***)rt)[2])(rt);
        if (pfnBeginScene) pfnBeginScene(dev);
        return 0;
    }

    int bpp = desc.ddpfPixelFormat.dwRGBBitCount;
    uint8_t *surf_ptr = (uint8_t *)desc.lpSurface;

    /* Safety guard: enforce surface bounds from locked descriptor */
    if (x1 + blit_w > (int)desc.dwWidth) blit_w = (int)desc.dwWidth - x1;
    if (y1 + blit_h > (int)desc.dwHeight) blit_h = (int)desc.dwHeight - y1;
    if (blit_w <= 0 || blit_h <= 0) {
        pfnUnlock(target_dds, NULL);
        if (rt) ((fRelease)(*(void ***)rt)[2])(rt);
        if (pfnBeginScene) pfnBeginScene(dev);
        return 1;
    }

    /* Diagnostic log per Claude Perubahan 2 */
    static int s_logged_diag = 0;
    if (s_logged_diag < 10) {
        s_logged_diag++;
        int samp_r = blit_h / 2;
        int samp_c = blit_w / 2;
        uint32_t sample_b = 0;
        uint32_t sample_s = src_bits[(src_start_y + samp_r) * img_w + (src_start_x + samp_c)];
        if (bpp == 32) {
            sample_b = *(uint32_t *)(surf_ptr + (y1 + samp_r) * desc.lPitch + (x1 + samp_c) * 4);
        } else if (bpp == 16) {
            sample_b = *(uint16_t *)(surf_ptr + (y1 + samp_r) * desc.lPitch + (x1 + samp_c) * 2);
        }
        uint32_t sample_z = blend_pixel(mode, sample_b, sample_s);
        logf_always("BlendDiag: mode=%d hr_lock=0x%08X dim=%ux%u pitch=%d bpp=%d masks=(R:0x%X, G:0x%X, B:0x%X) rt_is_dds=%d sample(B:0x%08X, S:0x%08X -> Z:0x%08X)",
                    mode, (unsigned)hr_lock, (unsigned)desc.dwWidth, (unsigned)desc.dwHeight,
                    (int)desc.lPitch, bpp,
                    (unsigned)desc.ddpfPixelFormat.dwRBitMask,
                    (unsigned)desc.ddpfPixelFormat.dwGBitMask,
                    (unsigned)desc.ddpfPixelFormat.dwBBitMask,
                    (rt == dds), sample_b, sample_s, sample_z);
    }

    /* 3. Blend pixels */
    if (bpp == 32) {
        for (int row = 0; row < blit_h; row++) {
            uint32_t *dst_row = (uint32_t *)(surf_ptr + (y1 + row) * desc.lPitch) + x1;
            const uint32_t *src_row = src_bits + (src_start_y + row) * img_w + src_start_x;
            for (int col = 0; col < blit_w; col++) {
                dst_row[col] = blend_pixel(mode, dst_row[col], src_row[col]);
            }
        }
    } else if (bpp == 16) {
        int is_565 = (desc.ddpfPixelFormat.dwGBitMask == 0x07E0);
        for (int row = 0; row < blit_h; row++) {
            uint16_t *dst_row = (uint16_t *)(surf_ptr + (y1 + row) * desc.lPitch) + x1;
            const uint32_t *src_row = src_bits + (src_start_y + row) * img_w + src_start_x;
            for (int col = 0; col < blit_w; col++) {
                uint16_t d16 = dst_row[col];
                uint32_t d32;
                if (is_565) {
                    uint32_t r = (d16 >> 11) & 0x1F; r = (r << 3) | (r >> 2);
                    uint32_t g = (d16 >> 5)  & 0x3F; g = (g << 2) | (g >> 4);
                    uint32_t b = (d16)       & 0x1F; b = (b << 3) | (b >> 2);
                    d32 = 0xFF000000 | (r << 16) | (g << 8) | b;
                } else {
                    uint32_t r = (d16 >> 10) & 0x1F; r = (r << 3) | (r >> 2);
                    uint32_t g = (d16 >> 5)  & 0x1F; g = (g << 3) | (g >> 2);
                    uint32_t b = (d16)       & 0x1F; b = (b << 3) | (b >> 2);
                    d32 = 0xFF000000 | (r << 16) | (g << 8) | b;
                }
                uint32_t blended = blend_pixel(mode, d32, src_row[col]);
                uint8_t br = (uint8_t)(blended >> 16);
                uint8_t bg = (uint8_t)(blended >> 8);
                uint8_t bb = (uint8_t)(blended);
                if (is_565) {
                    dst_row[col] = (uint16_t)(((br >> 3) << 11) | ((bg >> 2) << 5) | (bb >> 3));
                } else {
                    dst_row[col] = (uint16_t)(((br >> 3) << 10) | ((bg >> 3) << 5) | (bb >> 3));
                }
            }
        }
    }

    /* 4. Unlock surface */
    pfnUnlock(target_dds, NULL);

    /* Release reference acquired via GetRenderTarget */
    if (rt) {
        ((fRelease)(*(void ***)rt)[2])(rt);
    }

    /* 5. Resume scene */
    if (pfnBeginScene) pfnBeginScene(dev);

    return 1;
}

static void blend_to_mem_image(void *dst_img, uint32_t *dst_bits, void *img, const uint32_t *src_bits, void *g, int x, int y, int blend_mode) {
    int img_w = *(int *)((uint8_t *)img + 4);
    int img_h = *(int *)((uint8_t *)img + 8);
    int dst_w = *(int *)((uint8_t *)dst_img + 4);
    int dst_h = *(int *)((uint8_t *)dst_img + 8);

    if (img_w <= 0 || img_h <= 0 || dst_w <= 0 || dst_h <= 0) return;

    if (IsBadReadPtr(src_bits, sizeof(uint32_t)) ||
        IsBadReadPtr(src_bits + (size_t)img_w * img_h - 1, sizeof(uint32_t)) ||
        IsBadWritePtr(dst_bits, sizeof(uint32_t)) ||
        IsBadWritePtr(dst_bits + (size_t)dst_w * dst_h - 1, sizeof(uint32_t))) {
        return;
    }

    int trans_x = *(int *)((uint8_t *)g + 8);
    int trans_y = *(int *)((uint8_t *)g + 0xC);

    int clip_x = *(int *)((uint8_t *)g + 0x10);
    int clip_y = *(int *)((uint8_t *)g + 0x14);
    int clip_w = *(int *)((uint8_t *)g + 0x18);
    int clip_h = *(int *)((uint8_t *)g + 0x1C);

    int draw_x = x + trans_x;
    int draw_y = y + trans_y;

    int x0 = draw_x;
    int y0 = draw_y;
    int x1 = draw_x + img_w;
    int y1 = draw_y + img_h;

    if (clip_w > 0 && clip_h > 0) {
        if (x0 < clip_x) x0 = clip_x;
        if (y0 < clip_y) y0 = clip_y;
        if (x1 > clip_x + clip_w) x1 = clip_x + clip_w;
        if (y1 > clip_y + clip_h) y1 = clip_y + clip_h;
    }

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > dst_w) x1 = dst_w;
    if (y1 > dst_h) y1 = dst_h;

    if (x0 >= x1 || y0 >= y1) return;

    for (int cy = y0; cy < y1; cy++) {
        int sy = cy - draw_y;
        if (sy < 0 || sy >= img_h) continue;
        uint32_t *drow = dst_bits + (size_t)cy * dst_w;
        const uint32_t *srow = src_bits + (size_t)sy * img_w;
        for (int cx = x0; cx < x1; cx++) {
            int sx = cx - draw_x;
            if (sx < 0 || sx >= img_w) continue;
            drow[cx] = blend_pixel(blend_mode, drow[cx], srow[sx]);
        }
    }

    img_bits_changed(dst_img);
}

static int node_blend(const void *node) {
    if (!node) return ZPL_BLEND_NORMAL;
    uint32_t v = *(const uint32_t *)((const uint8_t *)node + 0x14);
    if ((v >> 24) != 0x5A) return ZPL_BLEND_NORMAL;      /* magic tag 0x5A guard against stack residue */
    int pri = (int)(v & 0xFFFF);
    int mode = get_blend_mode_for_pri(pri);
    if (mode < 0 || mode > ZPL_BLEND_MAX) mode = ZPL_BLEND_NORMAL;
    s_level_blend_counts[mode]++;
    if (!s_level_blend_logged && s_level_log_count < 45) {
        s_level_blend_logged = 1;
        logf_("LevelBlendStats: active mode=%d for pri=%d", mode, pri);
    }
    return mode;
}

void ZPL_API ZPL_DrawNode(void *node, void *g, void *img, int x, int y)
{
    if (!g || !img) return;

    int blend_mode = node_blend(node);

    /* Mode 0 (Normal) uses native hardware DrawImage */
    if (blend_mode == ZPL_BLEND_NORMAL) {
        ((DrawImgFn)g_sites.draw_image)(g, img, x, y);
        return;
    }

    /* Mode 10 (Add) uses native SetDrawMode(1) for speed */
    if (blend_mode == ZPL_BLEND_ADD) {
        int prev = *(int *)((uint8_t *)g + 0x44);
        ((SetModeFn)g_sites.set_draw_mode)(g, 1);
        ((DrawImgFn)g_sites.draw_image)(g, img, x, y);
        ((SetModeFn)g_sites.set_draw_mode)(g, prev);
        return;
    }

    void *dst_img = *(void **)((uint8_t *)g + 4);
    if (!dst_img) {
        ((DrawImgFn)g_sites.draw_image)(g, img, x, y);
        return;
    }

    uint32_t *src_bits = img_get_bits(img);
    uint32_t *dst_bits = img_get_bits(dst_img);

    static int s_logged_drawnode = 0;
    if (blend_mode > 0 && s_logged_drawnode < 20) {
        s_logged_drawnode++;
        logf_always("ZPL_DrawNode: blend_mode=%d dst_bits=%p src_bits=%p dst_img=%p img=%p x=%d y=%d",
                    blend_mode, dst_bits, src_bits, dst_img, img, x, y);
    }

    /* Target is MemoryImage (e.g. Earthquake bake snapshot, or 2D software mode) */
    if (dst_bits && src_bits) {
        blend_to_mem_image(dst_img, dst_bits, img, src_bits, g, x, y, blend_mode);
        return;
    }

    /* Target is DDImage (3D Hardware Accelerated Mode) */
    if (!dst_bits) {
        /* DirectDraw Surface Lock path for all advanced blend modes (Modes != Normal && != Add) */
        if (blend_mode != ZPL_BLEND_NORMAL && blend_mode != ZPL_BLEND_ADD) {
            if (!src_bits) {
                static int s_logged_nobits = 0;
                if (s_logged_nobits < 5) {
                    s_logged_nobits++;
                    logf_always("ZPL_DrawNode: mode=%d but src_bits is NULL! Falling back.", blend_mode);
                }
            } else {
                int res = blend_to_dd_surface(dst_img, img, src_bits, g, x, y, blend_mode);
                static int s_logged_dd_res = 0;
                if (s_logged_dd_res < 5) {
                    s_logged_dd_res++;
                    logf_always("ZPL_DrawNode: blend_to_dd_surface returned %d", res);
                }
                if (res) return;
            }
        }
    }

    /* Fallback to native DrawImage */
    ((DrawImgFn)g_sites.draw_image)(g, img, x, y);
}

static void init_sites_dynamic(void) {
    const uint8_t *p_xml = (const uint8_t *)0x00405AF0u;
    if (!IsBadReadPtr(p_xml, 6) && (p_xml[0] == 0x55 || p_xml[0] == 0xE9)) {
        g_sites.xml_draw = 0x00405AF0u;
        logf_("Site B / xml_draw verified at fixed VA 0x%08X (entry byte=0x%02X)",
              g_sites.xml_draw, p_xml[0]);
    } else {
        void *p_site_b = FindUniquePattern("53 8B CE E8 ?? ?? ?? ?? 5E 5B 8B E5 5D C2 04 00", "Site B / xml_draw");
        if (p_site_b) {
            int32_t rel = *(int32_t *)((char *)p_site_b + 4);
            g_sites.xml_draw = (uint32_t)(uintptr_t)((char *)p_site_b + 8 + rel);
        }
    }
    void *p_draw_img = FindUniquePattern("55 8B EC 83 EC 20 53 56 8B F1 8B 5E 08 8B 56 0C", "Graphics::DrawImage");
    if (p_draw_img) {
        g_sites.draw_image = (uint32_t)(uintptr_t)p_draw_img;
    }
    void *p_set_mode = FindUniquePattern("55 8B EC 8B 45 08 89 41 44 5D C2 04 00", "Graphics::SetDrawMode");
    if (p_set_mode) {
        g_sites.set_draw_mode = (uint32_t)(uintptr_t)p_set_mode;
    }
}

BOOL APIENTRY DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)h; (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        init_sites_dynamic();
        logf_("zumapriolayer.dll loaded (v0.6.0.4 High-Priority Layer & Earthquake Bake Engine active, xml_draw=0x%08X draw_img=0x%08X set_mode=0x%08X)",
              g_sites.xml_draw, g_sites.draw_image, g_sites.set_draw_mode);
    }
    return TRUE;
}
