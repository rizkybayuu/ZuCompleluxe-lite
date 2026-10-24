# Changelog

All notable changes to the **ZuCompleluxe** framework will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.5.1.3] - 2026-10-24

### Fixed
- **Pure `[RX]` (`0x60000020`) Enforcement on `.zres` Section (`builder/patch_customres.py`, `src/zumareshandler.dll.c`, `builder/verify_pe.py`)**:
  - Eliminated the `0xE0000060` (`[RWX]`) section characteristic on `.zres` that triggered `[WARN] Section .zres is both WRITABLE and EXECUTABLE (W+X)` during `verify_pe.py --lite`.
  - Relocated the 72-byte mutable `ZTable` synchronization block (`SLOT` at `+0x18`, `TRIED` at `+0x1C`, and dynamic boundary floats/ints) from `.zres` into `.zdata + 0x1800` (`0xC0000040 [RW]`), while retaining read-only metadata (`magic = 0x53455A52`, `ztable_va` at `+0x04`, and `w, h` at `+0x48`) inside `.zres`.
  - Updated `.zres` section characteristics to pure `0x60000020` (`IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ` — `[RX]`).
  - Updated `LoadTable()` in `src/zumareshandler.dll.c` to resolve `ZTable` from `.zdata + 0x1800` first (with backward-compatible fallback to `.zres`).
  - Removed the `.zres` W+X exemption from `builder/verify_pe.py`, leaving only the original PopCap `GARR` section exempted.
- **Python Windows Shim Target Resolution & Zig Local Cache Hardening (`builder/bootstrap.bat`)**:
  - Hardened `:EnsurePython` to resolve `.exe.__target__` shim redirects (e.g. `AppData\Local\Python\bin\python.exe.__target__`) to the underlying real `python.exe` binary, preventing unquoted path-with-spaces spawn errors.
  - Added `ZIG_LOCAL_CACHE_DIR=%CD%\.build\zigcache` alongside `ZIG_GLOBAL_CACHE_DIR` to prevent `error: CacheCheckFailed` when invoking `python -m ziglang cc` in isolated environments.

### Changed
- **Diagnostic Gatekeeper Version Bump (`zumadllchecker.dll`)**:
  - Bumped version metadata to `v0.5.1.3` across pre-launch diagnostic UI and window headers.

## [0.5.1.2] - 2026-10-23

### Fixed
- **Cached Thumbnail Golden Skull Holes & Full Tangent/Fixed Rotation Pipeline (`zumareshandler.dll`)**:
  - Fixed missing skull holes on generated map thumbnails (`levels/cached_thumbnails/*.png`) caused by `zumaextrapath.dll`'s Cave M detour bypassing `HoleMgr::Draw` when `va_ptr_checkholeprio != 0`.
  - Intercepted `CreateCachedThumbnail` downscaling call site (`0x0040CD8E`) via `hk_cached_thumbnail_scale` to explicitly render all active skull holes onto the staging canvas before downscaling.
  - Fixed skull holes rendering unrotated (facing strictly upright) on software `MemoryImage` canvases:
    - Zuma engine resolves skull hole rotation via XML attribute `skullrot` (degrees to radians) or falls back to the curve tangent of the entering waypoint (`curve.points[last].mTangent`).
    - The legacy 2003 software branch at `0x00404D9B` in `HoleMgr::DrawHole` (`0x00404D50`) bypassed `Graphics::DrawImageRotated` whenever `gApp->Is3DAccelerated()` returned false.
    - Patched opcode at `0x00404D9B` from `75 37` (`JNZ`) to `EB 37` (`JMP`), forcing `HoleMgr::DrawHole` to unconditionally route through the rotated drawing pipeline (`Graphics::DrawImageRotated` at `0x004515C0`), fully supported in software by `MemoryImage::BltRotated`.
    - Both `IMAGE_HOLE` (sun disc) and `IMAGE_HOLE_COVER` (closed golden skull) are now rendered with accurate orientation matching curve geometry.
  - Implemented safe, automated headless batch thumbnail generator triggered via `gen_thumbs.flag` after 90 frames of startup initialization.
- **Aspect-Fill Thumbnail Scaling & Centered Cover Blit (`zumareshandler.dll`)**:
  - Replaced stretching and distortion of non-4:3 map preview thumbnails (e.g., 16:9 1200x675 backgrounds) with dynamic aspect-fill (`cover`) cropping.
  - Intercepted `CreateCachedThumbnail` (`0x0040CD8E`) via `hk_cached_thumbnail_scale` and `GauntletScreen` preview rendering (`0x00439202`) via `hk_draw_gauntlet_thumbnail` with symmetric crop calculations, preserving 1:1 circular geometric proportions in Gauntlet doorway previews with zero distortion.
