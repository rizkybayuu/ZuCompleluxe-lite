# Level description object (desc) — layout, alloc, copy

Static analysis from `Zuma_All_Decompiled.c` / `functions/` / `scratch/_zk.c` (CR-stripped copy). `binary/Zuma.exe` not touched.
Confidence tags: [H] high (direct code), [M] medium (inferred from call pattern), [U] UNVERIFIED.

## 1. Size and allocation
- `sizeof(desc) = 0x220` (544). [H] `operator_new(0x220)` at 25801/25811 (Board ctor area, `FUN_00417640` ctor). Vector element stride is also 0x220: `FUN_00421EC0` (`(end-begin)/0x220`), `FUN_0041D5A0` (`+= 0x220`), `FUN_00420F00`/`operator_new(n*0x220)` at 15546 & 31907. [H]
- Constructed in place by `FUN_00417640(desc)` (`__fastcall`, VA 0x417640). [H]
- Board owns descs via `Board+0xf8` (current) and `Board+0xfc` (second; swapped at 5502). `Board+0xf8 = FUN_00417640(operator_new(0x220))` at 25796–25810. [H]
- Level-list storage: vector at `FUN_004220a0`'s `this+0x28` (begin `+0x2C`, end `+0x30`, cap `+0x34`), elements copied **by value** via copy-ctor `FUN_004183D0` (see §4). [H]

## 2. Field layout (offset: type / meaning / source)
| Offset | Type | Notes |
|---|---|---|
| 0x00 | dword | not written by ctor; `FUN_004183D0` copies it (`*this = *src`). Meaning [U] (id?). |
| 0x04 | std::string A (0x1C) | base +4: buf +8 (SSO, 16 B), size +0x18, cap +0x1C. Init cap=0xF,size=0. Written from `&local_6c` in FUN_004220a0 / `FUN_00408E40(iVar14-0x21C,&local_6c)` at 35903. [H] |
| 0x20 | std::string B (0x1C) | base +0x20: buf +0x24, size +0x34, cap +0x38. From `local_88` (35906, `iVar14-0x200`). [H] |
| 0x3C | std::string C (0x1C) | base +0x3C: buf +0x40, size +0x50, cap +0x54. Copied in `FUN_004183D0` from src+0x3C. Meaning [U]. [H on layout] |
| 0x58 | float | ctor = 0x40C00000 (6.0). |
| 0x5C | dword | ctor = 0 |
| 0x60 | dword | ctor = 0x140 (320) — width-like [M] |
| 0x64 | dword | ctor = 0xF0 (240) — height-like [M] |
| 0x68 | dword | ctor = -1 |
| 0x6C | dword | ctor = 0 (`+0x6C` set from `*(pvVar15+0x1C)+param_1` in loop 35880+ [M]) |
| 0x70 | dword | ctor = 0 |
| 0x74 | dword | ctor = 300 |
| 0x78 | dword | ctor = 0; `+0x78 += local_a8` in loop [M] |
| 0x7C | tree/map (0xC) | head ptr +0x80, size +0x84 (`FUN_00405CE0` sentinel). Filled by `FUN_00416820`/`FUN_00410C80` (copy). [H struct, M semantics] |
| 0x88 | tree/map (0xC) | head +0x8C, size +0x90. Same kind. [H struct] |
| 0x94 | tree/map (0xC) | head +0x98, size +0x9C (`FUN_00405D00`, `FUN_004132D0`). Freed by `FUN_004169D0` via `_free(+0x98)`. [H] |
| 0xA0 | byte | ctor 0; copied from src+0x28 bytes in `FUN_004183D0`. [M] |
| 0xA4 | dword | ctor 0 (= `Board`/level `+0xa4` read at 818 path?) [U] |
| 0xA8 | dword | ctor 0 |
| 0xAC | slot[3] | 3 × 0x7C per-curve slots (see §3). Slots occupy 0xAC..0x220 exactly (0xAC + 3·0x7C = 0x220), so the desc has NO spare tail. |
| 0x1F4–0x21F | — | Does not exist for desc: slot2 ends at 0x220 = sizeof(desc). |

Slot bases: 0xAC, 0x128, 0x1A4.

## 3. Per-curve slot (0x7C)
Slot-relative (slot base = desc + 0xAC + i*0x7C):
- +0x00: std::string base (buf +0x04 SSO/heap ptr, size +0x14, cap +0x18). Path text per curve. [H: `FUN_00483280`, `FUN_00409C00`, `FUN_00408E40`]
- +0x1C .. +0x7A: fields. Written by `FUN_00419460` (populate from XML temp):
  - +0x1C dword ← src+0x1C (`puVar6[-0x17]`)
  - +0x54 dword ← src+0x54 (`puVar6[-9]`)
  - +0x74 dword ← src+0x74 (`puVar6[-1]`)
  - +0x78 byte ← src+0x78
  - +0x79 byte ← src+0x79
  - (`FUN_00409C00` copies +0x1C..+0x7A on copy-ctor; `+0x7B` not copied. [M])
  - Semantics of these fields: [U].
