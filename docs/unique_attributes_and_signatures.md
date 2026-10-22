# AOB Signature Catalog & Unique Attributes — Zuma Deluxe (v1.0.0.1)

This document catalogs all dynamic byte patterns (AOB signatures), internal PopCap Framework object offsets, and coordinate calculation formulas used by the ZuCompleluxe resolution and sidecar system. The goal is to ensure that future reverse engineering and extension work never needs to rediscover signatures or rely on rigid, hardcoded addresses (`0x004xxxxx` / `0x005xxxxx`).

---

## 1. Core Principle: Zero Hardcoded Addresses

All runtime patches in `src/zumareshandler.dll.c` must use:
1. **Dynamic AOB Scanning (`FindPattern`)** — Locates a 100% unique instruction sequence or constant in the executable sections (`.text`, `.rdata`).
2. **Relative Reference Resolution** — Obtains function/object pointers through machine instructions (e.g. dereferencing `g_pApp` from opcode `A1 ?? ?? ?? ??`, or reading a VTable slot directly from the runtime vtable pointer).
3. **Idempotency** — Reads the existing value from memory before calling `VerifyAndPatchDword`, making it safe to re-apply on every `DLL_PROCESS_ATTACH` without a mismatch.

---

## 2. Core Engine & Global Symbols

| Symbol / Component | AOB Signature (Hex Pattern) | Description & Resolution |
| :--- | :--- | :--- |
| **Global App Pointer (`g_pApp`)** | `A1 ?? ?? ?? ?? 8B 88 C0 04 00 00` | Reads the pointer address from offset `+1` of a `mov eax, [g_pApp]` opcode. |
| **`Graphics::FillRect`** | `55 8B EC 83 EC 20 56 8B F1 8B 46 2C 85 C0 74 44` | Entry point of the solid-rect fill primitive. Used in background pre-clear hooks and transitions. |
| **`MemoryImage::MemoryImage(app)`** | `8B 4D 08 33 C0 56 89 45 FC C7 06 ?? ?? ?? ?? 89 4E 40` | Offset `-0x24` from the match is the constructor entry point. Used to create the outscreen border image at runtime. |
| **`Dialog::Resize`** | `55 8B EC 8B 45 14 8B 55 0C 53 56 57 50 8B 45 08 8B F1 8B 4D 10 51 52 50 8B CE E8 ?? ?? ?? ?? 8B 4E 60 85 C9` | Base virtual `Resize` entry point for PopCap dialogs. |
| **`Dialog::vtable` Slot 13 (`Resize`)** | `8A 45 28 84 C0 C7 45 FC 00 00 00 00 C7 06` | The `Dialog` constructor sets the VTable at offset `+14`. Slot 13 (`+0x34 / 4`) is the `Resize` function. |
| **`CutsceneMgr::Draw` Switch Case** | `55 8B EC 8B 41 60 48 83 F8 05 77` | Cutscene switch statement. Offset `+0x20` is the `jmp hk_DrawCongratulations` (game-complete case). The original target is `CutsceneMgr::DrawCongratulations`. |

---

## 3. PopCap Framework Object Offsets

### 3.1 `Widget` Base Class Memory Layout

Every UI control (`ButtonWidget`, `Dialog`, `Screen`, `Board`) inherits this layout:

| Offset | Type | Field |
| :--- | :--- | :--- |
| `+0x00` | `void*` | Pointer to Virtual Method Table (VTable) |
| `+0x04` | `Widget*` | `mPrev` — previous sibling |
| `+0x08` | `Widget*` | `mNext` — next sibling |
| `+0x0C` | `Widget*` | `mParent` — parent widget |
| `+0x10` | `int` | `mX` — local X coordinate relative to parent |
| `+0x14` | `int` | `mY` — local Y coordinate relative to parent |
| `+0x18` | `int` | `mWidth` |
| `+0x1C` | `int` | `mHeight` |
| `+0x18` (byte) | `uint8_t` | `mVisible` (`1` = drawn, `0` = hidden) |
| `+0x19` (byte) | `uint8_t` | `mMouseVisible` (responds to mouse input) |

- **VTable Slot 11 (`0x2C`)**: `Widget::Draw(Graphics *g)`
- **VTable Slot 13 (`0x34`)**: `Widget::Resize(int x, int y, int width, int height)`

---

## 4. Screen Transitions & Slide Animation

### 4.1 Premature Button Appearance Bug

When transitioning from `MainMenu` to `AdventureScreen` or `GauntletScreen`, the PopCap engine calls `Widget::Resize(new_x, 0, w, h)` every frame, where `new_x` interpolates from `-mWidth` toward `0`.

