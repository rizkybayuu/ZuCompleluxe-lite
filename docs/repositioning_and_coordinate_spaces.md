# Comprehensive Guide: Coordinate Spaces, Element Repositioning, and Aspect Ratio Handling in Zuma Deluxe

This document provides a thorough architectural explanation of the custom resolution system, graphics and input coordinate spaces, the exact mathematical reasons why 4:3 worked while non-4:3 (widescreen) previously broke, and the master catalog of all visual elements (images, buttons, text, dialogs, and HUD).

---

## 1. Why Did 4:3 Work Perfectly While 16:9 / Widescreen Broke?

### 1.1 The Original 4:3 Baseline ($640 \times 480$)

Zuma Deluxe was built natively for $640 \times 480$, which is a strict **4:3** aspect ratio:
$$\frac{640}{480} = 1.3333... = \frac{4}{3}$$

When adapting to an arbitrary target resolution $(W, H)$, the scaling factor and centering margins are calculated as:

$$\text{scale} = \min\left(\frac{W}{640}, \frac{H}{480}\right)$$
$$\text{content\_w} = \text{round}(640 \times \text{scale}), \quad \text{content\_h} = \text{round}(480 \times \text{scale})$$
$$X_{\text{offset}} (xo) = \frac{W - \text{content\_w}}{2}, \quad Y_{\text{offset}} (yo) = \frac{H - \text{content\_h}}{2}$$

---

### 1.2 The 4:3 Scenario (e.g. $800 \times 600$)

In any 4:3 resolution:
- $\frac{W}{640} = \frac{800}{640} = 1.25$
- $\frac{H}{480} = \frac{600}{480} = 1.25$
- $\text{scale} = 1.25$
- $\text{content\_w} = 640 \times 1.25 = 800 = W$
- $\text{content\_h} = 480 \times 1.25 = 600 = H$
- **$X_{\text{offset}} (xo) = \frac{800 - 800}{2} = 0$**
- **$Y_{\text{offset}} (yo) = \frac{600 - 600}{2} = 0$**

> [!IMPORTANT]
> **THE HIDDEN TRUTH:**
> Because $xo = 0$ and $yo = 0$:
> $$\text{SX\_VAL}(v) = (v \times \text{scale}) + xo = (v \times \text{scale}) + 0 = \text{SDIM\_VAL}(v)$$
> In 4:3, there is **zero numerical difference** between a local coordinate and a global coordinate. Every coordinate simply scaled by $1.25$. Any double-offset bugs were invisible because $xo$ was zero ($A + 0 = A$).

---

### 1.3 The Non-4:3 / Widescreen Scenario (e.g. $1000 \times 600$ or 16:9)

In widescreen resolutions like $1000 \times 600$:
- $\frac{W}{640} = \frac{1000}{640} = 1.5625$
- $\frac{H}{480} = \frac{600}{480} = 1.25$
- $\text{scale} = \min(1.5625, 1.25) = 1.25$
- $\text{content\_w} = 640 \times 1.25 = 800\text{ px}$ (4:3 central game area)
- $\text{content\_h} = 480 \times 1.25 = 600\text{ px}$
- **$X_{\text{offset}} (xo) = \frac{1000 - 800}{2} = \mathbf{100\text{ px}}$** (100px left and right pillarbox margins)
- **$Y_{\text{offset}} (yo) = \frac{600 - 600}{2} = \mathbf{0\text{ px}}$**

```
+-----------------------------------------------------------------------------+
| Full Target Window: W = 1000 px, H = 600 px                                 |
|                                                                             |
| <--- xo = 100 px ---> | <--------- Central 4:3 Canvas (800 px) --------> | <-100px-> |
|                       |                                                |          |
|  OUTSCREEN BORDER     | (0, 0) of 4:3 Game Box                         | OUTSCREEN|
|  (Stone Brick Tiling) | [MainMenu / Adventure / Gauntlet]              | BORDER   |
|                       |                                                |          |
|                       |  All elements are scaled by `scale = 1.25`:    |          |
|                       |  X_game = v * scale                            |          |
|                       |                                                |          |
|                       |  Graphics context translates entire box:       |          |
|                       |  mTransX = xo (100 px)                         |          |
|                       |  Rendered on screen at: xo + v * scale         |          |
+-----------------------------------------------------------------------------+
```

When $xo > 0$, previously two bugs occurred simultaneously:
1. **Double Offset ($2 \times xo$)**: Hook H5 (`hk_rootgfx`) shifted root graphics canvas by $+xo$ (`mTransX += xo`). If an element position also added $+xo$ manually, it rendered at $xo + (v \cdot \text{scale} + xo) = \mathbf{2 \cdot xo} + v \cdot \text{scale}$ (shifted 200px instead of 100px!).
2. **Hitbox Mismatch**: Mouse input in H6 had $xo$ subtracted once. If visuals had double-offset, clicks landed 100px away from the visual button!

### 1.4 The Universal Fix: Making Non-4:3 Identical to 4:3

