"""patch_custommap.py - Activates Custom Map support in Gauntlet Mode for Zuma Deluxe.

Usage: python patch_custommap.py <Zuma_in.exe> <Zuma_out.exe>
(Input: Zuma.exe output from patch_extrapath_stage2.py / build pipeline.)

Clean Sidecar Architecture:
  1. Integrates with zumacustommap.dll via static IAT imports.
  2. Four targeted hooks:
     - Hook A (0x0043BD02): Expands carousel count edx += N_custom, records g_V.
     - Hook B (0x0043BD34): Routes custom level IDs & DisplayNames to SyncLevel.
     - Hook C (0x0043BD5B): Unlocks Rabbit difficulty for custom maps while preserving vanilla locks.
     - Hook D (0x004393D9): Draws green "CUSTOM MAP" tag at (X=190, Y=260) and restores yellow color.
  3. Natural Progression:
     - When custom maps are beaten in Gauntlet, vanilla engine updates UserProfile.
     - Beating 7 stages unlocks Eagle, 14 Jaguar, 21 Sun God naturally.

Section .zcm is PURE RX CODE (characteristics 0x60000020: CODE | EXECUTE | READ).
All 4 write sites resolved dynamically via AOB pattern scanning and Capstone instruction inspection.
"""

import sys
import os
import struct
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import (
    X86_OP_MEM, X86_OP_REG, X86_OP_IMM,
    X86_REG_EAX, X86_REG_ECX, X86_REG_EDX,
    X86_REG_ESI, X86_REG_EBX, X86_REG_EDI
)

sys.path.append(os.path.dirname(__file__))
from pe_util import PEFile, align