- **In-Game Floating Text Banners Centering (`zumareshandler.dll`)**:
  - Dynamically patched `Board::SpawnFloatingBanner` (`0x00428C60`) using unique AOB signature scanning to center `"%d LIVES LEFT"`, `"LAST LIFE!"`, and `"GAME OVER"` floating banners dead-center at `(TARGET_W / 2, TARGET_H / 2)` instead of hardcoded vanilla `(320, 240)`.

### Added
- **Comprehensive Reverse-Engineering Documentation (`docs/cached_thumbnail_and_skull_rotation.md`)**:
  - Added full technical documentation detailing the cached thumbnail pipeline, Cave M root cause analysis, skull hole rotation resolution algorithms (`skullrot` vs curve tangent fallback), 2D software limitations, reverse-engineered dynamic patches, and verification results.

### Changed
- **Diagnostic Gatekeeper Version Bump (`zumadllchecker.dll`)**:
  - Bumped version metadata to `v0.5.1.2` across pre-launch diagnostic UI and window headers.

## [0.5.1.1] - 2026-10-23

### Fixed
- **Button Hover Hardware Custom Cursor Visual Corruption (`zumadisplay.dll`)**:
  - Fixed visual static and corruption occurring when hovering over interactive UI buttons (switching from the default arrow cursor to `CURSOR_HAND`).
  - PopCap `MemoryImage` initializes loaded GIF assets (such as `cursor_hand.gif`) as 8-bit paletted images with `mBits` (`+0x14`) initially set to `NULL` until explicitly unpacked.
  - The hardware cursor synthesizer previously mistook the palette table pointer (`mColorTable` @ `+0x24`) for raw 32-bit pixel data, reading out-of-bounds heap memory and corrupting the cursor bitmap.
  - Resolved by invoking `MemoryImage::GetBits()` via virtual method `vftable[22]` (`+0x58`) using `__thiscall` calling convention to trigger PopCap's native palette unpacking into a clean 32-bit ARGB buffer.
  - Added robust fallback to manual palette unpacking (`pal[idx[i]]`) if `mBits` cannot be resolved.
  - Zero-initializes the 1-bpp monochrome mask bitmap (`hbmMask`) with `calloc` instead of `CreateBitmap(..., NULL)` and converts colors to premultiplied 32-bit ARGB before passing to `CreateIconIndirect`.

### Changed
- **Unified Repository Licensing Scope (`LICENSE.md`)**:
  - Streamlined Section 4 of `LICENSE.md` to reference top-level architectural directories (`builder/`, `src/`, `docs/`, `assets/`, and root build scripts) rather than listing individual filenames, ensuring permanent, maintenance-free forward compatibility for future releases.
- **Diagnostic Gatekeeper Version Bump (`zumadllchecker.dll`)**:
  - Bumped version metadata to `v0.5.1.1` across pre-launch diagnostic UI and window headers.
## [0.5.1] - 2026-10-22

### Added
- **Dual-Manifest Asset Architecture (`properties/resources.xml` vs `properties/resources_extra.xml`)**:
  - `properties/resources.xml` is strictly preserved as the clean vanilla 6-color manifest (`NEXT_BALL` with `path="baDotz"` and `cols=6`).
  - Full Edition 11-color definitions are completely isolated into `properties/resources_extra.xml`, mapping `NEXT_BALL` to `path="baDotz_extra"` and `cols=11`.
- **Discrete Frog Dotz Indicator Assets (`baDotz.gif` vs `baDotz_extra.gif`)**:
  - Preserves untouched native 6-color `images/baDotz.gif` (`90x15` at 6 frames).
  - Isolates 11-color dotz into `images/baDotz_extra.gif` (`165x15` at 11 frames) for the Full Edition.
- **Dynamic Runtime Hooking in `zumaextracolor.dll`**:
  - When `zumaextracolor.dll` is active, its `DllMain` dynamically redirects `Zuma.exe`'s resource manifest pointer (`FUN_0046e3b0`) to `properties/resources_extra.xml` and patches the frog-dots divisor (`0x0040499E`) from `DIV6` to `DIV11`.
  - When `zumaextracolor.dll` is absent, disabled, or removed, `Zuma.exe` automatically executes with native `DIV6` and loads clean `resources.xml` and `baDotz.gif`, falling back 100% seamlessly to 6-color gameplay with zero texture misalignment or crashes.

