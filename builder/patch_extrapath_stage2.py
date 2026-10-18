"""patch_extrapath_stage2.py - STAGE 2: Activate 4th..10th curve paths in Zuma.exe (following Stage 1).

Usage: python patch_extrapath_stage2.py <Zuma_in.exe> <Zuma_out.exe>
Input MUST already be patched with Stage 1 (patch_extrapath.py).

Clean Sidecar Architecture:
  1. LevelDesc size remains untouched at vanilla 0x220 (NO struct resizing, NO push 0x588).
  2. Swap hook sums vanilla curve count (COUNT_FN) + ZEP_ExtraCount().
  3. BindExtras hook calls ZEP_BindExtras(Board*, LevelDesc*).
  4. CurveDesc routing hooks:
     - STAGE2_DESC_SITE1 (FUN_004173e0): routes curves >= 3 to ZEP_GetCurveDesc
     - STAGE2_DESC_SITE2 (FUN_00414550): routes curves >= 3 to ZEP_GetCurveDesc
     - STAGE2_DESC_SITE3 (FUN_004044d0): routes curves >= 3 to ZEP_GetCurveDesc
  5. Speed loop clamp: clamps iteration to min(mNumCurves, 3) to prevent LevelDesc overrun.
  6. HoleMgr dispatches (STAGE2_HOLE_HOOK1..3): entry guard dispatches curves >= 3
     via zumaextrapath.dll IAT.

Section .zep2 is PURE RX CODE (characteristics 0x60000020: CODE | EXECUTE | READ).
No entry point stubs, no entry point hijacking.
All 9 write sites and COUNT_FN resolved dynamically via site_resolver.py.
"""

import sys
import os
import struct
import hashlib

sys.path.append(os.path.dirname(__file__))
from pe_util import PEFile, align
from site_resolver import resolve_all_sites

SWAP_EXPECT = bytes.fromhex("E8EDF6FFFF")
BIND_EXPECT = bytes.fromhex("33C9894DEC")
DESC1_EXPECT = bytes.fromhex("8D8401AC000000")
DESC2_EXPECT = bytes.fromhex("8D9401AC000000")
DESC3_EXPECT = bytes.fromhex("8D9C02AC000000")
CLAMP_EXPECT = bytes.fromhex("8B8E8C000000")
CLAMP_EAX_EXPECT = bytes.fromhex("8B868C000000")

BALL_SET_EXPECT = bytes.fromhex("558BEC8B450C8B55088984919C0000005DC20800909090909090909090909090")
BALL_SET_PATCH  = bytes.fromhex("558BEC8B550883FA03730A8B450C8984919C0000005DC2080090909090909090")
BALL_GET_EXPECT = bytes.fromhex("558BEC8B45088B84819C0000005DC20400909090909090909090909090909090")
BALL_GET_PATCH  = bytes.fromhex("558BEC8B450883F803730B8B84819C0000005DC2040031C05DC2040090909090")

OVERLAY_DRAW_EXPECT = bytes.fromhex("558BEC8B450C")
HOLE_BG_END_EXPECT  = bytes.fromhex("8B477433DB")
HOLE_LOOP_PREFIX    = bytes.fromhex("53568BCFE8")



HOLE_MIN_CURVE = 3

ZEP2_CHARS = 0x60000020  # IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ

