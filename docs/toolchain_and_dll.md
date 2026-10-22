# Toolchain Setup and DLL Build Verification

## 1. Compiler: MinGW32 GCC (Primary)

The primary compiler for all sidecar DLLs is **MinGW32 GCC** (`C:\msys64\mingw32\bin\gcc.exe`).

- **Working build command**:
  ```bash
  gcc -m32 -O2 -s -shared -static-libgcc -o <output>.dll <source>.c
  ```
  - Produces a valid PE32 DLL (Machine: `0x14c` / i386).
  - `-static-libgcc` ensures no external `libgcc_s` runtime dependency.

- **Fallback compiler**: `python -m ziglang cc -target x86-windows-gnu -O2 -shared`
  - Used automatically by `build_full.bat` if MinGW32 GCC is not found on PATH.
  - Zig accepts `__attribute__((thiscall))` and `__attribute__((fastcall))` correctly.

> [!NOTE]
> **Keystone Assembler is no longer used.** All byte-level patches are generated using raw bytecode arrays in the Python builder scripts (`builder/patch_*.py`). There is no dependency on `keystone-engine`.

---

## 2. Sidecar DLL Inventory

All eight sidecar DLLs are compiled by `build_full.bat` and deployed to the `modules/` directory (excluded from version control — build output only).

| DLL | Source File | PE Section | Key Exports | Purpose |
|---|---|---|---|---|
| `zumadllchecker.dll` | `src/zumadllchecker.dll.c` | `.zdlc` | `ZDC_Check` | Startup diagnostic dialog; verifies sidecar integrity |
| `zumawebp.dll` | `src/zumawebp.dll.c` | `.zwbp` | `ZWP_Init`, `ZWP_Render` | Animated WebP playback for intro/splash sequences |
| `zumaextrapath.dll` | `src/zumaextrapath.dll.c` | `.zep` | `ZEP_Init`, `ZEP_LevelStart`, `ZEP_LevelEnd`, `ZEP_ExtraCount` | Extra curve paths (curve4..curve10) loaded from XML + CURV files |
| `zumaextracolor.dll` | `src/zumaextracolor.dll.c` | `.zec` | `ZEC_Init`, `ZEC_GetColor` | 11-color ball mod (extends vanilla 7-color palette) |
| `zumacustommap.dll` | `src/zumacustommap.dll.c` | `.zcm` | `ZCM_Init`, `ZCM_Load` | Custom Gauntlet map support |
| `zumacplxcredit.dll` | `src/zumacplxcredit.dll.c` | `.zcc` | `ZCC_Init`, `ZCC_DrawLine`, `ZCC_OnMouseDown` | Developer credits with interactive GitHub button |
| `zumatestingkit.dll` | `src/zumatestingkit.dll.c` | `.ztk` | `ZTK_Init`, `ZTK_HUD` | Developer testing kit: HUD overlay, middle-click score diff |
| `zumareshandler.dll` | `src/zumareshandler.dll.c` | `.zres` | `ZRH_Init`, `ZRH_GetScale`, `ZRH_Resize` | Main resolution & viewport handler; IAT hook, AOB UI patches, VTable resize hooks |

---

## 3. zumaextrapath.dll – Precise Summary

### Purpose
Sidecar DLL that loads extra path data (curve4..curve10) from XML and CURV files for Zuma Deluxe, providing hooks for extended gameplay.

### Key Definitions
- `ZEP_VANILLA_PATHS = 3` (indices 0–2 reserved for original game paths)
- `ZEP_MAX_PATHS = 10`, `ZEP_MAX_EXTRA = 7`
- `ZEP_HOOKS_READY = 1` — Stage 2 patch in `builder/patch_extrapath_stage2.py` activates curve4–10 runtime hooks at game launch
- `ZEP_XML_PATH = "levels/levels.xml"`
- `ZEP_MAX_CURV_POINTS = 1000000`

### Data Structures
- `ZepPoint`: `{float x, y; uint8_t in_tunnel, priority;}`
- `ZepCurv`: `{uint32_t version, count; ZepPoint *pts;}`
- `ZepLevel`:
  ```c
  char id[64];
  char curve[ZEP_MAX_PATHS + 1][128];   // indexes 0..10, only 4..10 used for extra paths
  ```
- Global state:
  - `ZepLevel *g_levels; int g_nlevels;` (parsed levels)
  - `ZepCurv g_extra[ZEP_MAX_PATHS + 1];` (loaded extra curves, indices 4..10)
  - `int g_extra_count;` (number of successfully loaded extra curves)
  - `int g_ready;` (init flag)