- **Root Cause**: Earlier hooks passed `WidgetResize(btn, PARENT_LOCAL_X(...), PARENT_LOCAL_Y(...))` without incorporating the parent screen's `x` argument. Buttons snapped to their final positions at frame 0 while the background was still sliding in.
- **Fix**:
  1. Button offsets must include the incoming `x` and `y`:
     `x + PARENT_LOCAL_X(...)` and `y + PARENT_LOCAL_Y(...)`
  2. Button visibility is locked to the completed-animation condition:
     ```c
     char vis = (x == 0);
     *(char *)((char *)btn + 0x18) = vis; /* mVisible */
     *(char *)((char *)btn + 0x19) = vis; /* mMouseVisible */
     ```
     Buttons are fully hidden during the slide-in and only appear when `x == 0`.

### 4.2 Child Widget Offsets by Screen

#### `AdventureScreen`:
- `+0x5C`: "Main Menu" button (`ButtonWidget*`, vanilla pos: 0, 438)
- `+0x60`: "Play" button (`ButtonWidget*`, vanilla pos: 542, 441)
- `+0x74`: Temple door array (12 elements @ 28 bytes each)
- Hook VTable: `fnAdvResize` (`55 8B EC 8B 45 14 53 8B 5D 08 56 57 8B 7D 0C 8B F1 8B 4D 10 50 51 57 53 8B CE E8 ?? ?? ?? ?? 8B 46 24`)

#### `GauntletScreen`:
- `+0x5C`: "Back" button
- `+0x60`: "Practice" button
- `+0x64`: "Survival" button
- `+0x68`: "Main Menu" button
- `+0x6C`: "Play" button
- `+0x70`: "Next" button
- Hook VTable: `fnGauntResize` (`55 8B EC 8B 45 14 53 8B 5D 0C 56 57 8B 7D 08 8B F1 8B 4D 10 50 51 53 57 8B CE E8 ?? ?? ?? ?? 8B 4E 60`)

---

## 5. Sky Backdrop & 4:3 Letterbox Presentation

### 5.1 4:3 Fit Letterbox Patches

To preserve the native 4:3 aspect ratio without distortion, sky and overlay dimensions are clamped to the game canvas (`content_w` × `content_h`), not the full target window resolution (`TARGET_W` × `TARGET_H`):

| Patch Target | AOB Signature | Dimensions | Purpose |
| :--- | :--- | :--- | :--- |
| **`pSkyFill`** | `8D 55 F0 52 8B CE E8 ?? ?? ?? ?? 68 E0 01 00 00 68 80 02 00 00 6A 00 6A 00 8B CE` | `H=content_h`, `W=content_w` | Scrolling cloud sky background, clamped to 4:3 during transitions |
| **`pMMOverlayClip`** | `68 E0 01 00 00 68 80 02 00 00 53 53 8B CF E8` | `H=content_h`, `W=content_w` | MainMenu additive fade clip — prevents bleed into pillarbox margins |
| **`pMMOverlayResize`** | `68 E0 01 00 00 68 80 02 00 00 51 89 46 58` | `H=content_h`, `W=content_w` | Main menu overlay widget bounding box |
| **`pMMOverlayFill`** | `68 E0 01 00 00 68 80 02 00 00 6A 00 6A 00 8B CF` | `H=content_h`, `W=content_w` | Main menu overlay fill rect |

---

## 6. In-Game & Gameplay Clamps

| Element | AOB Signature | Description |
| :--- | :--- | :--- |
| **Star Kill Threshold (`pStarKillThreshold`)** | `8B 38 3B 7E 60 74 49 D9 47 10 D8 47 0C D9 57 0C D8 1D ?? ?? ?? ?? DF E0` | Prevents space-level (Stage 13-1) star particles from being deleted at Y=480. Patch at offset `+18` (`D8 1D [ptr]`) redirects to `s_star_max_y = (float)TARGET_H`. |
| **Frog Perspective Desync (`pFrogScaleOffsets`)** | `D9 45 0C D8 25 ?? ?? ?? ?? 83 EC 08 D9 05` | Eliminates 50px/100px frog shift during the ending letter-writing cutscene. |
| **Stage 13 Frog Zoom Target (`pFrogZoomTarget`)** | `B9 40 01 00 00 BB C8 00 00 00 89 4D F8 89 5D FC` | Adjusts the frog fly-to-darkness animation endpoint to the center of the target resolution. |
| **Credits Whatever Button H/X** | `BB E0 01 00 00 2B D8 53 68 FE 01 00 00` | Positions the "Whatever" close button at the bottom of the 4:3 letterbox canvas. |
| **Credits Text Center X** | `FF 50 1C 8B 4D 08 99 2B C2 D1 F8 BA 40 01 00 00` | Aligns the scrolling credits text to `content_w / 2`. |
| **Credits Scroll Start Y** | `C6 45 FC 01 C7 46 5C E0 01 00 00` | Begins the credits scroll from just below the active screen bottom (`TARGET_H + 20`). |
| **Credits Scroll Reset Y** | `83 FF 9C 7D 0A 8B 4D ?? C7 41 5C 08 02 00 00` | Fixes the seamless loop: patches the hardcoded vanilla reset value (`520`, valid only for 480px height) to `TARGET_H + 20`. Without this patch, credits "pop in" above the bottom edge on non-480px resolutions. Patched at offset `+11` (DWORD). |