### Changed
- **Pristine Single-Binary Backup Architecture (`modules\bak\Zuma.exe`)**:
  - `modules\bak\` strictly preserves only the pristine original executable (`Zuma.exe`).
  - Extraneous asset backups (`baDotz.gif`, `resources.xml`) have been completely eliminated from `modules\bak\`.
- **Lite Edition 6-Color Integrity Enforcement (`build_lite.bat`)**:
  - Asset pipeline in `--lite` mode strictly bypasses 11-color asset synthesis, automatically restoring native 6-dot `baDotz.gif` dimensions and eliminating dotz frame offset errors in Lite builds.
- **Dedicated Omitted Status & Visual Styling in Diagnostic Gatekeeper (`zumadllchecker.dll`)**:
  - Replaced misleading red cross / `[DISABLED]` badges for omitted Lite sidecars with a dedicated muted grey rounded box featuring a horizontal strip dash (`-`) and explicit `[OMITTED]` badge text.
- **Build Console UI Standardization (`build_full.bat` & `build_lite.bat`)**:
  - Eliminated redundant stacked dividers between modules in Lite edition.
  - Standardized uniform module header borders and sequential patch steps (`[PATCH X/N]`) across both editions.

### Fixed
- **DevVIP Middle-Click Cheat Survival Mode Access Violation (`0x0041131F`)**:
  - Fixed a crash in `zumatestingkit.dll` when triggering middle-click cheat in Survival mode under Lite Edition or without `zumaextrapath.dll`.
  - In vanilla Zuma, `[board + 0x68]` points to a `Curve*` object (`mStagingCurves[0]`) rather than a dynamic heap pointer array (`Curve**`). Dereferencing it as an array passed the Curve vftable (`0x004FE5EC`) to `CurveMgr::DestroyAll`, resulting in crash at `0x0041131F`.
  - Added dynamic verification of `has_extrapath`, fallback to inline curve array (`board + 0x5C + i*4`), and runtime Curve vftable sanity checking before invoking `DestroyAll`.

## [0.5.0] - 2026-10-22

### Added

- **Arbitrary Custom Resolution Engine (`zumareshandler.dll` & `.zres` PE section)**:
  - Dynamic resolution handler `zumareshandler.dll` injected via new `.zres` PE section, supporting any target resolution ≥ 480×320 (no upper ceiling). Configured via `-w <width> -h <height>` flags in `build_full.bat` / `build_lite.bat` or interactively at build time.
  - Scales all UI coordinates, fonts, and gameplay viewports uniformly using letterboxed aspect-ratio fit: `scale = min(W/640, H/480)`, with pixel-exact `X_offset = (W - content_w) / 2` and `Y_offset = (H - content_h) / 2` margins.
  - Fully repositions every screen: `LoadingScreen`, `TitleScreen`, `MainMenu`, `AdventureMap`, `PracticeScreen` (Gauntlet), `UserDialog`, `OptionsScreen`, `HelpScreen`, `InGameHUD` (Board + Menubar), `CreditsScreen`, and `CompletionScreen`.
  - All 100+ coordinate patch sites resolved via dynamic AOB signature scanning at runtime — zero hardcoded virtual addresses in `zumareshandler.dll`.
  - Exports `ZRH_IsActive`, `ZRH_ScreenToGameCoords`, `ZRH_GameToScreenCoords`, `ZRH_IsFullscreen`, `ZRH_ApplyPatches` for external sidecar interop.

- **Borderless Fullscreen Mode & F11 Toggle (`zumadisplay.dll`)**:
  - `zumadisplay.dll` subclasses the game window (`GWL_WNDPROC`) to intercept `WM_KEYDOWN`.
  - Pressing **F11** (or **Alt+Enter**) calls `ToggleFullscreen()`: toggles between windowed and borderless fullscreen on-the-fly without restarting the game.
  - In fullscreen, applies `WS_POPUP`, spans `(0,0)` across full display dimensions via `GetSystemMetrics`, and synchronizes `ZumaApp` and DirectDraw cooperative flags in memory.
  - A hooked `PrimarySurface::Blt` executes a letterboxed `fit-to-screen` stretch blit to the physical monitor, while `ClearFullscreenBorders` fills pillarbox/letterbox margins with solid black.
  - Hooked `ScreenToClient` and `ClientToScreen` transparently remap all cursor coordinates through the letterbox transform, ensuring aiming, button hit-testing, and score popups remain pixel-accurate in fullscreen.

- **Modernized True 32-bit ARGB GPU Hardware Cursors (`HCURSOR`)**:
  - Replaced PopCap's legacy 2003 software cursor surface blits (`DDInterface::DrawCursor` and its 10ms `CursorThreadProc` loop) with native Win32 hardware `HCURSOR`s.
  - Dynamically constructs 32-bit premultiplied ARGB icons via `CreateIconIndirect` from PopCap's loaded `MemoryImage` bitmaps (`IMAGE_CURSOR_POINTER`, `IMAGE_CURSOR_HAND`, etc.) on first request.
  - Silenced `DDInterface::SetCursorPos` (`0x00450550`) with an immediate `ret 8` (`C2 08 00`), completely eliminating DirectDraw primary surface critical section contention.
  - Hooked `SexyAppBase::RehookCursor` (`0x00465001`) and `WM_SETCURSOR` to maintain native hardware cursor locking across client areas.
  - Completely eliminates gameplay stutter, CPU spinning, and top-right ghost cursor artifacts in fullscreen.

- **Preserved 3D Hardware Acceleration on Fullscreen Toggle (`zumadisplay.dll`)**:
  - Eliminated the direct overwrite of `mIs3D` (`dd + 0x10`) during `ToggleFullscreen()`, preserving the user's hardware 3D acceleration setting across windowed and fullscreen transitions.

- **Outscreen Brick Border Renderer (`outscreen_data.h` & `zumadisplay.dll`)**:
  - In windowed letterbox/pillarbox margins or menu/credits screens, out-of-content areas are rendered with a seamless 160×160 px tiling brick texture (`outscreen.jpg`).
  - Embedded as a C array in `src/outscreen_data.h` and instantiated as an in-memory `PopCap MemoryImage` at runtime, completely eliminating external filesystem dependencies.

- **Smart In-Game Stone Menubar Generator (`builder/patch_menubar.py`)**:
  - Synthesizes `menubar.jpg` and `_menubar.gif` for any arbitrary target width from master tile assets in `assets/media/`.
  - Preserves the 5 HUD compartments (Lives, Stage/Level, Score, Zuma Bar, Menu) at native `1:1` unscaled size (`zero distortion`); only bridge filler segments are stretched or trimmed. Bridge alpha smoothstep-blends to zero at narrow widths (≈ 480 px).
  - Dynamically patches menubar margins (`top`, `bottom`, `left`, `right`) in `Zuma.exe` via AOB and synchronizes HUD element positions via `calc_hud_coords(w)`.

- **`HappyEnding` Fit-To-Viewport Hook (`zumareshandler.dll`)**:
  - Intercepts `happyending.jpg` credits rendering to fit and center the graphic within the content viewport, applying tiling outscreen brick margins to match other full-screen sequences.

- **Dynamic Centering for Victory & Transition Screens (`zumareshandler.dll`)**:
  - Dynamically centers `Stage Complete` and `Temple Cleared` screens across any target resolution, maintaining consistent letterbox and outscreen border fill.

- **HelpScreen Multi-Page Layout Adaptation (`zumareshandler.dll` & `zumatestingkit.dll`)**:
  - Centers and scales How-To-Play instruction pages (w1, w2, w3), lower compartments, aiming target demo, and checkbox layout while preserving native 1:1 unscaled tutorial ball/mouse sprites (`tut_mouse_m`, `tut_ball_*`) and Cancun fonts.
  - Dynamically adapts the DevTester Guide two-column layout with responsive column centering (`col1_cx`, `col2_cx`) and interactive ball roll animations.

- **Diagnostic Checker UI Refinement (`zumadllchecker.dll`)**:
  - Highlighted `[OK] Required` badge in bold cyan (`#00E5FF`) in `zumadllchecker.dll`'s diagnostic window to visually distinguish the mandatory gatekeeper from optional sidecars.
  - Added dedicated resolution category (Group 3) displaying target resolution, aspect ratio, minimum limit (480×320), and dynamic asset mode (`imagescustomres/` vs `images/`).
  - Added interactive checkbox toggling allowing players to enable/disable optional sidecars on-the-fly before launching.

