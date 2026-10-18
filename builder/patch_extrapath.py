"""patch_extrapath.py - STAGE 1 of 4th..10th curve path support for Zuma Deluxe.

Usage: python patch_extrapath.py <Zuma_in.exe> <Zuma_out.exe>
(Input: Zuma.exe output from build_mod_exe.py / patch_anim_hook.py. in and out can be the same file.)

STAGE 1 DOES NOT add new paths. Its purpose: relocate Board's Curve pointer array
(which originally only fits 3 + 3 staging curves) to a heap array with capacity of 10,
WITHOUT altering gameplay behavior.
Section .zep is PURE RX CODE (characteristics 0x60000020: CODE | EXECUTE | READ).
No entry point stubs, no entry point hijacking.

Resolves all 39 write sites dynamically from base/Zuma.exe via site_resolver.py.
"""

import sys
import os
import struct

sys.path.append(os.path.dirname(__file__))
from pe_util import PEFile, align
from site_resolver import resolve_all_sites, HOLEMGR_MAX_HOLES, HOLEMGR_BUFFER_SIZE

SWAP_LEN = 0x48

SWAP_EXPECT = bytes.fromhex(
    "8B5668" "8B465C" "89565C" "894668"          # swap [5c]<->[68]
    "8B4E78" "8B4674" "894E74" "894678"          # swap [74]<->[78]
    "8B566C" "8B4660" "895660" "89466C"          # swap [60]<->[6c]
    "8B4E78" "8B4674" "894E74" "894678"
    "8B5670" "8B4664" "895664" "894670"          # swap [64]<->[70]
    "8B4E78" "8B4674" "894E74" "894678")
assert len(SWAP_EXPECT) == SWAP_LEN

SWAP_NEW = bytes.fromhex(
    "8B566C" "8B4668" "895668" "89466C"          # swap active<->staging base
    "8B4E78" "8B4674" "894E74" "894678"          # swap [74]<->[78] (three original swaps = one swap)
    "8B4668"                                     # mov eax,[esi+0x68]  (new active base)
    "8B08" "894E5C"                              # [5c] = A[0]
    "8B4804" "894E60"                            # [60] = A[1]
    "8B4808" "894E64")                           # [64] = A[2]
SWAP_NEW += b"\x90" * (SWAP_LEN - len(SWAP_NEW))

# Original 19 active loop instruction bytes
ACTIVE_ORIG = [
    "8D565C", "8D5E5C", "8D7E5C", "8D5E5C",
    "8D4E5C", "8D565C", "8D5E5C", "8D465C",
    "8D465C", "8D465C", "8D5E5C", "8D7E5C",
    "8D5E5C", "8D7B5C", "83C35C", "8D4B5C",
    "8D565C", "8D7E5C", "8D5E5C",
]

STAGING_ORIG = ["8D7E68", "8D4668", "8D5E68"]

def active_new(orig):
    b = bytearray.fromhex(orig)
    if b[0] == 0x8D:
        return bytes([0x8B, b[1], 0x68])
    if b[0] == 0x83 and b[1] == 0xC3:
        return bytes([0x8B, 0x5B, 0x68])
    raise ValueError(orig)

def staging_new(orig):
    b = bytearray.fromhex(orig)
    return bytes([0x8B, b[1], 0x6C])

def build_ctor_cave(va, back_va, opnew_va):
    c = bytearray()
    def call(target):
        c.extend(b"\xE8"); c.extend(struct.pack("<i", target - (va + len(c) + 4)))
    c += b"\x60"                                             # pushad
    c += b"\x6A\x28"; call(opnew_va); c += b"\x83\xC4\x04"   # A = new(40)
    c += b"\x89\xC7"                                         # mov edi,eax
    c += b"\x6A\x28"; call(opnew_va); c += b"\x83\xC4\x04"   # S = new(40)
    c += b"\x89\xC3"                                         # mov ebx,eax
    c += b"\x31\xC0"                                         # xor eax,eax
    for d in range(0x0C, 0x28, 4): c += b"\x89\x47" + bytes([d])   # A[3..9] = 0
    for d in range(0x0C, 0x28, 4): c += b"\x89\x43" + bytes([d])   # S[3..9] = 0
    c += b"\x8B\x46\x5C\x89\x07"                             # A[0] = [esi+5c]
    c += b"\x8B\x46\x60\x89\x47\x04"                         # A[1] = [esi+60]
    c += b"\x8B\x46\x64\x89\x47\x08"                         # A[2] = [esi+64]
    c += b"\x8B\x46\x68\x89\x03"                             # S[0] = [esi+68]
    c += b"\x8B\x46\x6C\x89\x43\x04"                         # S[1] = [esi+6c]
    c += b"\x8B\x46\x70\x89\x43\x08"                         # S[2] = [esi+70]
    c += b"\x89\x7E\x68"                                     # [esi+68] = A
    c += b"\x89\x5E\x6C"                                     # [esi+6c] = S
    c += b"\x61"                                             # popad
    c += b"\x68" + struct.pack("<I", HOLEMGR_BUFFER_SIZE)   # push 0x308 (active HoleMgr allocation)
    c += b"\xE9" + struct.pack("<i", back_va - (va + len(c) + 5))
    return bytes(c)

