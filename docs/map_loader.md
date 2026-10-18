# Zuma Deluxe level-XML loader: curve / curve1 / curve2 / curve3

Scope: static, read-only analysis. `binary/Zuma.exe` was opened with `'rb'` only.
Sources: `Zuma_All_Decompiled.c` (Ghidra output) cross-checked against Capstone (32-bit) disassembly of the exe.

## 1. Enclosing function

| Item | Value |
|---|---|
| Function | `FUN_004220a0` (`undefined1 __thiscall FUN_004220a0(void *this, int param_1, char param_2)`) |
| Entry VA | `0x004220A0` |
| End | `ret 8` at `0x00423DDF`; next function `FUN_00423df0` at `0x00423DF0` |
| Decompile lines | 34669–35960 |
| Single caller | decompile line 37115: `FUN_004220a0(param_1,(int)local_c0,'\x01' - (uVar5 != 0))` |
| Stack frame | `sub esp, 0x2BC` at `0x4220B8` (plus SEH frame and stack cookie) |
| `this` / target | `iVar14 = param_1` (line 34761) is the per-level/per-curve loader target |
| Prologue | SEH frame (`push 0x4F664C`, `fs:[0]`), cookie XOR with `[0x532AE0]`, `edi = ecx` (this), `esi = 0xF` |

Scratch copies (outside project): `fn_4220a0.c`, `asm_4220a0.txt`.

## 2. Attribute match and parse mechanism (curve, curve1, curve2, curve3)

These four attributes do **not** go through the generic registrar `FUN_0040a5c0`. They use an inline block:

```
FUN_00408ff0(local_30, "<name>", len)     // set attribute name into local_30
gate = FUN_0040d770(local_50)             // attribute-present check (bl/al result)
if (gate) {
    pvVar3 = FUN_0040a4c0(local_34, local_a4 or local_a0 or local_50, local_50)  // fetch value string
    FUN_00408e40(<dest>, pvVar3, 0, 0xFFFFFFFF)                                  // std::string assign into dest
}
```

String constants (VA, verified from `binary/Zuma.exe` `.rdata`):

| Attribute | VA | Bytes | Name push site (asm) | Seq (decomp) | Value dest |
|---|---|---|---|---|---|
| `curve`  | `0x004FF0E4` | `curve\0` | `0x00422BAF` / `0x00422BDA` | 0x1D/0x1E | `local_220` (char[64]) |
| `curve1` | `0x004FF0DC` | `curve1\0` | `0x00422C59` / `0x00422C7D` | 0x1F/0x20 | `local_220` (char[64]) |
| `curve2` | `0x004FF0D4` | `curve2\0` | `0x00422CFC` / `0x00422D20` | 0x21 | `local_1A4` (char[124]) |
| `curve3` | `0x004FF0CC` | `curve3\0\0` | `0x00422D9F` / `0x00422DCA` | 0x23 (approx.) | `local_128` (char[124]) |

### Value-store call sites (asm)

| Attribute | Store call VA | `ecx` (dest) | Source of `pvVar3` |
|---|---|---|---|
| `curve`  | `0x00422C39` | `[ebp-0x21C]` = `local_220` | `FUN_0040a4c0` at `0x422C25` |
| `curve1` | `0x00422CDC` | `[ebp-0x21C]` = `local_220` | `FUN_0040a4c0` at `0x422CC8` |
| `curve2` | `0x00422D7F` | `[ebp-0x1A0]` = `local_1A4` (`0x1A4` in decomp frame) | `FUN_0040a4c0` at `0x422D6B` |
| `curve3` | `0x00422E2E` | `[ebp-0x124]` = `local_128` | `FUN_0040a4c0` at `0x422E19` |

Note: `curve` and `curve1` both assign into `local_220`. The later assignment (`curve1`) overwrites the earlier one. Whether the struct/slot is shared or distinct is therefore only a question of intent; in the code path, only one value survives in `local_220`. (Verified from decompile lines 35336 and 35363 and asm stores at 0x422C39 / 0x422CDC.)

### Helper semantics

| Function | Verified behavior |
|---|---|
| `FUN_0040d770(void*)` | Calls `FUN_0040a790(this+0x58, &local_8, ...)`. Compares `local_8` with `[this+0x5C]`. If equal, returns `local_8 & 0xFFFFFF00` (gate = 0). Else `FUN_00408e40(param_1, local_8+0x28, 0, -1)` and returns `1` in low byte. So it is a name-lookup that returns a boolean "found" and copies a sub-name string. **Partially verified** (the `FUN_0040a790` body not read). |
| `FUN_0040a4c0(this, out, path)` | Searches `path` for `'/'` and `'\\'`. If neither found, calls `FUN_00409f10(out, this+0x38, path)` and returns `out`. Otherwise, copies the string via `FUN_00408e40`. Returns the `std::string*` value buffer. |
| `FUN_00408e40(this, src, pos, count)` | `std::string::assign(src, pos, count)` (MSVC-style). `this` = dest std::string, `param_1` = source string, `param_2` = pos, `param_3` = count (npos). Copies `src` into `this`. |
| `FUN_00402350(char*, int*)` | `_sscanf(s, "%d", dst)`. Used for integer attributes (`speed`, `start`, `colors`, `balls`, `slowfreq`, `accfreq`, etc.). |
| `FUN_00402370(char*, int*)` | `_sscanf(s, "%x", dst)`. Used for `curvecolor` (0x74). |
| `FUN_00402390` | Handler for `drawcurve` (0x78), `drawtunnels` (0x79), `destroyall` (0x7A). (Not read; handler name recorded from registration only.) |

