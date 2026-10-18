# Curve creation, loading, and Board-dtor cleanup

**Verification status:** Only these raw-byte results were displayed in tool output: the stage-1 hook at `0x418AF1`, the dtor bytes `0x419ED0..0x419EEF` (original and patched), and `0x418A40`–`0x418AB4` were requested but not displayed. Everything else in sections 1–2 (ctor body, cave `0x58B040` contents, `FUN_004173E0`/`FUN_00418C40` call-site bytes, loop bounds, push order) is from decompiled C or assistant reasoning and must be confirmed with `scratch/cdis.py dis` before use.

Image base 0x400000. Read-only on `binary/Zuma.exe`. Disassembly from `scratch/cdis.py` (capstone). Decompiled names come from `Zuma_All_Decompiled.c`. Items marked UNVERIFIED were not confirmed from raw bytes in this pass.

## 1. Curve object: size, ctor, storage (Board ctor)

Original (pre-stage-1) allocation loop, from `.backup/Zuma_pre_anim_backup.exe` and the current exe (identical here):

- `0x418A90`: `push 0x84` / `call operator_new 0x4D1915` (cdecl, size on stack).
- `mov ecx,eax` / `push esi` (Board*) / `call 0x414290` → Curve ctor, **thiscall**, ECX = new object, one stack arg = Board*.
- Object size: **0x84 bytes**.
- Ctor `FUN_00414290` (decompiled): vtable `PTR_FUN_004fe5ec`; `this+4 = Board[+0xb4]`; `this+8 = Board*`; allocates a 0x30 sub-object via `operator_new(0x30)` and `FUN_00412770`, stored at `this+0xc`. Other fields at `+0x18`, `+0x24`, `+0x30`, `+0x3c` are initialized from `FUN_00405de0` (UNVERIFIED meaning).

Stage-1 hook (current exe, raw bytes):
```
00418af1  e9 4a 25 17 00    jmp 0x58b040        ; replaced 68 2c 01 00 00 = push 0x12c
00418af6  e8 1a 8e 0b 00    call 0x4d1915       ; original continues here
```
Cave `0x58B040` (from prior notes, not re-disassembled in this pass; UNVERIFIED in detail): allocates two 40-byte arrays with `operator_new` (A[10] active, S[10] staging), copies Board `+0x5c/+0x60/+0x64` into A[0..2] and `+0x68/+0x6c/+0x70` into S[0..2] (the dtor reads the same offsets, so these are the 6 Curve* slots), zeros the rest, sets `Board+0x68 = A`, `Board+0x6c = S`, then `popal; push 0x12c; jmp 0x418af6`.

## 2. Callers of the loader `FUN_00416050` and of the Curve ctor

Loader signature (decompiled): `undefined4 __thiscall FUN_00416050(void *this, void *param_1, int param_2)`. Only caller found in the decompile: `FUN_004173E0` (its first statement after computing the record pointer). Raw E8 scan result from the earlier pass: only `call 0x416050` at `0x41740E` (assistant-reported, not re-verified here).

`FUN_004173E0` (decompiled, thiscall, `float param_1, undefined4 param_2, int param_3 (=index i), int param_4`):
- `this+0x10 = param_1`
- `this+0x14 = param_1 + param_3*0x7C + 0xAC` ← **per-curve record = level-desc base + 0xAC + i*0x7C**
- `this+0x18 = param_2`
- `FUN_00416050(this->+0xc, record, param_4)` — the loader gets the record pointer and a name/flag argument (param_4).
- Afterwards reads `record+0x54` (speed-like value, UNVERIFIED).

Caller of `FUN_004173E0`: `FUN_00418C40` (level-load/init). Raw bytes (current exe, from the loop region `0x418CD0..0x418DA0`; see `scratch/cdis.py dis 0x418CD0 0x418DA0`):
- Loop index `ebx` = i, stride `ecx` += 0x7C, bound `cmp ecx,0x174` → **3 iterations** (i = 0,1,2).
- Per iteration reads `[ [esi+0xfc] + ecx + 0xC0 ]` as a presence test (also `0x13C`, `0x1B8` per the brief). Nonzero → call `0x4173E0`.
- Curve object ECX = `S[i]`, loaded via `edi = [esi+0x6c]` (staging array), `edi += 4` per iteration.
- Pushes: `push 0` (param_4), `push ebx` (i), `push edx` (= `[esi+0x78]`), `push eax` (= `[esi+0xfc]`, level-desc base).