def resolve_custommap_sites(pe):
    image_base = pe.OPTIONAL_HEADER.ImageBase
    text_sec = None
    for s in pe.sections:
        if s.Name.rstrip(b'\x00') == b'.text':
            text_sec = s
            break
    if not text_sec:
        raise RuntimeError("Could not find .text section in executable")

    text_data = text_sec.get_data()
    text_va = image_base + text_sec.VirtualAddress

    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True

    # 1. Resolve Site A, B, C via PracticeScreen::SyncLevel
    # Signature: 83 FB 0C (cmp ebx, 0xc) followed by jge, dec edx (4a), cmp [esi+0xec], edx (39 96 ec 00 00 00)
    matches_a = []
    idx = text_data.find(b"\x83\xFB\x0C")
    while idx != -1:
        va = text_va + idx
        code = text_data[idx:idx+250]
        insns = list(md.disasm(code, va))
        for i, ins in enumerate(insns):
            if ins.mnemonic == "cmp" and len(ins.operands) == 2:
                op0, op1 = ins.operands
                if (op0.type == X86_OP_MEM and op0.mem.base == X86_REG_ESI and 
                    op0.mem.disp == 0xEC and op1.type == X86_OP_REG and op1.reg == X86_REG_EDX):
                    
                    if ins.size != 6:
                        raise RuntimeError(f"Site A instruction size mismatch: {ins.size} != 6")
                    
                    site_a = ins.address
                    disp_bytes_a = bytes(ins.bytes)

                    # Look forward for Site B (call 0x415100)
                    site_b = None
                    fn_b = None
                    for j in range(i+1, min(i+15, len(insns))):
                        if insns[j].mnemonic == "call" and insns[j].operands[0].type == X86_OP_IMM:
                            site_b = insns[j].address
                            fn_b = insns[j].operands[0].imm
                            break
                    if not site_b:
                        raise RuntimeError("Could not locate Site B (call 0x415100) after Site A")

                    # Look forward for Site C (je after cmp ecx, eax)
                    site_c = None
                    target_c = None
                    disp_bytes_c = None
                    for j in range(i+1, min(i+35, len(insns))):
                        if insns[j].mnemonic == "je" and insns[j-1].mnemonic == "cmp":
                            site_c = insns[j].address
                            target_c = insns[j].operands[0].imm
                            next_ins = insns[j+1]
                            if insns[j].size + next_ins.size != 5:
                                raise RuntimeError(f"Site C hook size mismatch: {insns[j].size} + {next_ins.size} != 5")
                            disp_bytes_c = bytes(next_ins.bytes)
                            break
                    if not site_c:
                        raise RuntimeError("Could not locate Site C (je 0x43bd8e) after Site A")

                    matches_a.append({
                        'site_a': site_a,
                        'disp_bytes_a': disp_bytes_a,
                        'site_b': site_b,
                        'fn_b': fn_b,
                        'site_c': site_c,
                        'target_c': target_c,
                        'disp_bytes_c': disp_bytes_c
                    })
                    break
        idx = text_data.find(b"\x83\xFB\x0C", idx + 1)

    if len(matches_a) != 1:
        raise RuntimeError(f"Expected exactly 1 match for SyncLevel pattern, got {len(matches_a)}")
    res_a = matches_a[0]

    # 2. Resolve Site D via PracticeScreen::Draw
    # Search for mov edx, 0x4fe6b8 (ba b8 e6 4f 00)
    matches_d = []
    idx_d = text_data.find(b"\xBA\xB8\xE6\x4F\x00")
    while idx_d != -1:
        va = text_va + idx_d
        code = text_data[idx_d:idx_d+150]
        insns = list(md.disasm(code, va))
        calls = [ins for ins in insns if ins.mnemonic == "call"]
        if len(calls) >= 3:
            # 3rd call is DrawString
            draw_call = calls[2]
            if draw_call.size != 5:
                raise RuntimeError(f"Site D call size mismatch: {draw_call.size} != 5")
            # Find preceding add edx, imm
            imm_val = None
            for ins in insns:
                if ins.address >= draw_call.address:
                    break
                if ins.mnemonic == "add" and len(ins.operands) == 2:
                    if ins.operands[0].type == X86_OP_REG and ins.operands[0].reg == X86_REG_EDX:
                        if ins.operands[1].type == X86_OP_IMM:
                            imm_val = ins.operands[1].imm
            if imm_val is not None:
                matches_d.append({
                    'site_d': draw_call.address,
                    'draw_fn': draw_call.operands[0].imm,
                    'imm_d': imm_val,
                    'tag_x': 0xAA + (0xAA - imm_val)
                })
        idx_d = text_data.find(b"\xBA\xB8\xE6\x4F\x00", idx_d + 1)

    if len(matches_d) != 1:
        raise RuntimeError(f"Expected exactly 1 match for Site D Draw pattern, got {len(matches_d)}")
    res_d = matches_d[0]

    # 3. Resolve Site E via SpriteMgr constructor AOB (Option 2 fix)
    AOB_CTOR = bytes.fromhex("895E74 885E59 C6465A01 895E50")
    idx_e = text_data.find(AOB_CTOR)
    if idx_e == -1:
        idx_e2 = text_data.find(bytes.fromhex("C6465A01895E50"))
        if idx_e2 != -1 and idx_e2 >= 6:
            site_e = text_va + idx_e2 - 6
        else:
            raise RuntimeError("Could not locate Site E (SpriteMgr constructor) pattern")
    else:
        site_e = text_va + idx_e

    # 4. Resolve Site Random & Site Gauntlet via Board::InitLevel pattern: 83 E9 05 83 F9 0A
    pattern_init = bytes.fromhex("83E905 83F90A")
    idx_init = text_data.find(pattern_init)
    if idx_init == -1:
        raise RuntimeError("Could not locate Board::InitLevel picker pattern")
    va_init = text_va + idx_init

    # Disassemble backwards to find fn_get_level_desc (call 0x415070)
    back_code = text_data[idx_init - 60:idx_init]
    insns_back = list(md.disasm(back_code, va_init - 60))
    fn_get_level_desc = None
    for ins in reversed(insns_back):
        if ins.mnemonic == "call" and len(ins.operands) == 1 and ins.operands[0].type == X86_OP_IMM:
            fn_get_level_desc = ins.operands[0].imm
            break
    if not fn_get_level_desc:
        raise RuntimeError("Could not locate fn_get_level_desc before InitLevel pattern")

    fwd_code = text_data[idx_init:idx_init + 350]
    insns_fwd = list(md.disasm(fwd_code, va_init))

    site_random = None
    fn_pick_random_vanilla = None
    site_gauntlet = None
    fn_pick_gauntlet_vanilla = None

    for i, ins in enumerate(insns_fwd):
        if ins.mnemonic == "call" and len(ins.operands) == 1 and ins.operands[0].type == X86_OP_IMM:
            if i + 1 < len(insns_fwd):
                next_ins = insns_fwd[i+1]
                if next_ins.mnemonic == "mov" and next_ins.op_str == "edi, dword ptr [ebp + 0xc]":
                    if site_random is None:
                        site_random = ins.address
                        fn_pick_random_vanilla = ins.operands[0].imm
                    elif site_gauntlet is None:
                        site_gauntlet = ins.address
                        fn_pick_gauntlet_vanilla = ins.operands[0].imm
                        break

    if not site_random or not site_gauntlet:
        raise RuntimeError(f"Could not locate Site Random ({site_random}) or Site Gauntlet ({site_gauntlet})")

    # 5. Dynamically resolve internal helper functions via unique opcode attributes
    # fn_assign_sub: first call inside fn_b
    off_b = res_a['fn_b'] - text_va
    insns_b = list(md.disasm(text_data[off_b:off_b + 150], res_a['fn_b']))
    fn_assign_sub = None
    for ins in insns_b:
        if ins.mnemonic == "call" and len(ins.operands) == 1 and ins.operands[0].type == X86_OP_IMM:
            fn_assign_sub = ins.operands[0].imm
            break
    if not fn_assign_sub:
        raise RuntimeError("Could not dynamically resolve fn_assign_sub from fn_b")

    # rabbit_unlock: cmp edx, 3 following site_c
    off_c = res_a['site_c'] - text_va
    insns_c = list(md.disasm(text_data[off_c:off_c + 60], res_a['site_c']))
    rabbit_unlock = None
    for ins in insns_c:
        if ins.mnemonic == "cmp" and ins.op_str == "edx, 3":
            rabbit_unlock = ins.address
            break
    if not rabbit_unlock:
        raise RuntimeError("Could not dynamically resolve rabbit_unlock following site_c")

    # fn_color_ctor, fn_set_color, fn_assign_str preceding site_d
    off_d = res_d['site_d'] - text_va
    back_code = text_data[max(0, off_d - 150):off_d]
    insns_d = list(md.disasm(back_code, res_d['site_d'] - len(back_code)))
    fn_color_ctor = None
    fn_set_color = None
    fn_assign_str = None
    for i, ins in enumerate(insns_d):
        if ins.mnemonic == "push" and ins.op_str == "0xffff00":
            for j in range(i + 1, min(i + 5, len(insns_d))):
                if insns_d[j].mnemonic == "call" and insns_d[j].operands[0].type == X86_OP_IMM:
                    fn_color_ctor = insns_d[j].operands[0].imm
                    for k in range(j + 1, min(j + 6, len(insns_d))):
                        if insns_d[k].mnemonic == "call" and insns_d[k].operands[0].type == X86_OP_IMM:
                            fn_set_color = insns_d[k].operands[0].imm
                            break
                    break
        if ins.mnemonic == "lea" and "ebp - 0x44" in ins.op_str:
            for j in range(i + 1, min(i + 4, len(insns_d))):
                if insns_d[j].mnemonic == "call" and insns_d[j].operands[0].type == X86_OP_IMM:
                    fn_assign_str = insns_d[j].operands[0].imm
                    break

    if not fn_color_ctor or not fn_set_color or not fn_assign_str:
        raise RuntimeError("Could not dynamically resolve color/string functions near site_d")

    return {
        'CUSTOMMAP_SITE_A': res_a['site_a'],
        'CUSTOMMAP_DISP_INSTR_A': res_a['disp_bytes_a'],
        'CUSTOMMAP_SITE_B': res_a['site_b'],
        'CUSTOMMAP_FN_B': res_a['fn_b'],
        'CUSTOMMAP_SITE_C': res_a['site_c'],
        'CUSTOMMAP_TARGET_C': res_a['target_c'],
        'CUSTOMMAP_DISP_INSTR_C': res_a['disp_bytes_c'],
        'CUSTOMMAP_SITE_D': res_d['site_d'],
        'CUSTOMMAP_DRAW_FN_D': res_d['draw_fn'],
        'CUSTOMMAP_IMM_D': res_d['imm_d'],
        'CUSTOMMAP_TAG_X': res_d['tag_x'],
        'CUSTOMMAP_SITE_E': site_e,
        'CUSTOMMAP_SITE_RANDOM': site_random,
        'CUSTOMMAP_FN_RANDOM_VANILLA': fn_pick_random_vanilla,
        'CUSTOMMAP_SITE_GAUNTLET': site_gauntlet,
        'CUSTOMMAP_FN_GAUNTLET_VANILLA': fn_pick_gauntlet_vanilla,
        'CUSTOMMAP_FN_GET_LEVEL_DESC': fn_get_level_desc,
        'CUSTOMMAP_FN_ASSIGN_SUB': fn_assign_sub,
        'CUSTOMMAP_RABBIT_UNLOCK': rabbit_unlock,
        'CUSTOMMAP_FN_COLOR_CTOR': fn_color_ctor,
        'CUSTOMMAP_FN_SET_COLOR': fn_set_color,
        'CUSTOMMAP_FN_ASSIGN_STR': fn_assign_str
    }

