# Changelog

All notable changes to the **ZuCompleluxe** framework will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.4.1] - 2026-10-19

### Added
- **Zero-Dependency Toolchain Bootstrap (`builder/bootstrap.bat`)**:
  - Unified toolchain engine shared across Full and Lite editions.
  - Automatic detection and portable fallback to embedded Python 3.11.9 amd64 with built-in cryptographic SHA-256 integrity verification.
  - Automated pip bootstrapping and package pinning (`requirements.txt`: capstone, numpy, packaging, pefile, pillow, ziglang).
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

### Added
- **7-Sidecar Modular Architecture & `modules\` Subdirectory Isolation**:
  - Expanded the framework from 3 root DLLs to 7 specialized, single-responsibility DLLs isolated inside the `modules\` subdirectory (1 mandatory gatekeeper + 6 optional feature sidecars):
    1. `modules\zumadllchecker.dll` (`.zldr` section): **Mandatory Pre-Launch Gatekeeper & Dependency Checker**.
    2. `modules\zumawebp.dll` (`.anim` section): Animated WebP background and multi-layer sprite engine.
    3. `modules\zumaextrapath.dll` (`.zep` & `.zep2` sections): Up to 10 simultaneous ball paths (`curve4`..`curve10`), 10-hole `HoleMgr`, and priority-aware skull rendering.
    4. `modules\zumaextracolor.dll` (`.zcol` section): 11-color ball expansion, dynamic Adventure/Gauntlet color progression, and PopCap resource tree extraction.
    5. `modules\zumacustommap.dll` (`.zcm` section): Gauntlet Custom Map expansion, unreferenced `levels.xml` map discovery, custom `<Gauntlet>` XML support, and 4-tier rank progression.
    6. `modules\zumacplxcredit.dll` (`.zcc` section): Dedicated Developer Credits sidecar (decoupled from `zumaextrapath.dll`) with interactive `GitHub` stone button.
    7. `modules\zumatestingkit.dll` (`.ztk` section): Conditional Developer Testing Kit (`DevTester` virtual profile) activated via `zumatest.log`.
- **Lite Edition 1-Click Builder (`build_lite.bat`) & Dynamic Neutralization**:
  - Added dedicated `build_lite.bat` pipeline for users desiring core gameplay additions without heavy sidecars (`zumawebp.dll`, `zumaextrapath.dll`, `zumaextracolor.dll`).
  - Automatically compiles and deploys the 4 core modules (`zumadllchecker.dll` with `-DZUCOMPLELUXE_LITE=1`, `zumacustommap.dll`, `zumacplxcredit.dll`, `zumatestingkit.dll`), while purging excluded sidecars from `modules\`.
  - In Lite Edition, `zumadllchecker.dll` displays `ZuCompleluxe v0.4 Lite`, informs users of the limited core mod scope via bottom notes, and marks omitted sidecars as `[DISABLED]`.
  - Added `DisableLiteExcludedModules()` inside `src/zumadllchecker.dll.c`: upon pressing **Run**, actively neutralizes excluded DLL paths in `.zldr` memory so that `LoadLibraryA` is never invoked and excluded DLLs are completely ignored even if manually copied into `modules\`.
- **Mandatory Pre-Launch Gatekeeper & Dependency Checker (`zumadllchecker.dll` & `builder/patch_imports.py`)**:
  - **Hard Gatekeeper Enforcement in `.zldr` (`0x0058C000`)**:
    - Unlike the 6 optional feature sidecars, `modules\zumadllchecker.dll` is a strictly required dependency enforced at the very start of the `.zldr` startup stub.
    - Resolves `ExitProcess` (`kernel32.dll`) and `MessageBoxA` (`user32.dll`) dynamically at startup. If `modules\zumadllchecker.dll` is missing, corrupt, or fails to export `ZDC_RunChecker`, `.zldr` displays a modal error dialog and immediately aborts execution via `ExitProcess(1)` before any game code runs.
    - Invokes `ZDC_RunChecker()` prior to loading optional feature sidecars; clicking **Quit** or closing the window cleanly terminates the process (`ExitProcess(0)`), while clicking **Run** proceeds to load available feature sidecars and jump to the PopCap entry point (`0x00589000`).
  - **Dynamic PE Version Resource Metadata Extraction**:
    - Queries `Zuma.exe`'s Win32 Version Resource (`GetFileVersionInfoA` / `VerQueryValueA` across `VS_FIXEDFILEINFO` and `\VarFileInfo\Translation`) with zero hardcoded game strings, dynamically displaying `Product` (`ProductName`), `Publisher` (`CompanyName`), `Version` (`FileVersion`), `Special Build` (`SpecialBuild`), and `Target File` (`Zuma.exe`).
  - **Patch & Author Attribution with Interactive GitHub Hyperlink**:
    - Displays `Patch: ZuCompleluxe v0.4`, `License: MIT License`, and `Developer: Rizky Bayuu`.
    - Renders `Rizky Bayuu` as an interactive hyperlink (underlined GitHub accent blue `#58A6FF` in Dark Mode / `#0969DA` in Light Mode, hover highlight `#79C0FF`, instant `IDC_HAND` / `IDC_ARROW` cursor state management across `WM_MOUSEMOVE`, `WM_SETCURSOR`, and `WM_MOUSELEAVE`, and `ShellExecuteA` click handler opening `https://github.com/rizkybayuu`).
  - **Classic Adaptive Win32 Dialog & Dependency Table**:
    - Inspects all 7 modules in `modules\` and renders a compact status table (`[OK] Required`, `[OK] Present`, or `[MISSING]`).
    - Automatically detects Windows system theme (`HKCU\...\Personalize\AppsUseLightTheme`), applies DWM Immersive Dark Mode titlebar attributes (`DwmSetWindowAttribute` 20/19), and renders double-buffered GDI controls with compact owner-drawn `Quit` and `Run` buttons (`68x24` px).
- **100% Independent & Optional Feature Sidecars (Zero-Dependency Graceful Degradation)**:
  - **Dynamic Runtime Sidecar Loader (`.zldr` RX section @ `0x0058C000`)**:
    - Replaced static PE import descriptors for mod DLLs with an in-executable dynamic loader stub in `.zldr`.
    - Terminates `DataDirectory[1]` (Import Directory) immediately after vanilla DLL imports (`KERNEL32.dll`, `USER32.dll`, `GDI32.dll`, `ADVAPI32.dll`, `SHELL32.dll`, `ole32.dll`, `WINMM.dll`, `WSOCK32.dll`, `bass.dll`), guaranteeing Windows PE loader never triggers "DLL Not Found" errors if optional feature sidecar DLLs are removed.
    - Dynamically probes `modules\<dll>` (with root folder fallback) via `LoadLibraryA` and resolves exports via `GetProcAddress` into zero-initialized `.zdata` IAT slots before returning cleanly to the original PopCap entry point (`0x00589000`).
  - **Comprehensive Per-Hook IAT Null-Checking & Vanilla Fallbacks**:
    - Every code cave across `.zcol`, `.anim`, `.zep2`, `.zcm`, `.zcc`, and `.ztk` validates its `.zdata` IAT function pointer before invocation and gracefully degrades to vanilla behavior when absent:
      - **Without `zumaextracolor.dll`**: Copies all 36 vanilla ball/powerup `Image*` globals in-engine into `.zdata` lookup tables, replicates slot 0 as a safe fallback for slots 6..10, and clamps Adventure and Gauntlet curve `mNumColors` to `<= 6`.
      - **Without `zumaextrapath.dll`**: Clamps curve counts to `<= 3` and routes `STAGE2_HOLE_BG_END` (Cave M) and `STAGE2_STATIC_BOARD_DRAW` (Cave L) back into the vanilla `FUN_00407250` hole drawing loop so skulls and glowing eye animations render identically to vanilla Zuma.
      - **Without `zumacustommap.dll`**: Gauntlet screen displays the 12 vanilla stages and uses standard level pickers.
      - **Without `zumacplxcredit.dll`**: Credits screen loads the original vanilla credits table (`0x00503A08` / `0x0054948C`) and standard fonts.
      - **Without `zumatestingkit.dll`**: User profile dialog, stage unlocking, Gauntlet defaults, Help screen, and dialog button rendering operate as pure vanilla.
      - **Without `zumawebp.dll`**: Image loading falls back directly to PopCap's built-in ImageLib (`.jpg`, `.png`, `.gif`).
- **Gauntlet Custom Map Sidecar (`zumacustommap.dll` & `builder/patch_custommap.py`)**:
  - Automatically discovers custom maps defined in `levels/levels.xml` outside of Adventure `StageProgression` (up to 256 custom maps).
  - Dynamically expands the Gauntlet carousel count (`zcm_AdjustCount`) and supplies custom `id` and `dispname` strings (`zcm_GetSrcId`, `zcm_GetSrcName`) to `PracticeScreen::SyncLevel`.
  - Supports optional `<Gauntlet>` XML tags for custom map ordering and rank thresholds while preserving natural 4-tier progression (7 stages -> Eagle, 14 -> Jaguar, 21 -> Sun God).
  - Hooks random and survival level pickers (`zcm_PickRandomLevel`, `zcm_PickGauntletLevel`) in pure RX section `.zcm`.
- **Developer Testing Kit Sidecar (`zumatestingkit.dll` & `builder/patch_testingkit.py`)**:
  - Conditionally activated only when `zumatest.log` is present in the game directory; completely invisible and inert during normal gameplay.
  - Injects a zero-disk-pollution virtual user profile **`DevTester`** directly beneath `(CREATE A NEW USER)` in `UserDialog` (`ZTK_OnUserDialogInit`, `ZTK_CheckCurUserSkip`).
  - Renders `DevTester` in the user list and `"Welcome to Zuma, DevTester!"` on the Main Menu in glowing neon cyan-blue (`0x00E5FF` via `ZTK_GetItemColor` and `ZTK_GetWelcomeColor`).
  - Unlocks all 12 Adventure stages (`ZTK_CheckStageUnlock`) and all Gauntlet maps up to **Sun God** rank via an in-memory MSVC 7.1 Dinkumware Red-Black Tree (`std::map<std::string, short>`).
  - Defaults Gauntlet mode for `DevTester` to **Random** map (`mSelectedStage = 0`) and **Practice** mode (`mIsSurvival = 0`).
  - Protects `DevTester` in `UserDialog`: clicking **Rename** or **Delete** triggers a centered native PopCap warning dialog (`"ACTION NOT ALLOWED"`, `"Built-in profile cannot be modified."` via `ZTK_CheckUserDialogAction`).
  - Visually locks `Rename` and `Delete` buttons when `DevTester` is highlighted (`ZTK_PreButtonDraw`, `ZTK_OnRegisterResources`), rendering custom grayscale stone button textures (`DialogButtonLocked.jpg` & `_Dialogbuttonlocked.gif`) with `#505050` text and continuous dirty redraws (`0x00433840` patch).
  - **Secret Temple 5 (`advtemple5` + `_advtemple5.gif`) & Door 5 (`advdoor5`) in Adventure Map (Stage 13: "The Void")**:
    - Introduced a secret 5th temple and interactive 5th door in `MapScreen` (Adventure Mode), strictly accessible only when the `DevTester` profile is active (`zumatest.log` mode).
    - Multi-layer visual hierarchy: rendered strictly above layer `advtemple4` and below layers `advtemple1`, `advtemple2`, and `advtemple3`, with alpha mask `_advtemple5.gif` and seamless 50% opacity fog blending applied via `IMAGE_ADVMIDDLE` overlay.
    - Precise geometry & middle anchor alignment:
      - Temple 5 (`advtemple5`): center anchor `(x=321, y=198)` -> top-left `(126, 148)`, size 391x100.
      - Door 5 (`advdoor5`): center anchor `(x=323, y=201)` -> top-left `(291, 176)`, normalized to 192x50 (64x50 per cel, 3 columns).
    - 3-State Interactive Doorway:
      - Frame 0 (Open / Black): Default idle state.
      - Frame 1 (Hover / Green): Mouse hover highlights door in green and activates `IDC_HAND` cursor (`ZTK_CheckDoor5Hit`).
      - Frame 2 (Selected / Red): Click highlights door in red and displays custom Stage 13 banner (`ZTK_CheckDoor5Click`).
    - **Isolated Virtual State in Sidecar**: Solved a fatal vanilla stack landmine where vanilla `MapScreen::Draw` allocated stack arrays `local_48` (4 temples) and `local_dc` (12 doors), causing instant Access Violation crashes if door index 12 was stored into `[MapScreen + 0x1B8]`. By maintaining `[MapScreen + 0x1B8] = -1` and managing selection state inside `zumatestingkit.dll`, vanilla loops are completely bypassed.
    - Custom Stage 13 Banner Display:
      - Centered title `"THE VOID"` rendered using `FONT_TITLE32` and description `"THE PLACE OF NO ENDING"` rendered using `FONT_BROWSER`, alongside stage badge (`IMAGE_ADVSTAGE`) with text `"13"`.
    - Seamless Level Launching:
      - Double-clicking Door 5 or clicking the vanilla `Play` button immediately launches Stage 13-1 (`space`) via `StartAdventure(app, 12)` using Hook Site P (`0x004334C0`) and export `ZTK_GetSelectedStage`.
  - **DevVIP Testing Cheat (Middle Mouse Click / `VK_MBUTTON`)**:
    - Available strictly for the `DevTester` profile during active stage gameplay (`Board`).
    - **Dynamic Score Differential**: Computes `diff = targetScore - currentScore` and invokes `Board::IncScore(board, diff, 1)` to instantly fill the Zuma progress bar to 100%.
    - **Floating Score Text**: Spawns PopCap `FloatingText` displaying `+<diff>` (e.g. `+8000`) floating upward directly from the mouse cursor coordinates (`ScreenToClient`), rendered in signature DevTester neon cyan-blue (`0x00E5FF`) with combo font.
    - **Instant `destroyall` & Level Clear**: Forces `mDestroyAll = 1` (`[board + 0x15B] = 1`), plays `SOUND_BAR_FULL`, triggers Zuma bar flash timers (`0xE0` and `0x32`), and executes `Board::WinLevel(board, 1, 0)` to instantly explode all balls on every curve and clear the stage immediately.
    - **Zero Hardcoded Addresses**: Injects Hook Site Q at `Board::Update` (`0x0040D330`) with Cave Q in section `.ztk`, dynamically resolving all internal engine routines via runtime PE AOB signature scanning.
  - **Custom DevTester Guide (`HelpScreen::Draw` Replacement @ `0x00437CA8` Hook Site R)**:
    - Replaces the standard 3-panel How-To-Play `HelpScreen` when `DevTester` is active (`ZTK_DrawHelpScreen` via Hook Site R at `0x00437CA8`).
    - Custom stone tablet frame (`helpfront_dev.jpg` + `_helpfront_dev.gif` alpha mask & `helpback_dev.jpg` parchment background).
    - Header: `"DEVTESTER GUIDE"` (`FONT_TITLE32`, gold `#FFEA01`) and subtitle `"Welcome, DevTester! Exclusive privileges unlocked:"` (`FONT_MAIN12`, cyan `#00E5FF`).
    - **Adventure Mode Column**: Pre-rendered 0.75x `CancunFloat14` header (`tut_title_adv.gif`), glowing green-eyed Secret Temple 5 preview (`advtemple5_tut.jpg`, `190x75`), and word-wrapped privilege summary.
    - **Gauntlet Mode Column**: Pre-rendered 0.75x header (`tut_title_gaunt.gif`), 5 animated 50-frame ball sprites rotated 90 degrees counter-clockwise (`tut_ball_red.gif`, `tut_ball_yellow.gif`, `tut_ball_green.gif`, `tut_ball_blue.gif`, `tut_ball_purple.gif`) with synced vertical rolling animation, and Sun God rank privilege summary.
    - **Middle Mouse Button - Instant Win Section**: Pre-rendered 0.75x cyan header (`tut_title_mmb.gif`), middle-mouse icon (`tut_mouse_m.jpg` + `tut_mouse_m_.jpg`), and wrapped description of the MMB instant-win mechanic.
