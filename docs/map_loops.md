# Stage 2 map: Board curve-count limits (read-only analysis of binary/Zuma.exe)

Status: PARTIAL. Loop-bound structure mapped for the 19 sites. Raw bytes per instruction are NOT captured here (VA + mnemonic/operands only). Items marked UNVERIFIED were not confirmed by reading the surrounding logic.

## 0. Key finding
- The curve count lives at **[obj+0x8c]**. Writers include `0x405603 mov [esi+0x8c], eax` (eax = FUN_00404CF0 result).
- **FUN_00404CF0 (VA 0x404CF0..0x404D17, thiscall, ECX = obj)** has no loop. It returns the count of non-zero dwords at ECX+0xC0, ECX+0x13C, ECX+0x1B8, so its range is **0..3**. This is the structural cap.
- The object passed to FUN_00404CF0 at 0x4055FE is **[esi+0xF8]**, not Board. Whether that object is the Board's curve container is UNVERIFIED.
- Raising the cap to 10 requires changing FUN_00404CF0 (or its replacement) and every consumer that assumes ≤3. Changing the constant alone will not work.

## 1. The 19 loop sites (from stage 1)

Method: `fdump` function window → back-edge `jl/jne` whose target lies before the site, then the `cmp` feeding it. Bound source is traced back to a load from `[reg+0x8c]` within the same function window.

| Site VA | Function (approx.) | Back-edge (VA → target) | Loop bound / compare | Bound source | Needs change for 10 slots? |
|---|---|---|---|---|---|
| 0x408016 | 0x407FA0 | jl 0x40803A → 0x408020 | `cmp edi, ebx` @0x408035; ebx = [esi+0x8c] @0x40800A | count field +0x8c | Bound is count-based, OK if +0x8c can reach 10. Index-loop slot access must be checked. |
| 0x40918D | 0x4090D0 | jl 0x4091B6 → 0x409190; jl 0x4091ED → 0x4091C7 | `cmp edi, eax` @0x4091B4; `cmp ebx, eax` @0x4091EB; eax from [esi+0x8c] @0x4091AA | +0x8c | Same as above. Two loops in one function. |
| 0x4091C4 | 0x4090D0 | jl 0x4091ED → 0x4091C7 | `cmp ebx, eax` @0x4091EB; eax = [esi+0x8c] @0x4091B8 | +0x8c | Same. |
| 0x4093A1 | 0x409360 | jl 0x4093BC → 0x4093A4 | `cmp edi, eax` @0x4093BA; eax = [esi+0x8c] @0x409394 | +0x8c | Same. |
| 0x413D17 | (contains 0x413D20..0x413D90) | jl 0x413D5A → 0x413D20; jmp 0x413D90 → 0x413D66 | `cmp edi, eax` @0x413D55; `cmp eax, ebx` @0x413D6A; eax/ecx = [esi+0x8c] @0x413D0B | +0x8c | Same. Has an unconditional back-jump, so verify the loop exit. |
| 0x416B90 | (0x416B84 area) | jl 0x416BB3 → 0x416B96 | `cmp ebx, eax` @0x416BAE; eax = [esi+0x8c] @0x416B84 | +0x8c | Same. |
| 0x4173A5 | (0x417398 area) | jl 0x4173BF → 0x4173A8 | `cmp edi, eax` @0x4173BD; eax = [esi+0x8c] @0x417398 | +0x8c | Same. |
| 0x41A7C2 | (0x41A7B6 area) | jl 0x41A7EC → 0x41A7D0; jl 0x41A82F → 0x41A810 | `cmp edi, eax` @0x41A7E7; `cmp edx, ecx` @0x41A82D; eax = [esi+0x8c] @0x41A7B6 | +0x8c | Same. Second loop compares edx/ecx (UNVERIFIED that ecx is the count). |
| 0x41C710 | 0x41C708 area | jl 0x41C73B → 0x41C716; jl 0x41C76D → 0x41C750 | `cmp dword [esi+0x8c], edi` @0x41C708 (direct memory compare); `cmp edi, eax` @0x41C736 and @0x41C768 | +0x8c | Direct compare with memory. Same count field. |
| 0x41C747 | 0x41C72C area | jl 0x41C76D → 0x41C750 | `cmp dword [esi+0x8c], edi` @0x41C73F; `cmp edi, eax` @0x41C768 | +0x8c | Same. |
| 0x4214A6 | (0x421499 area) | jl 0x4214C3 → 0x4214B0 | `cmp edi, eax` @0x4214C1; eax = [esi+0x8c] @0x421499 | +0x8c | Same. |
| 0x42173B | (0x42172F area) | jl 0x42177C → 0x421740 | `cmp ebx, eax` @0x42177A; eax = [esi+0x8c] @0x42172F | +0x8c | Same. |
| 0x4218A3 | (0x421897 area) | jl 0x4218B9 → 0x4218A6 | `cmp edi, eax` @0x4218B7; eax = [esi+0x8c] @0x421897 | +0x8c | Same. |
| 0x421966 | (0x421959 area) | jl 0x421981 → 0x421970; jl 0x4219B3 → 0x4219A2; jl 0x4219E1 → 0x4219D3 | `cmp edx, esi` @0x42197F; `cmp edi, esi` @0x4219B1; `cmp eax, esi` @0x4219DF; esi = [ebx+0x8c] @0x421959 | **+0x8c of ebx** (not esi) | **UNVERIFIED** that ebx is Board. Three loops share the bound esi. |
| 0x42199F | (same func as 0x421966) | jl 0x4219B3 → 0x4219A2; jl 0x4219E1 → 0x4219D3 | `cmp edi, esi` @0x4219B1; `cmp eax, esi` @0x4219DF | esi from 0x421959 | Same as 0x421966. |
| 0x4219D0 | (same func) | jl 0x4219E1 → 0x4219D3 | `cmp eax, esi` @0x4219DF | esi from 0x421959 | Same as 0x421966. |
| 0x421A89 | (0x421A7D area) | jne 0x421AA1 → 0x421A90 | `test al, al` @0x421A97 | flag, not count | **NOT a count loop.** Probably a boolean/flag scan. Check whether it iterates slots. |
| 0x421B49 | (0x421B36 area) | jl 0x421B76 → 0x421B50 | `cmp ebx, eax` @0x421B74; eax = [esi+0x8c] @0x421B36 | +0x8c | Same. |
| 0x421C91 | (0x421C85 area) | jl 0x421CA7 → 0x421C94 | `cmp edi, eax` @0x421CA5; eax = [esi+0x8c] @0x421C85 | +0x8c | Same. |

