/* zumapriolayer.h - ZuCompleluxe v0.6.1 High-Priority Layer & Earthquake Bake Sidecar API
 *
 * Sprites & Cutouts with priority >= 10 render on top of Frog, Coins, and Bullets.
 * During Stage Completion Earthquake (State 5), all visual elements (background, skulls,
 * cutouts, sprites) shake and crumble together into the abyss.
 */
#ifndef ZUMAPRIOLAYER_H
#define ZUMAPRIOLAYER_H

#include <stdint.h>

#define ZPL_API_VERSION 3
#define ZPL_PRI_MIN     10
#define ZPL_PRI_MAX     19                       /* pri > 19 clamped to 19 */
#define ZPL_NSLOTS      (ZPL_PRI_MAX - ZPL_PRI_MIN + 1)

#ifdef ZPL_BUILD
#  define ZPL_API __declspec(dllexport) __stdcall
#else
#  define ZPL_API __declspec(dllimport) __stdcall
#endif

typedef struct ZPL_Sites {
    uint32_t list_ctor;      /* 0x00408CC0  std::list ctor, thiscall ECX=list    */
    uint32_t list_dtor;      /* 0x0040ABF0  std::list dtor, thiscall ECX=list    */
    uint32_t list_erase;     /* 0x00409BA0  list::erase(begin,end)               */
    uint32_t xml_draw;       /* 0x00405AF0  thiscall(ECX=HoleMgr, g, prio) ret 8 */
    uint32_t draw_image;     /* 0x0044D470  Graphics::DrawImage ret 0xC          */
    uint32_t set_draw_mode;  /* 0x00447890  Graphics::SetDrawMode ret 4          */
} ZPL_Sites;

#ifdef __cplusplus
extern "C" {
#endif

void  ZPL_API ZPL_Init(const ZPL_Sites *sites);

/* --- Lifecycle & Load (dipanggil dari cave HoleMgr::LoadLevel) --- */
void  ZPL_API ZPL_OnLoadBegin(void *holemgr);                 /* reset list pri>=10 milik holemgr */
void  ZPL_API ZPL_OnHoleMgrDestroy(void *holemgr);            /* dtor cleanup milik holemgr       */
void *ZPL_API ZPL_GetListBase(void *holemgr, int pri);        /* B: B+4 == std::list pri          */

/* --- Render --- */
void  ZPL_API ZPL_DrawHigh(void *level, void *graphics);      /* gameplay + state 6               */
void  ZPL_API ZPL_DrawLow(void *level, void *graphics);       /* vanilla layers 0..4 (all cutouts)*/
void  ZPL_API ZPL_DrawBake(void *holemgr, void *graphics);    /* gempa: vanilla 0..4 + high       */
void  ZPL_API ZPL_OnStateDraw(int state);                     /* diagnostic logger: log mState change */

/* --- Interop zumawebpsprite.dll --- */
int   ZPL_API ZPL_GetPriRange(int *lo, int *hi);              /* return jumlah slot               */
void *ZPL_API ZPL_GetList(void *holemgr, int pri);            /* std::list* (layout vanilla) / NULL */

/* --- Milestone 2 (Full Affinity 33 Blend Modes Engine) --- */
/* Strictly sequential IDs (0..32) grouped by Category */
enum {
    /* Category 1: Normal / Standard */
    ZPL_BLEND_NORMAL          = 0,
    ZPL_BLEND_PIGMENT         = 1,

    /* Category 2: Darken */
    ZPL_BLEND_DARKEN          = 2,
    ZPL_BLEND_MULTIPLY        = 3,
    ZPL_BLEND_COLOR_BURN      = 4,
    ZPL_BLEND_LINEAR_BURN     = 5,
    ZPL_BLEND_DARKER_COLOR    = 6,

    /* Category 3: Lighten */
    ZPL_BLEND_LIGHTEN         = 7,
    ZPL_BLEND_SCREEN          = 8,
    ZPL_BLEND_COLOR_DODGE     = 9,
    ZPL_BLEND_ADD             = 10,
    ZPL_BLEND_LIGHTER_COLOR   = 11,

    /* Category 4: Contrast */
    ZPL_BLEND_OVERLAY         = 12,
    ZPL_BLEND_SOFT_LIGHT      = 13,
    ZPL_BLEND_HARD_LIGHT      = 14,
    ZPL_BLEND_VIVID_LIGHT     = 15,
    ZPL_BLEND_LINEAR_LIGHT    = 16,
    ZPL_BLEND_PIN_LIGHT       = 17,
    ZPL_BLEND_HARD_MIX        = 18,

    /* Category 5: Comparative */
    ZPL_BLEND_DIFFERENCE      = 19,
    ZPL_BLEND_EXCLUSION       = 20,
    ZPL_BLEND_SUBTRACT        = 21,
    ZPL_BLEND_DIVIDE          = 22,

    /* Category 6: Component / HSL */
    ZPL_BLEND_HUE             = 23,
    ZPL_BLEND_SATURATION      = 24,
    ZPL_BLEND_COLOR           = 25,
    ZPL_BLEND_LUMINOSITY      = 26,

    /* Category 7: Specialized */
    ZPL_BLEND_AVERAGE         = 27,
    ZPL_BLEND_NEGATION        = 28,
    ZPL_BLEND_REFLECT         = 29,
    ZPL_BLEND_GLOW            = 30,
    ZPL_BLEND_CONTRAST_NEGATE = 31,

    /* Category 8: Utility / Masking */
    ZPL_BLEND_ERASE           = 32,

    ZPL_BLEND_MAX             = 32
};

void  ZPL_API ZPL_DrawNode(void *node, void *g, void *img, int x, int y);
int   ZPL_API ZPL_HwBlendHook(void *dev);

#ifdef __cplusplus
}
#endif

#endif /* ZUMAPRIOLAYER_H */