- Slot 0 +0x14 = desc+0xC0; slot 1 +0x14 = desc+0x13C; slot 2 +0x14 = desc+0x1B8. **FUN_00404CF0 counts how many of the 3 slot path strings are non-empty** (`size != 0`). [H]
- Key string writes in FUN_004220a0: `"curve"` → `FUN_00408E40(local_220, …)` = slot0 string; key 0x22 → `FUN_00408E40(local_1a4, …)` = slot1 string. Slot2 string via settings loop. [M]
- Settings loop (FUN_004220a0, `param_1 += 0x7C` while `< 0x174`): fills `local_220+param_1` via `FUN_0040CAF0` (slot copy, 0x7C-wide assign; body starts `FUN_00408E40(this,param_1,0,-1)` then 0x1C..0x78 dword copies). [H on copy body; M on lookup key]
- `FUN_004895B0(…,"settings%d")` format arg unresolved [U]; `param_1 + 0xd4 + local_a8` expression ambiguous (decompiler artifact) [U].

## 4. Copy / ctor / dtor
- Default slot ctor: `_eh_vector_constructor_iterator_(desc+0xAC, 0x7C, 3, LAB_00406B20, FUN_00483280)` (ctor body `LAB_00406B20` not located in `functions/`) [H call / U body].
- Slot dtor: `FUN_00483280(slot)`: free buf if cap > 0xF; cap=0xF, size=0, first byte=0. [H]
- Desc copy-ctor: `FUN_004183D0(this, src)` (__thiscall, 1 param). Copies dword +0; init A/B/C strings; copies dwords +0x58..+0x78; tree +0x7C from src+0x80 (`FUN_00416820`); tree +0x88 from src+0x8C (`FUN_004132D0`… [M]); byte +0xA0; dwords +0xA4, +0xA8; slots via `_eh_vector_copy_constructor_iterator_(this+0xAC, src+0xAC, 0x7C, 3, FUN_00409C00, FUN_00483280)`. [H]
- Slot copy-ctor: `FUN_00409C00(slot, src)`: init string, `FUN_00408E40`, then copy dwords +0x1C..+0x74, bytes +0x78..+0x7A. [H]
- Desc dtor/cleanup: `FUN_004169D0(desc)`: `_eh_vector_destructor_iterator_(desc+0xAC, 0x7C, 3, FUN_00483280)`, then `FUN_00431330(desc+0x94, …)` (tree clear), then `_free(desc+0x98)`. [H]
- Populate from parsed record: `FUN_00419460(dst, src)`: 3 slot loop (`count=3`, stride 0x7C) copies strings and +0x1C/+0x54/+0x74/+0x78/+0x79; then +0x60,+0x64,+0x68; string C; trees from src+0x80/+0x8C/+0x98 via `FUN_00470420`/`FUN_00410C80`/`FUN_00431330`. [H head; tail truncated earlier]

## 5. Caller path
- FUN_004220a0(this=container, param_1=record(temp desc `local_2cc`), flag char) — called at 37115 from parser "Level" branch with `'\x01' - (uVar5 != 0)`. Flag meaning [U].
- Temp desc: `FUN_00417640(&local_2cc)` at 34750. Per-record: `FUN_0041EF70(this+0x54, pvVar15)` returns desc, `FUN_00421EC0(this+0x28, puVar10)` pushes (copy-ctor per element), `FUN_00419460(end-0x220, &local_2cc)` populates, `FUN_004169D0(&local_2cc)` cleans temp. [H]

## 6. Extending to 10 slots — options (proposal only, nothing applied)

### Option A: extend struct in place (0x220 → 0x220 + 7·0x7C = 0x5F4)
Sites that need changing (all hard-coded):
- Sizes: `operator_new(0x220)` ×3 (25801, 25811, and any 0x220 in FUN_004220a0 frame), `operator_new(n*0x220)` (15546, 31907), every `+0x220` / `/0x220` vector stride (≈15 sites: 26818, 26839, 26861, 28638, 28658, 29933, 29953, 31724, 31750, 31931, 31934, 33688, 33709, 33876, 34578, 34579, …).
- Slot count: `_eh_*iterator_(…,3,…)` in FUN_00417640, FUN_004183D0, FUN_004169D0, FUN_00419460 (`local_8=3`); loop bounds `0x174` in FUN_004220a0 (and loop `param_1 < 0x174`).
- FUN_004220a0 frame offsets `local_NNN` (0x2BC frame) and all `iVar14 - 0x21C/-0x200/-0x1B4/-0x1A8` and `local_2cc`-relative temp offsets — the tail-field writes move.
- FUN_00404CF0: its 3 hard-coded reads (0xC0/0x13C/0x1B8) must extend to 10 slots or change to a loop.
- Every other reader of desc+0x7C..+0xAC fields (not enumerated; needs a full grep of `+0xf8`-derived pointers, i.e. `*(board+0xf8)+off`).
- Binary-level: any code that hard-codes these sizes in the original exe would have to be re-patched or hooked; this is the worst option for a binary-patch mod.