### Logging & Crash Handling
- `logf_()`: writes timestamped messages to `zumaextrapath.log`.
- Vectored Exception Handler (`veh_`): logs access violations and other selected crashes occurring in the DLL or main module, then lets the game's handler proceed.

### XML Parsing (`levels/levels.xml`)
- **Format**: Simple attribute scanner (no full XML validation).
- **Element**: `<Graphics ...>` (not `<Level>`; the `id` attribute here matches the `graphics=` attribute on `<Level>` elements in the original XML).
- **Attributes extracted per `<Graphics>`**:
  - `id` → level key (string, up to 63 chars).
  - `curve4` through `curve10` → filenames for extra path data (each up to 127 chars).
- **Storage**: For each `<Graphics>` with at least one extra curve attribute and a non‑empty `id`, a `ZepLevel` is added to `g_levels`.
- **Result**: `g_nlevels` = number of levels that define extra paths.

### CURV File Loader (`curv_load`)
- **Format** (mirrors `dat_converter.py`):
  1. Magic bytes `"CURV"` (4 bytes).
  2. Version (`uint32_t`).
  3. If version ≥ 2: two extra bytes (`small` flag, `tunnels` flag).
  4. If not `small`: skip `uint32_t et` and `uint32_t bs` (back‑seek bytes), then seek forward `bs` bytes.
  5. Point count (`uint32_t n`, capped at `ZEP_MAX_CURV_POINTS`).
  6. For each point:
     - If first point (`i==0`) **or** version ≤ 1: read two `float` (x, y).
     - Else: read two `int8_t` (dx, dy) and accumulate: `x = ox + dx*0.01`, `y = oy + dy*0.01`.
     - If version ≤ 1 **or** `tunnels` flag: read `uint8_t tunnel` and `uint8_t priority`.
     - Store point, update origin (`ox`, `oy`).
- **Search paths** (in `try_load_()`):
  - `./<name>` and `./<name>.dat`
  - `levels/<name>/<name>` and `levels/<name>/<name>.dat`
  - Called with `name` from `level.curve[k]` (curve4..curve10) and `id` as the level identifier.

### Export Functions
1. `ZEP_Init(void)`
   - Called once per game start.
   - Resets log file (opens for write, then closes).
   - Loads and parses `levels/levels.xml` via `slurp_()` + `xml_parse_()`.
   - Sets `g_ready = 1`.
   - Logs: `"ZEP_Init OK: %d levels with curve4..curve10 (hooks %s)"` with `g_nlevels` and hook status.
   - Returns 1 on success, 0 on XML load failure.

2. `ZEP_LevelStart(const char *levelId)`
   - First calls `ZEP_LevelEnd()` to free any previous curves.
   - If not initialized, calls `ZEP_Init()` (returns 0 if that fails).
   - Looks up `levelId` (case‑insensitive) in `g_levels`.
   - For each extra curve index `k` from 4 to 10:
     - If `curve[k]` is non‑empty, attempts to load via `try_load_()`.
     - On success: increments `g_extra_count` and logs the loaded curve.
     - On failure: logs a warning.
   - Returns `g_extra_count` (number of extra curves successfully loaded).

3. `ZEP_LevelEnd(void)`
   - Frees all point arrays in `g_extra[0..10]` (though only 4..10 are used).
   - Resets `g_extra_count = 0`.

4. `ZEP_ExtraCount(void)`
   - Returns current `g_extra_count`.

### DLL Entry (`DllMain`)
- On `DLL_PROCESS_ATTACH`:
  - Disables thread‑library calls.
  - Installs the vectored exception handler (`veh_`).
  - Logs `"zumaextrapath.dll loaded"`.
- Returns `TRUE`.

### Notes
- All string comparisons are case‑insensitive (`_stricmp`, `_strnicmp`).
- Path loading tries multiple locations and both raw and `.dat` suffixes.
- The CURV loader supports both absolute‑float (versions 0/1) and delta‑encoded (version ≥2) point streams, with optional tunnel/priority flags.

---

## 4. Calling Conventions

All sidecar DLL exports use `__declspec(dllexport)` with default `__cdecl` convention.

When a sidecar hook must interact with game internals using `__thiscall` (e.g., `Widget::MouseDown`), the `this` pointer arrives in `ecx`. Any intervening `__cdecl` call will clobber `ecx` — callers must explicitly `push ecx` / `push edx` before and `pop edx` / `pop ecx` after. See `docs/credits_system.md` for the Cave C crash fix that demonstrates this pattern.
