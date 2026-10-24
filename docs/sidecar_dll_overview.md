# Sidecar DLL Overview

ZuCompleluxe injects ten modular sidecar DLLs into Zuma Deluxe (v1.0.0.1) at runtime. Modules are loaded cleanly via section `.zres` and the dynamic module loader stub (`.zldr` in `.zdata`). The sections are synthesized and injected by the Python builder pipeline in `builder/`.

> [!NOTE]
> The `modules/` directory (compiled output) is excluded from version control. Run `build_full.bat` to compile all ten sidecars, or `build_lite.bat` for the lightweight 6-sidecar edition.

---

## Sidecar DLL Table

| DLL | Source | PE Section | Key Exports | Purpose |
|---|---|---|---|---|
| `zumadllchecker.dll` | `src/zumadllchecker.dll.c` | `.zdata` (`.zldr`) | `ZDC_RunChecker` | Pre-launch diagnostic dialog. Inspects all sidecars in `modules/`, reports integrity status (`[OK] Required` / `[OK] Present` / `[DISABLED]`), shows aspect ratio & resolution, provides interactive checkbox toggles, and launches Zuma Deluxe. |
| `zumawebpbg.dll` | `src/zumawebpbg.dll.c` | `.anim`, `.zdata` | `Anim_PreDraw`, `Anim_LoadImage` | Animated WebP level background engine. Streams and loops `levels/<id>/bg.webp` at native 1:1 pixel aspect ratio onto the level background surface, and intercepts PopCap ImageLib. |
| `zumawebpsprite.dll` | `src/zumawebpsprite.dll.c` | `.zdata` (`.zldr`) | `ZSP_AnimateSprites`, `ZSP_IsActive` | Multi-layer animated sprite engine. Traverses `Level` linked lists across layers 0..4, resolves sprite coordinates and priorities from `LevelDesc`, and renders alpha-preserving WebP animations. |
| `zumaextrapath.dll` | `src/zumaextrapath.dll.c` | `.zep`, `.zep2`, `.zdata` | `ZEP_BindExtras`, `ZEP_LevelStart`, `ZEP_AddHole`, `ZEP_ExtraCount` | Multi-path curve engine (curve4..curve10). Synchronizes 96-byte runtime curve settings with `LevelDesc`, manages up to 10 physical skulls in `HoleMgr`, and activates curve4..10 runtime hooks. |
| `zumaextracolor.dll` | `src/zumaextracolor.dll.c` | `.zcol`, `.zdata` | `ZEC_ExtractAll`, `ZEC_GetBallImage`, `ZEC_ApplyColors` | 11-ball color pipeline engine. Expands the vanilla 4-to-6 color system to 11 full colors (adding Orange, Navy, Cyan, Black, Cream), managing color pools, palettes, and powerups. |
| `zumacustommap.dll` | `src/zumacustommap.dll.c` | `.zcm`, `.zdata` | `zcm_AdjustCount`, `zcm_PickGauntletLevel`, `zcm_GetSrcId` | Gauntlet custom map extender. Enables arbitrary user-created maps in Gauntlet mode (up to 256 custom maps) beyond the vanilla set. |
| `zumacplxcredit.dll` | `src/zumacplxcredit.dll.c` | `.zcc`, `.zdata` | `ZCC_GetCreditsTable`, `ZCC_PreDrawCreditLine`, `ZCC_OnMouseDown` | Developer credits screen mod. Adds extended credits entries (entries 39–41 including an interactive GitHub button), intercepts the credits draw loop, and preserves `__thiscall` `ecx`/`edx` registers in Cave C. |
| `zumatestingkit.dll` | `src/zumatestingkit.dll.c` | `.ztk`, `.zdata` | `ZTK_IsActive`, `ZTK_OnUserDialogInit`, `ZTK_DrawHelpScreen` | Developer testing kit & Secret Temple 5. Provides dev HUD, middle-click score differential popup, stage selector, unlock cheat, Secret Temple 5 gauntlet doors, and custom 2-column DevTester Guide. |
| `zumadisplay.dll` | `src/zumadisplay.dll.c` | `.zdata` (`.zldr`) | `ZDISP_IsActive`, `ZDISP_ScreenToGameCoords` | Display & window driver. DirectDraw windowed mode initialization (DDSCL_NORMAL), F11 & Alt+Enter borderless fullscreen toggle, 3D acceleration preservation (`mIs3D`), native 32-bit ARGB GPU Hardware Cursors (`HCURSOR`), PrimarySurface::Blt letterbox fit blit hook, ScreenToClient/ClientToScreen remapping, and outscreen stone border rendering. |
| `zumareshandler.dll` | `src/zumareshandler.dll.c` | `.zres` (`[RX]`), `.zdata` (`[RW]`) | `ZRH_IsActive` | Resolution & UI layout patcher. Dynamically scales viewports to any resolution >= 480x320 via AOB memory scanning across 20+ screens, modal dialog universal centering (`SetupDialog`), board clip rect expansion, stone menubar HUD margin synchronization, and dynamic asset redirection (`imagescustomres/` with automatic fallback to `images/`). Mutable `ZTable` lives at `.zdata + 0x1800` while `.zres` is strictly `[RX]`. |

---

## Architecture Notes

### Separation of Concerns: Display Driver vs UI Layout Patcher
In v0.5, resolution management is decoupled into two specialized modules:
- **`zumadisplay.dll`**: Low-level window and DirectDraw interface driver. Manages window messages, fullscreen state, 3D hardware acceleration preservation, GPU hardware cursor handling, primary surface stretch blits, coordinate transformation mathematics, and outscreen brick border rendering.
- **`zumareshandler.dll`**: Engine layout patcher and asset router. Modifies screen coordinates in engine memory via AOB scanning, expands clip rects, centers dialogs, repositions in-game HUD elements, and manages the dynamic `imagescustomres/` asset redirection with transparent fallback to `images/`.

### Separation of Concerns: Backgrounds vs Multi-Layer Sprites
WebP animation rendering is decoupled into:
- **`zumawebpbg.dll`**: Level background animation (`bg.webp`), native 1:1 scale decoding, and PopCap ImageLib asset loading interception.
- **`zumawebpsprite.dll`**: Multi-layer sprite playback across sprite layers 0..4, `LevelDesc` coordinate matching, and alpha transparency blending.
- Both WebP modules link against the bundled 32-bit `libwebp` static archives in `builder/libwebp/lib/` (`libwebpdemux.a` and `libwebpdecoder.a`), supporting both MinGW32 GCC and Zig CC out-of-the-box.

### Build Variants
- **`build_full.bat`** (Full Edition) — Compiles all 10 sidecars, applies all binary PE patches, generates assets, and verifies PE structural integrity.
- **`build_lite.bat`** (Lite Edition) — Compiles 6 core sidecars (`zumadllchecker.dll`, `zumacustommap.dll`, `zumacplxcredit.dll`, `zumatestingkit.dll`, `zumadisplay.dll`, `zumareshandler.dll`). Excludes the 4 content/heavy sidecars (`zumawebpbg.dll`, `zumawebpsprite.dll`, `zumaextrapath.dll`, `zumaextracolor.dll`). Supports arbitrary custom resolution and borderless fullscreen.

### Compiler Support
- Primary: **MinGW32 GCC** (`gcc -m32 -O2 -s -shared -static-libgcc`)
- Fallback: **Zig CC** (`python -m ziglang cc -target x86-windows-gnu -O2 -s -shared`)
