# ZuCompleluxe Lite (v0.5.1.2)

> **Version:** 0.5.1.2 (Lite Edition)  
> **Target Game:** *Zuma Deluxe* v1.0.0.1 (GameHouse / PopCap / Regional PE Builds)  
> **Project Scope:** Lightweight Core Engine Framework for Zuma Deluxe Modding  
> **Developer:** [Rizky Bayuu](https://github.com/rizkybayuu)  
> **Full Edition:** [ZuCompleluxe (Full Edition)](https://github.com/rizkybayuu/ZuCompleluxe)  
> **License:** [MIT License](LICENSE.md)  
> **Changelog:** [CHANGELOG.md](CHANGELOG.md)

**ZuCompleluxe Lite** is the official lightweight edition of the ZuCompleluxe reverse-engineering and modding framework for *Zuma Deluxe* (v1.0.0.1). It delivers the core engine enhancements—the pre-launch integrity gatekeeper (`zumadllchecker.dll`), Gauntlet custom map subsystem (`zumacustommap.dll`), developer credits mod (`zumacplxcredit.dll`), developer testing kit (`zumatestingkit.dll`), display driver with F11 borderless fullscreen (`zumadisplay.dll`), and the arbitrary custom resolution engine (`zumareshandler.dll`)—while omitting experimental multi-path, 11-ball color palettes, and animated WebP rendering.

This framework represents the foundational engine layer (**v0.5.1.2**) engineered specifically to power **Zuma Compleluxe**—an upcoming, highly ambitious total conversion mod currently in active development.

> [!NOTE]
> **Full Edition Status & Availability:**  
> The complete framework with full capabilities—including multi-path (up to 10 curves), 11-ball colors, and animated WebP background & sprite playback—is hosted at [ZuCompleluxe](https://github.com/rizkybayuu/ZuCompleluxe). The Full Edition is currently in private development and will be made **public** simultaneously with the official release of the **Zuma Compleluxe** mod.

---

## Core Architecture & Engineering Philosophy

The Lite framework is built upon four core reverse-engineering pillars:

### 1. `Zuma.exe`-Exclusive Binary Patching & `modules\` Isolation
Modifications are isolated cleanly from vanilla game files:
- **Exclusive Executable Modifications**: All engine modifications, hook stubs, and feature extensions are concentrated 100% within `Zuma.exe` through native PE section injection (`.zdata`, `.zldr`, `.zcm`, `.zcc`, `.ztk`, `.zres`).
- **Dedicated `modules\` Subdirectory**: All 6 modular sidecar DLLs (`zumadllchecker.dll`, `zumacustommap.dll`, `zumacplxcredit.dll`, `zumatestingkit.dll`, `zumadisplay.dll`, `zumareshandler.dll`) are housed inside `modules\` to keep the root game directory clean, while original vanilla files are backed up automatically to `modules\bak\`.
- **Untouched Commercial Dependencies**: Third-party commercial retail dependencies—specifically `zumares.dll` (GameHouse DRM handshake and CRT startup decoder) and `bass.dll` (Un4seen audio engine)—remain 100% untouched vanilla.

### 2. Mandatory Gatekeeper (`zumadllchecker.dll`) & Pre-Launch Diagnostics
Unlike the optional feature sidecars, `modules\zumadllchecker.dll` is a **strictly binding dependency**:
- **Hard Gatekeeper Enforcement in `.zldr` (`0x0058C000`)**: Before loading any optional feature modules or executing game code, `.zldr` enforces `modules\zumadllchecker.dll`. If `zumadllchecker.dll` is missing or fails to load, `Zuma.exe` displays a modal error dialog and immediately terminates via `ExitProcess(1)`.
- **Dynamic PE Version Metadata Extraction**: Reads `ProductName`, `CompanyName`, `FileVersion`, `SpecialBuild`, and `FileName` dynamically from `Zuma.exe`'s Win32 PE Version Resource (`VS_FIXEDFILEINFO` and `\VarFileInfo\Translation`) with zero hardcoded game strings.
- **Classic Adaptive Diagnostic Dialog**: Displays game metadata, patch version (`ZuCompleluxe v0.5.1.2 Lite`), developer link (**Rizky Bayuu** -> `https://github.com/rizkybayuu`), license (`MIT License`), aspect ratio, resolution, and real-time presence checks for all sidecars in `modules\`, adapting automatically to Windows Light/Dark mode (`AppsUseLightTheme` + DWM Immersive Dark Mode).

### 3. 100% Independent & Optional Feature Sidecars (Zero-Dependency Graceful Degradation)
Every feature sidecar DLL in ZuCompleluxe Lite is completely optional and independent:
- **Dynamic Runtime Sidecar Loader (`.zldr` @ `0x0058C000`)**: `DataDirectory[1]` (Import Directory) is terminated immediately after the vanilla DLL descriptors so the Windows PE loader never aborts startup if optional feature sidecars are removed. At startup (after `zumadllchecker.dll` approval), `.zldr` dynamically loads present feature DLLs from `modules\` via `LoadLibraryA` and populates `.zdata` IAT slots via `GetProcAddress` before jumping to the original PopCap entry point (`0x00589000`).
- **Airtight Per-Hook IAT Fallbacks**: Every code cave across `.zcm`, `.zcc`, and `.ztk` performs a null-check on its `.zdata` IAT pointer before calling into any sidecar:
  - **Missing `zumacustommap.dll`**: Gauntlet mode falls back to the 12 vanilla stages and standard level pickers.
  - **Missing `zumacplxcredit.dll`**: Credits roll falls back to the vanilla credit table (`0x00503A08` / `0x0054948C`) and standard fonts.
  - **Missing `zumatestingkit.dll`**: User profile dialog, stage unlocking, Gauntlet defaults, Help screen, and button rendering run as pure vanilla.
  - **Missing `zumareshandler.dll`**: Asset loading automatically falls back to standard `images/`.

### 4. Semantic Unique Attributes over Rigid Hardcoded Addresses (AOB Signature Scanning)
ZuCompleluxe replaces static address patching with **Dynamic AOB (Array of Bytes) & Capstone Structural Scanning**:
- **Semantic Instruction Fingerprinting**: The patcher scans raw `.text` opcodes using semantic instruction fingerprints (allocation calls, register swap sequences, unique struct displacements such as `[esi + 0x5C]`) rather than static addresses.
- **In-Place Unique Attribute Modification**: All coordinate modifications, viewport margins, and clip rects are resolved dynamically at load time with zero hardcoded RVAs.

---

## Key Features

- **Arbitrary Custom Resolution Engine (`zumareshandler.dll` & `.zres`)** *(New in v0.5.1)*:
  - Scales all UI coordinates, fonts, and gameplay viewports uniformly to any target resolution $\ge 480 \times 320$ (e.g. 800×600 SVGA, 1080p, 1440p, 4K, 21:9 Ultrawide).
  - Letterboxed aspect-ratio fit: `scale = min(W/640, H/480)` with symmetric `X_offset`/`Y_offset` pillarbox margins.
  - Repositions 20+ screens with zero hardcoded virtual addresses.
  - Generates scaled visual assets into `imagescustomres/` via Bicubic interpolation with automatic fallback to `images/`.
- **Borderless Fullscreen Mode & F11 Toggle (`zumadisplay.dll`)** *(New in v0.5.1)*:
  - Press **F11** (or **Alt+Enter**) during gameplay to toggle between windowed and borderless fullscreen without restarting the game.
  - **3D Acceleration 100% Preserved**: Hardware 3D acceleration is strictly preserved across fullscreen toggles.
  - Seamless outscreen brick border fill (`outscreen_data.h`) for letterbox/pillarbox margins.
  - Pixel-exact mouse coordinate remapping through `ScreenToClient` and `ClientToScreen` hooks.
- **Modernized True 32-bit ARGB GPU Hardware Cursors (`HCURSOR`)** *(New in v0.5.1)*:
  - Replaced legacy 2003 DirectDraw software cursor surface blits (`DDInterface::DrawCursor` & 10ms thread polling) with native Win32 hardware `HCURSOR`s.
  - Silenced `DDInterface::SetCursorPos` to eliminate DirectDraw critical section contention.
  - Completely eliminates cursor movement lag and top-right ghost cursor artifacts in fullscreen.
- **Pre-Launch Module & Dependency Checker (`zumadllchecker.dll`)**:
  - Mandatory gatekeeper dialog executed prior to engine initialization.
  - Dynamically queries PE version resources alongside `ZuCompleluxe v0.5.1.2 Lite` metadata, `MIT License` notice, and an interactive hyperlink to [Rizky Bayuu](https://github.com/rizkybayuu).
  - Verifies presence and health of all sidecars in `modules\` with interactive checkbox toggles.
- **1-Click Direct Installation Folder Patcher (`build_lite.bat`)**:
  - Opens a modern Windows Explorer folder selection dialog (`builder/select_folder.ps1`) or accepts CLI arguments (`build_lite.bat /yes -d "<path>" -w 800 -h 600`).
  - Automatic UPX decompression (`builder/ensure_unpacked.py`) to create clean baseline PE.
  - Single-Backup Architecture (`modules\bak\Zuma.exe`) guaranteeing strict idempotency.
- **Gauntlet Custom Map Subsystem (`zumacustommap.dll` & `.zcm`)**:
  - Automatically discovers custom maps defined in `levels/levels.xml` outside of Adventure `StageProgression` (up to 256 custom maps) and adds them to the Gauntlet carousel.
- **Developer Testing Kit (`zumatestingkit.dll` & `.ztk`)**:
  - Activated exclusively when `zumatest.log` exists in the game folder; completely inert during normal play.
  - Injects virtual user profile **`DevTester`**, unlocks all 12 Adventure stages, all Gauntlet maps at **Sun God** rank, Secret Temple 5 & Door 5 ("The Void"), Middle Mouse Button instant win cheat, and custom DevTester Guide.
- **Standalone Interactive Developer Credits (`zumacplxcredit.dll` & `.zcc`)**:
  - Extended developer credits roll (entries 39–41) with interactive `GitHub` button opening repository URL via `ShellExecuteA`.

---

## Repository Structure

```text
ZuCompleluxe-lite/
├── builder/                     # Byte-level PE patchers, code cave generators, and asset tools
│   ├── bootstrap.bat            # Multi-tier zero-dependency toolchain bootstrap engine
│   ├── customres_sigs.json      # Dynamic AOB signature definitions for custom resolution
│   ├── ensure_unpacked.py       # Ephemeral UPX decompression baseline tool
│   ├── find_target_exe.py       # Adaptive Zuma Deluxe executable discovery
│   ├── patch_assets.py          # Asset patcher (Temple 5 & Dev Help media, --lite mode)
│   ├── patch_custommap.py       # Gauntlet Custom Map hooks (.zcm RX section)
│   ├── patch_customres.py       # .zres section injection & resolution AOB patching
│   ├── patch_customres_assets.py# Bicubic-scaled high-res UI asset generator (imagescustomres/)
│   ├── patch_imports.py         # Mandatory gatekeeper + dynamic loader (.zldr) with --lite support
│   ├── patch_menubar.py         # Smart menubar synthesizer for arbitrary target widths
│   ├── patch_testingkit.py      # Developer Testing Kit hooks (.ztk RX section)
│   ├── patch_zccredit.py        # Developer Credits & GitHub button hooks (.zcc RX section)
│   ├── pe_util.py               # Pure-Python PE parser and section manipulator
│   ├── prepare_embed.py         # Isolated-mode .pth patcher for embedded Python
│   ├── probe_compiler.py        # 32-bit compiler detector and runtime GUI dialog suppressor
│   ├── requirements.txt         # Pinned Python package dependencies for automated bootstrap
│   ├── select_folder.ps1        # Modern Windows Explorer IFileOpenDialog folder picker
│   ├── select_folder.py         # Tkinter folder picker fallback
│   ├── site_resolver.py         # Dynamic AOB & Capstone structural site resolver
│   └── verify_pe.py             # Automated PE security, section, and export audit (--lite)
├── assets/
│   └── media/                   # Menubar tile assets, outscreen.jpg, and Secret Temple 5 / DevTester media
├── docs/                        # Reverse engineering audits, memory maps, and struct notes
│   ├── credits_system.md
│   ├── custom_resolution_system.md
│   ├── map_audit.md
│   ├── map_curve.md
│   ├── map_curvecreate.md
│   ├── map_leveldesc.md
│   ├── map_limits.md
│   ├── map_loader.md
│   ├── map_loops.md
│   ├── repositioning_and_coordinate_spaces.md
│   ├── sidecar_dll_overview.md
│   ├── toolchain_and_dll.md
│   └── unique_attributes_and_signatures.md
├── src/                         # C source files for the 6 Lite sidecar DLLs
│   ├── zumadllchecker.dll.c     # Mandatory pre-launch gatekeeper, PE metadata reader & dependency UI
│   ├── zumacplxcredit.dll.c     # Standalone Developer Credits & interactive GitHub button
│   ├── zumacustommap.dll.c      # Gauntlet Custom Map discovery, carousel & progression
│   ├── zumatestingkit.dll.c     # Virtual DevTester profile, Temple 5, MMB cheat & DevTester Guide
│   ├── zumadisplay.dll.c        # Display & window driver, F11 borderless fullscreen, GPU hardware cursor
│   ├── zumareshandler.dll.c     # Resolution & viewport handler, screen repositioning, asset redirection
│   └── outscreen_data.h         # Embedded 160x160 brick texture for pillarbox/letterbox fill
├── build_lite.bat               # 1-Click Direct Installation Folder Patcher & Builder (Lite Edition)
├── .gitattributes               # Line ending normalizations (*.bat eol=crlf)
├── .gitignore                   # Git exclusion rules
├── LICENSE.md                   # Legal notices and MIT license
├── CHANGELOG.md                 # Version release history
└── README.md                    # Project documentation
```

---

## Build & Installation Instructions

### Prerequisites
1. **Windows OS**: Windows 7, 8, 10, or 11 (32-bit or 64-bit).
2. **Authentic Zuma Deluxe Game**: A working installation of *Zuma Deluxe* v1.0.0.1 (GameHouse, PopCap retail, Steam, or compatible regional build).
3. **Zero Manual Toolchain Setup**:
   - The patcher runs out-of-the-box with zero prior development setup.
   - If Python or a C compiler is not installed on your system, the automated bootstrap engine (`builder/bootstrap.bat`) transparently provisions portable embedded Python 3.11.9 and a 32-bit Zig cross-compiler (`ziglang cc -target x86-windows-gnu`) with SHA-256 integrity verification inside `.build\tools\`.

---

### Step-by-Step Installation Guide (Quick Start)

> [!IMPORTANT]
> **Do NOT extract the patcher files directly into your Zuma Deluxe folder!**  
> Extract the downloaded release archive (`ZuCompleluxe-lite-v0.5.1.2-windows.zip`) into its own separate folder (e.g. `Downloads\ZuCompleluxe-lite\` or `Desktop\ZuCompleluxe-lite\`). The patcher will safely inject the necessary modules, assets, and code caves into your target game directory.

#### Step 1: Close the Game
Ensure *Zuma Deluxe* (`Zuma.exe`) is completely closed and not running in the background.

#### Step 2: Run the Patcher
Navigate to your extracted patcher folder and double-click:
- **`build_lite.bat`** for the **Lite Edition** (lightweight core engine with 6-ball colors, 3 curves, arbitrary custom resolutions, and F11 borderless fullscreen).

*(Tip: If your Zuma Deluxe is installed under protected system paths like `C:\Program Files (x86)\...`, right-click `build_lite.bat` and select **"Run as administrator"**).*

#### Step 3: Select Your Zuma Deluxe Folder
1. A Windows Explorer folder selection window will appear automatically.
2. Browse to and select your *Zuma Deluxe* installation directory (the folder containing `Zuma.exe`).
3. Click the **"Patch Folder"** (or **"Select Folder"**) button.

*(Note: If the graphical folder picker is closed or unavailable, the console will safely prompt you to paste your game path manually).*

#### Step 4: Choose Display Resolution
The console will ask you to select your preferred display resolution:
- **Option 1 (Default):** `640x480` — Authentic vanilla 4:3 resolution.
- **Option 2:** `800x600` — High-definition SVGA 4:3 with automatically synthesized high-res UI textures (`imagescustomres/`).
- **Option 3:** `Custom Resolution` — Enter any custom width and height $\ge 480 \times 320$ (e.g., `1280x720`, `1920x1080` FHD, `2560x1440` QHD, `3840x2160` 4K, or 21:9 Ultrawide).

#### Step 5: Automated Build & PE Injection
The builder pipeline automatically handles the entire installation process:
1. **Safety Backup**: Creates an untouched, byte-exact backup of your original executable at `<ZumaDir>\modules\bak\Zuma.exe` and archives vanilla manifests (`resources.xml`, `levels.xml`, `baDotz.gif`).
2. **Decompression**: Transparently decompresses UPX-packed executables to a clean baseline PE.
3. **Module Compilation**: Compiles the 6 Lite sidecar DLLs into `<ZumaDir>\modules\`:
   - `modules\zumadllchecker.dll` *(Mandatory Gatekeeper)*
   - `modules\zumacustommap.dll` *(Optional — Gauntlet Custom Maps)*
   - `modules\zumacplxcredit.dll` *(Optional — Developer Credits)*
   - `modules\zumatestingkit.dll` *(Optional — DevTester & Temple 5)*
   - `modules\zumadisplay.dll` *(Optional — Display Driver, F11 Fullscreen, Hardware Cursors)*
   - `modules\zumareshandler.dll` *(Optional — Resolution Engine & Asset Router)*
4. **PE Injection**: Injects native code cave sections (`.zdata`, `.zldr`, `.zcm`, `.zcc`, `.ztk`, `.zres`) directly into `Zuma.exe`.
5. **Asset Synthesis**: Generates locked button sprites, Secret Temple 5 media, DevTester HelpScreen assets, and high-res UI graphics (`imagescustomres/`).
6. **Integrity Audit**: Runs `verify_pe.py --lite` to ensure 100% clean PE header compliance and export validity.

#### Step 6: Launch & Play!
1. Open your *Zuma Deluxe* game folder.
2. Launch **`Zuma.exe`**.
3. The **ZuCompleluxe Pre-Launch Gatekeeper** (`zumadllchecker.dll`) dialog will appear:
   - It displays detected game metadata, patch version (`ZuCompleluxe v0.5.1.2 Lite`), and the real-time presence of all modules in `modules\`.
   - Click **Run** to start the game!

---

### Restoring Vanilla Game (Unpatching)

ZuCompleluxe Lite uses a strictly non-destructive **Single-Backup Architecture**. If you ever wish to revert your game to 100% vanilla:
1. Navigate to your Zuma Deluxe folder: `<ZumaDir>\modules\bak\`.
2. Copy `Zuma.exe` from `modules\bak\` back into your root Zuma Deluxe folder, replacing the patched `Zuma.exe`.
3. *(Optional)* Delete the `modules\` and `imagescustomres\` folders.
4. Your game is now restored to its authentic vanilla state.

---

### Command-Line & Automation Interface (CLI)

For headless deployments, batch modding, or automated testing, you can pass parameters directly to bypass all interactive prompts:

```cmd
:: Patch Lite Edition with 1080p resolution in non-interactive mode
build_lite.bat -d "C:\Games\Zuma Deluxe" -w 1920 -h 1080 /yes

:: Patch Lite Edition at 800x600 SVGA
build_lite.bat -d "C:\Games\Zuma Deluxe" --res 800x600 -y
```

#### Supported CLI Options:

| Option | Shorthand | Description | Default |
| :--- | :--- | :--- | :--- |
| `--dir <path>` | `-d <path>` | Path to the target Zuma Deluxe installation folder | Interactive Picker |
| `--width <w>` | `-w <w>` | Target game width in pixels (minimum: `480`) | `640` |
| `--height <h>`| `-h <h>` | Target game height in pixels (minimum: `320`) | `480` |
| `--res <WxH>`  | | Combined resolution shortcut (e.g. `--res 1920x1080`) | `640x480` |
| `--non-interactive` | `/yes`, `-y` | Run silently without any interactive prompts | Disabled |

---

### Troubleshooting & FAQ

- **Error: "Target executable is already patched with ZuCompleluxe sections"**:
  - The patcher prevents double-patching an already-modified executable to protect binary integrity.
  - *Solution:* If you want to change resolutions or re-patch, copy the clean backup from `modules\bak\Zuma.exe` back to the root folder first, or delete the patched `Zuma.exe` and restore it from `modules\bak\`.
- **Antivirus warning on `upx.exe`**:
  - `builder/.tools/upx.exe` is fetched directly from the official UPX GitHub repository with SHA-256 verification solely to decompress retail Zuma executables. Some antivirus software flags UPX with heuristic false positives. It is completely safe.
- **Permission Denied / Access Denied when patching**:
  - If your game is in `C:\Program Files (x86)\...`, Windows restricts write access to regular user accounts.
  - *Solution:* Right-click `build_lite.bat` and choose **"Run as administrator"**.
- **Can I run `build_lite.bat` multiple times?**
  - Yes! Because the original vanilla executable is safely archived in `modules\bak\Zuma.exe`, the patcher always uses the clean backup as its source baseline whenever re-patching.

---

### Activating Developer Testing Mode (`DevTester`)
To enable the built-in **`DevTester`** profile (unlocking all 12 Adventure stages, Secret Stage 13 / Temple 5, all Gauntlet maps at Sun God rank, Middle Mouse Button instant win, and the custom DevTester Guide without modifying your save files):
- Create an empty file named `zumatest.log` in your Zuma Deluxe root folder.
- Launch `Zuma.exe`, click **Run** in the Dependency Checker dialog, open **Change User**, and select **`DevTester`**.
- Delete or rename `zumatest.log` at any time to return to normal gameplay.

---

## License

This project is licensed under the [MIT License](LICENSE.md).