## 3. Storage destination for the curve values

The four curve attributes are stored as **std::string locals** in the frame of `FUN_004220a0`, not directly at object struct offsets:

| Attribute | Stored in (frame local) | Frame offset (asm) | Type | Stride note |
|---|---|---|---|---|
| `curve`  | `local_220` | `ebp-0x21C` | std::string (char[64] per decomp) | slot 0 |
| `curve1` | `local_220` | `ebp-0x21C` | same slot (overwrites `curve`) | slot 0 |
| `curve2` | `local_1A4` | `ebp-0x1A0` | std::string | slot 1 (`local_220 + 0x7C`, see §4) |
| `curve3` | `local_128` | `ebp-0x124` | std::string | slot 2 (`local_220 + 0xF8`, see §4) |

Frame layout consistency: `local_1a4 - local_220 = -0x7C` and `local_128 - local_220 = +0x... ` (decomp naming); the stride of **0x7C** between the three per-curve slots is confirmed by the copy loop (§4).

## 4. Per-curve struct (stride 0x7C) and the copy loop

Copy loop in `FUN_004220a0` (decompile 34954–34956 and surroundings):

```
do {
    ...
    FUN_0040caf0(local_220 + param_1, (void*)(param_1 + 0xD4 + local_a8));
    param_1 = param_1 + 0x7C;
} while (param_1 < 0x174);
```

- Iterations: `param_1 = 0x00, 0x7C, 0xF8` (three per-curve slots).
- Destination: `local_220 + param_1` (stride 0x7C).
- Source: `object + 0xD4 + local_a8` (from the level object, not from XML).
- `FUN_0040caf0(this, src)` = struct copy: `FUN_00408e40(this, src, 0, -1)` (copies the std::string at +0), then copies dwords at offsets `+0x1C .. +0x60` (`+0x1C,0x20,...,0x60,0x64`) from `src` to `this`. Total struct size = 0x7C. Field list beyond 0x64 not fully read (**UNVERIFIED** tail, to 0x7C).

Per-curve struct map (from `FUN_0040caf0`, **partially verified**):

| Offset | Size | Content |
|---|---|---|
| 0x00 | std::string (0x18 MSVC) | string member (curve / curve1 / curve2 / curve3 value) — **UNVERIFIED size** |
| 0x1C–0x64 | 4 each | dword fields copied verbatim (see `FUN_0040caf0` body) |
| 0x64–0x7C | — | **UNVERIFIED** |

Note: the decompile reports `local_220[64]` and `local_1a4[124]`/`local_128[124]` sizes. The 64 vs 124 discrepancy is a Ghidra array-size artifact; treat the std::string size as UNVERIFIED.

## 5. Where the three structs go next (LevelDesc)

- The loop above fills three per-curve structs on the stack of `FUN_004220a0`.
- Tail of `FUN_004220a0` (decompile 35898–35906) writes into the level object through `iVar14 = *(int*)((int)pvVar3 + 0x30)`:
  - `FUN_00419460(iVar14 - 0x220, &local_2cc)`
  - `*(iVar14 - 0x1B4) = *(pvVar15 + 0x1C) + param_1`
  - `*(iVar14 - 0x1A8) += local_a8`
  - if `*(iVar14 - 0x208) == 0`: `FUN_00408e40(iVar14 - 0x21C, &local_6c, ...)`
  - if `*(iVar14 - 0x1EC) == 0`: `FUN_00408e40(iVar14 - 0x200, local_88, ...)`
  
  This suggests the `-0x21C` slot is the object-side copy of `local_220` (`curve` value) and `-0x200` the copy of another string. **UNVERIFIED**: the exact copy of the three per-curve structs into LevelDesc was not located as a single block. The `iVar14 - 0x21C` = object offset relative to `pvVar3+0x30` must be resolved to a concrete LevelDesc offset.

- Board-side check: `FUN_00404cf0` (VA `0x404CF0`) reads `obj+0xC0`, `obj+0x13C`, `obj+0x1B8` (presence booleans, one per curve 0..2 with stride 0x7C = 0xC0/0x13C/0x1B8 differ by 0x7C). This supports the 3-slot × 0x7C layout, with obj base such that slot0 at `+0xC0`. **UNVERIFIED** as the same object as `iVar14`.