Summary of the 19 sites:
- 17 loops are bounded by a count loaded from **[obj+0x8c]**. None is a literal 3. They are not hard-limited in the loop itself, but they inherit the cap of 3 from the writer (section 2).
- 0x421966 / 0x42199F / 0x4219D0 use the count from `[ebx+0x8c]` (UNVERIFIED that ebx = Board).
- 0x421A89 is a flag/bool loop, not a count loop.
- No loop bound uses the pointer-end comparison (`cmp reg, base+0x68` end). Not checked exhaustively. UNVERIFIED for the full window.

## 2. Writers and readers of +0x8c

Source: linear sweep of .text (capstone), `scratch/sweep.txt`.

| VA | Instruction | Notes |
|---|---|---|
| 0x405603 | `mov [esi+0x8c], eax` | After `call 0x404CF0`. **Main count writer (cap 0..3).** |
| 0x4094B6 | `mov dword [esi+0x8c], 0x3D4CCCCD` | Float 0.1f. So +0x8c is not always int. |
| 0x42471D | `mov [esi+0x8c], ebx` | UNVERIFIED |
| 0x42A383 | `mov [esi+0x8c], eax` | UNVERIFIED |
| 0x431864 | `mov [esi+0x8c], ebx` | UNVERIFIED |
| 0x435883 | `mov [esi+0x8c], 0xFFFFFFFF` | Sentinel. UNVERIFIED |
| 0x437F4E | `mov [esi+0x8c], 0xF` | 15. UNVERIFIED |
| 0x438374 | `mov [esi+0x8c], 0xF` | 15. UNVERIFIED |
| 0x43B2C9 | `mov [esi+0x8c], ebx` | UNVERIFIED |
| 0x43BD7A | `mov [esi+0x8c], edx` | UNVERIFIED |
| **0x43BD82** | `mov dword [esi+0x8c], 3` | **Literal 3. Hard limit candidate.** |
| 0x43BD8E | `mov [esi+0x8c], 0xFFFFFFFF` | Sentinel |
| **0x43BDDB** | `mov dword [esi+0x8c], 3` | **Literal 3. Hard limit candidate.** |
| 0x45C1D5 | `mov [edi+0x8c], eax` | Different object (edi). UNVERIFIED |
| 0x4767D2 | `mov [esi+0x8c], eax` | UNVERIFIED |
| 0x4787BF | `mov [esi+0x8c], eax` | UNVERIFIED |
| 0x478A3E | `mov dword [ecx+0x8c], 0` | Reset. UNVERIFIED |
| 0x478A8B | `mov [esi+0x8c], ecx` | UNVERIFIED |
| 0x478AA8 | `mov dword [esi+0x8c], 0` | Reset |
| 0x47D5BD | `mov dword [esi+0x8c], 0` | Reset |
| 0x47FA88 | `mov [esi+0x8c], eax` | UNVERIFIED |
| 0x47FB02 | `mov [esi+0x8c], eax` | UNVERIFIED |
| 0x4B4931 | `mov dword [esi+0x8c], 0` | Reset |

