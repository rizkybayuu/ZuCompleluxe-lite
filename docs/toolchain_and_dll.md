# Toolchain Setup and DLL Build Verification

## 1. Keystone Assembler
- Installed: `keystone-engine` (via pip)
- Verified: Python import and assembly test
  ```python
  import keystone
  ks = keystone.Ks(keystone.KS_ARCH_X86, keystone.KS_MODE_32)
  print(ks.asm('mov eax,[esi+0x8c]; ret'))
  # Output: ([139, 134, 140, 0, 0, 0, 195], 2)
  ```

## 2. Zig Compiler
- Installed: `ziglang` (via pip)
- Version: `0.16.0` (`python -m ziglang version`)

## 3. 32-bit Windows DLL Build with Zig
- **Working command**:
  ```bash
  python -m ziglang cc -target x86-windows-gnu -O2 -shared -o /tmp/zep_test.dll zumaextrapath.c
  ```
  - No additional flags needed (`-s`/`-static` optional; omitted for simplicity).
  - Produced a valid PE32 DLL (Machine: 0x14c).

### DLL Exports (via `pefile`)
- Machine: `0x14c` (Intel 386, i386)
- Exported functions:
  - `ZEP_ExtraCount` (RVA: 0x1b50)
  - `ZEP_Init` (RVA: 0x1000)
  - `ZEP_LevelEnd` (RVA: 0x1490)
  - `ZEP_LevelStart` (RVA: 0x1620)

### Calling Convention
- All exports use `__declspec(dllexport)` with default `__cdecl` (no explicit attributes like `__stdcall`).
- Verified that Zig accepts `__attribute__((thiscall))` and `__attribute__((fastcall))` in test snippets (though the cache error prevented full output; the attributes are parsed by Zig).

## 4. zumaextrapath.c – Precise Summary

### Purpose
Sidecar DLL that loads extra path data (curve4..curve10) from XML and CURV files for Zuma Deluxe, providing hooks for extended gameplay.

### Key Definitions
- `ZEP_VANILLA_PATHS = 3` (indices 0‑2 reserved for original game paths)
- `ZEP_MAX_PATHS = 10`, `ZEP_MAX_EXTRA = 7`
- `ZEP_HOOKS_READY = 0` (placeholder; hooks not implemented in this phase)
- `ZEP_XML_PATH = "levels/levels.xml"`
- `ZEP_MAX_CURV_POINTS = 1000000`

### Data Structures
- `ZepPoint`: `{float x, y; uint8_t in_tunnel, priority;}`
- `ZepCurv`: `{uint32_t version, count; ZepPoint *pts;}`
- `ZepLevel`: 
  ```c
  char id[64];
  char curve[ZEP_MAX_PATHS + 1][128];   // indexes 0..10, but only 4..10 used for extra paths
  ```
- Global state:
  - `ZepLevel *g_levels; int g_nlevels;` (parsed levels)
  - `ZepCurv g_extra[ZEP_MAX_PATHS + 1];` (loaded extra curves, indices 4..10)
  - `int g_extra_count;` (number of successfully loaded extra curves)
  - `int g_ready;` (init flag)

### Logging & Crash Handling
- `logf_()`: writes timestamped messages to `zumaextrapath.log`.
- Vectored Exception Handler (`veh_`): logs access violations and other selected crashes occurring in the DLL or main module, then lets the game’s handler proceed.

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
- **Search paths** (in `try_load_`):
  - `./<name>` and `./<name>.dat`
  - `levels/<name>/<name>` and `levels/<name>/<name>.dat`
  - `levels/<name>/<name>` and `levels/<name>/<name>.dat` (duplicate? Actually the code has three formats: `"%s%s"`, `"levels/%s%s"`, `"levels/%s/%s%s"`; the last two are distinct).
  - Called with `name` from `level.curve[k]` (curve4..curve10) and `id` as the level identifier.

### Export Functions
1. `ZEP_Init(void)`
   - Called once per game start.
   - Resets log file (opens for write, then closes).
   - Loads and parses `levels/levels.xml` via `slurp_()` + `xml_parse_()`.
   - Sets `g_ready = 1`.
   - Logs: `"ZEP_Init OK: %d level punya curve4..curve10 (hooks %s)"` with `g_nlevels` and hook status.
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
  - Logs `"zumaextrapath.dll dimuat"`.
- Returns `TRUE`.

### Notes
- The DLL does **not** alter game behavior until hooks are implemented (currently `ZEP_HOOKS_READY = 0`).
- All string comparisons are case‑insensitive (`_stricmp`, `_strnicmp`).
- Path loading tries multiple locations and both raw and `.dat` suffices.
- The CURV loader supports both absolute‑float (versions 0/1) and delta‑encoded (version ≥2) point streams, with optional tunnel/priority flags.