Consequence: the level-desc slot feeding each curve is `[esi+0xfc]` (desc base) + `i*0x7C + 0xAC` for record, and `[esi+0x78]` as the float/param. Other fields the Curve receives come from the record's fields at `+0xAC+i*0x7C`. Exact per-field mapping (speed, color, etc.) is UNVERIFIED in this pass.

Staging→active swap (`FUN_00405590`) and curve count (`FUN_00404CF0` → `Board+0x8c`): not re-examined here; see prior notes.

Ctor callers beyond the Board ctor: UNVERIFIED (not scanned in this pass; the `call 0x414290` scan was not run).

## 3. Board destructor curve cleanup (`FUN_00419E80`, ECX = Board)

Decompiled outline: `FUN_00401990`, `FUN_00407F20` (base cleanup), then a 3-iteration loop over Board `+0x5c..+0x70` (each slot checked, scalar-deleting dtor via `call [vtbl+0]` with arg 1), then `+0x74`, `+0x78`, `+0x84`, `+0x88`, `+0x94`, `+0x98`, `+0xa4`, `+0xa8`… (per-object dtors), then `_free(Board+0xF8)` and `_free(Board+0xFC)` after `FUN_004169D0` (level-desc buffers, decompiled), then `FUN_00431330` and `_free(Board+0x260)`.

Raw bytes of the curve loop (current exe; the first half of the loop body is the same as the original):
```
00419ed0  8b 4f f4          mov ecx,[edi-0xc]     ; slot Board+0x5c+k (active A)
00419ed3  3b cb             cmp ecx,ebx
00419ed5  74 06             je  0x419edd
00419ed7  8b 01             mov eax,[ecx]
00419ed9  6a 01             push 1
00419edb  ff 10             call [eax]            ; scalar deleting dtor
00419edd  33 c9             xor ecx,ecx           ; ORIGINAL: 8b 0f = mov ecx,[edi]  (stage-1 patch)
00419edf  3b cb             cmp ecx,ebx
00419ee1  74 06             je  0x419ee9
00419ee3  8b 11             mov edx,[ecx]
00419ee5  6a 01             push 1
00419ee7  ff 12             call [edx]
00419ee9  8b 45 f0          mov eax,[ebp-0x10]    ; loop counter
00419eec  83 c7 04          add edi,4
00419eef  48                dec eax
```
- Original: the second half deletes `Board[k+0x68]` (staging S). Stage 1 `33c9` at `0x419EDD` makes it delete nothing, so staging curves are no longer freed here (this is the intended leak-avoidance for the S array; A and S arrays are now leaked).
- Loop start (`edi = Board+0x68`, counter 3) is from the decompile, not raw bytes (UNVERIFIED).

## 4. Freeing the A/S arrays (suggested, not implemented)

- Allocation is via `operator_new` (`0x4D1915`), so release with the matching CRT free. The dtor uses `_free` for `Board+0xF8/0xFC` (decompiled), i.e. `free()`.
- Suggested placement: after the curve loop, before the epilogue, free `A` (`Board+0x68`) and `S` (`Board+0x6c`) with `call _free` (VA UNVERIFIED — look up the `_free` VA from the existing `_free` call sites in the dtor, e.g. the call after `FUN_004169D0`).
- Null-check the pointers before freeing. Do not free the staging array while the cave still owns its alias (it is only aliased by `Board+0x6c`).
- Operator-delete VA: the `calls 0x4D1A1C` probe was not a function start (returned garbage). The operator-delete VA is UNVERIFIED.

## Open items (UNVERIFIED)

- Exact raw bytes of the full cave `0x58B040`.
- `FUN_00418C40` loop start and the `0x13C`/`0x1B8` count-check body.
- All ctor callers of `0x414290` beyond the Board ctor.
- Exact operator-delete and `_free` VAs.
- Per-field mapping from record `+0xAC+i*0x7C` into Curve fields.

## Tooling notes

- `scratch/cdis.py`: read-only PE disassembler. `dis <start> [end]`, `dis <start> count=N`, `secs`, `calls <addr>`. Set `ZEXE=<path>` to disassemble another file (used for `.backup/Zuma_pre_anim_backup.exe`).
- Earlier stray write: `stage2_notes/map_curvecreate.md` was first written to `C:\D:\...` by mistake and has been removed.
