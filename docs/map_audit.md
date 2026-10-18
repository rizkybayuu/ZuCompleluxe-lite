# Stage 2 map: Board curve-count limits (read-only analysis of binary/Zuma.exe)

## Goal
Audit the 19 curve loops and literal‑3/6 sites for overflow risk with up to 10 curves.

## Method
- Extracted loop start VAs from stage2_notes/map_loops.md.
- For each loop, disassembled ~80 bytes and analyzed for memory operands that depend on a loop‑index register (incremented inside the loop) other than the Curve* array access.
- Collected writes and compares of the curve‑count field ([obj+0x8c]) and literal constants 3/6.
- All analysis is read‑only; no binary was modified.

## Findings

### 1. The 19 loop sites
All 19 loops are bounded by a count loaded from `[obj+0x8c]` (or `[ebx+0x8c]` for three loops where ebx is unverified as Board). None uses a literal 3 or 6 as the loop bound.

**Unsafe indexed memory accesses**:  
The automated scan found **no** memory operands where a register used as base/index in `[reg+offset]` or `[reg+reg*scale+offset]` is also incremented/decremented within the loop.  
This suggests that each loop accesses only:
- the Curve* array via the loop index (e.g., `mov eax, [ebx+edi*4]` where ebx is the array base and edi is the index), or
- fields of the current Curve* object (via a register that holds the Curve* pointer, not the index), or
- constant‑offset fields of the Board or other objects (using a register that holds the Board base and is not modified in the loop).

**However**, the scan only looked at the first ~80 bytes of each loop and relied on simple patterns; it did **not** verify the full loop body or confirm that every memory access was examined. Therefore, the result is **UNVERIFIED** for completeness.

### 2. Board object size and per‑curve arrays
- The size of the Board object (the argument to `operator new`) was **not identified** in this read‑only pass. No `push 0x??` immediately preceding a call to allocation routines was inspected.
- Consequently, we cannot confirm whether the Board contains per‑curve inline arrays (e.g., `[board+0x80 + i*4]`, a 3‑entry table, etc.) that would overflow if the curve count were raised to 10.
- This item is **UNVERIFIED**.

### 3. Writers and readers of `[obj+0x8c]`
**Writes** observed in the sweep:
- `0x405603 mov [esi+0x8c], eax` (after `call 0x404CF0`) – main writer, value 0..3.
- `0x43BD82 mov dword [esi+0x8c], 3` – literal 3.
- `0x43BDDB mov dword [esi+0x8c], 3` – literal 3.
- Other writes (0x4094B6, 0x42471D, etc.) are unverified or set sentinel/float values.

**Reads/compares** observed:
- `0x41C708 cmp dword [esi+0x8c], edi`
- `0x41C73F cmp dword [esi+0x8c], edi`
- `0x4393DE cmp dword [edi+0x8c], 3` – literal 3 compare.
- Many other `cmp [reg+0x8c], reg` using the count field as loop bound.

### 4. FUN_00404CF0 (VA 0x404CF0)
- This function counts non‑zero dwords at `[ecx+0xC0]`, `[ecx+0x13C]`, `[ecx+0x1B8]` and returns the count (0..3).
- It contains **no loop**; the three slots are the structural source of the cap.
- All callers observed (0x4055FE, 0x41A494, 0x42398F, 0x42399F) use the result to set `[obj+0x8c]`.
- Unless those three slots are expanded or the function is changed, the count cannot exceed 3.

### 5. Literal‑3/6 hard‑limit candidates
| VA        | Form                          | Notes                                                                 |
|-----------|-------------------------------|-----------------------------------------------------------------------|
| 0x43BD82  | `mov [esi+0x8c], 3`           | Direct write of 3 to count field.                                     |
| 0x43BDDB  | `mov [esi+0x8c], 3`           | Direct write of 3 to count field.                                     |
| 0x4393DE  | `cmp [edi+0x8c], 3`           | Compare count with 3.                                                 |
| 0x4239E1  | `mov edx, 3`                  | UNVERIFIED role; appears in a function that also calls FUN_00404CF0.  |
| 0x41A483  | `cmp esi, 6` / `mov [ebp-0x10],6` | Clamp to 6 for a different counter (verified at 0x41A494 call site). |

All of the above would need to be modified to allow a count of 10, but **changing them alone will not suffice** because FUN_00404CF0 imposes a hard cap of 3 unless its three source slots are increased.

### 6. Specific items from the request
- **0x41A494**: Tail‑calls through `jmp 0x4FDC80`; preceded by a clamp to 6 (UNVERIFIED relation to curve count). The object passed to FUN_00404CF0 is `[edi+0xF8]` (not Board). Whether that object is the Board's curve container is **UNVERIFIED**.
- **0x433123 / 0x433233**: Not found in the sweep; likely addresses are outside the scanned range or mistyped. **UNVERIFIED**.
- **0x4393DE**: As above, literal‑3 compare on `[edi+0x8c]`. The object (`edi`) is unverified as Board. If it is Board, this compare breaks with count>3 (YES).
- **0x43BD82 / 0x43BDDB**: Literal‑3 writes to `[esi+0x8c]`. If `esi` is Board, these enforce a cap of 3 (YES).

## Conclusion
- No unsafe indexed memory accesses were **detected** in the limited disassembly window, but full verification is pending.
- The curve count is fundamentally limited to 3 by FUN_00404CF0 and its three source slots.
- Raising the limit to 10 requires:
  1. Expanding the three source slots in FUN_00404CF0 (or replacing the function).
  2. Updating every write/compare that currently uses the literal 3 (or 6) to use 10 or a variable.
  3. Ensuring the Board and any per‑curve storage can accommodate 10 curves (size verification needed).

All analysis is **read‑only**; no changes were made to the binary.

## Open items (for future stages)
- Determine the Board object size via `operator new` call sites.
- Verify the full loop bodies for all 19 sites to confirm absence of unsafe indexing.
- Identify the exact role of unverified literal‑3 sites (0x4239E1, etc.).
- Confirm whether `[edi+0x8c]` and `[esi+0x8c]` in the literal‑3 sites refer to the Board object.