- **Aligned Lite Edition (`build_lite.bat`)**:
  - Fully aligned `build_lite.bat` with `build_full.bat` — compiles 6 sidecars (`zumadllchecker.dll`, `zumacustommap.dll`, `zumacplxcredit.dll`, `zumatestingkit.dll`, `zumadisplay.dll`, `zumareshandler.dll`) and omits strictly the 4 content/heavy sidecars (`zumawebpbg.dll`, `zumawebpsprite.dll`, `zumaextrapath.dll`, `zumaextracolor.dll`).
  - Lite Edition now fully supports arbitrary custom resolutions, borderless fullscreen (F11), and high-resolution asset scaling (`imagescustomres/`).
  - Added `--lite` CLI support to `builder/patch_imports.py` and `builder/verify_pe.py` for automated PE structural verification.

- **New Technical Documentation**:
  - `docs/credits_system.md`: Cave A/B/C architecture, `__thiscall` calling convention analysis, loop fix, and fullscreen button routing.
  - `docs/sidecar_dll_overview.md`: Complete matrix of all 10 sidecars (names, sources, PE sections, key exports, purposes).
  - `docs/repositioning_and_coordinate_spaces.md`: Exhaustive guide to coordinate spaces, child parenting rules, and macro hierarchy (`SX_VAL` / `SY_VAL` / `SDIM_VAL` / `PARENT_LOCAL_*`).
  - `docs/unique_attributes_and_signatures.md`: Complete AOB signature catalog updated for `zumareshandler.dll`.