ZEP_CHARS = 0x60000020  # IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ


def patch_extrapath(src, dst, base_exe=None):
    pe = PEFile(src)

    if pe.find_section_by_name('.zep'):
        print("[INFO] Section .zep already present in executable.")
        if src != dst:
            pe.save(dst)
        return []

    # 1. Resolve all sites from baseline binary
    if not base_exe or not os.path.isfile(base_exe):
        for cand in [
            os.path.join(os.path.dirname(__file__), "..", ".base", "Zuma.exe"),
            os.path.join(os.path.dirname(__file__), "..", "base", "Zuma.exe"),
            src
        ]:
            if os.path.isfile(cand):
                base_exe = cand
                break
    if not base_exe or not os.path.isfile(base_exe):
        sys.exit(f"[ABORT] Base vanilla executable not found at: {base_exe}")
    sites = resolve_all_sites(base_exe, enforce_hash=False)

    # 2. Build .zep section content: PURE RX CODE (ctor cave)
    last_sec = max(pe.sections, key=lambda s: s.virtual_address)
    new_rva = align(last_sec.virtual_address + max(last_sec.virtual_size, last_sec.raw_size), pe.section_alignment)
    new_va = pe.image_base + new_rva

    ctor_hook_va = sites['STAGE1_CTOR_HOOK']
    ctor_back_va = ctor_hook_va + 5
    blob = bytearray()
    blob.extend(build_ctor_cave(new_va, ctor_back_va, sites['STAGE1_OPNEW']))

    # 3. Add section .zep (0x60000020: CODE | EXECUTE | READ)
    new_sec = pe.add_section('.zep', blob, ZEP_CHARS)
    print(f"[OK] Section .zep added: RVA=0x{new_sec.virtual_address:08X}, "
          f"VSz=0x{new_sec.virtual_size:X}, Chars=0x{new_sec.characteristics:08X}")

    # 4. Apply code patches using strict write_site helper
    write_log = []

    def write_site(name, va, expect, new):
        off = pe.va_to_offset(va)
        if off is None:
            sys.exit(f"[ABORT] Site {name} @ 0x{va:08X} outside PE bounds")
        got = bytes(pe.data[off : off + len(expect)])
        if got != expect:
            sys.exit(f"[ABORT] Site {name} @ 0x{va:08X} byte mismatch:\n"
                     f"  Got     : {got.hex(' ')}\n"
                     f"  Expected: {expect.hex(' ')}")
        if len(new) != len(expect):
            sys.exit(f"[ABORT] Site {name} length mismatch: new ({len(new)}) != expect ({len(expect)})")
        pe.data[off : off + len(new)] = new
        write_log.append((name, va, expect, new))

    # Apply 19 Active loop sites
    for i in range(19):
        name = f'STAGE1_ACTIVE_{i+1:02d}'
        va = sites[name]
        orig = ACTIVE_ORIG[i]
        write_site(name, va, bytes.fromhex(orig), active_new(orig))

    # Apply 3 Staging loop sites
    for i in range(3):
        name = f'STAGE1_STAGING_{i+1}'
        va = sites[name]
        orig = STAGING_ORIG[i]
        write_site(name, va, bytes.fromhex(orig), staging_new(orig))

    # Apply Board destructor staging unbind
    write_site('STAGE1_DTOR_VA', sites['STAGE1_DTOR_VA'], bytes.fromhex("8B0F"), bytes.fromhex("33C9"))

    # Apply HoleMgr patches
    write_site('STAGE1_HOLEMGR_ALLOC', sites['STAGE1_HOLEMGR_ALLOC'],
               bytes.fromhex("682C010000"), b"\x68" + struct.pack("<I", HOLEMGR_BUFFER_SIZE))
    write_site('STAGE1_HOLEMGR_CREDITS_ALLOC', sites['STAGE1_HOLEMGR_CREDITS_ALLOC'],
               bytes.fromhex("682C010000"), b"\x68" + struct.pack("<I", HOLEMGR_BUFFER_SIZE))
    write_site('STAGE1_HOLEMGR_EH_CTOR', sites['STAGE1_HOLEMGR_EH_CTOR'],
               bytes.fromhex("6A03"), bytes([0x6A, HOLEMGR_MAX_HOLES]))
    write_site('STAGE1_HOLEMGR_EH_DTOR', sites['STAGE1_HOLEMGR_EH_DTOR'],
               bytes.fromhex("6A03"), bytes([0x6A, HOLEMGR_MAX_HOLES]))

    # Apply Extra loop patches
    write_site('STAGE1_EXTRA1', sites['STAGE1_EXTRA1'],
               bytes.fromhex("8B4D088B0C01"), bytes.fromhex("8B48688B0C99"))
    write_site('STAGE1_EXTRA2', sites['STAGE1_EXTRA2'],
               bytes.fromhex("8B56048B45FC8B0C10"), bytes.fromhex("8B56048B52688B0C9A"))

    write_site('STAGE1_EXTRA3_BASE', sites['STAGE1_EXTRA3_BASE'],
               bytes.fromhex("BB5C0000008D642400"), bytes.fromhex("8B5E048B5B68909090"))
    write_site('STAGE1_EXTRA3_SUB1', sites['STAGE1_EXTRA3_SUB1'],
               bytes.fromhex("8B4E048B0C0B"), bytes.fromhex("8B0CBB909090"))
    write_site('STAGE1_EXTRA3_SUB2', sites['STAGE1_EXTRA3_SUB2'],
               bytes.fromhex("8B56048B0C13"), bytes.fromhex("8B0CBB909090"))
    write_site('STAGE1_EXTRA3_SUB3', sites['STAGE1_EXTRA3_SUB3'],
               bytes.fromhex("83C304"), bytes.fromhex("909090"))

    write_site('STAGE1_EXTRA4_BASE', sites['STAGE1_EXTRA4_BASE'],
               bytes.fromhex("BF5C0000008BFF"), bytes.fromhex("8B7E048B7F6890"))
    write_site('STAGE1_EXTRA4_SUB1', sites['STAGE1_EXTRA4_SUB1'],
               bytes.fromhex("83FF5C"), bytes.fromhex("83FB00"))
    write_site('STAGE1_EXTRA4_SUB2', sites['STAGE1_EXTRA4_SUB2'],
               bytes.fromhex("8B5604518B0C3A"), bytes.fromhex("518B0C9F909090"))
    write_site('STAGE1_EXTRA4_SUB3', sites['STAGE1_EXTRA4_SUB3'],
               bytes.fromhex("83C704"), bytes.fromhex("909090"))

    # Apply Swap region
    write_site('STAGE1_SWAP_REGION', sites['STAGE1_SWAP_REGION'], SWAP_EXPECT, SWAP_NEW)

    # Apply Board constructor cave jump
    ctor_jmp = b"\xE9" + struct.pack("<i", new_va - (ctor_hook_va + 5))
    write_site('STAGE1_CTOR_HOOK', ctor_hook_va, bytes.fromhex("682C010000"), ctor_jmp)

    assert len(write_log) == 39, f"Expected 39 sites written in Stage 1, got {len(write_log)}"
    print(f"[OK] All 39 Stage 1 sites written and logged cleanly.")

    pe.save(dst)
    print(f"[DONE] Stage 1 applied -> {dst} (ctor cave 0x{new_va:08X})")
    return write_log


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    try:
        patch_extrapath(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) == 4 else None)
    except Exception as e:
        print("\n" + "=" * 67)
        print(" [COMPATIBILITY ERROR] Failed to detect required Stage 1 curve array structures.")
        print(f" Detail: {e}")
        print("=" * 67)
        print(" [WARNING]")
        print(" The provided executable does not have the expected structure or attributes.")
        print(" Please provide a compatible Zuma.exe binary.")
        print(" (RECOMMENDED: Zuma Deluxe v1.0.0.1 GameHouse / PopCap Retail)")
        print("=" * 67 + "\n")
        sys.exit(1)