- **Standalone Developer Credits Sidecar (`zumacplxcredit.dll` & `builder/patch_zccredit.py`)**:
  - Extracted developer credits logic from `zumaextrapath.dll` into its own dedicated sidecar `zumacplxcredit.dll` and pure RX section `.zcc`.
  - Appends custom developer credits at the bottom of the Zuma Credits roll (42 entries total):
    - Title: `ZuCompleluxe Project` (rendered with scaled Native Alien font at 66.67% scale).
    - Role: `Developer` (`FONT_HEADER`).
    - Name: `Rizky Bayuu "Lamp?x"` (`FONT_NAME`).
    - Interactive Button: 3-state `GitHub` stone button (`DialogButton.jpg`, 120x48) with hover highlight, `IDC_HAND` cursor, press state, and `ShellExecuteA` click handler opening `https://github.com/rizkybayuu/ZuCompleluxe`.
- **Direct Installation Folder Patcher (`build_full.bat` & `build_lite.bat`) & Automated Asset Synthesis (`patch_assets.py`)**:
  - **Direct Game Folder Patching**: `build_full.bat` (Full Edition) and `build_lite.bat` (Lite Edition) operate directly on any target Zuma Deluxe installation folder.
  - **Modern Folder Selection Dialog (`builder/select_folder.ps1`)**: Native Windows Explorer `IFileOpenDialog` (`FOS_PICKFOLDERS`) with a custom `"Patch Folder"` button, alongside CLI argument (`build_full.bat "<path>" /yes`) and console fallback support.
  - **Centralized Backup Vault (`modules\bak\`)**: Automatically backs up original vanilla `Zuma.exe`, `resources.xml`, `levels.xml`, and `baDotz.gif` into `modules\bak\`, migrating any legacy `.bak` files cleanly.
  - **Automated 11-Color, Temple 5 & DevTester HelpScreen Asset Generator (`builder/patch_assets.py` & `color_tables.json`)**:
    - Synthesizes all 30 extra ball and powerup GIF sprites (`Orange`, `Navy`, `Cyan`, `Black`, `Cream` across `Accuracy`, `Backwards`, `Ball`, `Bomb`, `Light`, `Slow`) from vanilla GIFs using exact palette LUTs (`color_tables.json`) and affine RGB fallback transforms.
    - Automatically extends `images/baDotz.gif` from 6 to 11 frog back-dots.
    - Generates `images/DialogButtonLocked.jpg` and `images/_Dialogbuttonlocked.gif` for locked UI buttons.
    - Deploys Secret Temple 5 (`advtemple5.jpg`, `_advtemple5.gif`, `advdoor5.jpg`) and synthesizes DevTester HelpScreen media (`advtemple5_tut.jpg`, `tut_title_*.gif`, `tut_ball_*.gif`).
    - Updates `properties/resources.xml` and `levels/levels.xml` (scaling stage color counts up to 11 colors) idempotently.

### Fixed
- **Off-by-3 Jump Displacement Crash in `.zcol` Image Getter Thunks (`0x0058D083`)**:
  - Fixed an Access Violation crash (`0xc0000005` at `0x0058D083`) when `zumaextracolor.dll` was removed. Replaced hardcoded `\x75\x0B` displacement in `make_getter` with `\x75\x05` (skipping the 5-byte `mov eax, [va_default]` instruction cleanly to `ret`), preventing mid-instruction execution (`0x58` / `pop eax`) and stack pointer misalignment.
- **Off-by-1 Jump Displacement in `.zcol` Adventure Color Clamp Loop**:
  - Fixed a conditional jump displacement (`\x7E\x07` -> dynamic `+6` bytes over `mov dword ptr [edi], 6`) in `builder/patch_extracolor.py` when clamping Adventure curves without `zumaextracolor.dll`.
- **Off-by-1 Jump Displacement in `.zcc` Credits Table Fallback (Cave A)**:
  - Fixed a conditional jump displacement (`\x74\x06` -> dynamic `+7` bytes over 6-byte `call [ZCC_GetCreditsTable]` + 1-byte `ret`) in `builder/patch_zccredit.py`, ensuring `mov eax, orig_table` executes properly when `zumacplxcredit.dll` is absent.
- **Missing Skulls When `zumaextrapath.dll` is Removed**:
  - Added Cave M (`va_cave_hole_bg_end`) at `STAGE2_HOLE_BG_END` (`0x0040727C`) and updated Cave L (`va_draw_static_board`) in `builder/patch_extrapath_stage2.py` so that when `ZEP_CheckHolePriority` is null, `FUN_00407250` executes its native hole rendering loop instead of jumping unconditionally to the function epilogue.
- **Stage 13 Banner Occlusion & Temple 5 Alpha Mask (`0x0043653A` Hook Site O & `_advtemple5.gif`)**:
  - Registered `alphamask="_advtemple5"` in `resources.xml` and `ZTK_OnRegisterResources` so `advtemple5.jpg` renders with clean alpha transparency instead of opaque black bounding boxes.
  - Added Site O (`0x0043653A`) with Cave O (`va_cave_o`) replacing `jl 0x00436781` (6 bytes). When `[MapScreen + 0x1B8] < 0` and Door 5 is selected, Cave O invokes `ZTK_DrawStage13Banner` on top of Temple 1 to render the authentic `STAGE 13` stone badge (`IMAGE_ADVSTAGE` + `"13"` in `FONT_MAIN14`), `"THE VOID"` in `FONT_TITLE32` (`#FFFFFF`), and `"THE PLACE OF NO ENDING"` in `FONT_BROWSER` (`#FFEA01`).
  - Automatically initializes selection to Door 5 upon entering Adventure mode as `DevTester` (`[MapScreen + 0x1B8] >= 12`), matching vanilla's behavior of selecting the highest unlocked stage.
- **Play Button Launch Dispatch for Stage 13 / Door 5 (`0x004334C0` Hook Site P)**:
  - Added Site P (`0x004334C0`) and Cave P (`va_cave_p`) in section `.ztk` (replacing 10 bytes: `mov eax, [edx + 0x160]` + `test eax, eax` + `jl 0x4334d9`), calling `ZTK_GetSelectedStage(MapScreen*)` so clicking PLAY with Door 5 selected launches Stage 13-1 (`space`).
  - When `zumatestingkit.dll` is absent, Cave P executes verbatim vanilla fallback logic, maintaining 100% graceful degradation.

---

## [0.3.0.3] - 2026-10-16

### Added
- **Priority-Aware Skull (Hole) Rendering Subsystem**:
  - Implemented dynamic priority tracking for all skulls (holes): each skull now inherits its rendering priority (`0..4`) from the terminal waypoint (`waypoint[count - 1].priority`) of the curve feeding into it (with multi-curve shared skulls adopting the maximum priority among feeding curves).
  - Implemented strict intra-priority layer ordering: within the same priority level `P`, elements are drawn in the order `Overlay (Cutout/Sprite, prio P)` -> `Skull (prio P)` -> `Balls (prio P)`. Same-priority cutouts and sprites no longer occlude skulls.
  - Seamless inter-priority occlusion: cutouts and sprites with higher priority (`prio > P`) naturally cover skulls with lower priority (`prio P`), while lower-priority elements (`prio < P`) render beneath them.
  - Full screen support: Priority-aware layering operates across gameplay (`Board::Draw`), board rendering, level preview/intro, and level start/completion transitions (`FUN_00407300`).
- **Dynamic Patcher Site Expansion**:
  - Added 4 new Stage 2 hook and write sites to `site_resolver.py` (`STAGE2_OVERLAY_DRAW`, `STAGE2_HOLE_BG_END`, `STAGE2_HOLE_DRAW_LOOP`, `STAGE2_STATIC_BOARD_DRAW`), expanding total resolved sites from 59 to 63 (57 total write sites).
  - All branch targets dynamically extracted via Capstone without hardcoded virtual addresses.
- **Sidecar Hole Storage Expansion**:
  - Expanded `ZepHoleData` in `zumaextrapath.dll` to support up to 32 hole priority entries across 16 round-robin instances to guarantee zero slot exhaustion across long gameplay sessions.
### Fixed
- **Credits Screen HoleMgr Buffer Overflow Crash**:
  - Fixed an Access Violation crash (`0xc0000005` at `0x004017A8`) occurring upon entering the Credits screen (e.g. by clicking the Main Menu UFO easter egg or completing the game).
  - Cause: In vanilla Zuma, `CreditsScreen` (`FUN_00430490`) allocated only `300` bytes (`0x12C`) for its internal `HoleMgr` instance used to render space starfield graphics (`images/nebula1`). With `HOLEMGR_MAX_HOLES` expanded to 10 in Stage 1, `HoleMgr`'s constructor initialized 10 hole objects (`0x2B4` bytes), overflowing the 300-byte allocation and writing into unmapped heap memory.
  - Fix: Added `STAGE1_HOLEMGR_CREDITS_ALLOC` write site (`0x004304E8`), expanding `CreditsScreen`'s `HoleMgr` heap allocation to `HOLEMGR_BUFFER_SIZE` (`0x308` / 776 bytes).
  - Expanded dynamic site resolver from 63 to 64 sites (58 total write sites).

---

## [0.3.0.2] - 2026-10-16

### Added
- **Adaptive Executable Staging in `build.bat`**:
  - Direct folder path ingestion: Users can specify either a direct executable path (`Zuma.exe`) or the parent directory containing the game (e.g., `"D:\...\Zuma800x600"`), with automatic handling of surrounding quotes and trailing slashes.
  - Automatic executable discovery: If a directory is specified, the builder dynamically locates and stages `Zuma.exe`, `Zuma800x600.exe`, or any available `*.exe` without producing corrupt directory metadata files.
- **Polite Compatibility Advisory & Centralized Failure Handling**:
  - Unified error handling across all 5 patch scripts (`patch_imports.py`, `patch_extracolor.py`, `patch_anim_hook.py`, `patch_extrapath.py`, `patch_extrapath_stage2.py`).
  - Added centralized `:PatchFailed` handler in `build.bat` displaying clear technical error details alongside a polite, non-coercive recommendation:
    `(RECOMMENDED: Zuma Deluxe v1.0.0.1 GameHouse / PopCap Retail)`.

### Changed
- **Eliminated Rigid Upfront Byte-Size and Hash Gatekeeping**:
  - Removed rigid `1,292,157` byte-size checks and upfront SHA-256 rejection. The builder now attempts dynamic AOB patching on any valid PE executable provided.
  - Verified 100% full patching compatibility with unpacked DRM-free binaries and widescreen community editions (`Zuma800x600.exe`, 1,290,240 bytes).
- **Eliminated Hardcoded Sibling Directory Sniffing**:
  - Removed automatic background scanning of arbitrary sibling folders (`..\Zuma Deluxe - Vanilla`, `Clean Assets`, `ProgramFiles`, etc.), ensuring explicit, predictable user-controlled input paths.

---

## [0.3.0.1] - 2026-10-15

### Fixed
- **Strict `levels.xml` Asset Resolution in `zumawebp.dll`**:
  - Eliminated automatic folder-to-`bg.webp` redirection in `resolve_webp_path`: previously, any level with `image="<level_id>"` (such as `image="spiral"`) was hijacked to load `bg.webp` if present in the level folder. The loader now strictly follows the declared attribute in `levels.xml`.
  - Removed blind `try_candidate` fallback in `find_bg_path`: non-WebP backgrounds (vanilla JPG/PNG assets) are no longer overridden at runtime by unreferenced `bg.webp` files.
  - Full backward compatibility: levels explicitly specifying `image="bg"` continue to load and animate `bg.webp` as intended.

---

## [0.3.0] - 2026-10-15

### Added
- **Dynamic AOB & Capstone Structural Signature Resolver (`site_resolver.py`)**:
  - Dynamically resolves all 59 engine sites (38 Stage 1, 15 Stage 2, 4 Engine Config symbols, 1 read reference, and 1 count function) from vanilla baseline using signature patterns and Capstone x86 operand inspection.
  - Completely eliminated brittle hardcoded virtual addresses.
- **Dynamic Engine Config v2 (`.zdata` @ +0x400)**:
  - Injected 52-byte runtime engine configuration passing engine RVAs (`Curve::Curve`, `CurveDesc::CurveDesc`, `std::string::assign`, `Curve::Init`, `op_new`, etc.), `sizeof(Curve)=0x84`, and dynamic `max_holes=10`.
  - Single Source of Truth architecture: constants shared directly between patcher and sidecar DLL.
  - Automatic fail-safe fallback to vanilla mode (3 holes, vanilla curves) if config is invalid or out of bounds.
- **10-Hole Skull & Multi-Path Subsystem**:
  - Unlocked `HoleMgr` proxy guards in `zumaextrapath.dll` to support up to 10 distinct skull endpoints.
  - Full visual rendering and independent mouth-opening animation for all 10 paths simultaneously.
- **5-Gate Strict Acceptance Test Suite (`test_aob_scan.py`)**:
  - Gate 1: Cryptographic SHA-256 hash validation across all 4 mod binaries.
  - Gate 2: Nominal VA assertions for all 59 sites.
  - Gate 3: Negative tamper and patched binary rejection tests.
  - Gate 4: Bidirectional byte-by-byte diff audit (proving exactly 263 modified bytes in `.text`, 0 unowned diffs).
  - Gate 5: Zero ghost sites, zero overlapping sites.

### Fixed
- **Definitive Resolution of Ball Waypoint Crash (PageHeap UAF / Out-of-Bounds)**:
  - Repointed `Ball::SetWayPoint` (`0x00401D20`) and `Ball::GetWayPoint` (`0x00401D40`) to dynamic sidecar implementations, eliminating hardcoded 3-curve bounds in the `Ball` struct.
- **Multi-Curve Loop Clamping & Crash Preventions**:
  - Clamped `TreasurePoint` node distance loop (`0x00413D0B` / `0x00413D48`) to `min(mNumCurves, 3)` via pure RX cave in `.zep2`, preventing heap overread beyond the fixed 3-element distance array.
  - Clamped `DescReset` loop (`0x00421BF7` / `0x00421C61`) to `min(mNumCurves, 3)` via `.zep2` cave (`va_clamp_ecx` & `va_clamp_eax`), preventing heap overwrite past `LevelDesc`.
  - Comprehensive audit of all 30+ curve-iterating loops in the engine.
- **Multi-Skull Visual Bug (Missing Skulls on Path 9 & 10)**:
  - Relieved hardcoded `num_holes < 3` and `h >= 3` checks in `zumaextrapath.dll`, allowing distinct curve endpoints to provision physical skull holes and render all skulls in multi-path levels (`spiral`).

---

## [0.2.0] - 2026-10-14

### Added
- **Dedicated 11-Color Sidecar (`zumaextracolor.dll`)**:
  - Implemented dynamic ball color progression across both Adventure and Gauntlet modes.
  - Adventure mode dynamically maps stage progression (4 up to 11 colors), while Gauntlet mode scales by rank (Rabbit: 4, Eagle: 6, Jaguar: 8, Sun God: 10/11).
  - Explicit `colors="X"` attributes in `levels.xml` cleanly override dynamic progression and propagate across all curve slots.
  - Runtime std::map tree walker dynamically extracts mod image assets (`IMAGE_<COLOR>_<KIND>`) directly from PopCap's resource manager without hardcoded pointers.
  - Implemented sentinel constructor patch in `CurveDesc` (`mNumColors = 0`) to distinguish explicit XML overrides from unassigned curves.
- **Pure RX `.zcol` Trampoline Architecture**:
  - Introduced `.zcol` pure executable section (Execute/Read) for trampoline stubs and IAT forwarding.
  - Eliminated raw `.text` padding code caves (formerly 0x4FD900 cave) and abolished all W+X (Writable + Executable) section flags.
  - Preserved `.rdata` read-only characteristics (`0x40000040`) and authentic PopCap DRM entry point `0x00589000`.
- **Clean 4-File Mod Distribution Model**:
  - Removed redundant commercial vanilla libraries (`zumares.dll`, `bass.dll`) from build output and prerequisite checks.
  - Distribution produces strictly the 4 modified files (`Zuma.exe`, `zumawebp.dll`, `zumaextrapath.dll`, `zumaextracolor.dll`).
- **PE Security & Integrity Audit Tool (`verify_pe.py`)**:
  - Validates section permissions (no W+X, `.rdata` read-only, `.zcol` pure RX), entry point authenticity, IAT integrity, and DLL export presence.

### Removed
- Removed legacy monolithic patcher `builder/build_mod_exe.py` (fully replaced by `builder/patch_extracolor.py` and `zumaextracolor.dll`).
- Removed obsolete `src/zumares.dll.helper.c` and `src/zumares.dll.def`.

---

## [0.1.1] - 2026-10-14

### Added
- **Smart WebP Asset Resolution**:
  - `zumawebp.dll` now automatically detects and loads `.webp` files even when referenced without file extensions in `levels.xml` (e.g., `image="bg"` and `<Sprite image="sprite" />` seamlessly resolve to `bg.webp` and `sprite.webp`).
- **Format Priority Hierarchy**:
  - Implemented strict format priority: if multiple image formats are present (`bg.webp`, `bg.png`, `bg.jpg`, `bg.bmp`, `bg.img`), WebP is always prioritized as #1.
  - Seamless fallback to Zuma's vanilla asset loader (`.jpg`, `.png`, `.gif`) if no matching WebP asset is located.

### Changed
- Relaxed string length constraints (`size < 1` instead of `size < 6`) in `Anim_LoadImage` to allow short asset names like `bg`.
- Removed hardcoded extension filtering in `Anim_LoadImage` and level memory scanner (`try_explicit_path`).

---

## [0.1.0] - 2026-10-13

Initial milestone release establishing the core binary modification framework engineered to power the upcoming **Zuma Compleluxe** total conversion mod.

### Added
- **Multi-Curve Architecture**:
  - Full support for up to 10 simultaneous ball paths per level (`curve` through `curve10`), overcoming the engine's hardcoded 3-curve limitation.
  - Rewritten Board curve array references across all engine game loops and transition routines for complete runtime stability.
- **Dynamic Skull Hole Subsystem**:
  - Expanded `HoleMgr` heap layout to `0x308` bytes (10 inline `Hole` objects + 7 extra curve maps, percentages, and aux values).
  - Proxy hooking for `HoleMgr::AddHole` (`0x00402860`), `HoleMgr::SetHolePercent` (`0x00402790`), and `HoleMgr::SetHoleSomething` (`0x00402810`) routed via `zumaextrapath.dll`.
  - Automatic hole reuse for curves ending at shared skull endpoints (distance threshold < 20 px).
  - Merged skull mouth opening percentage calculation (`max` across all curves feeding into the hole).
  - Dedicated staging reset synchronization (`ZEP_HoleReset`) ensuring clean level transitions.
- **11-Color Ball Extension**:
  - Custom palette definitions in `levels.xml` allowing levels with up to 11 distinct ball colors.
- **Animated WebP Backgrounds & Multi-Layer Sprites**:
  - `zumawebp.dll` sidecar utilizing `libwebp` to stream animated `bg.webp` backgrounds and dynamic sprite overlays.
  - Runtime hooks on `PreDraw` and `LoadImage` without unpacking `.pak` archives.
- **Automated Build Pipeline**:
  - `build_binary.bat`: One-click master builder detecting toolchains (MSVC / MinGW-w64).
  - Python-based PE injectors in `builder/` (`build_mod_exe.py`, `patch_anim_hook.py`, `patch_extrapath.py`, `patch_extrapath_stage2.py`).
- **Comprehensive Technical Documentation**:
  - Detailed audit notes, disassemblies, loop indexes, and struct memory maps in `docs/`.
  - Thorough legal disclaimers and licensing documentation in `LICENSE.md`.
- **Production Silent Mode**:
  - Defaulted all sidecar DLLs (`zumaextrapath.dll`, `zumawebp.dll`, `zumares.dll`, `bass.dll`) to silent mode, eliminating disk writes and log file clutter.