### Changed

- **Total Modular Sidecar Count: 7 → 10 (Decoupled Architecture)**:
  - Decoupled resolution & display engine into:
    - **`zumadisplay.dll`**: Low-level DirectDraw & window driver (F11 & Alt+Enter borderless fullscreen toggle, 3D acceleration preservation, native 32-bit ARGB GPU hardware cursors, PrimarySurface::Blt fit-to-screen blit hook, ScreenToClient/ClientToScreen coordinate remapping, outscreen brick border rendering).
    - **`zumareshandler.dll`**: Engine UI & coordinate layout patcher (dynamic AOB memory scanning and layout adjustments across 20+ screens, modal dialog universal centering VTable hook, gameplay board clip rect expansion, stone menubar HUD margin synchronization, and `CreateFileA` asset redirection to `imagescustomres/` with fallback to `images/`).
  - Decoupled animated WebP engine into:
    - **`zumawebpbg.dll`**: Animated WebP level background engine (`bg.webp` playback at native 1:1 aspect ratio) and PopCap ImageLib asset loader interceptor.
    - **`zumawebpsprite.dll`**: Multi-layer animated sprite engine (layer 0..4 node traversal, `LevelDesc` coordinate matching, and alpha-preserving transparent playback).
  - `build_full.bat` compiles and deploys all 10 modules (`zumadllchecker.dll`, `zumawebpbg.dll`, `zumawebpsprite.dll`, `zumaextrapath.dll`, `zumaextracolor.dll`, `zumacustommap.dll`, `zumacplxcredit.dll`, `zumatestingkit.dll`, `zumadisplay.dll`, `zumareshandler.dll`).
  - `zumadllchecker.dll` pre-launch diagnostic window dynamically displays status checks for all 10 sidecars in real time.

### Fixed

- **DirectDraw 3D Acceleration Reset on F11 Fullscreen Toggle (`src/zumadisplay.dll.c`)**:
  - Fixed an issue where `ToggleFullscreen()` inadvertently overwrote `mIs3D` (`dd + 0x10`) with `0`, causing 3D hardware acceleration to be silently disabled when toggling fullscreen via F11. 3D acceleration is now 100% preserved.

- **Custom Cursor Rendering Lag & Fullscreen Ghost Artifacts (`src/zumadisplay.dll.c`)**:
  - Replaced legacy DirectDraw software primary surface blits (`DDInterface::DrawCursor` & 10ms thread polling) with native Win32 hardware `HCURSOR`s. Silenced `DDInterface::SetCursorPos` (`0x00450550`) with `ret 8` (`C2 08 00`), hooked `SexyAppBase::RehookCursor` (`0x00465001`), and intercepted `WM_SETCURSOR`. Eliminates cursor latency, CPU contention, and top-right ghost cursor artifacts in fullscreen.

- **Dynamic Asset Redirection & Clean Fallback (`src/zumareshandler.dll.c`)**:
  - Hooked `CreateFileA` inside `zumareshandler.dll` to redirect asset loads to `imagescustomres/` only when custom resolution assets exist. When `zumareshandler.dll` is omitted or deleted, asset loading automatically and cleanly falls back to vanilla `images/`.

- **Animated WebP Canvas Stretching & Aspect Distortion (`src/zumawebpbg.dll.c` & `src/zumawebpsprite.dll.c`)**:
  - Fixed a critical regression where animated WebP backgrounds and sprites were stretched to fill destination dimensions (`dw, dh`), causing distorted aspect ratios in custom resolutions.
  - Rebuilt `blit()` to enforce native 1:1 pixel rendering bounded by `min(sw, dw)` and `min(sh, dh)` with clean edge clearing.
  - Implemented `find_sprite_webp_in_leveldesc` to accurately resolve sprite WebP files per layer from `LevelDesc` coordinates `(x, y)` and priority `k`, eliminating fallback to background animations.
  - Added multi-tier pixel sampling (`sample_pixels`) supporting opaque (`alpha == 0xFF`), semi-transparent (`alpha > 0`), and uniform grid sampling, ensuring blended WebP animations maintain frame synchronization.