---

## 7. Developer Quick Reference

When adding a new feature or repositioning a UI element:

1. **Never embed a constant hex address** (`0x004xxxxx` / `0x005xxxxx`) as a direct pointer or function argument.
2. Find the instruction byte signature using Capstone / Ghidra / AOB scanning, using `??` wildcards for rel32 operands and pointers.
3. Add the new signature to this document in the relevant table.
4. Use the established dynamic helpers: `FindPattern()`, `GetAppPtr()`, `VerifyAndPatchDword()`, `VerifyAndPatchCall()`.

---

## 8. WebP Rendering & Blit Compatibility (`zumawebp.dll`)

### 8.1 Native 1:1 Pixel Mapping

All WebP graphic assets (stage backgrounds and sprite/cutout layers) **must be blitted at their native resolution** without interpolation or stretching, consistent with other image formats (PNG/JPG).

- **Root Cause (historical bug)**: The `blit()` function performed resampled stretching (`srow[(int64_t)x * sw / dw]` and `y * sh / dh`) when the destination size differed from the source. On custom resolutions (e.g. a 1200×600 board canvas), a background WebP at 800×600 was incorrectly stretched, while a 800px-wide sprite was drawn 1:1. This caused severe visual seams at X=800.
- **Fix specification**:
  1. Copy dimensions are strictly clamped: `copy_w = min(sw, dw)`, `copy_h = min(sh, dh)`.
  2. Each row is copied 1:1 (no index scaling).
  3. **Sprite mode (`keep_alpha = 1`)**: Direct 32-bit ARGB copy. Remaining area is zeroed to fully transparent (`0x00000000`).
  4. **Background mode (`keep_alpha = 0`)**: Each pixel is passed through `flat()` (composite over black). Canvas area outside the WebP bounds is filled with solid black (`0xFF000000`) as a clean letterbox/pillarbox fill.

---

## 9. Background Pre-Clear & Alpha=0 Blackout (`zumareshandler.dll`)

### 9.1 VRAM Leak Prevention

The PopCap engine never clears the backbuffer automatically between frames, assuming the background image always covers 100% of the 640×480 screen. With custom resolutions:

1. Standard level backgrounds (e.g. `spiral.jpg`) only cover the top-left portion of the `mBackground` canvas.
2. The remaining region on the right (`X=640..TARGET_W`) and bottom (`Y=480..TARGET_H`) retains stale VRAM content from the previous screen, which bleeds through every frame.
3. Sprites with `alpha=0` pixels blitted without a black background leave the previous frame's pixels intact, producing stacked visual artifacts.

### 9.2 Solution

1. **Pre-clear `mBackground` in `Level::Load` (`pLevelBgGfx`)**:
   - AOB: `8B 43 4C 50 8D 8D 58 FF FF FF E8` (offset `+10` is `call Graphics::Graphics(mBackground)`).
   - Hook `hk_LevelBgGraphics`: immediately after the `Graphics` object for `mBackground` is created, the entire `TARGET_W × TARGET_H` canvas is filled with solid black (`0xFF000000`) via `FillRect`. All out-of-artwork areas and `alpha=0` pixels are now permanently clean black.
2. **Root backbuffer clearing in `hk_rootgfx`**:
   - At the start of every frame, the full-screen canvas is cleared to solid black (`0xFF000000`) via `FillBlack(g, 0, 0, W, H)` before any widget is drawn. Any transparency or empty area is now a pure black background with no stacking errors.

---

## 10. Fail-Soft Architecture (`zumareshandler.dll`)

The resolution handler (`modules/zumareshandler.dll`) is always linked into `Zuma.exe` via the `.zres` section, regardless of target resolution.

### Key Guarantees

1. **Always Active (Even at 640×480)**:
   - `Zuma.exe` is always linked with `.zres`, which references `modules\zumareshandler.dll`.
   - At vanilla 640×480, `zumareshandler.dll` remains active and provides:
     - Pre-clearing `mBackground` level with solid black (`hk_LevelBgGraphics`) to prevent VRAM leaks.
     - Root backbuffer management and black-wipe for unused / `alpha=0` areas.
     - 1:1 element placement without `imagescustomres/` redirection.

2. **Fail-Soft Guarantee (Zero Crash if DLL is Missing)**:
   - The dynamic trampoline inside `.zres` reads the `P->slot` field.
   - If `zumareshandler.dll` is removed from `modules/`, the trampoline detects `P->slot == 0` and immediately falls back (`jmp orig`) to the original PopCap engine routines (DirectX Init, DirectDraw Init, Graphics ctor, Screen Draw).
   - The game runs normally at 640×480 with no crash whatsoever.
