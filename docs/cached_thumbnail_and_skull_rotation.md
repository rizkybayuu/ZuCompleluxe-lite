# Cached Thumbnail Generation & Skull Hole Rotation Architecture

This document provides a comprehensive technical reference for the cached map thumbnail generation system, skull hole rendering mechanics, rotation resolution algorithms, and the reverse-engineered patches implemented in **ZuCompleluxe** (`zumareshandler.dll`).

---

## 1. Background & Architecture Overview

In Zuma Deluxe, levels display a 320×240 preview thumbnail in the Gauntlet level selection menu. These thumbnails are stored on disk in `levels/cached_thumbnails/<id>.png`. 

If a thumbnail does not exist when a level is registered or previewed, the engine dynamically invokes `Board::CreateCachedThumbnail` (`0x00418C40`), which calls `CreateCachedThumbnail` (`0x0040CBB0`).

### High-Level Thumbnail Pipeline
1. **Canvas Construction**: An offscreen canvas `MemoryImage` (default 800×600 or custom 1200×675 in ZuCompleluxe) and corresponding `Graphics` context are instantiated.
2. **Background Rendering**: The level's background image is loaded and blitted onto the canvas.
3. **Hole Rendering**: The level's `HoleMgr` (`0x00407250`) renders skull holes at the terminus of all active curves.
4. **Downscaling & File Export**: The canvas is stretched down to 320×240 via `Graphics::DrawImageStretch` (`0x0044D5F0`) and saved to PNG format via `WritePNGImage` (`0x00499660`).

---

## 2. Root Cause Analysis: Missing Skull Holes

When sidecar DLLs like `zumaextrapath.dll` were introduced to support unlimited dynamic paths (e.g. 8+ curves per map), custom and regenerated thumbnails appeared completely empty—displaying only the background tracks without any skull holes or covers.

### Why Holes Disappeared
1. In `HoleMgr::Draw` (`0x00407250`), `zumaextrapath.dll` installed a detour hook (Cave M at `0x0040727C -> 0x005902C0`).
2. When custom layered hole priority checking is enabled (`va_ptr_checkholeprio != 0`), Cave M jumps straight to `0x004072F7`, bypassing the native hole drawing loop entirely. This was done so `zumaextrapath.dll` could perform layered drawing inside `Board::Draw`.
3. However, `CreateCachedThumbnail` only calls `HoleMgr::Draw` and **never** calls `Board::Draw`. As a result, the hole drawing loop was skipped, leaving thumbnails completely devoid of skull holes.

---

## 3. Skull Hole Rotation Resolution Algorithm

Zuma Deluxe calculates skull hole positions and orientations inside `Board::InitLevel` / `Curve::SetupHole` (`FUN_00416050` & `FUN_00402860`):

```
                   +-----------------------------------+
                   | Read <Graphics> / <Level> in XML  |
                   +-----------------------------------+
                                     |
                                     v
                        Is "skullrot" attribute defined?
                               /           \
                             YES            NO (< 0)
                             /                \
                            v                  v
         +--------------------------+   +------------------------------------+
         | Fixed Angle Mode         |   | Fallback to Curve Tangent Mode     |
         | rot = skullrot * (PI/180)|   | Sample final curve waypoint tangent|
         |                          |   | rot = curve.points[last].mTangent  |
         +--------------------------+   +------------------------------------+
                            \                  /
                             \                /
                              v              v
                   +-----------------------------------+
                   | HoleMgr::AddHole(curve, x, y, rot)|
                   | Stored into mHoles[i].mRotation   |
                   | Offset: HoleMgr + 0xA0 + i * 0x38 |
                   +-----------------------------------+
```

### 1. Fixed Angle Mode (`skullrot` attribute)
- Defined in `levels/levels.xml` on `<Graphics>` elements, for example:
  - `<Graphics id="serpents" ... skullrot="0" />`
  - `<Graphics id="tripleserpent" ... skullrot="0.0" />`
- If `skullrot >= 0.0f`, the value in degrees is converted directly to radians:
  $$\text{rot} = \text{skullrot} \times \frac{\pi}{180.0}$$

### 2. Automatic Curve Tangent Fallback Mode
- When `skullrot` is omitted or set to negative (`< 0`), the engine queries the curve object:
  ```c
  if (param_1 < 0.0f) {
      // pvVar1 is Curve pointer
      int numPoints = (curve->mPointsEnd - curve->mPointsStart) / sizeof(CurvePoint);
      if (numPoints > 0) {
          // Tangent angle of the last point entering the pit
          param_1 = curve->mPointsEnd[-1].mTangent; // offset -0x0C
      }
  }
  ```
- This ensures the skull face naturally aligns with the incoming marble path (e.g., in spiral loops, snake pits, or multidirectional curves).

---

## 4. The 2D Software Fallback Bug (Why Skulls Appeared Unrotated)

Even though the engine correctly computed `mRotation` (e.g. `3.588`, `1.571`, `5.698` radians), generated thumbnails still rendered all skulls facing strictly upright (rotation 0).