- **Extra Paths 4..10 Ignoring Level Settings (`zumaextrapath.dll` & `builder/patch_extrapath_stage2.py`)**:
  - In vanilla Zuma, `LevelDesc` only provides 3 inline `CurveDesc` structures (`+0xAC`, `+0x128`, `+0x1A4`). Extra paths 4..10 (`curve_idx >= 3`) failed to receive curve properties declared in `levels.xml` (`settings="<id>"` such as ball speed, start position, repeat count, score thresholds, acceleration, and timers).
  - Implemented dynamic descriptor synchronization (`ZEP_SyncLevelDesc` / `zep_sync_level_desc_`), copying 96 bytes (`0x1C..0x7B`) of runtime settings from `CurveDesc[0]` (or level settings) into `extra_desc[k]` while preserving unique trajectory paths (`mPath`).
  - Injected Cave N (`va_advance_sync`) in section `.zep2` at `STAGE2_ADVANCE_SYNC` (`0x0042173B`) before the `Board::AdvanceLevel` curve loop, ensuring extra paths 4..10 always advance with identical speed and difficulty settings.

- **Multi-Curve Color Progression Desynchronization (`builder/patch_extracolor.py`)**:
  - Fixed an issue where `zumaextracolor.dll`'s `.zcol` color loop fell out of sync with `zumaextrapath.dll` on tracks with more than 3 curves.
  - Unified the `.zcol` color loop to walk all active curve descriptors, apply `ZEC_ApplyColors` across all curves, respect manual `levels.xml` color overrides (`[edi + 0xE4]`), and clamp Lite edition curves to `<= 6` colors without corrupting extra curve arrays.

- **Testing Kit Crash on Middle-Click Cheat with > 3 Paths (`src/zumatestingkit.dll.c`)**:
  - In vanilla Zuma, `[board + 0x68]` (`mActiveCurveMgrs`) points to `&board->mCurveMgr[0]` (3 curves). With `zumaextrapath.dll`, `[board + 0x68]` points to a dynamic heap array of up to 10 `CurveMgr*` pointers.
  - Accessing `[board + 0x5C + i*4]` for `i >= 3` was reading `[board + 0x68]` itself (the pointer array), treating it as a `CurveMgr` object and calling `CurveMgr::DestroyAll` on the array pointer, causing an immediate Access Violation crash (`0xC0000005` at `0x0041132A`).
  - Fixed the curve iteration loop to read from `active_curves[i]` (`*(void***)((char*)board + 0x68)`), properly destroying balls on all active curves (1..10) in Survival and Practice modes without crashes.

- **Testing Kit Floating Score Cursor Offset in Fullscreen & Custom Resolutions (`src/zumatestingkit.dll.c`)**:
  - Retrieved cursor position from widget manager coordinates (`wm->mX`, `wm->mY`) and mapped through `ZRH_ScreenToGameCoords`, ensuring `+<diff>` floating score text spawns squarely at the cursor in fullscreen and custom resolutions.

- **True Geometric Dialog Centering (`src/zumatestingkit.dll.c`)**:
  - Overrode PopCap's hardcoded vertical bias (`+210`) in `SetupDialog` to center modals symmetrically at `(screen_w - w)/2` and `(screen_h - h)/2`.

- **Board Clipping Rect & Starfield Y Bounds (`zumareshandler.dll`)**:
  - Vanilla `Board::Draw` hardcoded the clipping rectangle to `640x480`, prematurely clipping balls, particle effects, and frog projectiles when traveling beyond vanilla boundaries in widescreen. Patched `mClipRect` and starfield Y-bounds to span the full canvas (`TARGET_W x TARGET_H`).

- **Testing Kit `DialogButtonLocked` Texture & Button State Hardening (`src/zumatestingkit.dll.c`)**:
  - Hardened `UserDialog::ButtonListener` vtable signature scanning (`push 1; push 7; ... mov [esi + 0x58], vtable`), capturing active dialog and listbox handles on init to guarantee locked grayscale textures render reliably on `Rename`/`Delete` buttons when `DevTester` is selected.

- **Floating Score Boundary Clamp (`src/zumatestingkit.dll.c`)**:
  - Clamped middle-click floating score text coordinates to prevent upward-drifting score labels from clipping or vanishing when clicking near screen edges.

- **Cave C Access Violation (`0xC0000005` at `0x00592082`) in Developer Credits (`builder/patch_zccredit.py`)**:
  - `Widget::MouseDown` uses `__thiscall` (`ecx = this`). The C `__cdecl` call to `ZCC_OnMouseDown` clobbered `ecx` and `edx`. The vanilla fallback `mov byte ptr [ecx+0x1f], 1` then wrote to a corrupted address.
  - Fixed Cave C to emit `push ecx` / `push edx` before the call and `pop edx` / `pop ecx` after, with stack argument offsets adjusted for the two saved registers. Crash at `0x00592082` fully resolved.

