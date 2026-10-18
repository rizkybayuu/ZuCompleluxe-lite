# ZuCompleluxe Lite (v0.4.0.1)

> **Version:** 0.4.0.1 (Lite Edition)  
> **Target Game:** *Zuma Deluxe* v1.0.0.1 (GameHouse / PopCap / Regional PE Builds)  
> **Project Scope:** Lightweight Core Engine Framework for Zuma Deluxe Modding  
> **Developer:** [Rizky Bayuu](https://github.com/rizkybayuu)  
> **Full Edition:** [ZuCompleluxe (Full Edition)](https://github.com/rizkybayuu/ZuCompleluxe)  
> **License:** [MIT License](LICENSE.md)  
> **Changelog:** [CHANGELOG.md](CHANGELOG.md)

**ZuCompleluxe Lite** is a lightweight edition of the ZuCompleluxe reverse-engineering framework for *Zuma Deluxe* (v1.0.0.1). It provides the core pre-launch integrity gatekeeper (`zumadllchecker.dll`), Gauntlet custom map carousel (`zumacustommap.dll`), developer credits (`zumacplxcredit.dll`), and developer testing kit (`zumatestingkit.dll`), while omitting experimental multi-path, 11-ball colors, and animated WebP sidecars.

This framework represents the foundational engine layer (**v0.4.0.1**) engineered specifically to power **Zuma Compleluxe**—an upcoming, highly ambitious total conversion mod currently in active development.

> [!NOTE]
> **Full Edition Status & Availability:**  
> The complete framework with full capabilities—including multi-path (up to 10 curves), 11-ball colors, and animated WebP rendering—is hosted at [ZuCompleluxe](https://github.com/rizkybayuu/ZuCompleluxe). The Full Edition is currently in private development and will be made **public** simultaneously with the official release of the **Zuma Compleluxe** mod.

---

## Core Architecture & Engineering Philosophy

The framework is built upon four core reverse-engineering pillars:

### 1. `Zuma.exe`-Exclusive Binary Patching & `modules\` Isolation
Modifications are isolated cleanly from vanilla game files:
- **Exclusive Executable Modifications**: All engine modifications, hook stubs, and feature extensions are concentrated 100% within `Zuma.exe` through native PE section injection (`.zdata`, `.zldr`, `.zcol`, `.anim`, `.zep`, `.zep2`, `.zcm`, `.zcc`, `.ztk`).
- **Dedicated `modules\` Subdirectory**: All 7 custom DLLs (`zumadllchecker.dll`, `zumawebp.dll`, `zumaextrapath.dll`, `zumaextracolor.dll`, `zumacustommap.dll`, `zumacplxcredit.dll`, `zumatestingkit.dll`) are housed inside `modules\` to keep the root game directory clean, while original vanilla files are backed up automatically to `modules\bak\`.
- **Untouched Commercial Dependencies**: Third-party commercial retail dependencies—specifically `zumares.dll` (GameHouse DRM handshake and CRT startup decoder) and `bass.dll` (Un4seen audio engine)—remain 100% untouched vanilla.

### 2. Mandatory Gatekeeper (`zumadllchecker.dll`) & Pre-Launch Diagnostics
Unlike the 6 feature sidecars, `modules\zumadllchecker.dll` is a **strictly binding dependency**:
- **Hard Gatekeeper Enforcement in `.zldr` (`0x0058C000`)**: Before loading any optional feature modules or executing game code, `.zldr` enforces `modules\zumadllchecker.dll`. If `zumadllchecker.dll` is missing or fails to load, `Zuma.exe` displays a modal error dialog and immediately terminates via `ExitProcess(1)`.
- **Dynamic PE Version Metadata Extraction**: Reads `ProductName`, `CompanyName`, `FileVersion`, `SpecialBuild`, and `FileName` dynamically from `Zuma.exe`'s Win32 PE Version Resource (`VS_FIXEDFILEINFO` and `\VarFileInfo\Translation`) with zero hardcoded game strings.
- **Classic Adaptive Diagnostic Dialog**: Displays game metadata, patch version (`ZuCompleluxe v0.4.0.1`), developer link (**Rizky Bayuu** -> `https://github.com/rizkybayuu`), license (`MIT License`), and real-time presence checks for all 7 modules in `modules\`, adapting automatically to Windows Light/Dark mode (`AppsUseLightTheme` + DWM Immersive Dark Mode).

### 3. 100% Independent & Optional Feature Sidecars (Zero-Dependency Graceful Degradation)
Every feature sidecar DLL in ZuCompleluxe is completely optional and independent:
- **Dynamic Runtime Sidecar Loader (`.zldr` @ `0x0058C000`)**: `DataDirectory[1]` (Import Directory) is terminated immediately after the vanilla DLL descriptors so the Windows PE loader never aborts startup if optional feature sidecars are removed. At startup (after `zumadllchecker.dll` approval), `.zldr` dynamically loads present feature DLLs from `modules\` via `LoadLibraryA` and populates `.zdata` IAT slots via `GetProcAddress` before jumping to the original PopCap entry point (`0x00589000`).
- **Airtight Per-Hook IAT Fallbacks**: Every code cave across `.zcol`, `.anim`, `.zep2`, `.zcm`, `.zcc`, and `.ztk` performs a null-check on its `.zdata` IAT pointer before calling into any sidecar:
  - **Missing `zumaextracolor.dll`**: In-engine fallback copies all 36 vanilla ball/powerup `Image*` globals into `.zdata` lookup arrays and clamps Adventure/Gauntlet curves to `<= 6` colors.
  - **Missing `zumaextrapath.dll`**: Curves 4..10 are safely ignored (`<= 3` curves), and `STAGE2_HOLE_BG_END` (Cave M) + `STAGE2_STATIC_BOARD_DRAW` (Cave L) automatically restore the vanilla `FUN_00407250` hole drawing loop so skulls and glowing eye animations render normally.
  - **Missing `zumacustommap.dll`**: Gauntlet mode falls back to the 12 vanilla stages and standard level pickers.
  - **Missing `zumacplxcredit.dll`**: Credits roll falls back to the vanilla credit table (`0x00503A08` / `0x0054948C`) and standard fonts.
  - **Missing `zumatestingkit.dll`**: User profile dialog, stage unlocking, Gauntlet defaults, Help screen, and button rendering run as pure vanilla.
  - **Missing `zumawebp.dll`**: Image loader falls back directly to PopCap's built-in ImageLib (`.jpg`, `.png`, `.gif`).

### 4. Semantic Unique Attributes over Rigid Hardcoded Addresses (AOB Signature Scanning)
Legacy patches rely on brittle hardcoded virtual addresses that break across binary re-alignments and distributor releases. ZuCompleluxe replaces static address patching with **Dynamic AOB (Array of Bytes) & Capstone Structural Scanning**:
- **Semantic Instruction Fingerprinting**: The patcher scans raw `.text` opcodes using semantic instruction fingerprints (allocation calls with `push 0x12C`, register swap sequences, unique struct displacements such as `[esi + 0x5C]`, and opcode color divisors) rather than static addresses.
- **In-Place Unique Attribute Modification**:
  - Allocation capacity: `push 0x12C` (300 B) -> `push 0x308` (776 B)
  - Vector constructor bounds: `push 3` (`6A 03`) -> `push 10` (`6A 0A`)
  - Frog ball color divisor: `6` -> `11`
  - Sentinel descriptor status: default `mNumColors` set to `0` to discriminate explicit XML overrides from unassigned slots.
  - Loop redirection: Active and staging curve pointer arrays repointed to dynamic heap arrays.
- **Dynamic Runtime Config Injection (`.zdata` @ `+0x400`)**: Engine function RVAs (`Curve::Curve`, `CurveDesc`, `std::string::assign`, `Curve::Init`, `op_new`, `Graphics::DrawImage`, `Graphics::SetFont`, `Graphics::SetColor`, font pointers, `IMAGE_DIALOG_BUTTON`, `ZumaApp`, etc.) and buffer limits (`max_holes=10`) are resolved at build time and injected into `ZEP_EngineConfig` v3 in section `.zdata`.

---

## Key Features

- **Pre-Launch Module & Dependency Checker (`zumadllchecker.dll`)**:
  - Mandatory gatekeeper dialog executed prior to engine initialization.
  - Dynamically queries PE version resources (`Product`, `Publisher`, `Version`, `Special Build`, `Target File`) alongside `ZuCompleluxe v0.4.0.1` metadata, `MIT License` notice, and an interactive hyperlink to [Rizky Bayuu](https://github.com/rizkybayuu).
  - Verifies the status of all 7 sidecar modules inside `modules\` (`[OK] Required`, `[OK] Present`, `[MISSING]`) with `Run` and `Quit` controls.
- **1-Click Direct Installation Folder Patcher (`build_lite.bat`)**:
  - Opens a modern Windows Explorer folder selection dialog (`builder/select_folder.ps1`) featuring a custom **"Patch Folder"** button (or accepts a target directory via CLI `build_lite.bat "<path>" /yes`).
  - Automatically backs up vanilla files (`Zuma.exe`, `resources.xml`, `levels.xml`, `baDotz.gif`) into `modules\bak\`.
  - Automatically synthesizes all 30 extra ball/powerup GIF assets, extends `baDotz.gif` to 11 dots, generates locked button textures, deploys Secret Temple 5 (`advtemple5.jpg`, `_advtemple5.gif`, `advdoor5.jpg`) and DevTester HelpScreen media, and patches `resources.xml` and `levels.xml` via `builder/patch_assets.py`.
- **Gauntlet Custom Map Subsystem (`zumacustommap.dll` & `.zcm`)**:
  - Automatically discovers custom maps defined in `levels/levels.xml` outside of Adventure `StageProgression` (up to 256 custom maps) and adds them to the Gauntlet carousel.
  - Supports custom `<Gauntlet>` XML tags and preserves natural 4-tier rank progression (7 stages -> Eagle, 14 -> Jaguar, 21 -> Sun God).
- **Developer Testing Kit (`zumatestingkit.dll` & `.ztk`)**:
  - Activated exclusively when `zumatest.log` exists in the game folder; completely inert during normal play.
  - Injects a zero-disk-pollution virtual user profile **`DevTester`** directly beneath `(CREATE A NEW USER)` in `UserDialog`, rendered in glowing neon cyan-blue (`0x00E5FF`) alongside `"Welcome to Zuma, DevTester!"` on the Main Menu.
  - Unlocks all 12 Adventure stages and all Gauntlet maps (vanilla and custom) straight to **Sun God** rank via an in-memory MSVC 7.1 Dinkumware Red-Black Tree (`std::map<std::string, short>`).
  - Defaults Gauntlet selection for `DevTester` to **Random** map and **Practice** mode.
  - Visually locks `Rename` and `Delete` buttons with custom grayscale stone textures when `DevTester` is selected and displays a centered native PopCap warning dialog (`"ACTION NOT ALLOWED"`) if clicked.
  - **Secret Temple 5 (`advtemple5` + `_advtemple5.gif`) & Door 5 (`advdoor5`) (Stage 13: "The Void")**:
    - Unveils a secret 5th temple and doorway in Adventure mode MapScreen exclusively for `DevTester` (`zumatest.log`).
    - Layered strictly above `advtemple4` and beneath `advtemple1..3` with proper alpha mask transparency (`_advtemple5.gif`) and dynamic 50% opacity fog blending via `IMAGE_ADVMIDDLE`.
    - Features a 3-state doorway (Open/Black, Hover/Green with hand cursor, Clicked/Red with custom Stage 13 banner `"THE VOID: THE PLACE OF NO ENDING"`).
    - Selecting Door 5 safely maintains `[MapScreen + 0x1B8] = -1` in an isolated virtual state, completely avoiding vanilla out-of-bounds stack array crashes (`local_48`/`local_dc`).
    - Double-clicking Door 5 or pressing Play immediately launches Stage 13-1 (`space`).
  - **Middle Mouse Button (`VK_MBUTTON`) Instant Win Cheat**:
    - Pressing the Middle Mouse Button during gameplay as `DevTester` computes the exact score differential to fill the Zuma bar (`Board::IncScore`), spawns upward-floating cyan score text (`+<diff>`), triggers `mDestroyAll`, and invokes `Board::WinLevel` to clear the stage immediately.
  - **Custom DevTester Guide (`HelpScreen::Draw` Replacement @ `0x00437CA8`)**:
    - Replaces the standard How-To-Play screen when `DevTester` is active with a custom two-column + bottom section **DevTester Guide** (`helpfront_dev.jpg`, `helpback_dev.jpg`).
    - Showcases Adventure Mode (with glowing green-eyed `advtemple5_tut.jpg` preview), Gauntlet Mode (with 5 animated 90-degree rotated rolling ball sprites `tut_ball_*.gif`), and the Middle Mouse Button Instant Win guide (`tut_mouse_m.jpg`).
- **Standalone Interactive Developer Credits (`zumacplxcredit.dll` & `.zcc`)**:
  - Appends custom developer credits at the bottom of the credits roll (`ZuCompleluxe Project` in scaled Native Alien font, `Developer`, `Rizky Bayuu "Lamp?x"`) with an interactive 3-state `GitHub` stone button (`DialogButton.jpg`) that opens the repository URL via `ShellExecuteA`.
- **Multi-Curve Architecture (Up to 10 Paths)**:
  - Bypasses the native 3-curve limitation (`curve`, `curve2`, `curve3`), providing scalable runtime support for up to 10 concurrent active ball curves (`curve4` through `curve10`) per level.
- **Dynamic Multi-Skull Subsystem (10 Holes) & Priority-Aware Rendering**:
  - Native physical hole allocation for up to 10 skulls with automatic multi-curve mouth-opening interpolation (`max` percentage across feeding curves).
  - Priority-aware layering (`Overlay [prio P] -> Skull [prio P] -> Balls [prio P]`) ensuring same-priority cutouts never occlude skulls while higher-priority cutouts cover lower-priority skulls naturally.
- **Dedicated 11-Color Sidecar (`zumaextracolor.dll` & `.zcol`)**:
  - Expands Zuma's 6 ball colors to 11 colors (`Blue`, `Yellow`, `Red`, `Green`, `Purple`, `White`, `Orange`, `Navy`, `Cyan`, `Black`, `Cream`) with dynamic stage/rank progression and PopCap resource tree walking.
- **Animated WebP Backgrounds & Sprites (`zumawebp.dll` & `.anim`)**:
  - Integrated `libwebp` demuxing and decoding enabling animated backgrounds (`bg.webp`) and multi-layer sprites with extensionless XML resolution and `#1` WebP priority.
- **Pure RX Code Caves & Clean PE Compliance (`verify_pe.py`)**:
  - Zero code caves in `.text` padding, zero writable-executable (W+X) code caves, `.rdata` strictly read-only (`0x40000040`), and authentic PopCap DRM entry point `0x00589000`.

---

## Resolved Critical Bugs (from v0.2.0 to v0.4.0.1)

### 1. Independent Sidecar Fallback & Displacement Bugs (Resolved in v0.4.0.1)
- **`.zcol` Image Getter Thunk Displacement Crash (`0x0058D083`)**: Replaced an off-by-3 hardcoded jump (`\x75\x0B`) in `make_getter` with `\x75\x05` (skipping the 5-byte `mov eax, [va_default]` cleanly to `ret`), preventing mid-instruction execution (`0x58` / `pop eax`) and stack pointer misalignment when `zumaextracolor.dll` is removed.
- **`.zcol` Adventure Color Clamp Displacement**: Replaced an off-by-1 jump (`\x7E\x07` -> dynamic `+6` bytes over `mov dword ptr [edi], 6`) in `builder/patch_extracolor.py` when clamping Adventure curves without `zumaextracolor.dll`.
- **`.zcc` Credits Table Fallback Displacement**: Replaced an off-by-1 jump (`\x74\x06` -> dynamic `+7` bytes over `call [ZCC_GetCreditsTable]` + `ret`) in `builder/patch_zccredit.py` so `mov eax, orig_table` executes properly when `zumacplxcredit.dll` is missing.
- **Missing Skulls Without `zumaextrapath.dll`**: Added Cave M (`va_cave_hole_bg_end`) at `STAGE2_HOLE_BG_END` (`0x0040727C`) and updated Cave L (`va_draw_static_board`) so `FUN_00407250` runs its native hole rendering loop when `zumaextrapath.dll` is absent.

### 2. Multi-Path & Heap Safety Bugs (Resolved in v0.3.0 - v0.3.0.3)
- **`CreditsScreen` HoleMgr Buffer Overflow (`0x004017A8`)**: Expanded `CreditsScreen`'s internal `HoleMgr` heap allocation (`0x004304E8`) from `0x12C` (300 B) to `0x308` (776 B) to match the 10-hole constructor.
- **`Ball::mNearestWaypoint` Out-of-Bounds (`0x00401D46`)**: Guarded `Ball::SetWayPoint` (`0x00401D20`) and `Ball::GetWayPoint` (`0x00401D40`) against curve indices `>= 3`.
- **`TreasurePoint` & `DescReset` Loop Overruns**: Installed semantic clamp caves (`min(mNumCurves, 3)`) in `.zep2` for `FUN_00413BA0` and `FUN_00421A10`.
- **10-Hole Skull Instantiation & Mouth Animations**: Unlocked `HoleMgr` proxy capacity to `max_holes=10` across `ZEP_AddHole`, `ZEP_SetHolePercent`, and `ZEP_SetHoleSomething`.

---

## Repository Structure

```text
ZuCompleluxe-lite/
├── builder/                     # Byte-level PE patchers, code cave generators, and asset tools
│   ├── color_tables.json         # Exact RGB palette lookup tables for 11-color GIF synthesis
│   ├── patch_anim_hook.py        # WebP drawing hooks (PreDraw and LoadImage in .anim)
│   ├── patch_assets.py           # Automated asset synthesizer, Temple 5 & Dev Help media patcher
│   ├── patch_custommap.py        # Gauntlet Custom Map hooks (.zcm RX section)
│   ├── patch_extracolor.py       # 11-color mod (.zcol RX section, vanilla fallback, palettes)
│   ├── patch_extrapath.py        # Stage 1: Board curve heap array expansion (.zep RX section)
│   ├── patch_extrapath_stage2.py # Stage 2: Runtime curve 4..10 & skull priority (.zep2 RX section)
│   ├── patch_imports.py          # Mandatory gatekeeper + dynamic loader (.zldr) & ZEP_EngineConfig v3 (.zdata)
│   ├── patch_testingkit.py       # Developer Testing Kit hooks (.ztk RX section, Sites A2..R)
│   ├── patch_zccredit.py         # Developer Credits & GitHub button hooks (.zcc RX section)
│   ├── pe_util.py                # Pure-Python PE parser and section manipulator
│   ├── select_folder.ps1         # Modern Windows Explorer IFileOpenDialog folder picker
│   ├── select_folder.py          # Tkinter folder picker fallback
│   ├── site_resolver.py          # Dynamic AOB & Capstone structural site resolver
│   └── verify_pe.py              # Automated PE security, section, and export audit
├── assets/
│   └── media/                    # Custom textures & masks for Secret Temple 5 and DevTester HelpScreen
├── docs/                         # Reverse engineering audits, memory maps, and struct notes
│   ├── map_audit.md
│   ├── map_curve.md
│   ├── map_curvecreate.md
│   ├── map_leveldesc.md
│   ├── map_limits.md
│   ├── map_loader.md
│   ├── map_loops.md
│   └── toolchain_and_dll.md
├── src/                          # C source files for the 4 core modular sidecar DLLs
│   ├── zumadllchecker.dll.c      # Mandatory pre-launch gatekeeper, PE metadata reader & Lite dependency UI
│   ├── zumacplxcredit.dll.c      # Standalone Developer Credits & interactive GitHub button
│   ├── zumacustommap.dll.c       # Gauntlet Custom Map discovery, carousel & progression
│   └── zumatestingkit.dll.c      # Virtual DevTester profile, Temple 5, MMB cheat & DevTester Guide
├── build_lite.bat                # 1-Click Direct Installation Folder Patcher & Builder (Lite Edition)
├── .gitignore                    # Git exclusion rules
├── LICENSE.md                    # Legal notices and MIT license
├── CHANGELOG.md                  # Version release history
└── README.md                     # Project documentation
```

---

## Build & Installation Instructions

### Prerequisites
1. **Windows OS** (Windows 7 / 8 / 10 / 11).
2. **Zuma Deluxe Installation Folder**: A working installation of *Zuma Deluxe* v1.0.0.1 (GameHouse / PopCap retail, unpacked DRM-free, or compatible build).
3. **Compiler & Python Toolchain (Automated)**:
   - **C Compiler**: MinGW-w64 (GCC), MSVC, Zig (`zig cc`), or Clang. If missing, `build_lite.bat` automatically bootstraps a portable toolchain.
   - **Python 3.8+**: With `pefile`, `capstone`, `Pillow`, and `numpy`.

### 1-Click Lite Edition Patching (`build_lite.bat`)
Double-click `build_lite.bat` or run it from the terminal:

```cmd
build_lite.bat
```
*(Or pass your Zuma Deluxe installation directory directly in non-interactive mode: `build_lite.bat "D:\Games\Zuma Deluxe" /yes`)*.

The builder pipeline will automatically:
1. Open the modern File Explorer folder selector (click **"Patch Folder"** on your Zuma Deluxe directory).
2. Back up your original vanilla `Zuma.exe`, `resources.xml`, `levels.xml`, and `baDotz.gif` into `<ZumaDir>\modules\bak\`.
3. Detect your 32-bit C compiler and Python runtime adaptively.
4. Compile the 4 core sidecar DLLs into `<ZumaDir>\modules\`:
   - `modules\zumadllchecker.dll` *(Mandatory Gatekeeper - Lite Edition)*
   - `modules\zumacustommap.dll` *(Gauntlet Custom Map Subsystem)*
   - `modules\zumacplxcredit.dll` *(Developer Credits Subsystem)*
   - `modules\zumatestingkit.dll` *(Developer Testing Kit Subsystem)*
5. Synthesize and patch `<ZumaDir>\Zuma.exe` with sections `.zdata`, `.zldr`, `.zcol`, `.anim`, `.zep`, `.zep2`, `.zcm`, `.zcc`, and `.ztk`.
6. Generate required assets, Temple 5 media, DevTester HelpScreen assets, and updated XML definitions via `builder/patch_assets.py`.
7. Execute `verify_pe.py --lite` to confirm PE section permissions, entry point integrity, and export tables.

**Key Characteristics of Lite Edition**:
- **4 Core Modules Deployed**: Compiles strictly `zumadllchecker.dll` (Lite Edition), `zumacustommap.dll`, `zumacplxcredit.dll`, and `zumatestingkit.dll`.
- **Excluded Sidecars Omitted**: `zumawebp.dll`, `zumaextrapath.dll`, and `zumaextracolor.dll` are omitted from `modules\`.
- **Pure Vanilla Gameplay Fallbacks**: Zuma Deluxe operates using PopCap's authentic vanilla image decoding, 3 native curves, and 6 original ball colors with zero crashes.
- **Full Edition (Private Development)**: If you require multi-path (up to 10 curves), 11-ball colors, and animated WebP decoding, the Full Edition is hosted at [ZuCompleluxe](https://github.com/rizkybayuu/ZuCompleluxe). It is currently private and will be released publicly alongside the launch of the **Zuma Compleluxe** mod.


### Activating Developer Testing Mode (`DevTester`)
To enable the built-in **`DevTester`** profile (unlocking all 12 Adventure stages, Secret Stage 13 / Temple 5, all Gauntlet maps at Sun God rank, Middle Mouse Button instant win, and the custom DevTester Guide without modifying your save files):
- Create an empty file named `zumatest.log` in your Zuma Deluxe root folder.
- Launch `Zuma.exe`, click **Run** in the Dependency Checker dialog, open **Change User**, and select **`DevTester`**.
- Delete or rename `zumatest.log` at any time to return to normal gameplay.

---

## Configuration Example (`levels.xml`)

To define up to 10 curves and up to 11 ball colors in a level, configure the `<Graphics>` and `<StageProgression>` tags in `levels/levels.xml`:

```xml
<Graphics id="spiral" 
          curve="spiral-1" 
          curve2="spiral-2" 
          curve3="spiral-3" 
          curve4="spiral-4" 
          curve5="spiral-5" 
          curve6="spiral-6" 
          curve7="spiral-7" 
          curve8="spiral-8" 
          curve9="spiral-9" 
          curve10="spiral-10" 
          image="bg" 
          dispname="Spiral of Doom" 
          gx="327" 
          gy="233">
```

- **Multi-Path**: Curves `curve4` through `curve10` are automatically bound by `zumaextrapath.dll`.
- **Custom Gauntlet Maps**: Any `<Graphics>` map not listed in Adventure `<StageProgression>` is automatically discovered by `zumacustommap.dll` and added to the Gauntlet screen.
- **Smart WebP Resolution**: Setting `image="bg"` automatically resolves `bg.webp` with `#1` format priority over `.png`/`.jpg`/`.gif`, falling back cleanly if no `.webp` exists.

---

## Technical Details: PE Layout & Memory Structures

### Patched `Zuma.exe` PE Section Layout
```text
Section    VA           VirtualSize    RawSize    Chars        Access   Purpose
--------------------------------------------------------------------------------------------------------------------
.text      0x00401000   0x000FC8F2     0x0FD000   0x60000020   [RX]     Original game code + hook jumps
.rdata     0x004FE000   0x0002F91C     0x030000   0x40000040   [R]      Read-only constants & vtables
.data      0x0052E000   0x00053E38     0x006000   0xC0000040   [RW]     Game globals
.rsrc      0x00582000   0x00006818     0x007000   0x40000040   [R]      Win32 resources
GARR       0x00589000   0x00001000     0x000719   0xE0000060   [RWX]    PopCap startup entry point (0x00589000)
.zdata     0x0058A000   0x00002000     0x002000   0xC0000040   [RW]     Zero-init sidecar IAT, ZEP_EngineConfig v3, color arrays
.zldr      0x0058C000   0x00001000     0x001000   0x60000020   [RX]     Mandatory gatekeeper check & dynamic sidecar loader
.zcol      0x0058D000   0x00001000     0x001000   0x60000020   [RX]     11-color thunks, vanilla image copy & color clamps
.anim      0x0058E000   0x00001000     0x001000   0x60000020   [RX]     WebP PreDraw and LoadImage trampolines
.zep       0x0058F000   0x00001000     0x001000   0x60000020   [RX]     Stage 1: 10-curve Board heap array allocation
.zep2      0x00590000   0x00001000     0x001000   0x60000020   [RX]     Stage 2: Curve 4..10 routing, clamps & skull priority
.zcm       0x00591000   0x00001000     0x001000   0x60000020   [RX]     Gauntlet Custom Map carousel & level picker caves
.zcc       0x00592000   0x00001000     0x001000   0x60000020   [RX]     Developer Credits & interactive GitHub button caves
.ztk       0x00593000   0x00001000     0x001000   0x60000020   [RX]     Developer Testing Kit (DevTester) caves (Sites A2..R)
```

### Extended `HoleMgr` Layout (`0x308` bytes, Dynamic Allocation)
```text
+0x000: vtable pointer (0x004FE734)
+0x074: int mNumHoles               (Active hole count)
+0x078: int mCurveToHole[3]         (Vanilla curves 0..2 mapping)
+0x084: Hole mHoles[10]             (10 inline Hole objects @ 0x38 bytes, spanning +0x084..+0x2B4)
        ├─ +0x00: id / flags
        ├─ +0x0C: float x
        ├─ +0x10: float y
        ├─ +0x14: int mFrame        (Skull animation frame, read by DrawSingleHole)
        ├─ +0x18: int mTimer        (0 readers in engine)
        ├─ +0x1C: float mRotation   (Skull rotation angle)
        ├─ +0x20: float mPercent[3] (Vanilla curve approach percentage)
        └─ +0x2C: int mSomething[3] (Auxiliary value buffer)
+0x2B4..+0x308: Safe buffer headroom (84 bytes safe padding)
```

### Layout `ZEP_EngineConfig` v3 (`.zdata` @ `+0x400`, 92 bytes / 23 dwords)
```text
[Offset] Type      Field                      Description
+0x00    uint32_t  magic                      0x4350455A ("ZEPC")
+0x04    uint32_t  version                    3 (Structure v3)
+0x08    uint32_t  struct_size                92 (0x5C bytes)
+0x0C    uint32_t  rva_curve_ctor             RVA Curve::Curve (0x00414290)
+0x10    uint32_t  rva_curvedesc_ctor         RVA CurveDesc::CurveDesc (0x00406B20)
+0x14    uint32_t  rva_str_assign             RVA std::string::assign (0x00409B00)
+0x18    uint32_t  rva_curve_init             RVA Curve::Init (0x004173E0)
+0x1C    uint32_t  rva_op_new                 RVA operator new (0x004D1915)
+0x20    uint32_t  rva_add_hole               RVA HoleMgr::AddHole (0x00402860)
+0x24    uint32_t  rva_set_percent            RVA HoleMgr::SetHolePercent (0x00402790)
+0x28    uint32_t  rva_set_something          RVA HoleMgr::SetHoleSomething (0x00402810)
+0x2C    uint32_t  sizeof_curve               Size of struct Curve (0x84 bytes)
+0x30    uint32_t  max_holes                  Maximum HoleMgr capacity (10)
+0x34    uint32_t  va_graphics_set_font       VA Graphics::SetFont (0x00447840)
+0x38    uint32_t  va_draw_image_rect         VA Graphics::DrawImage (0x0044D730)
+0x3C    uint32_t  va_credits_count_ptr       VA Credits entry count global
+0x40    uint32_t  va_font_header_ptr         VA FONT_HEADER global pointer
+0x44    uint32_t  va_font_name_ptr           VA FONT_NAME global pointer
+0x48    uint32_t  va_image_dialog_button_ptr VA IMAGE_DIALOG_BUTTON global pointer
+0x4C    uint32_t  va_font_title_ptr          VA FONT_TITLE global pointer
+0x50    uint32_t  va_original_credits_table  VA Vanilla credits table (0x00503A08)
+0x54    uint32_t  va_zuma_app_ptr            VA ZumaApp* global pointer (0x0057EF00)
+0x58    uint32_t  va_font_dialog_ptr         VA FONT_DIALOG global pointer
+0x5C    uint32_t  va_graphics_set_color      VA Graphics::SetColor (0x00447850)
```

---

## License & Legal Disclaimer

- **Trademarks & Copyrights**: *Zuma* and *Zuma Deluxe* are registered trademarks and copyrights of **PopCap Games, Inc.** (a subsidiary of **Electronic Arts Inc.**). The retail edition analyzed in this project originates from the **GameHouse, Inc.** distribution channel.
- **Non-Commercial Research**: This repository is an independent reverse engineering and software preservation project conducted strictly for educational and interoperability purposes under statutory exemptions (e.g., 17 U.S.C. § 1201(f)). It is not affiliated with or endorsed by PopCap Games, Electronic Arts, or GameHouse.
- **No Assets Distributed**: This repository does not distribute any proprietary game assets (such as `.pak` archives, audio tracks, or retail executables). Users must legally own an authentic retail copy of *Zuma Deluxe* (v1.0.0.1) to apply the patches.
- **Source License**: All original code, scripts in `builder/`, and sidecar libraries in this repository are authored by [Rizky Bayuu](https://github.com/rizkybayuu) and licensed under the [MIT License](LICENSE.md).
