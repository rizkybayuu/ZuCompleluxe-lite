# Map limits: hard-coded 3-curve assumptions (Zuma.exe 1.0.0.1)

Status: PARTIAL. Binary read-only (MD5 ee8a1f2363af762723a3054d66b545c0). Tags: VERIFIED = seen in disassembly or decompile this pass; UNVERIFIED = inferred or not traced.
Tools: `scratch/zdis_limits.py` (capstone PE disasm), `scratch/sweep_limits.py` (.text sweep, 2615 hits, output `sweep_limits.txt`), `scratch/ctx_limits.py` (context dump).

## 1. Curve-slot layout (VERIFIED)
- Curve slot record = **0x7c bytes**. `FUN_004220a0` loops `param_1 += 0x7c` while `< 0x174`. 0x174 = 3 x 0x7c exactly, so the parser reads exactly 3 curve slots (`curve`/`curve1`, `curve2`, `curve3`). Arithmetic VERIFIED; the "3 slots" intent is consistent with the code.
- `FUN_00404CF0` (line 4670, `char __fastcall`) returns `(+0xC0 != 0) + (+0x13C != 0) + (+0x1B8 != 0)`. Offsets 0xC0, 0x13C, 0x1B8 are 0xC0 + i*0x7c for i = 0,1,2. VERIFIED. This is the curve count, structurally capped at 3.
- Its caller `FUN_00405590` (line 5494) passes `*(this+0xF8)` as the object (`mov ecx,[esi+0xf8]` at 0x4055F8) and stores the result at `[this+0x8c]` (0x405603). VERIFIED.

## 2. Literal 3/6 sites

| VA | Function (decompile name) | Code | What it is | Must change for 10? |
|---|---|---|---|---|
| 0x4239E1 | FUN_004220a0 (level parser, line 34669) | `mov edx,3` then loop `dec edx; jne` with `add ecx,0x7c` (0x4239E6..0x423A01) | Loop over 3 curve slots at `ebp-0x1dc`, stride 0x7c. Divides a float field at `ecx-4` by the field at `ecx`. | **YES.** Patch `ba 03 00 00 00` (mov edx,3) to `ba 0a 00 00 00` (mov edx,10), and enlarge the stack buffer so 10 slots fit. VERIFIED that it is a 3-count slot loop. Local slot buffers `local_1a4[124]` and `local_128[124]` are sized for 1 slot each (element type UNVERIFIED). |
| 0x42398F / 0x42399F | FUN_004220a0 | `call 0x404CF0` on `ebp-0x2c8` (a stack curve-set object), then `cmp eax,1; jle`, then `fild` | Counts non-empty curve slots in the parser's local set, then uses it in a float multiply. | **YES, via 0x404CF0** (sec 3). Stack object size must hold 10 slots (UNVERIFIED). |
| 0x404CF0 | FUN_00404CF0 | three fixed checks at +0xC0, +0x13C, +0x1B8 | Curve count, max 3. | **YES.** Must become a loop over i=0..N-1 at `0xC0 + i*0x7c`. Return type `char` is fine up to 10. Rewrite function (or code cave). Exact bytes UNVERIFIED. |
| 0x405603 | FUN_00405590 | `mov [esi+0x8c],eax` | Writes curve count to `this+0x8c`. | No change if 0x404CF0 is fixed. |
| 0x41A488 / 0x41A494 | FUN_0041A4xx (caller of 0x404CF0, `ecx=edi`) | `cmp esi,6; jle; mov [ebp-0x10],6` | Clamps a computed value (`(x/...)+4`) to **6** before the call. | **UNVERIFIED.** 6 is 2 x 3. If the value is curve-related, raise to 12 or 2 x N. Function not decompiled this pass. |
| 0x43BD73 / 0x43BD82 | FUN_0043bc60 (line 57794) | `cmp edx,3; jle; mov [esi+0x8c],3` (else `-1`) | Writes the result of `(+0x90 - +0x8c)/0x1c` to `+0x8c` (line 57835), then clamps to [-1..3]. | **UNVERIFIED — do not patch yet.** The `+0x8c` pointer is overwritten with a count, and the clamp target may be a node/record index, not a curve count. Needs its meaning confirmed. |
| 0x43BDDB | FUN_0043bc60 | `mov [esi+0x8c],3` under `DAT_00549660 != 0` (line 57874) | Forced value 3 when a global flag is set. | **UNVERIFIED.** Same object as 0x43BD82. If that object is the curve set, this must become 10 or `N`. |
| 0x4393DE | FUN_00439170 (line 55777) | `cmp [edi+0x8c],3; jl 0x4394da` | Skips a block when `+0x8c < 3`. | **UNVERIFIED.** Likely a "fewer than 3 curves" gate. If `+0x8c` is the curve count, change 3 to the new minimum behavior (review). |
| 0x428333 | FUN_00428320 | `mov [esi+0x60],6` | Writes literal 6 to object+0x60 (not Board, UNVERIFIED). | No, unless object is Board. |
| 0x428EC1 | function containing 0x428E9D (UNVERIFIED name) | `mov [esi+0x60],3` | Same object as above. | No, unless object is Board. |
| 0x433123 / 0x433233 | FUN_00433110 / FUN_00433220 (`ebx=this`) | `cmp [ebx+0x64],3` and `cmp [ebx+0x64],6` | Compare a value at +0x64 to 3 and 6. | **UNVERIFIED.** +0x64 is a Board mirror of A[2] (Stage 1). If it holds a curve count, it fails for >3. |