- **GitHub Button Unresponsive in Fullscreen (`src/zumacplxcredit.dll.c`)**:
  - `open_github_url_()` now passes the game HWND to `ShellExecuteA` and calls `ShowWindow(hwnd, SW_MINIMIZE)` to iconify the fullscreen DirectDraw surface, bringing the browser to the foreground.

- **Credits Pop-In on Loop (`src/zumareshandler.dll.c`)**:
  - `CreditsScrollResetY` and `CreditsScrollStartY` AOB patches replace the vanilla hardcoded `520` threshold with `TARGET_H + 20`, eliminating abrupt credit title appearance on loop restart at any resolution.

- **Unified Sidecar Logging into `zumatest.log` (`src/zumareshandler.dll.c`)**:
  - Replaced standalone `zuma_reshandler.log` with unified logging into `zumatest.log`, aligning `zumareshandler.dll` with all other sidecars and adhering strictly to the framework's Production Silent Mode.

---

## [0.4.2] - 2026-10-19

### Added
- **1-Click Universal Target Folder Patcher & Adaptive Game Discovery**:
  - Patcher now accepts any target Zuma Deluxe installation directory directly (via CLI argument, drag-and-drop, or native Windows folder picker).
  - Implemented `builder/find_target_exe.py` to adaptively discover valid game executables across various distributor packagings (`Zuma.exe`, `popcapgame1.exe`, etc.) with zero hardcoded paths.
  - Implemented `builder/ensure_unpacked.py` with automatic, ephemeral UPX decompression during build time, creating a clean baseline for analysis while leaving zero clutter.
  - Implemented Single-Backup Architecture (`modules\bak\Zuma.exe`) guaranteeing strict idempotency and zero duplicate backup proliferation across repeated patch runs.
  - Unified local tooling convention: isolated internal helper binaries under `builder/.tools/` with automatic ignore patterns.

### Fixed
- **PE Import Directory & GameHouse Retail DRM Protection (`builder/patch_imports.py`)**:
  - Fixed `DataDirectory[12]` (IAT) bounds calculation that previously spanned 570 KB across `.rdata`, `.data`, `.rsrc`, and `GARR` sections.
  - Preserved original IAT bounds when present (`RVA=0xFE000, Size=0x398`) and strictly confined dynamic fallback calculations within `.rdata`.
  - Prevented Windows PE Loader from marking the GameHouse DRM `GARR` section as `PAGE_READONLY`, eliminating the fatal `0xC0000005` Access Violation crash in `zumares.dll` (`0x100032B0: xor byte ptr [eax], 0x83`).
- **Engine Spatial Hash Priority Overflow Crash (`0x00401EEA`)**:
  - Injected in-place assembly clamping (`cmp eax, 4; jbe .ok; push 4; pop eax`) into engine subroutines `STAGE2_SPATIAL_ADD1` (`0x00401EA0`) and `STAGE2_SPATIAL_ADD2` (`0x00401ED0`).
  - Clamped track waypoints with `priority >= 5` in `zumaextrapath.dll` (`curv_load`), preventing out-of-bounds layer bucket indexing and fatal memory corruption when balls approach the skull pit on multi-path maps (e.g. `tripleserpent-3.dat`).
- **Developer Middle-Click Cheat in Survival Mode (`src/zumatestingkit.dll.c`)**:
  - Dynamically resolved `CurveMgr::DestroyAll` (`0x00411310`) via runtime AOB signature scanning (`53 8B D9 8B 43 30 85 C0 57 75 04 33 FF EB 02 8B 38 3B 7B 30`).
  - Added an active curve iteration loop specifically for Survival mode (`mIsSurvival != 0`), bypassing vanilla engine jumps to detonate all balls instantly on middle-click.
- **Floating Score Text Color (`src/zumatestingkit.dll.c`)**:
  - Updated middle-click floating score text color in `ZTK_UpdateDevCheat` from dev cyan (`0x00E5FF`) to authentic combo yellow (`0xFFFF00` / RGB 255, 255, 0).
- **11-Color Cream Progression in Gauntlet Mode (`src/zumaextracolor.dll.c`)**:
  - Corrected 0-based sublevel threshold logic (`sub >= 7 ? 11 : 10`) in `ZEC_NumColors`, properly unlocking the 11th ball color (Cream) starting at Sun God 8.
- **Scope-Based Button Draw Wrapper & State Integrity (`src/zumatestingkit.dll.c`)**:
  - Replaced persistent image swapping with a non-destructive scope-based wrapper around `ButtonWidget::Draw`.
  - Removed reliance on global dialog pointers and hardcoded buffer addresses (`0x0054802c`), using dynamic button validation (`*(dialog+0x120/0x124) == btn`) and safe virtual memory queries.