To make widescreen behave **identically to 4:3**:
1. All elements, buttons, doors, text, and hitboxes are placed at pure scaled coordinates:
   $$\text{SX\_VAL}(v) = \text{round}(v \times \text{scale})$$
   $$\text{SY\_VAL}(v) = \text{round}(v \times \text{scale})$$
   *(No manual $+xo$ or $+yo$ added to individual elements!)*
2. The entire 4:3 canvas is translated to the center by `hk_rootgfx`:
   $$\text{mTransX} = xo, \quad \text{mTransY} = yo$$
3. Mouse input is normalized by H6:
   $$X_{\text{game}} = X_{\text{window}} - xo, \quad Y_{\text{game}} = Y_{\text{window}} - yo$$
4. The remaining borders ($0 \dots xo$ and $xo+cw \dots W$) are filled with the `outscreen.jpg` brick pattern.

---

## 2. Why the Outscreen Brick Pattern Was Not Appearing and How It Is Fixed

When tiling `outscreen.jpg` outside the 4:3 screen area:
1. **The Missing `GetBits()` Call**:
   In PopCap's `MemoryImage`, calling `Create(160, 160)` initializes image header parameters and sets `mBits` (`[mem + 0x14]`) to `NULL`. The actual pixel buffer is only allocated when `GetBits()` (vtable slot 22 at offset `0x58`) is invoked:
   ```c
   typedef uint32_t *(THISCALL *fMemGetBits)(void *this);
   uint32_t *bits = ((fMemGetBits)vt[0x58 / 4])(mem);
   ```
   Previously, slot `0x58` was never called, leaving `mBits == NULL`. PopCap's `Graphics::DrawImage` exits on line 1 if `theImage->mBits == NULL`, causing zero pixels to be drawn!
2. **Fixed Allocation & Blitting**:
   Calling `fMemGetBits(mem)` allocates the 102,400 bytes ($160 \times 160 \times 4$), returns the valid pixel pointer, copies `s_outscreen_pixels` (the uncompressed ARGB values), and notifies the engine via `BitsChanged()` (slot `0x4C`).
3. **Dirty Rectangle Blitting**:
   `hk_drawscreen` draws `DrawOutscreenBorders` directly onto the backbuffer before `WidgetManager::DrawScreen` blits. With `ZRES_UNCLIP = 1`, `Unclip` sets `mIsTransparent = 0` on CursorWidget so `DrawScreen` blits the entire backbuffer surface to the display without perimeter cropping.

---

## 3. Master Catalog: Where Every Element & Image Is Defined and Patched

The system manages visual assets across **4 layers**:

### Layer 1: Image Resource Definitions (`properties/resources.xml`)
- Defines image IDs (e.g. `IMAGE_MAIN_MENU_ADVENTURE`, `IMAGE_ADVSKY`).
- Sets relative paths inside `images/`.
- Sets spritesheet dimensions (`cols` and `rows`).

### Layer 2: Asset Scalers (`builder/patch_customres_assets.py` & `builder/patch_assets.py`)
- `builder/patch_assets.py`: Copies raw unscaled assets directly to `<zuma_dir>/images/`:
  - `assets/media/outscreen.jpg` (160x160) $\to$ `<zuma_dir>/images/outscreen.jpg` (NOT resized).
  - Extra color balls and secret door 5 media.
- `builder/patch_customres_assets.py`: Reads `resources.xml` and scales UI backgrounds and menu buttons via Bicubic interpolation into `<zuma_dir>/imagescustomres/`.

### Layer 3: Filesystem Redirection Hook (`src/zumareshandler.dll.c`)
- `hk_CreateFileA` intercepts `kernel32!CreateFileA`.
- If an asset exists in `imagescustomres/`, requests to `images/` are transparently redirected to the high-res scaled version. If not (like `outscreen.jpg`), it loads from `images/`.

### Layer 4: Binary Memory Patches (`src/zumareshandler.dll.c`)
All hardcoded coordinates in `Zuma.exe` are located via AOB signatures at runtime and patched:

#### Loading Screen
| Element | Signature / Location | Value | Description |
|---|---|---|---|
| **Loading Bar Y** | `5D 01 00 00 68 80` | `SY_VAL(349)` | Vertical loading bar position |
| **Loading Bar X** | `80 00 00 00 89 4D E0` | `SX_VAL(128)` | Horizontal loading bar position |
| **"Click Here To Play" Y** | `AE 01 00 00 03` | `SY_VAL(430)` | Play button after loading completes |

