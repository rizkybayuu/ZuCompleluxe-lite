# Zuma Deluxe 1.0.0.1 — Curve subsystem map (READ-ONLY)

Status legend: **VERIFIED** = read in decompile/source this session. **UNVERIFIED** = inferred or not reached. Nothing here was written into game or mod sources.

## 1. CURV `.dat` file format (VERIFIED, `Zuma_All_Decompiled.c` ~21340–21484)

Reader is the `.dat` parser used by the Curve path loader.
- Magic: 4 bytes, must equal `"CURV"`.
- Version: int, must be in `[1, DAT_0052e0b8]`. Otherwise error `"Invalid file version"` (via `FUN_0040a730` / `FUN_0040d550`).
- Second int stored at `this+0x10` (meaning UNVERIFIED).
- Name block: length `< 0xf4241` (1,000,001), read into a buffer, copied into `this+0x18`. This is the stored curve name, used for the cache check below.
- Point count: int (`FUN_00429dd0`).
- Point records:
  - version `>= 2`: delta-encoded. Signed bytes scaled by `_DAT_00533da4` and added to the previous x/y.
  - version 1: absolute float x, y (`FUN_00429e40`), then a bool (`FUN_00429e30`) and a byte (`FUN_00429e10`).
- Structural failure: `"File is corrupt"`.

Independent cross-check: `zumaextrapath.c` states its CURV loader uses the "same format as `dat_converter.py`" and caps points at `ZEP_MAX_CURV_POINTS 1000000u`. The CURV loader body was NOT recovered (truncated output), so no finer per-field claim from the sidecar is verified. UNVERIFIED: any sidecar-side handling of version flags or tunnel bytes beyond the header comment.

## 2. By-name curve loader (VERIFIED, `Zuma_All_Decompiled.c` ~23252–23379)

Function `FUN_00416050(this, param_1, param_2)`, returns `undefined4`. Name-based load from a Curve-like object (`this`).
- If the requested name has no extension (`FUN_00404f40` returns `0xffffffff`), appends `".dat"` (`DAT_004fe7f0`, via `FUN_004079b0`).
- Cache check: `__stricmp(this+0x18, requested)`. On match it returns 0 without reloading.
- Otherwise: copies the new name into `this+0x14`, frees and zeroes `this+0x8`, `this+0xc`, `this+0x10`, then calls `FUN_00410fc0` and `FUN_00413e70(local_b0, name)`. On failure it returns `FUN_0040e0d0`.
- On success: iterates points in triples (stride 3 floats), pushes each through `FUN_00412b20` into `this+0x4`, and filters coordinates against bounds `_DAT_004fe3b4`, `_DAT_004fe7ec`, `_DAT_004fe750`.
- `FUN_00413e70(this, float)` is a separate function (line 21335). Its role in the load is UNVERIFIED.

Curve object size and ctor: NOT FOUND this session. The `FUN_004D1915` / `0x2c0` search matched but its callers were not read. UNVERIFIED.

## 3. Level description → curve link (PARTIALLY VERIFIED, `functions/FUN_004220a0_004220a0.c`)

`FUN_004220a0` parses one `<Graphics>` element (the per-level description). Observed reads, in order:
- `"id"` (`DAT_004fea38`): stored at `this+0xa4` (`FUN_00408e40`).
- `"dir"` (`DAT_004ff174`): presence check via `FUN_0040d770`. If absent, builds `"levels/" + id` via `FUN_0040c710(local_a4, "levels/", this+0xa4)` and stores at `this+0x38`. So the default curve directory is `levels/<id>`. If `dir` is present, the code path is not yet read (UNVERIFIED).
- Loop: `param_1 += 0x7c` while `param_1 < 0x174` (lines 266–294). Likely iterates 3 curve slots of stride `0x7c`. Slot count and base offset are UNVERIFIED (the loop start value was not read).
- `"curve"` (`DAT_004ff0e4`, line 657–679): `FUN_0040a4c0(local_34, local_a4, local_50)` value → `local_220`.
- `"curve1"` (`DAT_004ff0dc`, lines 684–706): value → `local_220` (same slot as `curve`; overwrite vs. second slot is UNVERIFIED).
- `"curve2"` (`DAT_004ff0d4`, lines 711–733): value → `local_1a4`.
- `"curve3"` (`DAT_004ff0cc`, lines 738–760): value → `local_128`.
- Also read in the same function: `"curvecolor"` (line 765–772, via `FUN_0040a5c0(..., 0x74, FUN_00402370)`), `"drawcurve"` (781–788, via `FUN_0040a5c0(..., 0x78, FUN_00402390)`), `"drawtunnels"`, `"destroyall"`, `"speed"`, `"maxspeed"`, `"start"`, `"colors"`, `"balls"` (registrations seen at lines ~797–910), `"graphics"` (line 298, used as a context string).

Offsets: `local_220`, `local_1a4`, `local_128` are local buffers. Their mapping to the 0x7c-stride slots and to Curve objects is UNVERIFIED. The link from these strings to a Curve object was not traced to a constructor call.

Level-object lookup (from prior notes, not re-verified this session): level object at `Board+0xf8`. `FUN_00404cf0` counts nonzero `[obj+0xc0]`, `[obj+0x13c]`, `[obj+0x1b8]`. These are probably the three curve slots (strides `0x7c` vs `0x7c` spacing differ: 0xc0 → 0x13c = 0x7c, 0x13c → 0x1b8 = 0x7c). This is consistent with the stride, but UNVERIFIED.