- **Honest Distributor & Mod Metadata Detection (`src/zumadllchecker.dll.c`)**:
  - Prioritized explicit `launch.ini [Main] RealArcade=Yes` and `partner.xml` markers over guessed substrings.
  - Clearly separated distributor claims from mod expansion labels, displaying `(detected)` tags for inferred data.

## [0.4.1.1] - 2026-10-19

### Fixed
- **`levels.xml` Stage Color Progression Differentiation**:
  - Differentiated stage color progression logic in `builder/patch_assets.py` between Full and Lite editions.
  - **Lite Edition Progression**: Adjusted stages 4–13 to adhere strictly to the vanilla PopCap 6-color engine boundary (Stage 1–3: 4 colors; Stage 4–6: 5 colors; Stage 7–13: 6 colors), preventing out-of-bounds palette crashes caused by attempts to spawn colors beyond vanilla limits without `zumaextracolor.dll`.
  - **Full Edition Progression**: Preserved custom 11-color palette progression powered by `zumaextracolor.dll` (Stage 1–3: 4 colors; Stage 4–6: 6 colors; Stage 7–9: 8 colors; Stage 10–12: 10 colors; Stage 13: 11 colors).
  - Updated `build_lite.bat` to pass `--lite` flag to `builder/patch_assets.py`, and added directory name suffix matching (`-lite`) for automated workspace detection.
- **Diagnostic Checker & Script Version Alignment**:
  - Synchronized `ZUCOMPLELUXE_VERSION` define and diagnostic window titles to `v0.4.1.1` in `src/zumadllchecker.dll.c`.
  - Updated builder banners and logging messages in `build_full.bat` and `build_lite.bat`.

## [0.4.1] - 2026-10-19

### Added
- **Zero-Dependency Toolchain Bootstrap (`builder/bootstrap.bat`)**:
  - Unified toolchain engine shared across Full and Lite editions.
  - Automatic detection and portable fallback to embedded Python 3.11.9 amd64 with built-in cryptographic SHA-256 integrity verification.
  - Automated pip bootstrapping and package pinning (`builder/requirements.txt`: capstone, numpy, packaging, pefile, pillow, ziglang).
  - Multi-tier 32-bit compiler probing (`builder/probe_compiler.py`) with kernel-level GUI error dialog suppression (`SetErrorMode(0x8003)`).
  - Universal Zig cross-compiler fallback targeting `x86-windows-gnu` for clean machines without native MinGW.
- **Git CRLF Line Ending Enforcement (`.gitattributes`)**:
  - Mandated `eol=crlf` for all `.bat` and `.cmd` files to ensure stability across git checkouts and zip releases.

### Fixed
- **Missing Python Module Crashes**:
  - Resolved `ModuleNotFoundError: No module named 'numpy'` by enforcing prioritized dependency verification before asset generation.
- **Batch Parenthesis Syntax Error**:
  - Eliminated cmd.exe parenthesized block parse crashes (`. was unexpected at this time`).
- **Export Symbol Name Preservation**:
  - Added strict PE export verification via `pefile` to validate exact undecorated symbols across all sidecar DLLs.
- **Testing Kit Tutorial Screen & Asset Optimization (`src/zumatestingkit.dll.c` & `builder/patch_assets.py`)**:
  - Bound tutorial screen ball display directly to vanilla PopCap ball textures (`IMAGE_RED_BALL` .. `IMAGE_PURPLE_BALL`) via `ResourceManager` with global engine pointer fallbacks, rendered dynamically with 90° CCW rotation via native `Graphics::DrawImageRotatedF` (`0x00450740`).
  - Rendered mode titles (`ADVENTURE MODE`, `GAUNTLET MODE`, `MAIN MENU`) dynamically using native `CancunFloat14` font, eliminating obsolete `tut_ball_*.gif` and `tut_title_*.gif` bitmap dependencies.
  - Removed all `os.remove` file deletions from `builder/patch_assets.py`, ensuring zero-touch non-destructive behavior across game directories.

## [0.4.0.1] - 2026-10-18

### Fixed
- **Asset Patcher Re-patch & Replace (`builder/patch_assets.py`)**:
  - Modified asset pipeline to always load pristine original assets from `modules\bak\` (`baDotz.gif`, `resources.xml`, `levels.xml`) and re-synthesize/overwrite target files instead of skipping when files already exist.
  - Ensured `DialogButtonLocked.jpg` and `_Dialogbuttonlocked.gif` are always regenerated from their respective base assets and replaced deterministically.

## [0.4.0] - 2026-10-18