### Decompiled `HoleMgr::DrawHole` (`0x00404D50`)
```c
void __thiscall HoleMgr::DrawHole(void *this, Graphics *g, int hole_idx) {
    int iVar2 = hole_idx * 0x38;
    ...
    // Check 3D hardware acceleration
    if (!gApp->Is3DAccelerated()) {
        // [LEGACY 2003 SOFTWARE FALLBACK]
        // Bypasses rotation completely to save CPU on Pentium III processors!
        DrawImage(g, IMAGE_HOLE, hole->mX, hole->mY);
        DrawImageCel(g, IMAGE_HOLE_COVER, hole->mX, hole->mY, hole->mCel);
        return; // <--- Discards mRotation!
    }

    // [3D ACCELERATED / ROTATED PIPELINE]
    double rot = (double)hole->mRotation;
    Graphics::DrawImageRotated(g, IMAGE_HOLE, hole->mX, hole->mY, rot, NULL);
    Graphics::DrawImageRotated(g, IMAGE_HOLE_COVER, hole->mX, hole->mY, rot, &rect);
}
```

### The Issue
- When rendering offscreen thumbnails, the canvas is a software `MemoryImage`. `gApp->Is3DAccelerated()` evaluates to `false` (or 0).
- The function unconditionally branches to the unrotated 2D legacy path at `0x00404D9B` (`75 37` - `JNZ 0x00404DD4`).
- **However**, `MemoryImage` in SexyAppFramework fully implements software texture rotation via `MemoryImage::BltRotated` (`0x0044E3B0` at vtable `+0x20`). Software rotation is fast, stable, and accurate on modern CPUs.

---

## 5. Reverse-Engineered Patches in `zumareshandler.dll`

To solve this completely across both thumbnails and gameplay, `zumareshandler.dll` applies two complementary hooks:

### Patch A: `HoleMgr_ForceRotatedDraw`
- **Location**: `0x00404D9B`
- **Original Bytes**: `84 C0 8B 7D 08 75 37 8B 46 10` (`75 37` = `JNZ +0x37`)
- **Patched Bytes**: `84 C0 8B 7D 08 EB 37 8B 46 10` (`EB 37` = `JMP +0x37`)
- **Effect**: Unconditionally forces `HoleMgr::DrawHole` to route through the rotated drawing pipeline (`Graphics::DrawImageRotated` at `0x004515C0`), correctly applying `mRotation` (from either curve tangent or `skullrot`) even on software `MemoryImage` canvases.

### Patch B: `CachedThumbnailScale` Hook (`hk_cached_thumbnail_scale`)
- **Hook Site**: `0x0040CD8E` (`call Graphics::DrawImageStretch`)
- **Inspection**:
  - `img` is located at `[ebp - 0x100]`. Caller EBP is obtained via `caller_ebp = (char *)img + 0x100`.
  - `level` (`HoleMgr`) is at `[caller_ebp + 8]`.
  - `gCanvas` is at `[caller_ebp - 0x1B0]`.
- **Hole Rendering**:
  Iterates `i = 0 .. num_holes - 1` and calls `HoleMgr::DrawHole(level, gCanvas, i)`. With Patch A active, both the sun disc and closed skull cover are drawn rotated to the exact curve tangent angle before downscaling.
- **Aspect-Fill Blit**:
  Crops the source widescreen canvas (1200×675, 16:9) to standard 4:3 before downscaling to 320×240, eliminating horizontal stretching and distortion.

---

## 6. Safe Automated Batch Thumbnail Generator

To regenerate outdated or missing thumbnails without running into access violations or corrupting active gameplay state:

1. **Trigger**: Place an empty flag file named `gen_thumbs.flag` in the game root directory.
2. **Execution Timing**: After 90 frames of `DrawScreen` have elapsed (guaranteeing that all XMLs, resources, and SexyAppFramework sound/sprite pools are fully loaded).
3. **Headless Staging**:
   - Traverses `LevelMgr` descriptors (`vector<LevelDesc>` at `ZumaApp + 0x608`).
   - Allocates an isolated `temp_board` via `malloc(0x400)`.
   - Initializes it using `Board::Board(temp_board, app)` (`0x004186D0`).
   - Loops through every `LevelDesc`, setting `temp_board->mLevelDesc` and calling `Board::CreateCachedThumbnail` (`0x00418C40`).
   - Cleans up and deletes `gen_thumbs.flag`.

---

## 7. Verification Results

| Thumbnail | Curves | Rotation Source | Visual Result |
| :--- | :--- | :--- | :--- |
| `spiralof8.png` | 8 curves | Curve Tangent Fallback | All 8 golden skulls arranged in a radial wheel, each tilted along its curve's tangent. |
| `triplesnakepit.png` | 3 curves | Curve Tangent Fallback | All 3 skulls rotated to follow their respective inward spiral tracks. |
| `tripleblackswirley.png` | 3 curves | Curve Tangent Fallback | All 3 skulls swirling along the vortex trajectory. |
| `tripleserpent.png` | 3 curves | `skullrot="0.0"` in XML | Center pit rendered with upright golden skull cover matching XML specification. |
| `longrangeadv.png` | 4 curves | `skullrot="0.0"` in XML | Top skull rendered upright with golden rays matching XML specification. |