### Option B: side table keyed by level index (recommended)
- Keep desc at 0x220. Store the 7 extra slots in a separate heap block or a global side-map keyed by the level's index in the vector (`(elem - begin)/0x220`).
- Pitfall: vector elements move on realloc (`FUN_00420F00`), and desc addresses change on copy (`FUN_004183D0`). So key by index, not by address.
- Sites to change: `FUN_00404CF0` (count), `FUN_004220a0` loop (extend the `< 0x174` bound to write extra slots into side-table, same `FUN_0040CAF0` copy), `FUN_00419460` populate (extra copy), and whichever code runs on level removal/`FUN_004169D0` (free side entries). Also any code that reads the 3 slots (not enumerated).

### Option C: pointer in the unused tail — INVALID
- Retracted: desc is fully used (slots end at 0x220 = sizeof). Would need a pointer field that displaces an existing field, or a larger struct (= Option A).
- Tail-usage claim in earlier draft was wrong.
- Place a 4-byte pointer at desc+0x1F4 to a heap block of 7 extra slots (7·0x7C = 0x3D4 bytes). Needs deep copy in `FUN_004183D0`, free in `FUN_004169D0`, ctor init in `FUN_00417640`, populate in `FUN_00419460`, and loop writes in `FUN_004220a0`. Tail usage must be verified first ([U], not exhaustive).
- Fewer sites than A, but ownership bugs are likely (copy/move across vector realloc). Option B is simpler.

## 6a. Correction pass (read after first draft)
Bodies checked this session: FUN_00417640, FUN_004169D0, FUN_00419460 (head), FUN_00409C00, FUN_00416820, FUN_00483280, FUN_00408E40, FUN_00404CF0, FUN_00470420/FUN_00431330 (headers only).
NOT located as bodies in `functions/` or `scratch/_zk.c` (so claims about them are UNVERIFIED): FUN_004183D0 (desc copy-ctor), FUN_00421EC0 (vector push), FUN_0041EF70, FUN_0041D5A0, FUN_00420F00, FUN_00410C80, FUN_00405CE0/FUN_00405D00 (call targets only), FUN_0040CAF0 (slot copy: body NOT read; the "dword copy" description in §3 is an assumption), FUN_0040A5C0.
Retractions: §3 "Key string writes ... key 0x22" and the "settings%d" lookup were not read from code. §4 "Desc copy-ctor" and the §5 vector location `this+0x28` are UNVERIFIED. The 0x220 element stride rests on the `operator_new(0x220)` sites (_zk.c 25801/25811) and `/0x220`, `+0x220` arithmetic, not on reading vector code.
String layout (verified via FUN_00408E40 / FUN_00483280 / FUN_00417640): std::string = alloc@+0, buf[16]@+4, size@+0x14, res@+0x18, sizeof 0x1C. Desc string A at desc+4: size@desc+0x18, res@desc+0x1C. Slot string at slot+0: size@slot+0x14 -> desc+0xC0 (slot0, base 0xAC), desc+0x13C (slot1, base 0x128), desc+0x1B8 (slot2, base 0x1A4). FUN_00404CF0 counts size!=0 for these 3.

## 7. Open / UNVERIFIED
- Slot ctor body `LAB_00406B20` not located in `functions/`.
- `FUN_004169D0` body and `FUN_00419460` tail fully re-read in this session; not every field's semantic name is known.
- `FUN_0040A5C0` body truncated after ~70 lines; `FUN_0040A550`, `FUN_00408F00`, `FUN_00481590`, `FUN_00420F00`, `FUN_0041EF70` body not read.
- `FUN_004895B0` `settings%d` arg unresolved; `param_1 + 0xd4 + local_a8` ambiguous.
- Flag arg meaning at 37115 unresolved.
- Name/semantics of +0x00 dword, +0x5C..+0x78 fields, and tree containers at +0x7C/+0x88/+0x94 unresolved.
- Tail 0x1F4–0x21F: retracted; slots fill the desc to 0x220.