Board setup that calls the by-name loader (for each curve slot, with bound 3): NOT FOUND this session. UNVERIFIED.

## 4. levels.xml format (VERIFIED from files)

Files found:
- `D:/Files/Project/Reverse Engineer/Zuma Deluxe - Vanilla/levels/levels.xml` (422 lines, 23,336 bytes)
- `D:/Files/Project/Reverse Engineer/Zuma Deluxe - Clean Assets/levels/levels.xml`
- `D:/Files/Project/Reverse Engineer/Zuma Deluxe - Recompiled Mod/levels/levels.xml`

Structure: `<Levels>` root containing `<Graphics ...>` elements. Child elements include `<TreasurePoint x y dist1|dist2>`, `<Cutout image pri x y>`.

Real examples (Vanilla, lines 5, 11, 100):
- `<Graphics id="tiltspiral" curve="tiltspiral" image="tiltspiral" dispname="When Spirals Attack" gx="242" gy="248">`
- `<Graphics id="underover" curve="underover" image="underover" dispname="Mud Slide" gx="334" gy="247">`
- `<Graphics id="blackswirley" image="blackswirley" curve="blackswirley-1" curve2="blackswirley-2" dispname="Dark Vortex" gx="320" gy="240" >`

Note on the "2–3 curves" case: the grep of Vanilla `levels.xml` for `curve` returned 15 lines. Only one (`blackswirley`) has two curves (`curve`, `curve2`). No `curve3` appears in Vanilla. The Recompiled mod's levels.xml was not grepped.

Attributes observed for curves: `curve`, `curve2` (Vanilla); `curve1`, `curve2`, `curve3`, `curvecolor`, `drawcurve`, `drawtunnels`, `destroyall` (game registrations in FUN_004220a0). Note: the sidecar `zumaextrapath.c` parses `curve4`..`curve10` on `<Graphics>`. The game itself only registers up to `curve3`. The `curve` attribute value in Vanilla is a bare name like `tiltspiral`. The `.dat` file is found by appending `.dat`.

## 5. zumaextrapath.c (VERIFIED, 275 lines)

Sidecar DLL for extra paths 4..10 (`curve4`..`curve10`) for Zuma Deluxe v1.0.0.1. Stage 1: `ZEP_HOOKS_READY 0`, so it does not change game behaviour until hooks are installed. Logs to `zumaextrapath.log`; crash capture via vectored exception handler (`veh_`), same convention as `zuma_anim.c`.

Exports (VERIFIED names): `ZEP_Init`, `ZEP_LevelStart(levelId)`, `ZEP_LevelEnd`, `ZEP_ExtraCount`.
- `ZEP_LevelStart`: calls `ZEP_LevelEnd`, matches `g_levels[i].id` with `_stricmp`, and for `k` = 4..10 with non-empty `curve[k]` calls `try_load_`, counting successes.
- `ZEP_Init` reads `ZEP_XML_PATH` = `"levels/levels.xml"` and fills `g_levels`. Internal details UNVERIFIED (body not recovered).

Constants (VERIFIED): `ZEP_VANILLA_PATHS 3`, `ZEP_MAX_PATHS 10`, `ZEP_MAX_EXTRA 7`, `ZEP_MAX_CURV_POINTS 1000000u`.

Data structures (VERIFIED names, field layout from header comment): `ZepPoint {float x, y; uint8_t in_tunnel, priority}`, `ZepCurv {uint32_t version, count; ZepPoint *pts}`. The `g_levels`/`g_extra` declarations were not confirmed in recovered text, so treat their exact shape as UNVERIFIED.

Path resolution in `try_load_(id, name)`: the body was NOT recovered. UNVERIFIED whether it tries the `levels/<id>/` directory or the `.dat` suffix. Only the call `try_load_(levelId, g_levels[i].curve[k], &g_extra[k])` and the failure log `curve%d '%s' FAILED to load` were seen.

What it would need to plug in (per its own header, and as I read it):
- Creating extra Curve objects through the game's own code. Not implemented. Needs the Curve ctor and the operator-new size, which were not found (see §2).
- Hooks in the Board update/draw/collision/win/lose loops. Not implemented (`ZEP_HOOKS_READY = 0`).
- An EXE patcher to load the DLL (cf. `patch_anim_hook.py`). Not implemented.
- The curve attributes in levels.xml are read by the sidecar only for `curve4`..`curve10`. The game's own loader reads `curve`..`curve3` (§3). These sets do not overlap.

## Unverified / not reached

- Curve class constructor, vtable, and operator-new size (`0x004D1915` callers): not traced.
- Board init / level-start function that iterates curves and calls the by-name loader: not located.
- `dir` attribute non-default branch in FUN_004220a0.
- Whether `curve` and `curve1` write to the same slot (`local_220`) or different ones.
- Lines 801–1292 of FUN_004220a0 beyond the attribute registrations.
- `FUN_00413e70` body and its role.
- Recompiled Mod `levels.xml` curve usage.

## Files touched

- Created: `stage2_notes/map_curve.md` (this file).
- Modified: none.