- Other `+0xf8` uses: `Board+0xf8` holds a pointer (`*(param_1+0xf8)`); `FUN_00404cf0` is called with `*(param_1+0xf8)` (decomp 5523). Consistent with the LevelDesc being reached via Board+0xf8.

- sizeof(LevelDesc), allocation site, and copy ctor: **NOT RESOLVED** in this pass.

## 6. Registration-based attributes (for context)

Registered via `FUN_0040a5c0((int)&local_2cc, iVar14, (int)local_30, <off>, <handler>)`:

| Attr | Offset | Handler | Decomp line |
|---|---|---|---|
| curvecolor | 0x74 | `FUN_00402370` (`%x`) | 35435 |
| drawcurve | 0x78 | `FUN_00402390` | 35451 |
| drawtunnels | 0x79 | `FUN_00402390` | 35467 |
| destroyall | 0x7A | `FUN_00402390` | 35483 |
| speed | 0x20 | `FUN_00402330` | 35499 |
| maxspeed | 0x60 | `FUN_00402330` | 35515 |
| start | 0x24 | `FUN_00402350` | 35531 |
| colors | 0x38 | `FUN_00402350` | 35547 |
| balls | 0x64 | `FUN_00402350` | 35627 (partly elided) |
| slowfreq | 0x68 | `FUN_00402350` | 35643 |
| accfreq | 0x6C | `FUN_00402350` | 35659 |

Other offsets registered (names not resolved): 0x2C, 0x50, 0x54, 0x48, 0x70, 0x30, 0x34, 0x28, 0x1C, 0x44, 0x3C, 0x40, 0x5C, 0x58. Name-VA mapping for these is **UNVERIFIED** (string pool entries 0x4FF09C, 0x4FF078, 0x4FF06C, 0x4FF064, 0x4FF058, 0x4FF050, 0x4FF048, 0x4FF040, 0x4FF038, 0x4FF02C, 0x4FF024, 0x4FF018, 0x4FF0F8, 0x4FF0B4 not yet named).

Open conflict: offset `0x1C` (decomp 35731, handler `FUN_00402350`) vs `powerfreq` (VA 0x4FF0EC, pushed at asm 0x422AEF). `powerfreq` uses an `sscanf "%d"` path into `local_1BC` (3×4 int table), not the registrar. Which name owns `0x1C` is **UNVERIFIED**.

## 7. Function-level checklist: LevelDesc offsets 0xA4..0x234

Only the following were directly confirmed:
- `FUN_00404cf0` @ `0x404CF0` reads `+0xC0`, `+0x13C`, `+0x1B8`.
- `FUN_004220a0` @ `0x4220A0` writes `iVar14 + {-0x21C,-0x200,-0x220,-0x1B4,-0x1A8,-0x208,-0x1EC}` (object-relative, `iVar14 = pvVar3+0x30`).
- `FUN_0040caf0` @ decompile line 14022: struct copy with stride 0x7C fields.
- Other usages of `+0xa4`/`+0x58` at decompile lines 818 and 943 (not analyzed for this struct).

Full grep pass for all `+0xa4..+0x234` references was **not completed** in this pass.

## 8. Unresolved / UNVERIFIED items

1. `FUN_0040a790` body (used by `FUN_0040d770`) — gate semantics only partly inferred.
2. Exact `std::string` size in the per-curve struct (0x18 vs. decompile array sizes 64/124).
3. Mapping of `iVar14 - 0x21C` (object) to a LevelDesc field: the `local_220` slot is copied at the tail, but the offset in LevelDesc is not confirmed.
4. sizeof(LevelDesc), allocation (operator new size), copy ctor / operator= site: not located.
5. Name for offset 0x1C (`powerfreq` conflict).
6. Full string-pool name mapping for the 14 unresolved VAs above.
7. Full field list of `FUN_0040caf0` beyond 0x64.
8. Whether `curve`/`curve1` should share the slot (code says yes, overwrite behavior confirmed in code path only).

## 9. Hook candidates (for a binary patcher; no patch applied)

- curve name/parse blocks (exe VA): `curve` 0x422BAF–0x422C58, `curve1` 0x422C59–0x422CFB, `curve2` 0x422CFC–0x422D9E, `curve3` 0x422D9F–0x422E56 (approximate block boundaries from asm; raw bytes not dumped in this pass).
- Safest hook point for "value fetched" is the `call 0x40A4C0` return (`0x422C25`, `0x422CC8`, `0x422D6B`, `0x422E19`), where `eax` holds the `std::string*` value.
- Struct copy hook: `FUN_0040CAF0` call in the loop (decompile 34954–34956; asm not located in this pass).

Raw byte dump and capstone listing of each block: **not completed in this pass**; write-up is based on asm excerpts and decompile.