Compares to +0x8c:

| VA | Instruction | Notes |
|---|---|---|
| 0x41C708 | `cmp dword [esi+0x8c], edi` | Loop bound (section 1) |
| 0x41C73F | `cmp dword [esi+0x8c], edi` | Loop bound (section 1) |
| 0x438352 | `cmp dword [esi+0x8c], edi` | UNVERIFIED |
| **0x4393DE** | `cmp dword [edi+0x8c], 3` | **Literal 3 compare. Hard limit candidate.** |

## 3. FUN_00404CF0 callers (all E8 rel32 targets seen in sweep)

| Call site | ECX (this) | Result use | Notes |
|---|---|---|---|
| 0x4055FE | `[esi+0xF8]` (fn 0x405590) | `mov [esi+0x8c], eax` @0x405603 | **Sets the curve count.** |
| 0x41A494 | `edi` (fn 0x41A1D0) | Tail-calls through `jmp 0x4FDC80` @0x41A499; result passed through | Preceded by a clamp: counter = [edi+0xA4]/3 + 4, clamped to **6** (0x41A483..0x41A48F). The clamp at 6 is a different limit. Its relation to the cap is UNVERIFIED. |
| 0x42398F | stack local `[ebp-0x2C8]` (fn 0x4220A0) | `cmp eax, 1` @0x423994; `jle 0x4239BD` @0x423997 | Branch on count ≤ 1. |
| 0x42399F | stack local `[ebp-0x2C8]` | `mov [ebp+8], eax`; scaled to float; `call 0x4E11D8` | Count × float × int. Consumer UNVERIFIED. |

Same function (0x4220A0): literal `mov edx, 3` at **0x4239E1** (UNVERIFIED role). **Hard limit candidate.**

## 4. Literal-3 / 6 hard limits found

| VA | Form | Status |
|---|---|---|
| 0x43BD82 | write `+0x8c = 3` | Candidate cap. Needs reading of the function. |
| 0x43BDDB | write `+0x8c = 3` | Candidate cap |
| 0x4393DE | cmp `[edi+0x8c], 3` | Candidate cap |
| 0x4239E1 | `mov edx, 3` | Candidate cap. Role UNVERIFIED |
| 0x41A483 | clamp `cmp esi, 6` / `mov [ebp-0x10], 6` | Clamp at 6, different counter. |

## 5. Curve construction (Board ctor / level load)

NOT YET DONE. The Board ctor range (0x418A?..0x418DFF), the `operator new` size, and the Curve ctor VA are not mapped. The `call 0x4D1915` list (26+ sites, e.g. 0x406155, 0x406192, 0x40E4FC, 0x411075) is found but its identity as "Curve ctor" is UNVERIFIED.

## 6. Board destructor (around 0x419EDD)

NOT YET DONE.

## 7. Direct references to Board+0x5C..0x70 outside loops

NOT YET DONE. The stage-1 staging sites `[esi+0x6c]` at 0x418CED, 0x418D40, 0x418D90 (ctor) are known but not re-checked here.

## 8. Open items before stage 2 can change anything

1. Confirm the object type at FUN_00404CF0's ECX (`[esi+0xF8]`). Is it the Board's curve container or a sub-object? (UNVERIFIED)
2. Read what FUN_00404CF0's three fields (+0xC0, +0x13C, +0x1B8) hold. Is the curve count capped structurally by these three slots? (Inferred, not confirmed.)
3. Decide what the literal-3 writes at 0x43BD82 / 0x43BDDB and the compare at 0x4393DE mean.
4. Confirm that 0x421966 / 0x42199F use the Board object (ebx).
5. Complete sections 5–7 (ctor, dtor, direct references).
6. Check the cross-references in `Zuma_All_Decompiled.c` and `Zuma_Types_And_Declarations.h`. Not done in this stage.

No files in `binary/` were modified. Scratch helpers are in `scratch/` (zdis.py, fdump.py, sweep.py, classify.py, classify2.py).