## 3. Board +0x5c..0x78 accesses (VERIFIED for the listed functions)
- `FUN_00405590` (5494–5535): swaps +0xF8/+0xFC; +0x5C<->+0x68; +0x74<->+0x78 (x3, net 1); +0x60<->+0x6C; +0x64<->+0x70. Writes +0x8c. **Fixed 3-curve mirror**: the swaps touch 3 slot mirrors (+0x5c/60/64) only. Must become a loop over N if those mirrors are per-curve. UNVERIFIED that the 8 fields are curve slots.
- `FUN_0043bfd0` (57970–57982): reads +0x60, +0x5C, +0x64, +0x68, +0x70, +0x6C. Does not read +0x74/+0x78. Fixed 3-curve read set.
- `FUN_00439170` (55836–55850): loop over curve index `i`, state from `+0x84 == i`, `+0x88 == i`, `+0x8c < i`. Loop bound is `+0x8c` (count), so this loop **already scales** with count. The pass-through `*(this+0x74)` at line 55829 goes to FUN_0044d470 (UNVERIFIED target).
- Sweep: 168 displacement hits on `+0x8c`, many not Board (shared offsets across object types). Full Board-only classification NOT completed. UNVERIFIED for any hit not listed here.

## 4. Per-curve fixed arrays
- `FUN_004220a0` locals (34663–34722): `local_254[10]` (10 ints, already sized for 10), `local_1bc[6]`, `local_1a4[124]`, `local_128[124]`, `local_220[64]`, `local_1e0[4]`, `local_290[28]`, `local_2c8[20]`, `local_2ac[20]`. **Per-curve**: `local_1a4` and `local_128` (124 bytes each = 0x7c). For 10 curves, the slot buffer needs 10 x 0x7c = 0x4D8 bytes. UNVERIFIED element type.
- The 0x174 bound (3 x 0x7c) in FUN_004220a0 must become 0x4D8 (10 x 0x7c = 1240) if the loop is meant to read 10 slots. The bound's exact location and bytes were NOT extracted (not found in the sweep's imm search). UNVERIFIED.
- Stride 0x1c (28) used in FUN_0043bc60 (line 57835) for the 0x1c-record list. Not a curve stride. No change.
- Other 0x7c-stride sites (imul/lea) were NOT enumerated this pass. UNVERIFIED.

## 5. Must-change summary (for 10 curves)
1. **0x404CF0**: rewrite 3 fixed checks as loop over N slots at 0xC0 + i*0x7c. REQUIRED.
2. **FUN_004220a0 loop bound 0x174 → 0x4D8** (10 x 0x7c). REQUIRED. Exact location UNVERIFIED.
3. **0x4239E1**: `mov edx,3` → `mov edx,10` (bytes `ba 03 00 00 00` → `ba 0a 00 00 00`). REQUIRED. Encoding VERIFIED.
4. **Stack/slot buffers** `local_1a4`, `local_128`, `ebp-0x2c8` object, `ebp-0x1dc` region: enlarge to 10 slots. REQUIRED. Size UNVERIFIED.
5. **0x41A494 clamp 6**: review; if curve-related, raise. UNVERIFIED.
6. **0x43BD82 / 0x43BDDB / 0x43BD73**: clamp to 3 on `+0x8c`. Identify the object before patching. UNVERIFIED.
7. **0x4393DE** (`cmp [edi+0x8c],3`): review. UNVERIFIED.
8. **+0x5c/60/64 swaps in FUN_00405590**: confirm whether they are per-curve; if so, loop over N. UNVERIFIED.
9. **Literal 6/3 at +0x60 and +0x64** (0x428333, 0x428EC1, 0x433123, 0x433233): UNVERIFIED role; do not patch until the object type is confirmed.

## 6. Open items
- Board object identity for `+0xF8` and `+0x8c` in FUN_0043bc60 vs FUN_00405590 (UNVERIFIED that these are the same object).
- Curve ctor and allocation size for the 0x7c record (not found).
- FUN_0044d470 target of `+0x74` (UNVERIFIED).
- Exact bytes for the 0x174 bound and feasibility of rewriting FUN_00404CF0 in place (its body is small; a code cave may be required; UNVERIFIED).