#### MainMenu Screen
| Element | Signature / Location | Value | Description |
|---|---|---|---|
| **Button 1: Adventure** | `40 68 C4 01 00 00` | `X: SX_VAL(452), Y: SY_VAL(64)` | Adventure carved stone button |
| **Button 2: Gauntlet** | `99 00 00 00 68 B4` | `X: SX_VAL(436), Y: SY_VAL(153)` | Gauntlet carved stone button |
| **Button 3: Options** | `EC 00 00 00 68 A2` | `X: SX_VAL(418), Y: SY_VAL(236)` | Options carved stone button |
| **Button 4: More Games** | `31 01 00 00 68 8A` | `X: SX_VAL(394), Y: SY_VAL(305)` | More Games carved stone button |
| **Button 5: Quit Game** | `3B 01 00 00 68 F0` | `X: SX_VAL(496), Y: SY_VAL(315)` | Quit Game carved stone button |
| **Frog: Eye Center** | `BF 00 00 00 B9 4B` | `X: SX_VAL(191), Y: SY_VAL(331)` | Center pivot of frog eyes |
| **Frog: Sun Highlight** | `C1 6A BB 50` | `round(-63*scale), round(-69*scale)` | Sun specular reflection on head |
| **Frog: Eye Tracking Ray** | `C7 45 DC 00 60 6B 40` | Double precision vector scale | Dynamic gaze tracking cursor |
| **Frog: Eyelid Blink** | `F7 00 00 00 68 BC` | `X: SX_VAL(188), Y: SY_VAL(247)` | Eyelid animation sprite |
| **"Welcome User" Text** | `80 02 00 00 2B C1` | Centered against `content_w` | Player name display string |

#### AdventureScreen
| Element | Signature / Location | Value | Description |
|---|---|---|---|
| **Doors 1-12 Hitboxes** | `PopulateAdventureDoors` | `SX_VAL, SY_VAL` | Click collision boxes for temples 1-4 |
| **Temple Sprites 1-4** | `hk_adv_draw_temple1..4` | `SX_VAL, SY_VAL` | Temple background & door overlays |
| **Main Menu Button** | `hk_adventure_resize` | `X: SX_VAL(0), Y: SY_VAL(438)` | Return to Main Menu button |
| **Play Button** | `hk_adventure_resize` | `X: SX_VAL(542), Y: SY_VAL(441)` | Start temple button |
| **Highscore Title & Score** | `C8 01 00 00 52` | `SX_VAL(456), SX_VAL(575)` | High score leaderboard columns |
| **Stage Number & Name** | `68 8C 01 00 00 68 58` | `SX_VAL(344), SY_VAL(396)` | Current stage identifier |
| **Info Box** | `C7 45 90 14 00 00 00` | `X: SX_VAL(20), Y: SY_VAL(60)` | Lower stage information panel |

#### GauntletScreen
| Element | Signature / Location | Value | Description |
|---|---|---|---|
| **Gauntlet Doors 1-4** | `PopulateGauntletDoors` | `SX_VAL, SY_VAL` | Door hitboxes for gauntlet arenas |
| **Practice Button** | `hk_gauntlet_resize` | `X: SX_VAL(177), Y: SY_VAL(444)` | Practice mode selector |
| **Survival Button** | `hk_gauntlet_resize` | `X: SX_VAL(277), Y: SY_VAL(444)` | Survival mode selector |
| **Back Button** | `hk_gauntlet_resize` | `X: SX_VAL(390), Y: SY_VAL(444)` | Back button |
| **Main Menu Button** | `hk_gauntlet_resize` | `X: SX_VAL(10), Y: SY_VAL(422)` | Return to Main Menu button |
| **Next & Play Buttons** | `hk_gauntlet_resize` | `X: SX_VAL(505/506), Y: ...` | Next page and play buttons |
| **Map Thumbnail Preview** | `hk_draw_gauntlet_thumbnail`| `SDIM_VAL(320) x SDIM_VAL(240)` | Arena preview graphic |
| **Leaderboard Columns** | `BD 01 00 00 8D` | `SX_VAL(445), SX_VAL(575)` | High score names and points |

#### In-Game Stone Menubar (HUD)
| Element | Signature / Location | Dynamic Calculation | Description |
|---|---|---|---|
| **Frog Lives Counter** | `C7 45 EC 1C 00 00 00` | `lives_x = (hud_w <= 480) ? 26 : 28` | Top-left lives icon |
| **"STAGE X-Y" Text** | `B9 C8 00 00 00 2B C8` | Centered at pillar 2 (`c2_x + 66`) | Level display |
| **Player Score Text** | `B9 68 01 00 00 2B C8` | Right-aligned at pillar 3 (`c3_x + 90`) | Running score |
| **Zuma Progress Bar** | `6A 02 68 99 01 00 00` | Positioned at pillar 4 (`c4_x + 19`) | Zuma meter progress |
| **In-Game Menu Button** | `6A 03 68 1C 02 00 00` | Placed at pillar 5 (`c5_x + 22`) | Menu/Pause button |
| **Menubar Stone Bridges**| `DrawMenubarHUD` | Dynamic tile bridge (`a, b, c, d`) | Fills wide aspect menubar gap |

---

## 4. Summary

By keeping `SX_VAL(v) = SDIM_VAL(v) = round(v * scale)`, the game runs **identically to 4:3**, perfectly centered by `hk_rootgfx` translation, with `DrawOutscreenBorders` tiling `outscreen.jpg` seamlessly along all pillarbox margins.