def patch_extrapath_stage2(src, dst, base_exe=None):
    pe = PEFile(src)

    if not pe.find_section_by_name('.zep'):
        sys.exit("[ABORT] Section .zep not found - run Stage 1 first.")
    if pe.find_section_by_name('.zep2'):
        print("[INFO] Section .zep2 already present in executable.")
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

    count_fn        = sites['COUNT_FN']
    swap_va         = sites['STAGE2_SWAP_COUNT']
    bind_va         = sites['STAGE2_BIND_VA']
    desc_site1      = sites['STAGE2_DESC_SITE1']
    desc_site2      = sites['STAGE2_DESC_SITE2']
    desc_site3      = sites['STAGE2_DESC_SITE3']
    clamp_site      = sites['STAGE2_CLAMP_SITE']
    t_clamp_a       = sites['STAGE2_TREASURE_CLAMP_A']
    t_clamp_b       = sites['STAGE2_TREASURE_CLAMP_B']
    d_clamp_a       = sites['STAGE2_DESCRESET_CLAMP_A']
    d_clamp_b       = sites['STAGE2_DESCRESET_CLAMP_B']
    hole_hook1      = sites['STAGE2_HOLE_HOOK1']
    hole_hook2      = sites['STAGE2_HOLE_HOOK2']
    hole_hook3      = sites['STAGE2_HOLE_HOOK3']
    ball_set_va     = sites['STAGE2_BALL_SET_WAYPOINT']
    ball_get_va     = sites['STAGE2_BALL_GET_WAYPOINT']
    overlay_draw_va = sites['STAGE2_OVERLAY_DRAW']
    hole_bg_end_va  = sites['STAGE2_HOLE_BG_END']
    hole_loop_va    = sites['STAGE2_HOLE_DRAW_LOOP']
    static_board_va = sites['STAGE2_STATIC_BOARD_DRAW']
    hole_funcs = {hole_hook1: 0x08, hole_hook2: 0x08, hole_hook3: 0x10}
    hole_prologues = {
        hole_hook1: (6, "752d295d4b35e84bb368c9514d4c5fc8dd12e5ea23bbc5bf50c4d518aca7d346"),
        hole_hook2: (6, "65c8e94615d9debf237b1fd8eea43f7ccfbc5665a10a1c187d3cc2f48a9e9f17"),
        hole_hook3: (6, "dff450abf084d622f21cd9d7f7ca9ce9627a6a5d4221f084bad43a6547be71c6")
    }

    # 2. Lookup IAT slots from zumaextrapath.dll
    va_ptr_bind = pe.find_iat('zumaextrapath.dll', 'ZEP_BindExtras')
    va_ptr_addhole = pe.find_iat('zumaextrapath.dll', 'ZEP_AddHole')
    va_ptr_setpct = pe.find_iat('zumaextrapath.dll', 'ZEP_SetHolePercent')
    va_ptr_setsomething = pe.find_iat('zumaextrapath.dll', 'ZEP_SetHoleSomething')
    va_ptr_extracount = pe.find_iat('zumaextrapath.dll', 'ZEP_ExtraCount')
    va_ptr_getdesc = pe.find_iat('zumaextrapath.dll', 'ZEP_GetCurveDesc')
    va_ptr_checkholeprio = pe.find_iat('zumaextrapath.dll', 'ZEP_CheckHolePriority')

    for name, ptr in [('ZEP_BindExtras', va_ptr_bind),
                      ('ZEP_AddHole', va_ptr_addhole),
                      ('ZEP_SetHolePercent', va_ptr_setpct),
                      ('ZEP_SetHoleSomething', va_ptr_setsomething),
                      ('ZEP_ExtraCount', va_ptr_extracount),
                      ('ZEP_GetCurveDesc', va_ptr_getdesc),
                      ('ZEP_CheckHolePriority', va_ptr_checkholeprio)]:
        if not ptr:
            sys.exit(f"[ABORT] IAT slot {name} from zumaextrapath.dll not found. Run patch_imports.py first!")

    def steal(va, need=5):
        if va not in hole_prologues:
            sys.exit(f"[ABORT] Unregistered prologue function: 0x{va:08X}")
        n, expected_hash = hole_prologues[va]
        assert n >= need, f"Prologue length {n} < requested {need}"
        off = pe.va_to_offset(va)
        if off is None:
            sys.exit(f"[ABORT] VA 0x{va:08X} outside PE bounds")
        stolen = bytes(pe.data[off:off + n])
        calc_hash = hashlib.sha256(stolen).hexdigest()
        if calc_hash != expected_hash:
            sys.exit(f"[ABORT] Prologue byte verification failed for 0x{va:08X}! Expected SHA-256 {expected_hash}, got {calc_hash}.")
        return stolen, n

    # 3. Validate sites against intermediate binary and extract dynamic branch targets
    bad = []
    def check(va, expect, what):
        off = pe.va_to_offset(va)
        if off is None:
            bad.append(f"  {what} @0x{va:08X}: outside PE")
            return
        got = bytes(pe.data[off:off + len(expect)])
        if got != expect:
            bad.append(f"  {what} @0x{va:08X}: got {got.hex(' ')} | expected {expect.hex(' ')}")

    check(swap_va, SWAP_EXPECT, "Swap Curve Count Hook (FUN_00405590)")
    check(bind_va, BIND_EXPECT, "BindExtras site (FUN_00418c40)")
    check(desc_site1, DESC1_EXPECT, "CurveDesc site 1 (FUN_004173e0)")
    check(desc_site2, DESC2_EXPECT, "CurveDesc site 2 (FUN_00414550)")
    check(desc_site3, DESC3_EXPECT, "CurveDesc site 3 (FUN_004044d0)")
    check(clamp_site, CLAMP_EXPECT, "Speed loop clamp site (FUN_0041a500)")
    check(t_clamp_a, CLAMP_EXPECT, "Treasure loop clamp site A (FUN_00413ba0)")
    check(t_clamp_b, CLAMP_EAX_EXPECT, "Treasure loop clamp site B (FUN_00413ba0)")
    check(d_clamp_a, CLAMP_EXPECT, "DescReset loop clamp site A (FUN_00421a10)")
    check(d_clamp_b, CLAMP_EAX_EXPECT, "DescReset loop clamp site B (FUN_00421a10)")
    check(overlay_draw_va, OVERLAY_DRAW_EXPECT, "Overlay draw prologue (FUN_00405af0)")
    check(hole_bg_end_va, HOLE_BG_END_EXPECT, "HoleMgr background end (FUN_00407250)")
    check(hole_loop_va, HOLE_LOOP_PREFIX, "HoleMgr draw loop head (FUN_00407250)")
    check(static_board_va, b"\xE9", "Static board draw jump (FUN_00407300)")

    if bad:
        sys.exit("[ABORT] Byte mismatch, nothing written:\n" + "\n".join(bad))

    # Extract dynamic relative targets from FUN_00407250 and FUN_00407300
    off_bg = pe.va_to_offset(hole_bg_end_va)
    jle_epilogue_disp = struct.unpack('b', bytes(pe.data[off_bg + 8 : off_bg + 9]))[0]
    epilogue_va = (hole_bg_end_va + 9) + jle_epilogue_disp

    off_loop = pe.va_to_offset(hole_loop_va)
    hole_loop_expect = bytes(pe.data[off_loop : off_loop + 9])
    call_hole_disp = struct.unpack('<i', hole_loop_expect[5:9])[0]
    draw_single_hole_va = (hole_loop_va + 9) + call_hole_disp
    jle_skip_disp = struct.unpack('b', bytes(pe.data[off_loop + 16 : off_loop + 17]))[0]
    skip_hole_va = (hole_loop_va + 17) + jle_skip_disp

    off_static = pe.va_to_offset(static_board_va)
    static_board_expect = bytes(pe.data[off_static : off_static + 5])
    jmp_bg_disp = struct.unpack('<i', static_board_expect[1:5])[0]
    draw_bg_va = (static_board_va + 5) + jmp_bg_disp

    hole_steal = {va: steal(va) for va in hole_funcs}
    print("[OK] All Stage 2 sites validated successfully.")

    # 4. Build .zep2 section content: PURE RX CODE
    last_sec = max(pe.sections, key=lambda s: s.virtual_address)
    new_rva = align(last_sec.virtual_address + max(last_sec.virtual_size, last_sec.raw_size), pe.section_alignment)
    new_va = pe.image_base + new_rva

    blob = bytearray()
    def here(): return new_va + len(blob)
    def pad():
        while len(blob) % 16: blob.append(0xCC)

    class Asm:
        def __init__(s): s.b = bytearray(); s.start = here()
        def va(s): return s.start + len(s.b)
        def raw(s, x): s.b += x
        def call(s, t): s.b += b"\xE8" + struct.pack("<i", t - (s.va() + 5))
        def jmp(s, t): s.b += b"\xE9" + struct.pack("<i", t - (s.va() + 5))

    def emit(a):
        assert a.start == new_va + len(blob)
        blob.extend(a.b)
        pad()

    # Cave A: va_swap (Swap Curve Count Hook)
    # Entry: swap_va calls va_swap instead of count_fn
    a = Asm(); va_swap = a.start
    a.raw(b"\x51")                          # push ecx
    a.call(count_fn)                        # call count_fn -> eax = vanilla curve count
    a.raw(b"\x59")                          # pop ecx
    a.raw(b"\x83\x3D" + struct.pack("<I", va_ptr_extracount) + b"\x00") # cmp dword ptr [va_ptr_extracount], 0
    jz_extra = len(a.b); a.raw(b"\x74\x00") # jz skip_extra
    a.raw(b"\x50")                          # push eax
    a.raw(b"\xFF\x15" + struct.pack("<I", va_ptr_extracount))  # call dword ptr [va_ptr_extracount] -> eax = extra count
    a.raw(b"\x5A\x01\xD0")                  # pop edx ; add eax, edx
    a.b[jz_extra+1] = len(a.b) - (jz_extra+2) # skip_extra:
    a.raw(b"\xC3")                          # ret
    emit(a)

    # Cave B: va_bind (ZEP_BindExtras in FUN_00418c40)
    # Entry: bind_va jumps to va_bind
    a = Asm(); va_bind = a.start
    a.raw(b"\x60\x9C")                      # pushad ; pushfd
    a.raw(b"\x83\x3D" + struct.pack("<I", va_ptr_bind) + b"\x00") # cmp dword ptr [va_ptr_bind], 0
    jz_bind = len(a.b); a.raw(b"\x74\x00")  # jz skip
    a.raw(b"\xFF\xB6\xFC\x00\x00\x00")      # push dword ptr [esi + 0xfc] (staging LevelDesc*)
    a.raw(b"\x56")                          # push esi (Board*)
    a.raw(b"\xFF\x15" + struct.pack("<I", va_ptr_bind)) # call dword ptr [va_ptr_bind]
    a.raw(b"\x83\xC4\x08")                  # add esp, 8
    a.b[jz_bind+1] = len(a.b) - (jz_bind+2) # skip:
    a.raw(b"\x9D\x61")                      # popfd ; popad
    a.raw(b"\x33\xC9\x89\x4D\xEC")          # xor ecx, ecx ; mov [ebp - 0x14], ecx (stolen)
    a.jmp(bind_va + 5)                      # jmp back to STAGE1_STAGING_1
    emit(a)

    # Cave C: va_desc1 (CurveDesc in FUN_004173e0)
    # Replaces 'lea eax, [ecx + eax + 0xac]' (7 bytes) at desc_site1
    # Output: eax = CurveDesc*. Must preserve EDX (HoleMgr* in flight across hook)
    a = Asm(); va_desc1 = a.start
    a.raw(b"\x83\xFF\x03")                  # cmp edi, 3
    jl_desc1 = len(a.b); a.raw(b"\x7C\x00") # jl vanilla
    a.raw(b"\x83\x3D" + struct.pack("<I", va_ptr_getdesc) + b"\x00") # cmp dword ptr [va_ptr_getdesc], 0
    je_desc1_null = len(a.b); a.raw(b"\x74\x00") # je vanilla
    a.raw(b"\x52")                          # push edx (PRESERVE HoleMgr*)
    a.raw(b"\x57\x50")                      # push edi ; push eax
    a.raw(b"\xFF\x15" + struct.pack("<I", va_ptr_getdesc)) # call [ZEP_GetCurveDesc]
    a.raw(b"\x83\xC4\x08")                  # add esp, 8
    a.raw(b"\x5A")                          # pop edx (RESTORE HoleMgr*)
    a.raw(b"\xC3")                          # ret
    a.b[jl_desc1+1] = len(a.b) - (jl_desc1+2) # vanilla:
    a.b[je_desc1_null+1] = len(a.b) - (je_desc1_null+2)
    a.raw(b"\x8D\x84\x01\xAC\x00\x00\x00")  # lea eax, [ecx + eax + 0xac]
    a.raw(b"\xC3")                          # ret
    emit(a)

    # Cave D: va_desc2 (CurveDesc in FUN_00414550)
    # Replaces 'lea edx, [ecx + eax + 0xac]' (7 bytes) at desc_site2
    # Output: edx = CurveDesc*. Must preserve EAX (LevelDesc*) and ECX
    a = Asm(); va_desc2 = a.start
    a.raw(b"\x83\x7E\x1C\x03")              # cmp dword ptr [esi + 0x1c], 3
    jl_desc2 = len(a.b); a.raw(b"\x7C\x00") # jl vanilla
    a.raw(b"\x83\x3D" + struct.pack("<I", va_ptr_getdesc) + b"\x00") # cmp dword ptr [va_ptr_getdesc], 0
    je_desc2_null = len(a.b); a.raw(b"\x74\x00") # je vanilla
    a.raw(b"\x51\x50")                      # push ecx ; push eax (PRESERVE)
    a.raw(b"\xFF\x76\x1C\x50")              # push dword ptr [esi + 0x1c] ; push eax
    a.raw(b"\xFF\x15" + struct.pack("<I", va_ptr_getdesc)) # call [ZEP_GetCurveDesc]
    a.raw(b"\x83\xC4\x08")                  # add esp, 8
    a.raw(b"\x89\xC2")                      # mov edx, eax (result in edx)
    a.raw(b"\x58\x59")                      # pop eax ; pop ecx (RESTORE)
    a.raw(b"\xC3")                          # ret
    a.b[jl_desc2+1] = len(a.b) - (jl_desc2+2) # vanilla:
    a.b[je_desc2_null+1] = len(a.b) - (je_desc2_null+2)
    a.raw(b"\x8D\x94\x01\xAC\x00\x00\x00")  # lea edx, [ecx + eax + 0xac]
    a.raw(b"\xC3")                          # ret
    emit(a)

    # Cave E: va_desc3 (CurveDesc in FUN_004044d0)
    # Replaces 'lea ebx, [edx + eax + 0xac]' (7 bytes) at desc_site3
    # Output: ebx = CurveDesc*. Must preserve EAX (LevelDesc*), ECX (Curve*), and EDX
    a = Asm(); va_desc3 = a.start
    a.raw(b"\x83\x79\x1C\x03")              # cmp dword ptr [ecx + 0x1c], 3
    jl_desc3 = len(a.b); a.raw(b"\x7C\x00") # jl vanilla
    a.raw(b"\x83\x3D" + struct.pack("<I", va_ptr_getdesc) + b"\x00") # cmp dword ptr [va_ptr_getdesc], 0
    je_desc3_null = len(a.b); a.raw(b"\x74\x00") # je vanilla
    a.raw(b"\x51\x52\x50")                  # push ecx ; push edx ; push eax (PRESERVE)
    a.raw(b"\xFF\x71\x1C\x50")              # push dword ptr [ecx + 0x1c] ; push eax
    a.raw(b"\xFF\x15" + struct.pack("<I", va_ptr_getdesc)) # call [ZEP_GetCurveDesc]
    a.raw(b"\x83\xC4\x08")                  # add esp, 8
    a.raw(b"\x89\xC3")                      # mov ebx, eax (result in ebx)
    a.raw(b"\x58\x5A\x59")                  # pop eax ; pop edx ; pop ecx (RESTORE)
    a.raw(b"\xC3")                          # ret
    a.b[jl_desc3+1] = len(a.b) - (jl_desc3+2) # vanilla:
    a.b[je_desc3_null+1] = len(a.b) - (je_desc3_null+2)
    a.raw(b"\x8D\x9C\x02\xAC\x00\x00\x00")  # lea ebx, [edx + eax + 0xac]
    a.raw(b"\xC3")                          # ret
    emit(a)

    # Cave F: va_clamp_ecx (Speed Loop Clamp in FUN_0041a500, Treasure Clamp A in FUN_00413ba0, DescReset Clamp A in FUN_00421a10)
    # Replaces 'mov ecx, [esi + 0x8c]' (6 bytes)
    a = Asm(); va_clamp_ecx = a.start
    a.raw(b"\x8B\x8E\x8C\x00\x00\x00")      # mov ecx, dword ptr [esi + 0x8c]
    a.raw(b"\x83\xF9\x03")                  # cmp ecx, 3
    jle_clamp_ecx = len(a.b); a.raw(b"\x7E\x00") # jle clamp_ok
    a.raw(b"\xB9\x03\x00\x00\x00")          # mov ecx, 3
    a.b[jle_clamp_ecx+1] = len(a.b) - (jle_clamp_ecx+2) # clamp_ok:
    a.raw(b"\xC3")                          # ret
    emit(a)

    # Cave F2: va_clamp_eax (Treasure Clamp B in FUN_00413ba0, DescReset Clamp B in FUN_00421a10)
    # Replaces 'mov eax, [esi + 0x8c]' (6 bytes)
    a = Asm(); va_clamp_eax = a.start
    a.raw(b"\x8B\x86\x8C\x00\x00\x00")      # mov eax, dword ptr [esi + 0x8c]
    a.raw(b"\x83\xF8\x03")                  # cmp eax, 3
    jle_clamp_eax = len(a.b); a.raw(b"\x7E\x00") # jle clamp_ok
    a.raw(b"\xB8\x03\x00\x00\x00")          # mov eax, 3
    a.b[jle_clamp_eax+1] = len(a.b) - (jle_clamp_eax+2) # clamp_ok:
    a.raw(b"\xC3")                          # ret
    emit(a)

    # Cave G: HoleMgr::AddHole dispatch (hole_hook3) - intercepts all curves 0..9 and packs last waypoint priority
    va_hole = {}
    stolen3, n3 = hole_steal[hole_hook3]
    a = Asm(); va_hole[hole_hook3] = (a.start, n3)
    # btr dword ptr [esp+4], 31 (test & clear recursion bit from ZEP_AddHole -> g_fn_addhole)
    a.raw(b"\x0F\xBA\x74\x24\x04\x1F")
    jc_orig3 = len(a.b); a.raw(b"\x72\x00") # jc original
    # cmp dword ptr [va_ptr_addhole], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", va_ptr_addhole) + b"\x00")
    je_orig3 = len(a.b); a.raw(b"\x74\x00") # je original
    # Validate esi (Curve*) and [esi + 0x18] == ecx (HoleMgr*), then read last waypoint priority
    a.raw(b"\x85\xF6")                      # test esi, esi
    jz_disp3_1 = len(a.b); a.raw(b"\x74\x00")
    a.raw(b"\x39\x4E\x18")                  # cmp dword ptr [esi + 0x18], ecx
    jne_disp3_2 = len(a.b); a.raw(b"\x75\x00")
    a.raw(b"\x8B\x46\x0C")                  # mov eax, dword ptr [esi + 0x0c] (WayPointMgr*)
    a.raw(b"\x85\xC0")                      # test eax, eax
    jz_disp3_3 = len(a.b); a.raw(b"\x74\x00")
    a.raw(b"\x8B\x50\x0C")                  # mov edx, dword ptr [eax + 0x0c] (wpm->end)
    a.raw(b"\x3B\x50\x08")                  # cmp edx, dword ptr [eax + 0x08] (wpm->begin)
    jbe_disp3_4 = len(a.b); a.raw(b"\x76\x00")
    a.raw(b"\x0F\xB6\x52\xFD")              # movzx edx, byte ptr [edx - 3] (last_waypoint.priority)
    a.raw(b"\xC1\xE2\x08")                  # shl edx, 8
    a.raw(b"\x09\x54\x24\x04")              # or dword ptr [esp + 4], edx
    # dispatch:
    for br_idx in (jz_disp3_1, jne_disp3_2, jz_disp3_3, jbe_disp3_4):
        a.b[br_idx + 1] = len(a.b) - (br_idx + 2)
    a.raw(b"\xFF\x25" + struct.pack("<I", va_ptr_addhole))
    # original:
    a.b[jc_orig3 + 1] = len(a.b) - (jc_orig3 + 2)
    a.b[je_orig3 + 1] = len(a.b) - (je_orig3 + 2)
    a.raw(stolen3)
    a.jmp(hole_hook3 + n3)
    emit(a)

    # Cave H, I: HoleMgr SetHolePercent / SetHoleSomething dispatches (curves >= 3)
    for va, (nargs, slot_ptr) in [(hole_hook1, (0x08, va_ptr_setpct)),
                                  (hole_hook2, (0x08, va_ptr_setsomething))]:
        stolen, n = hole_steal[va]
        a = Asm(); va_hole[va] = (a.start, n)
        # cmp dword ptr [esp+4], 3
        a.raw(b"\x83\x7C\x24\x04" + bytes([HOLE_MIN_CURVE]))
        jl_off_idx = len(a.b); a.raw(b"\x7C\x00")             # jl original
        # cmp dword ptr [slot_ptr], 0
        a.raw(b"\x83\x3D" + struct.pack("<I", slot_ptr) + b"\x00")
        je_off_idx = len(a.b); a.raw(b"\x74\x00")             # je do_ret
        # jmp dword ptr [slot_ptr]
        a.raw(b"\xFF\x25" + struct.pack("<I", slot_ptr))
        # do_ret:
        a.b[je_off_idx+1] = len(a.b) - (je_off_idx+2)
        a.raw(b"\xC2" + struct.pack("<H", nargs))            # ret nargs
        # original:
        a.b[jl_off_idx+1] = len(a.b) - (jl_off_idx+2)
        a.raw(stolen)
        a.jmp(va + n)
        emit(a)

    # Cave J1: va_orig_5af0 (Trampoline for original FUN_00405af0 overlay drawing)
    a = Asm(); va_orig_5af0 = a.start
    a.raw(OVERLAY_DRAW_EXPECT)              # push ebp ; mov ebp, esp ; mov eax, [ebp + 0x0c]
    a.jmp(overlay_draw_va + 6)
    emit(a)

    # Cave J2: va_run_hole_loop (Helper __thiscall(HoleMgr *this, Graphics *g) entering FUN_00407250 hole loop)
    a = Asm(); va_run_hole_loop = a.start
    a.raw(b"\x55\x8B\xEC\x83\xEC\x10\x53\x56\x8B\x75\x08\x57\x8B\xF9") # frame setup + edi=this, esi=g
    a.raw(HOLE_BG_END_EXPECT)               # mov eax, [edi + 0x74] ; xor ebx, ebx
    a.jmp(hole_bg_end_va + 5)               # jmp 0x00407281 (test eax, eax ; jle epilogue_va)
    emit(a)

    # Cave J3: va_draw_prio_layer (Hooked from STAGE2_OVERLAY_DRAW @ FUN_00405af0)
    # Draws Overlays[priority] first, then draws Skulls[priority] (with mBrilliance glow) on top!
    # If zumaextrapath.dll is missing (va_ptr_checkholeprio is 0), skips hole loop to keep vanilla overlays
    a = Asm(); va_draw_prio_layer = a.start
    a.raw(b"\x55\x8B\xEC\x56\x8B\xF1")      # push ebp ; mov ebp, esp ; push esi ; mov esi, ecx
    a.raw(b"\xFF\x75\x0C\xFF\x75\x08\x8B\xCE") # push [ebp+0x0c] ; push [ebp+0x08] ; mov ecx, esi
    a.call(va_orig_5af0)                    # call original FUN_00405af0 (ret 8)
    # Check if zumaextrapath.dll is active
    a.raw(b"\x83\x3D" + struct.pack("<I", va_ptr_checkholeprio) + b"\x00")
    jz_skip_hole_layer = len(a.b); a.raw(b"\x74\x00")
    a.raw(b"\xFF\x75\x08\x8B\xCE")          # push [ebp+0x08] ; mov ecx, esi
    a.call(va_run_hole_loop)                # call va_run_hole_loop (ret 4)
    a.b[jz_skip_hole_layer + 1] = len(a.b) - (jz_skip_hole_layer + 2) # skip_hole_layer:
    a.raw(b"\x5E\x5D\xC2\x08\x00")          # pop esi ; pop ebp ; ret 8
    emit(a)

    # Cave K: va_hole_loop_check (Hooked from STAGE2_HOLE_DRAW_LOOP @ 0x00407290)
    # Filters each hole ebx in HoleMgr *edi against caller priority at [[ebp] + 0x0c]
    a = Asm(); va_hole_loop_check = a.start
    a.raw(b"\x83\x3D" + struct.pack("<I", va_ptr_checkholeprio) + b"\x00")
    je_draw_it = len(a.b); a.raw(b"\x74\x00") # je draw_it
    a.raw(b"\x8B\x45\x00")                  # mov eax, dword ptr [ebp] (va_draw_prio_layer ebp)
    a.raw(b"\xFF\x70\x0C")                  # push dword ptr [eax + 0x0c] (int prio)
    a.raw(b"\x53\x57")                      # push ebx (hole_idx) ; push edi (HoleMgr*)
    a.raw(b"\xFF\x15" + struct.pack("<I", va_ptr_checkholeprio))
    a.raw(b"\x83\xC4\x0C")                  # add esp, 12
    a.raw(b"\x85\xC0")                      # test eax, eax
    jz_skip_it = len(a.b); a.raw(b"\x74\x00") # jz skip_it
    # draw_it:
    a.b[je_draw_it + 1] = len(a.b) - (je_draw_it + 2)
    a.raw(b"\x53\x56\x8B\xCF")              # push ebx ; push esi ; mov ecx, edi
    a.call(draw_single_hole_va)             # call DrawSingleHole (0x00404D50)
    a.jmp(hole_loop_va + 9)                 # jmp 0x00407299 (mBrilliance > 0 glow check)
    # skip_it:
    a.b[jz_skip_it + 1] = len(a.b) - (jz_skip_it + 2)
    a.jmp(skip_hole_va)                     # jmp 0x004072E6 (next hole iteration)
    emit(a)

    # Cave L: va_draw_static_board (Hooked from STAGE2_STATIC_BOARD_DRAW @ FUN_00407300)
    # If zumaextrapath.dll is absent: calls FUN_00407250 directly (draws background and all skulls once)
    # If present: calls FUN_00407250 (draws background only), then loops prio = 0..4 calling va_draw_prio_layer
    a = Asm(); va_draw_static_board = a.start
    a.raw(b"\x55\x8B\xEC\x56\x57\x8B\xF1")  # push ebp ; mov ebp, esp ; push esi ; push edi ; mov esi, ecx
    a.raw(b"\x83\x3D" + struct.pack("<I", va_ptr_checkholeprio) + b"\x00") # cmp dword ptr [va_ptr_checkholeprio], 0
    jnz_prio = len(a.b); a.raw(b"\x75\x00")

    # Vanilla fallback: draw background & skulls via FUN_00407250
    a.raw(b"\xFF\x75\x08\x8B\xCE")          # push dword ptr [ebp + 8] ; mov ecx, esi
    a.call(draw_bg_va)                      # call FUN_00407250 (ret 4)
    a.raw(b"\x5F\x5E\x5D\xC2\x04\x00")      # pop edi ; pop esi ; pop ebp ; ret 4

    # Custom prio loop (zumaextrapath.dll active):
    a.b[jnz_prio + 1] = len(a.b) - (jnz_prio + 2)
    a.raw(b"\xFF\x75\x08\x8B\xCE")          # push dword ptr [ebp + 8] ; mov ecx, esi
    a.call(draw_bg_va)                      # call FUN_00407250 (draws background only, ret 4)
    a.raw(b"\x33\xFF")                      # xor edi, edi (prio = 0)
    loop_prio_off = len(a.b)
    a.raw(b"\x57\xFF\x75\x08\x8B\xCE")      # push edi ; push [ebp + 8] ; mov ecx, esi
    a.call(va_draw_prio_layer)              # call va_draw_prio_layer (ret 8)
    a.raw(b"\x47\x83\xFF\x05")              # inc edi ; cmp edi, 5
    jl_disp = loop_prio_off - (len(a.b) + 2)
    a.raw(b"\x7C" + struct.pack("b", jl_disp)) # jl loop_prio_off
    a.raw(b"\x5F\x5E\x5D\xC2\x04\x00")      # pop edi ; pop esi ; pop ebp ; ret 4
    emit(a)

    # Cave M: va_cave_hole_bg_end (Hooked from STAGE2_HOLE_BG_END @ 0x0040727C)
    # If zumaextrapath.dll is absent (va_ptr_checkholeprio == 0):
    #   Executes stolen instructions (mov eax, [edi+0x74]; xor ebx, ebx) and jumps back to 0x00407281
    #   to let the vanilla hole drawing loop in FUN_00407250 render all skulls normally!
    # If zumaextrapath.dll is present:
    #   Jumps to epilogue_va to skip the loop, allowing Cave J3 to render skulls per-priority layer.
    a = Asm(); va_cave_hole_bg_end = a.start
    a.raw(b"\x83\x3D" + struct.pack("<I", va_ptr_checkholeprio) + b"\x00") # cmp dword ptr [va_ptr_checkholeprio], 0
    jnz_skip_holes = len(a.b); a.raw(b"\x75\x00")                         # jnz .skip_holes
    # Vanilla fallback path:
    a.raw(HOLE_BG_END_EXPECT)                                              # mov eax, [edi + 0x74] ; xor ebx, ebx
    a.jmp(hole_bg_end_va + 5)                                              # jmp 0x00407281 (test eax, eax ; jle epilogue_va)
    # .skip_holes (Mod active):
    a.b[jnz_skip_holes + 1] = len(a.b) - (jnz_skip_holes + 2)
    a.jmp(epilogue_va)                                                     # jmp 0x004072F7 (epilogue ret 4)
    emit(a)



    # 5. Add section .zep2 (0x60000020: CODE | EXECUTE | READ)
    new_sec = pe.add_section('.zep2', blob, ZEP2_CHARS)
    print(f"[OK] Section .zep2 added: RVA=0x{new_sec.virtual_address:08X}, "
          f"VSz=0x{new_sec.virtual_size:X}, Chars=0x{new_sec.characteristics:08X}")

    # 6. Apply code patches using strict write_site helper
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

    # Swap curve count
    write_site('STAGE2_SWAP_COUNT', swap_va, SWAP_EXPECT,
               b"\xE8" + struct.pack("<i", va_swap - (swap_va + 5)))

    # BindExtras hook
    write_site('STAGE2_BIND_VA', bind_va, BIND_EXPECT,
               b"\xE9" + struct.pack("<i", va_bind - (bind_va + 5)))

    # CurveDesc routing hooks (5 bytes call + 2 bytes nop = 7 bytes)
    write_site('STAGE2_DESC_SITE1', desc_site1, DESC1_EXPECT,
               b"\xE8" + struct.pack("<i", va_desc1 - (desc_site1 + 5)) + b"\x90\x90")
    write_site('STAGE2_DESC_SITE2', desc_site2, DESC2_EXPECT,
               b"\xE8" + struct.pack("<i", va_desc2 - (desc_site2 + 5)) + b"\x90\x90")
    write_site('STAGE2_DESC_SITE3', desc_site3, DESC3_EXPECT,
               b"\xE8" + struct.pack("<i", va_desc3 - (desc_site3 + 5)) + b"\x90\x90")

    # StartLevel speed loop clamp (5 bytes call + 1 byte nop = 6 bytes)
    write_site('STAGE2_CLAMP_SITE', clamp_site, CLAMP_EXPECT,
               b"\xE8" + struct.pack("<i", va_clamp_ecx - (clamp_site + 5)) + b"\x90")

    # TreasurePoint loop clamp A (5 bytes call + 1 byte nop = 6 bytes)
    write_site('STAGE2_TREASURE_CLAMP_A', t_clamp_a, CLAMP_EXPECT,
               b"\xE8" + struct.pack("<i", va_clamp_ecx - (t_clamp_a + 5)) + b"\x90")

    # TreasurePoint loop clamp B (5 bytes call + 1 byte nop = 6 bytes)
    write_site('STAGE2_TREASURE_CLAMP_B', t_clamp_b, CLAMP_EAX_EXPECT,
               b"\xE8" + struct.pack("<i", va_clamp_eax - (t_clamp_b + 5)) + b"\x90")

    # DescReset loop clamp A (5 bytes call + 1 byte nop = 6 bytes)
    write_site('STAGE2_DESCRESET_CLAMP_A', d_clamp_a, CLAMP_EXPECT,
               b"\xE8" + struct.pack("<i", va_clamp_ecx - (d_clamp_a + 5)) + b"\x90")

    # DescReset loop clamp B (5 bytes call + 1 byte nop = 6 bytes)
    write_site('STAGE2_DESCRESET_CLAMP_B', d_clamp_b, CLAMP_EAX_EXPECT,
               b"\xE8" + struct.pack("<i", va_clamp_eax - (d_clamp_b + 5)) + b"\x90")

    # HoleMgr dispatches (5 bytes jmp + 1 byte nop = 6 bytes)
    for name, va in [('STAGE2_HOLE_HOOK1', hole_hook1),
                     ('STAGE2_HOLE_HOOK2', hole_hook2),
                     ('STAGE2_HOLE_HOOK3', hole_hook3)]:
        start_va, n = va_hole[va]
        stolen, _ = hole_steal[va]
        patch_bytes = b"\xE9" + struct.pack("<i", start_va - (va + 5)) + b"\x90" * (n - 5)
        write_site(name, va, stolen, patch_bytes)

    # Ball::SetNearestWaypoint & Ball::GetNearestWaypoint (bounds check against mNearestWaypoint[3])
    write_site('STAGE2_BALL_SET_WAYPOINT', ball_set_va, BALL_SET_EXPECT, BALL_SET_PATCH)
    write_site('STAGE2_BALL_GET_WAYPOINT', ball_get_va, BALL_GET_EXPECT, BALL_GET_PATCH)

    # Priority-Aware Skull (Hole) Rendering sites
    write_site('STAGE2_OVERLAY_DRAW', overlay_draw_va, OVERLAY_DRAW_EXPECT,
               b"\xE9" + struct.pack("<i", va_draw_prio_layer - (overlay_draw_va + 5)) + b"\x90")
    write_site('STAGE2_HOLE_BG_END', hole_bg_end_va, HOLE_BG_END_EXPECT,
               b"\xE9" + struct.pack("<i", va_cave_hole_bg_end - (hole_bg_end_va + 5)))
    write_site('STAGE2_HOLE_DRAW_LOOP', hole_loop_va, hole_loop_expect,
               b"\xE9" + struct.pack("<i", va_hole_loop_check - (hole_loop_va + 5)) + b"\x90\x90\x90\x90")
    write_site('STAGE2_STATIC_BOARD_DRAW', static_board_va, static_board_expect,
               b"\xE9" + struct.pack("<i", va_draw_static_board - (static_board_va + 5)))

    assert len(write_log) == 19, f"Expected 19 sites written in Stage 2, got {len(write_log)}"
    print(f"[OK] All 19 Stage 2 sites written and logged cleanly.")

    pe.save(dst)
    print(f"[DONE] Stage 2 applied cleanly -> {dst} (.zep2 @0x{new_sec.virtual_address + pe.image_base:08X})")
    return write_log


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    try:
        patch_extrapath_stage2(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) == 4 else None)
    except Exception as e:
        print("\n" + "=" * 67)
        print(" [COMPATIBILITY ERROR] Failed to detect required Stage 2 runtime hooks.")
        print(f" Detail: {e}")
        print("=" * 67)
        print(" [WARNING]")
        print(" The provided executable does not have the expected structure or attributes.")
        print(" Please provide a compatible Zuma.exe binary.")
        print(" (RECOMMENDED: Zuma Deluxe v1.0.0.1 GameHouse / PopCap Retail)")
        print("=" * 67 + "\n")
        sys.exit(1)