def find_zcm_iat_entries(pe_raw):
    """Finds IAT virtual addresses for zumacustommap.dll exports using PEFile."""
    pe = PEFile(pe_raw)
    imps = pe.get_imports()
    zcm_dict = imps.get('zumacustommap.dll', {})
    iat_map = {}
    for fn in ['zcm_AdjustCount', 'zcm_GetSrcId', 'zcm_GetSrcName', 'zcm_GetV', 'zcm_PickRandomLevel', 'zcm_PickGauntletLevel']:
        iat_map[fn] = zcm_dict.get(fn)
    return iat_map

def patch_custommap(src_path, dst_path):
    print(f"[INFO] Patching Custom Map Gauntlet support from {src_path} -> {dst_path}")
    pe = PEFile(src_path)

    # Idempotency guard
    if pe.find_section_by_name('.zcm'):
        print("[INFO] Section .zcm already present in executable.")
        if src_path != dst_path:
            pe.save(dst_path)
        return

    # 1. Resolve IAT entries
    iat_entries = find_zcm_iat_entries(bytes(pe.data))
    required_exports = [
        'zcm_AdjustCount', 'zcm_GetSrcId', 'zcm_GetSrcName', 'zcm_GetV',
        'zcm_PickRandomLevel', 'zcm_PickGauntletLevel'
    ]
    for req in required_exports:
        if req not in iat_entries:
            sys.exit(f"[ABORT] Missing required IAT import for zumacustommap.dll: {req}. Run patch_imports.py first!")

    va_iat_adjust_count = iat_entries['zcm_AdjustCount']
    va_iat_get_src_id   = iat_entries['zcm_GetSrcId']
    va_iat_get_src_name = iat_entries['zcm_GetSrcName']
    va_iat_get_v        = iat_entries['zcm_GetV']
    va_iat_pick_random  = iat_entries['zcm_PickRandomLevel']
    va_iat_pick_gauntlet= iat_entries['zcm_PickGauntletLevel']

    print(f"[OK] IAT zcm_AdjustCount:      0x{va_iat_adjust_count:08X}")
    print(f"[OK] IAT zcm_GetSrcId:          0x{va_iat_get_src_id:08X}")
    print(f"[OK] IAT zcm_GetSrcName:        0x{va_iat_get_src_name:08X}")
    print(f"[OK] IAT zcm_GetV:              0x{va_iat_get_v:08X}")
    print(f"[OK] IAT zcm_PickRandomLevel:   0x{va_iat_pick_random:08X}")
    print(f"[OK] IAT zcm_PickGauntletLevel: 0x{va_iat_pick_gauntlet:08X}")

    # 2. Resolve Sites dynamically via Capstone
    p_pe = pefile.PE(data=bytes(pe.data))
    sites = resolve_custommap_sites(p_pe)

    site_a = sites['CUSTOMMAP_SITE_A']
    disp_bytes_a = sites['CUSTOMMAP_DISP_INSTR_A']
    site_b = sites['CUSTOMMAP_SITE_B']
    fn_b   = sites['CUSTOMMAP_FN_B']
    site_c = sites['CUSTOMMAP_SITE_C']
    target_c = sites['CUSTOMMAP_TARGET_C']
    disp_bytes_c = sites['CUSTOMMAP_DISP_INSTR_C']
    site_d = sites['CUSTOMMAP_SITE_D']
    draw_fn = sites['CUSTOMMAP_DRAW_FN_D']
    tag_x  = sites['CUSTOMMAP_TAG_X']
    site_e = sites.get('CUSTOMMAP_SITE_E')
    site_random = sites['CUSTOMMAP_SITE_RANDOM']
    fn_random_vanilla = sites['CUSTOMMAP_FN_RANDOM_VANILLA']
    site_gauntlet = sites['CUSTOMMAP_SITE_GAUNTLET']
    fn_gauntlet_vanilla = sites['CUSTOMMAP_FN_GAUNTLET_VANILLA']
    fn_get_level_desc = sites['CUSTOMMAP_FN_GET_LEVEL_DESC']
    fn_assign_sub = sites['CUSTOMMAP_FN_ASSIGN_SUB']
    rabbit_unlock = sites['CUSTOMMAP_RABBIT_UNLOCK']
    fn_color_ctor = sites['CUSTOMMAP_FN_COLOR_CTOR']
    fn_set_color = sites['CUSTOMMAP_FN_SET_COLOR']
    fn_assign_str = sites['CUSTOMMAP_FN_ASSIGN_STR']

    print(f"[OK] Site A: 0x{site_a:08X} (displaced: {disp_bytes_a.hex(' ')})")
    print(f"[OK] Site B: 0x{site_b:08X} (fn: 0x{fn_b:08X})")
    print(f"[OK] Site C: 0x{site_c:08X} (target: 0x{target_c:08X}, displaced: {disp_bytes_c.hex(' ')})")
    print(f"[OK] Site D: 0x{site_d:08X} (draw_fn: 0x{draw_fn:08X}, tag_x: {tag_x})")
    if site_e:
        print(f"[OK] Site E: 0x{site_e:08X} (SpriteMgr ctor)")
    print(f"[OK] Site Random:   0x{site_random:08X} (calls 0x{fn_random_vanilla:08X})")
    print(f"[OK] Site Gauntlet: 0x{site_gauntlet:08X} (calls 0x{fn_gauntlet_vanilla:08X})")
    print(f"[OK] Fn GetLevelDesc: 0x{fn_get_level_desc:08X}")
    print(f"[OK] Fn AssignSub: 0x{fn_assign_sub:08X}")
    print(f"[OK] Rabbit Unlock: 0x{rabbit_unlock:08X}")
    print(f"[OK] Color Ctor: 0x{fn_color_ctor:08X}, SetColor: 0x{fn_set_color:08X}, AssignStr: 0x{fn_assign_str:08X}")

    # 3. Determine new section RVA
    last_sec = max(pe.sections, key=lambda s: s.virtual_address)
    new_rva = align(last_sec.virtual_address + max(last_sec.virtual_size, last_sec.raw_size), pe.section_alignment)
    new_va = pe.image_base + new_rva

    # 4. Construct .zcm content
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

    # String constant: "CUSTOM MAP\0"
    va_str_custom_map = here()
    blob.extend(b"CUSTOM MAP\x00")
    pad()

    # Cave A: Expand carousel count (Hook A at 0x0043BD02)
    a = Asm(); va_cave_a = a.start
    a.raw(b"\x83\x3D" + struct.pack("<I", va_iat_adjust_count) + b"\x00") # cmp dword ptr [va_iat_adjust_count], 0
    jz_vanilla_a = len(a.b); a.raw(b"\x74\x00")
    a.raw(b"\x51")                                      # push ecx
    a.raw(b"\x52")                                      # push edx (items)
    a.raw(b"\xFF\x15" + struct.pack("<I", va_iat_adjust_count)) # call [zcm_AdjustCount] -> eax = items + N_custom
    a.raw(b"\x5A")                                      # pop edx (clean arg)
    a.raw(b"\x89\xC2")                                  # mov edx, eax (edx = new items count)
    a.raw(b"\x59")                                      # pop ecx
    a.b[jz_vanilla_a + 1] = len(a.b) - (jz_vanilla_a + 2) # vanilla_a:
    a.raw(disp_bytes_a)                                 # execute displaced instruction (cmp [esi+0xec], edx)
    a.jmp(site_a + len(disp_bytes_a))                   # jmp back to 0x0043BD08
    emit(a)

    # Cave B: Route custom level info (outId and outDisp) (Hook B at 0x0043BD34)
    a = Asm(); va_cave_b = a.start
    # If zumacustommap.dll is not loaded, jump directly to vanilla fn_b
    a.raw(b"\x83\x3D" + struct.pack("<I", va_iat_get_v) + b"\x00") # cmp dword ptr [va_iat_get_v], 0
    jz_vanilla_b_init = len(a.b); a.raw(b"\x74\x00")

    # Original function expects:
    # ecx = LevelMgr*
    # stack: [esp]=ret_addr, [esp+4]=index, [esp+8]=outId, [esp+12]=outDisp
    a.raw(b"\x53\x56\x57\x55")                          # push ebx, esi, edi, ebp
    a.raw(b"\x51")                                      # push ecx (preserve LevelMgr*!)
    # stack now:
    # [esp+0]=saved ecx, [esp+4]=saved ebp, [esp+8]=saved edi, [esp+12]=saved esi, [esp+16]=saved ebx
    # [esp+20]=ret_addr, [esp+24]=index, [esp+28]=outId, [esp+32]=outDisp

    a.raw(b"\xFF\x15" + struct.pack("<I", va_iat_get_v)) # call [zcm_GetV] -> eax = g_V
    a.raw(b"\x89\xC2")                                  # mov edx, eax (edx = g_V)
    a.raw(b"\x8B\x44\x24\x18")                          # mov eax, [esp + 24] (index: 24 = 0x18)
    a.raw(b"\x39\xD0")                                  # cmp eax, edx
    jle_vanilla_b = len(a.b); a.raw(b"\x7E\x00")        # jle .vanilla_b (vanilla map!)

    # Custom map!
    a.raw(b"\x29\xD0")                                  # sub eax, edx
    a.raw(b"\x48")                                      # dec eax (eax = idx = index - g_V - 1)
    a.raw(b"\x89\xC6")                                  # mov esi, eax (esi = idx)

    # 1. zcm_GetSrcId(idx)
    a.raw(b"\x56")                                      # push esi (idx)
    a.raw(b"\xFF\x15" + struct.pack("<I", va_iat_get_src_id)) # call [zcm_GetSrcId] -> eax = &srcId
    a.raw(b"\x5A")                                      # pop edx (clean up 4-byte argument for __cdecl!)
    a.raw(b"\x85\xC0")                                  # test eax, eax
    jz_vanilla_b2 = len(a.b); a.raw(b"\x74\x00")

    # outId.assign(srcId, 0, -1)
    a.raw(b"\x6A\xFF")                                  # push -1
    a.raw(b"\x6A\x00")                                  # push 0
    a.raw(b"\x50")                                      # push eax (&srcId)
    a.raw(b"\x8B\x4C\x24\x28")                          # mov ecx, [esp + 40] (outId: 0x28)
    a.call(fn_assign_sub)                               # call std::string::assign (__thiscall pops 12 bytes)

    # 2. zcm_GetSrcName(idx)
    a.raw(b"\x56")                                      # push esi (idx)
    a.raw(b"\xFF\x15" + struct.pack("<I", va_iat_get_src_name)) # call [zcm_GetSrcName] -> eax = &srcName
    a.raw(b"\x5A")                                      # pop edx (clean up 4-byte argument for __cdecl!)
    a.raw(b"\x85\xC0")                                  # test eax, eax
    jz_vanilla_b3 = len(a.b); a.raw(b"\x74\x00")

    # outDisp.assign(srcName, 0, -1)
    a.raw(b"\x6A\xFF")                                  # push -1
    a.raw(b"\x6A\x00")                                  # push 0
    a.raw(b"\x50")                                      # push eax (&srcName)
    a.raw(b"\x8B\x4C\x24\x2C")                          # mov ecx, [esp + 44] (outDisp: 0x2C)
    a.call(fn_assign_sub)                               # call std::string::assign (__thiscall pops 12 bytes)

    # Custom finish: clean return
    a.raw(b"\x59")                                      # pop ecx (discard saved ecx)
    a.raw(b"\x5D\x5F\x5E\x5B")                          # pop ebp, edi, esi, ebx
    a.raw(b"\xC2\x0C\x00")                              # ret 12

    # Patch jumps to vanilla
    for j_off in (jle_vanilla_b, jz_vanilla_b2, jz_vanilla_b3):
        a.b[j_off + 1] = len(a.b) - (j_off + 2)

    # Vanilla path: restore ecx, restore ebp, edi, esi, ebx, tail jump to vanilla fn_b
    a.raw(b"\x59")                                      # pop ecx (restore LevelMgr*!)
    a.raw(b"\x5D\x5F\x5E\x5B")                          # pop ebp, edi, esi, ebx
    a.b[jz_vanilla_b_init + 1] = len(a.b) - (jz_vanilla_b_init + 2) # vanilla_b_init:
    a.jmp(fn_b)                                         # jmp fn_b
    emit(a)

    # Cave C: Unlock Rabbit for custom maps (Hook C at site_c)
    a = Asm(); va_cave_c = a.start
    # Check flags from preceding cmp ecx, eax: if not equal (found), jump to .found_c
    jne_found_c = len(a.b); a.raw(b"\x75\x00")

    # Not found in mBoardStats:
    a.raw(b"\x83\x3D" + struct.pack("<I", va_iat_get_v) + b"\x00") # cmp dword ptr [va_iat_get_v], 0
    jz_locked_c_null = len(a.b); a.raw(b"\x74\x00")
    a.raw(b"\xFF\x15" + struct.pack("<I", va_iat_get_v)) # call [zcm_GetV] -> eax = g_V
    a.raw(b"\x39\x86\xEC\x00\x00\x00")                  # cmp [esi + 0xec], eax
    jle_locked_c = len(a.b); a.raw(b"\x7E\x00")         # jle .locked_c (vanilla locked)

    # Custom map! Fake unlock Rabbit:
    a.raw(b"\x31\xD2")                                  # xor edx, edx
    a.jmp(rabbit_unlock)                                # jmp rabbit_unlock (sets unlocked=1, stage=0)

    # Locked path (vanilla map not unlocked in Adventure):
    a.b[jle_locked_c + 1] = len(a.b) - (jle_locked_c + 2)
    a.b[jz_locked_c_null + 1] = len(a.b) - (jz_locked_c_null + 2)
    a.jmp(target_c)                                     # jmp target_c

    # Found path: execute displaced instruction (mov ecx, [ecx + 0x28]) and resume vanilla
    a.b[jne_found_c + 1] = len(a.b) - (jne_found_c + 2)
    a.raw(disp_bytes_c)                                 # execute displaced instruction
    a.jmp(site_c + 5)                                   # jmp site_c + 5
    emit(a)

    # Cave D: Draw green "CUSTOM MAP" tag at (X=tag_x, Y=260) and restore yellow
    a = Asm(); va_cave_d = a.start
    a.call(draw_fn)                                     # call draw_fn (draw original yellow tag)
    a.raw(b"\x83\x3D" + struct.pack("<I", va_iat_get_v) + b"\x00") # cmp dword ptr [va_iat_get_v], 0
    jz_back_d_null = len(a.b); a.raw(b"\x74\x00")
    a.raw(b"\xFF\x15" + struct.pack("<I", va_iat_get_v)) # call [zcm_GetV] -> eax = g_V
    a.raw(b"\x39\x87\xEC\x00\x00\x00")                  # cmp [edi + 0xec], eax
    jle_back_d = len(a.b); a.raw(b"\x7E\x00")           # jle .back_d (vanilla map!)

    # Custom map! Draw green tag:
    a.raw(b"\x53")                                      # push ebx

    # 1. SetColor(0x00FF00) (Green)
    a.raw(b"\x68\x00\xFF\x00\x00")                      # push 0x00FF00
    a.raw(b"\x8D\x4D\xD8")                              # lea ecx, [ebp - 0x28]
    a.call(fn_color_ctor)                               # call Color::Color(0x00FF00)
    a.raw(b"\x8D\x4D\xD8\x51\x89\xF1")                  # lea ecx, [ebp-0x28]; push ecx; mov ecx, esi
    a.call(fn_set_color)                                # call Graphics::SetColor

    # 2. Assign "CUSTOM MAP" to [ebp - 0x44]
    a.raw(b"\x6A\x0A")                                  # push 10 (length)
    a.raw(b"\x68" + struct.pack("<I", va_str_custom_map)) # push offset "CUSTOM MAP"
    a.raw(b"\x8D\x4D\xBC")                              # lea ecx, [ebp - 0x44]
    a.call(fn_assign_str)                               # call std::string::assign

    # 3. DrawString(&str, tag_x, 260)
    a.raw(b"\x68\x04\x01\x00\x00")                      # push 0x104 (Y = 260)
    a.raw(b"\x68" + struct.pack("<I", tag_x))           # push tag_x (X = 190)
    a.raw(b"\x8D\x45\xBC\x50\x89\xF1")                  # lea eax, [ebp-0x44]; push eax; mov ecx, esi
    a.call(draw_fn)                                     # call Graphics::DrawString

    # 4. Restore Yellow color (0x00FFFF00)
    a.raw(b"\x68\x00\xFF\xFF\x00")                      # push 0x00FFFF00
    a.raw(b"\x8D\x4D\xD8")                              # lea ecx, [ebp - 0x28]
    a.call(fn_color_ctor)                               # call Color::Color(0x00FFFF00)
    a.raw(b"\x8D\x4D\xD8\x51\x89\xF1")                  # lea ecx, [ebp-0x28]; push ecx; mov ecx, esi
    a.call(fn_set_color)                                # call Graphics::SetColor

    a.raw(b"\x5B")                                      # pop ebx

    # Resume engine execution after DrawString:
    a.b[jle_back_d + 1] = len(a.b) - (jle_back_d + 2)
    a.b[jz_back_d_null + 1] = len(a.b) - (jz_back_d_null + 2)
    a.jmp(site_d + 5)                                   # jmp 0x004393DE
    emit(a)

    # Cave E: Zero-initialize SpriteMgr mWaveIdx[0..2] to prevent heap-garbage crash in Gauntlet Play (0x004027A8)
    if site_e:
        a = Asm(); va_cave_e = a.start
        a.raw(b"\x89\x5E\x74")                               # mov [esi + 0x74], ebx (mNumWaves = 0)
        a.raw(b"\x89\x5E\x78")                               # mov [esi + 0x78], ebx (mWaveIdx[0] = 0)
        a.raw(b"\x89\x5E\x7C")                               # mov [esi + 0x7C], ebx (mWaveIdx[1] = 0)
        a.raw(b"\x89\x9E\x80\x00\x00\x00")                   # mov [esi + 0x80], ebx (mWaveIdx[2] = 0)
        a.raw(b"\x88\x5E\x59")                               # mov byte ptr [esi + 0x59], bl (displaced)
        a.jmp(site_e + 6)                                    # jmp back to 0x0040DEFA
        emit(a)

    # Cave F: Hook for Random Level (call 0x419170 @ site_random)
    a = Asm(); va_cave_f = a.start
    a.raw(b"\x83\x3D" + struct.pack("<I", va_iat_pick_random) + b"\x00") # cmp dword ptr [va_iat_pick_random], 0
    jz_vanilla_f = len(a.b); a.raw(b"\x74\x00")
    a.raw(b"\x68" + struct.pack("<I", fn_get_level_desc))       # push fn_get_level_desc (arg 7)
    a.raw(b"\x68" + struct.pack("<I", fn_random_vanilla))      # push fn_random_vanilla (arg 6)
    a.raw(b"\xFF\x74\x24\x18")                                  # push dword ptr [esp + 24] (recent_list)
    a.raw(b"\xFF\x74\x24\x18")                                  # push dword ptr [esp + 24] (UserProfile*)
    a.raw(b"\xFF\x74\x24\x18")                                  # push dword ptr [esp + 24] (level_num)
    a.raw(b"\xFF\x74\x24\x18")                                  # push dword ptr [esp + 24] (difficulty)
    a.raw(b"\x51")                                              # push ecx (LevelMgr*)
    a.raw(b"\xFF\x15" + struct.pack("<I", va_iat_pick_random))  # call [zcm_PickRandomLevel]
    a.raw(b"\x83\xC4\x1C")                                      # add esp, 28
    a.raw(b"\xC2\x10\x00")                                      # ret 16
    # vanilla_f:
    a.b[jz_vanilla_f + 1] = len(a.b) - (jz_vanilla_f + 2)
    a.jmp(fn_random_vanilla)
    emit(a)

    # Cave G: Hook for Gauntlet Progression Level (call 0x418f50 @ site_gauntlet)
    a = Asm(); va_cave_g = a.start
    a.raw(b"\x83\x3D" + struct.pack("<I", va_iat_pick_gauntlet) + b"\x00") # cmp dword ptr [va_iat_pick_gauntlet], 0
    jz_vanilla_g = len(a.b); a.raw(b"\x74\x00")
    a.raw(b"\x68" + struct.pack("<I", fn_get_level_desc))       # push fn_get_level_desc (arg 6)
    a.raw(b"\x68" + struct.pack("<I", fn_gauntlet_vanilla))    # push fn_gauntlet_vanilla (arg 5)
    a.raw(b"\xFF\x74\x24\x14")                                  # push dword ptr [esp + 20] (UserProfile*)
    a.raw(b"\xFF\x74\x24\x14")                                  # push dword ptr [esp + 20] (level_num)
    a.raw(b"\xFF\x74\x24\x14")                                  # push dword ptr [esp + 20] (difficulty)
    a.raw(b"\x51")                                              # push ecx (LevelMgr*)
    a.raw(b"\xFF\x15" + struct.pack("<I", va_iat_pick_gauntlet))# call [zcm_PickGauntletLevel]
    a.raw(b"\x83\xC4\x18")                                      # add esp, 24
    a.raw(b"\xC2\x0C\x00")                                      # ret 12
    # vanilla_g:
    a.b[jz_vanilla_g + 1] = len(a.b) - (jz_vanilla_g + 2)
    a.jmp(fn_gauntlet_vanilla)
    emit(a)

    print(f"[OK] Section .zcm built ({len(blob)} bytes at RVA 0x{new_rva:08X}, VA 0x{new_va:08X})")

    # 5. Add section .zcm to PE
    pe.add_section('.zcm', bytes(blob), 0x60000020)

    # 6. Apply Patches at Sites
    # Patch Site A: jmp va_cave_a + nop
    off_a = pe.rva_to_offset(site_a - pe.image_base)
    rel_a = va_cave_a - (site_a + 5)
    patch_a = b"\xE9" + struct.pack("<i", rel_a) + b"\x90"
    assert len(patch_a) == 6
    pe.data[off_a : off_a + 6] = patch_a
    print(f"[PATCH] Site A @ 0x{site_a:08X} -> jmp 0x{va_cave_a:08X} + nop")

    # Patch Site B: call va_cave_b
    off_b = pe.rva_to_offset(site_b - pe.image_base)
    rel_b = va_cave_b - (site_b + 5)
    patch_b = b"\xE8" + struct.pack("<i", rel_b)
    assert len(patch_b) == 5
    pe.data[off_b : off_b + 5] = patch_b
    print(f"[PATCH] Site B @ 0x{site_b:08X} -> call 0x{va_cave_b:08X}")

    # Patch Site C: jmp va_cave_c (5 bytes)
    off_c = pe.rva_to_offset(site_c - pe.image_base)
    rel_c = va_cave_c - (site_c + 5)
    patch_c = b"\xE9" + struct.pack("<i", rel_c)
    assert len(patch_c) == 5
    pe.data[off_c : off_c + 5] = patch_c
    print(f"[PATCH] Site C @ 0x{site_c:08X} -> jmp 0x{va_cave_c:08X}")

    # Patch Site D: jmp va_cave_d
    off_d = pe.rva_to_offset(site_d - pe.image_base)
    rel_d = va_cave_d - (site_d + 5)
    patch_d = b"\xE9" + struct.pack("<i", rel_d)
    assert len(patch_d) == 5
    pe.data[off_d : off_d + 5] = patch_d
    print(f"[PATCH] Site D @ 0x{site_d:08X} -> jmp 0x{va_cave_d:08X}")

    # Patch Site E: jmp va_cave_e + nop
    if site_e:
        off_e = pe.rva_to_offset(site_e - pe.image_base)
        rel_e = va_cave_e - (site_e + 5)
        patch_e = b"\xE9" + struct.pack("<i", rel_e) + b"\x90"
        assert len(patch_e) == 6
        pe.data[off_e : off_e + 6] = patch_e
        print(f"[PATCH] Site E @ 0x{site_e:08X} -> jmp 0x{va_cave_e:08X} + nop")

    # Patch Site Random: call va_cave_f (5 bytes)
    off_rnd = pe.rva_to_offset(site_random - pe.image_base)
    rel_rnd = va_cave_f - (site_random + 5)
    patch_rnd = b"\xE8" + struct.pack("<i", rel_rnd)
    assert len(patch_rnd) == 5
    pe.data[off_rnd : off_rnd + 5] = patch_rnd
    print(f"[PATCH] Site Random @ 0x{site_random:08X} -> call 0x{va_cave_f:08X}")

    # Patch Site Gauntlet: call va_cave_g (5 bytes)
    off_gaunt = pe.rva_to_offset(site_gauntlet - pe.image_base)
    rel_gaunt = va_cave_g - (site_gauntlet + 5)
    patch_gaunt = b"\xE8" + struct.pack("<i", rel_gaunt)
    assert len(patch_gaunt) == 5
    pe.data[off_gaunt : off_gaunt + 5] = patch_gaunt
    print(f"[PATCH] Site Gauntlet @ 0x{site_gauntlet:08X} -> call 0x{va_cave_g:08X}")

    # 7. Save output
    pe.save(dst_path)
    print(f"[SUCCESS] Custom Map Gauntlet support successfully installed into {dst_path}")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit("Usage: python patch_custommap.py <Zuma_in.exe> <Zuma_out.exe>")
    patch_custommap(sys.argv[1], sys.argv[2])
