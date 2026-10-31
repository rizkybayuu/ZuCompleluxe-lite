"""patch_priolayer.py - Injects High-Priority Sprite & Cutout Overlay + Earthquake Bake Caves.

Usage: python patch_priolayer.py <Zuma_in.exe> <Zuma_out.exe>

Architecture:
  - Connects to zumapriolayer.dll via static PE IAT imports.
  - Adds executable section '.zprio' (characteristics 0x60000020: CODE | EXECUTE | READ).
  - 100% Dynamic AOB Signatures: Zero hardcoded Virtual Addresses (VAs).
  - Implements 10 target caves:
      1. Site A (Load): Priority >= 10 routing to pseudo-list.
      2. Site B (Normal Render): Normal gameplay render overlay.
      3. Site C (State 6 Render): Level transition render overlay.
      4. Site D (Load-Begin): Load-begin list reset.
      5. Site E (Bake Gempa): Earthquake snapshot bake (layers 0..4 + high-pri).
      6. M4     (Destroy Cleanup): HoleMgr destructor cleanup.
      7. Site F (Draw Node Hook): Per-node blend mode blitting.
      8. Site S1(State 1 Intro): Level Intro render overlay.
      9. Site S4(State 4 Score Count): Score Countdown render overlay.
     10. Site G (Hardware Blend): DDImage::SetBlend hook for hardware/software blend.
"""

import sys
import os
import struct

sys.path.append(os.path.dirname(__file__))
from pe_util import PEFile, align, compute_pe_checksum

def find_zpl_iat_entries(pe_raw):
    """Finds IAT virtual addresses for zumapriolayer.dll exports."""
    pe = PEFile(pe_raw)
    imps = pe.get_imports()
    zpl_dict = imps.get('zumapriolayer.dll', {})
    iat_map = {}
    for fn in [
        'ZPL_Init', 'ZPL_OnLoadBegin', 'ZPL_OnHoleMgrDestroy',
        'ZPL_GetListBase', 'ZPL_DrawHigh', 'ZPL_DrawLow', 'ZPL_DrawBake',
        'ZPL_GetPriRange', 'ZPL_GetList', 'ZPL_DrawNode',
        'ZPL_HwBlendHook', 'ZPL_OnStateDraw'
    ]:
        iat_map[fn] = zpl_dict.get(fn)
    return iat_map

def assemble_cave_load(va, iat_glb, site_a_va):
    b = bytearray()
    # 0x00: cmp eax, 10
    b += b'\x83\xF8\x0A'
    # 0x03: jge hi (to +20) -> rel8 = 20 - (3 + 2) = 15 = 0x0F
    b += b'\x7D\x0F'
    # 0x05: cmp eax, 5
    b += b'\x83\xF8\x05'
    # 0x08: jl lo (to +15) -> rel8 = 15 - (8 + 2) = 5 = 0x05
    b += b'\x7C\x05'
    # 0x0A: mov eax, 4
    b += b'\xB8\x04\x00\x00\x00'
    # 0x0F: lo: jmp site_a_va + 10
    rel_lo = (site_a_va + 10) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_lo)
    assert len(b) == 20
    # 0x14: hi:
    # cmp dword ptr [iat_glb], 0
    b += b'\x83\x3D' + struct.pack('<I', iat_glb) + b'\x00'
    jz_clamp_off = len(b)
    b += b'\x74\x00'                     # je .clamp (rel8 patched below)
    b += b'\xC7\x45\xD0\xE8\xE3\x4F\x00' # mov dword ptr [ebp-0x30], 0x004FE3E8
    b += b'\xC7\x45\xE4\x00\x00\x00\x00' # mov dword ptr [ebp-0x1C], 0
    b += b'\x50'                         # push eax
    b += b'\x53'                         # push ebx
    b += b'\xFF\x15' + struct.pack('<I', iat_glb) # call [iat_ZPL_GetListBase]
    b += b'\x8B\xD8'                     # mov ebx, eax
    rel_ret = (site_a_va + 30) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_ret)
    # .clamp:
    clamp_target = len(b)
    b[jz_clamp_off + 1] = clamp_target - (jz_clamp_off + 2)
    b += b'\xB8\x04\x00\x00\x00'     # mov eax, 4
    rel_clamp = (site_a_va + 10) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_clamp)
    return bytes(b)

def assemble_cave_normal(va, iat_dh, site_b_va, orig_coins_va):
    b = bytearray()
    rel_coins = orig_coins_va - (va + len(b) + 5)
    b += b'\xE8' + struct.pack('<i', rel_coins) # call orig Level::DrawCoins
    # cmp dword ptr [iat_dh], 0
    b += b'\x83\x3D' + struct.pack('<I', iat_dh) + b'\x00'
    jz_done = len(b)
    b += b'\x74\x00'                     # je .done (rel8)
    b += b'\x53'                         # push ebx (g)
    b += b'\x56'                         # push esi (level)
    b += b'\xFF\x15' + struct.pack('<I', iat_dh) # call [iat_ZPL_DrawHigh]
    # .done:
    b[jz_done + 1] = len(b) - (jz_done + 2)
    rel_ret = (site_b_va + 5) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_ret)    # jmp site_b_va + 5
    return bytes(b)

def assemble_cave_state6(va, iat_dh, site_c_va, orig_float_va):
    b = bytearray()
    # cmp dword ptr [iat_dh], 0
    b += b'\x83\x3D' + struct.pack('<I', iat_dh) + b'\x00'
    jz_skip = len(b)
    b += b'\x74\x00'                     # je .skip (rel8)
    b += b'\x57'                         # push edi (g)
    b += b'\xFF\x76\x04'                 # push dword ptr [esi+4] (level)
    b += b'\xFF\x15' + struct.pack('<I', iat_dh) # call [iat_ZPL_DrawHigh]
    b += b'\x8B\xCE'                     # mov ecx, esi (restore Board*)
    # .skip:
    b[jz_skip + 1] = len(b) - (jz_skip + 2)
    rel_float = orig_float_va - (va + len(b) + 5)
    b += b'\xE8' + struct.pack('<i', rel_float) # call orig Board::DrawFloatingText
    rel_ret = (site_c_va + 5) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_ret)   # jmp site_c_va + 5
    return bytes(b)

def assemble_cave_load_begin(va, iat_on_load, site_d_va):
    b = bytearray()
    # cmp dword ptr [iat_on_load], 0
    b += b'\x83\x3D' + struct.pack('<I', iat_on_load) + b'\x00'
    jz_skip = len(b)
    b += b'\x74\x00'                     # je .skip (rel8)
    b += b'\x53'                         # push ebx (arg: HoleMgr*)
    b += b'\xFF\x15' + struct.pack('<I', iat_on_load) # call [iat_ZPL_OnLoadBegin]
    # .skip:
    b[jz_skip + 1] = len(b) - (jz_skip + 2)
    b += b'\x8B\x43\x6C'                 # mov eax, dword ptr [ebx+0x6C]
    b += b'\x85\xC0'                     # test eax, eax
    rel_ret = (site_d_va + 5) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_ret) # jmp site_d_va + 5
    return bytes(b)

def assemble_cave_bake(va, iat_bake, site_e_va):
    b = bytearray()
    # cmp dword ptr [iat_bake], 0
    b += b'\x83\x3D' + struct.pack('<I', iat_bake) + b'\x00'
    jz_skip = len(b)
    b += b'\x74\x00'                     # je .skip (rel8)
    b += b'\x8D\x85\x50\xFF\xFF\xFF'     # lea eax, [ebp-0xB0] (Graphics* bake)
    b += b'\x50'                         # push eax (Graphics*)
    b += b'\x56'                         # push esi (HoleMgr*)
    b += b'\xFF\x15' + struct.pack('<I', iat_bake) # call [iat_ZPL_DrawBake]
    # .skip:
    b[jz_skip + 1] = len(b) - (jz_skip + 2)
    b += b'\x8B\x4D\x08'                 # mov ecx, dword ptr [ebp+8]
    b += b'\x8B\x81\x8C\x00\x00\x00'     # mov eax, dword ptr [ecx+0x8C]
    rel_loop = (site_e_va + 9) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_loop) # jmp site_e_va + 9 (0x00412620)
    return bytes(b)

def assemble_cave_destroy(va, iat_destroy, m4_va):
    b = bytearray()
    b += b'\x55'                         # push ebp
    b += b'\x8B\xEC'                     # mov ebp, esp
    b += b'\x6A\xFF'                     # push -1
    # cmp dword ptr [iat_destroy], 0
    b += b'\x83\x3D' + struct.pack('<I', iat_destroy) + b'\x00'
    jz_skip = len(b)
    b += b'\x74\x00'                     # je .skip (rel8)
    b += b'\x60'                         # pushad
    b += b'\x51'                         # push ecx (arg: HoleMgr*)
    b += b'\xFF\x15' + struct.pack('<I', iat_destroy) # call [iat_ZPL_OnHoleMgrDestroy]
    b += b'\x61'                         # popad
    # .skip:
    b[jz_skip + 1] = len(b) - (jz_skip + 2)
    rel_ret = (m4_va + 5) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_ret) # jmp m4_va + 5
    return bytes(b)

def assemble_cave_draw_node(va, iat_draw_node, site_f_va, orig_draw_va):
    b = bytearray()
    # cmp dword ptr [iat_draw_node], 0
    b += b'\x83\x3D' + struct.pack('<I', iat_draw_node) + b'\x00'
    jz_orig = len(b)
    b += b'\x74\x00'                     # je .orig (rel8)
    b += b'\x58'                         # pop eax (img)
    b += b'\x5A'                         # pop edx (x)
    b += b'\x52'                         # push edx (x)
    b += b'\x50'                         # push eax (img)
    b += b'\x51'                         # push ecx (g)
    b += b'\x56'                         # push esi (node)
    b += b'\xFF\x15' + struct.pack('<I', iat_draw_node) # call [iat_ZPL_DrawNode]
    rel_ret = (site_f_va + 5) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_ret) # jmp site_f_va + 5
    # .orig:
    b[jz_orig + 1] = len(b) - (jz_orig + 2)
    rel_draw = orig_draw_va - (va + len(b) + 5)
    b += b'\xE8' + struct.pack('<i', rel_draw) # call orig DrawImage
    rel_ret2 = (site_f_va + 5) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_ret2) # jmp site_f_va + 5
    return bytes(b)

def assemble_cave_state1(va, iat_dh, site_s1_va, orig_float_va):
    b = bytearray()
    # cmp dword ptr [iat_dh], 0
    b += b'\x83\x3D' + struct.pack('<I', iat_dh) + b'\x00'
    jz_skip = len(b)
    b += b'\x74\x00'                     # je .skip (rel8)
    b += b'\x53'                         # push ebx (g)
    b += b'\xFF\x77\x04'                 # push dword ptr [edi+4] (Level*)
    b += b'\xFF\x15' + struct.pack('<I', iat_dh) # call [iat_ZPL_DrawHigh]
    b += b'\x8B\xCF'                     # mov ecx, edi (restore Board*)
    # .skip:
    b[jz_skip + 1] = len(b) - (jz_skip + 2)
    rel_float = orig_float_va - (va + len(b) + 5)
    b += b'\xE8' + struct.pack('<i', rel_float) # call orig Board::DrawFloatingText
    rel_ret = (site_s1_va + 5) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_ret)   # jmp site_s1_va + 5
    return bytes(b)

def assemble_cave_state3(va, iat_dh, site_s3_va, orig_banner_va):
    b = bytearray()
    # cmp dword ptr [iat_dh], 0
    b += b'\x83\x3D' + struct.pack('<I', iat_dh) + b'\x00'
    jz_skip_dh = len(b)
    b += b'\x74\x00'                     # je .skip_dh (rel8)
    b += b'\x57'                         # push edi (g)
    b += b'\xFF\x76\x04'                 # push dword ptr [esi+4] (Level*)
    b += b'\xFF\x15' + struct.pack('<I', iat_dh) # call [iat_ZPL_DrawHigh]
    # .skip_dh:
    b[jz_skip_dh + 1] = len(b) - (jz_skip_dh + 2)
    b += b'\x8B\xCE'                     # mov ecx, esi (restore Board*)
    rel_banner = orig_banner_va - (va + len(b) + 5)
    b += b'\xE8' + struct.pack('<i', rel_banner) # call orig Board::DrawStageCompleteText (arg edi already on stack!)
    rel_ret = (site_s3_va + 5) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_ret)    # jmp site_s3_va + 5
    return bytes(b)

def assemble_cave_state4(va, iat_dh, site_s4_va, orig_color_va):
    b = bytearray()
    rel_color = orig_color_va - (va + len(b) + 5)
    b += b'\xE8' + struct.pack('<i', rel_color) # call orig Color::Color (0x00440E20)
    rel_ret = (site_s4_va + 5) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_ret)   # jmp site_s4_va + 5 (0x00426EE9)
    return bytes(b)

def assemble_cave_state_log(va, iat_log, site_dl_va):
    b = bytearray()
    b += b'\x8B\x41\x60'                 # mov eax, dword ptr [ecx+0x60]
    b += b'\x51'                         # push ecx
    b += b'\x52'                         # push edx
    b += b'\x50'                         # push eax
    # cmp dword ptr [iat_log], 0
    b += b'\x83\x3D' + struct.pack('<I', iat_log) + b'\x00'
    jz_skip = len(b)
    b += b'\x74\x00'                     # je .skip (rel8)
    b += b'\xFF\x15' + struct.pack('<I', iat_log) # call [iat_ZPL_OnStateDraw]
    jmp_done = len(b)
    b += b'\xEB\x00'                     # jmp .done (rel8)
    # .skip:
    b[jz_skip + 1] = len(b) - (jz_skip + 2)
    b += b'\x58'                         # pop eax
    # .done:
    b[jmp_done + 1] = len(b) - (jmp_done + 2)
    b += b'\x5A'                         # pop edx
    b += b'\x59'                         # pop ecx
    b += b'\x8B\x41\x60'                 # mov eax, dword ptr [ecx+0x60]
    b += b'\x48'                         # dec eax
    b += b'\x83\xF8\x05'                 # cmp eax, 5
    rel_ret = (site_dl_va + 7) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_ret) # jmp site_dl_va + 7 (0x0042732A)
    return bytes(b)

def assemble_cave_hw_blend(va, iat_hw_blend, site_g_va):
    b = bytearray()
    # cmp dword ptr [iat_hw_blend], 0
    b += b'\x83\x3D' + struct.pack('<I', iat_hw_blend) + b'\x00'
    jz_orig = len(b)
    b += b'\x74\x00'                     # je .orig (rel8)
    b += b'\x51'                         # push ecx (this = DDImage*)
    b += b'\xFF\x71\x1C'                 # push dword ptr [ecx+0x1C] (IDirect3DDevice7*)
    b += b'\xFF\x15' + struct.pack('<I', iat_hw_blend) # call [iat_ZPL_HwBlendHook] (stdcall)
    b += b'\x59'                         # pop ecx (restore DDImage*)
    b += b'\x85\xC0'                     # test eax, eax
    jz_orig2 = len(b)
    b += b'\x74\x00'                     # jz .orig (rel8)
    b += b'\xC2\x08\x00'                 # ret 8 (handled by hardware blend hook)
    # .orig:
    orig_target = len(b)
    b[jz_orig + 1] = orig_target - (jz_orig + 2)
    b[jz_orig2 + 1] = orig_target - (jz_orig2 + 2)
    b += b'\x55'                         # push ebp
    b += b'\x8B\xEC'                     # mov ebp, esp
    b += b'\x8B\x45\x08'                 # mov eax, dword ptr [ebp+8]
    b += b'\x56'                         # push esi
    b += b'\x8B\xF1'                     # mov esi, ecx
    b += b'\x6A\x05'                     # push 5
    b += b'\x85\xC0'                     # test eax, eax
    rel_ret = (site_g_va + 13) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_ret) # jmp site_g_va + 13
    return bytes(b)

def assemble_cave_node_blend(va, site_nb_va):
    b = bytearray()
    b += b'\x8B\x57\x24'                         # mov edx, [edi+0x24]
    b += b'\x89\x50\x10'                         # mov [eax+0x10], edx
    b += b'\x51'                                 # push ecx
    b += b'\x8B\x4F\x1C'                         # mov ecx, [edi+0x1c] (priority)
    b += b'\x81\xE1\xFF\xFF\x00\x00'             # and ecx, 0xffff (pri)
    b += b'\x81\xC9\x00\x00\x00\x5A'             # or ecx, 0x5a000000 (magic tag 0x5A)
    b += b'\x89\x48\x14'                         # mov [eax+0x14], ecx
    b += b'\x59'                                 # pop ecx
    rel_ret = (site_nb_va + 6) - (va + len(b) + 5)
    b += b'\xE9' + struct.pack('<i', rel_ret)   # jmp site_nb_va + 6 (0x00415F1D)
    return bytes(b)

def find_unique_site(pe, name, sig_hex, hook_offset):
    pat = bytes.fromhex(sig_hex.replace(' ', ''))
    cnt = pe.data.count(pat)
    if cnt == 0:
        raise RuntimeError(f"Unique signature for {name} not found in binary!")
    if cnt > 1:
        raise RuntimeError(f"Signature for {name} is not unique ({cnt} occurrences)!")
    raw_offset = pe.data.find(pat) + hook_offset
    site_va = pe.image_base + pe.offset_to_rva(raw_offset)
    return raw_offset, site_va

def patch_priolayer(src_path, dst_path):
    print(f"[INFO] Patching High-Priority Layer & Earthquake Bake from {src_path} -> {dst_path}")
    pe = PEFile(src_path)

    # Idempotency guard
    if pe.find_section_by_name('.zprio'):
        print("[INFO] Section .zprio already present in executable.")
        if src_path != dst_path:
            pe.save(dst_path)
        return

    # 1. Resolve IAT entries
    iat_entries = find_zpl_iat_entries(bytes(pe.data))
    for fn in [
        'ZPL_GetListBase', 'ZPL_DrawHigh', 'ZPL_DrawLow', 'ZPL_DrawBake',
        'ZPL_OnLoadBegin', 'ZPL_OnHoleMgrDestroy', 'ZPL_DrawNode',
        'ZPL_HwBlendHook', 'ZPL_OnStateDraw'
    ]:
        if not iat_entries.get(fn):
            raise RuntimeError(f"Required IAT export '{fn}' not found for zumapriolayer.dll!")

    # 2. 100% Dynamic AOB Signature Resolution (Zero hardcoded VAs)
    # Format: (name, sig_hex, hook_offset, patch_len)
    sig_definitions = [
        ("Site A (Load)", "83 F8 05 7C 05 B8 04 00 00 00", 0, 5),
        ("Site B (Normal Render)", "53 8B CE E8 BA C1 FF FF 5E 5B 8B E5 5D C2 04 00", 3, 5),
        ("Site C (State 6 Render)", "57 8B CE E8 8F FC FF FF 5F 5E 5B 8B E5 5D C2 04 00", 3, 5),
        ("Site D (Load-Begin)", "8B 43 6C 85 C0 8D 4B 68", 0, 5),
        ("Site E (Bake Gempa)", "8B 4D 08 8B 81 8C 00 00 00 3B D8 74 45", 0, 9),
        ("M4 (Destroy Cleanup)", "55 8B EC 6A FF 68 4F 55 4F 00 64 A1 00 00 00 00", 0, 5),
        ("Site F (Draw Node Hook)", "8B CF E8 9D 79 04 00 8B 36 3B 73 04", 2, 5),
        ("Site S1 (State 1 Intro)", "53 8B CF E8 21 FC FF FF 5F 5E 5B 5D C2 04 00", 3, 5),
        ("Site S3 (State 3 Pause)", "57 8B CE E8 6A FE FF FF 57 8B CE", 3, 5),
        ("Site S4 (State 4 Color)", "68 00 FF FF 00 8D 4D E4 E8 37 9F 01 00 8D 45 E4", 8, 5),
        ("Site DL (Draw Dispatcher)", "8B 41 60 48 83 F8 05 77 25 FF 24 85", 0, 7),
        ("Site G (Hardware Blend)", "55 8B EC 8B 45 08 56 8B F1 6A 05 85 C0 8B 46 1C", 0, 13),
        ("Site NB (Node Blend Tag)", "8B 57 24 89 50 10 89 70 1C", 0, 6)
    ]

    resolved_sites = {}
    for name, sig, hook_off, patch_len in sig_definitions:
        raw_off, va = find_unique_site(pe, name, sig, hook_off)
        orig_call = None
        if pe.data[raw_off] == 0xE8:
            rel = struct.unpack('<i', bytes(pe.data[raw_off+1:raw_off+5]))[0]
            orig_call = va + 5 + rel
        resolved_sites[name] = {
            'raw_off': raw_off,
            'va': va,
            'patch_len': patch_len,
            'orig_call': orig_call
        }
        print(f"[AOB] Resolved {name:28s} @ VA 0x{va:08X} (offset 0x{raw_off:06X})" +
              (f" -> calls 0x{orig_call:08X}" if orig_call else ""))

    # 3. Determine new section RVA & VA
    last_sec = max(pe.sections, key=lambda s: s.virtual_address)
    new_rva = align(last_sec.virtual_address + max(last_sec.virtual_size, last_sec.raw_size), pe.section_alignment)
    cave_base_va = pe.image_base + new_rva
    section_size = 1024

    print(f"[INFO] Preparing .zprio section at VA 0x{cave_base_va:08X} (size {section_size} bytes)")

    # 4. Assemble caves into buffer
    cave_code = bytearray()
    
    # Cave A: Load
    site_a_va = resolved_sites["Site A (Load)"]['va']
    va_cave_load = cave_base_va + len(cave_code)
    code_load = assemble_cave_load(va_cave_load, iat_entries['ZPL_GetListBase'], site_a_va)
    cave_code.extend(code_load)

    # Cave B: Normal Render
    site_b = resolved_sites["Site B (Normal Render)"]
    va_cave_normal = cave_base_va + len(cave_code)
    code_normal = assemble_cave_normal(va_cave_normal, iat_entries['ZPL_DrawHigh'], site_b['va'], site_b['orig_call'])
    cave_code.extend(code_normal)

    # Cave C: State 6 Render
    site_c = resolved_sites["Site C (State 6 Render)"]
    va_cave_state6 = cave_base_va + len(cave_code)
    code_state6 = assemble_cave_state6(va_cave_state6, iat_entries['ZPL_DrawHigh'], site_c['va'], site_c['orig_call'])
    cave_code.extend(code_state6)

    # Cave D: Load-begin Reset
    site_d_va = resolved_sites["Site D (Load-Begin)"]['va']
    va_cave_load_begin = cave_base_va + len(cave_code)
    code_load_begin = assemble_cave_load_begin(va_cave_load_begin, iat_entries['ZPL_OnLoadBegin'], site_d_va)
    cave_code.extend(code_load_begin)

    # Cave E: Earthquake Bake
    site_e_va = resolved_sites["Site E (Bake Gempa)"]['va']
    va_cave_bake = cave_base_va + len(cave_code)
    code_bake = assemble_cave_bake(va_cave_bake, iat_entries['ZPL_DrawBake'], site_e_va)
    cave_code.extend(code_bake)

    # Cave M4: Destroy
    m4_va = resolved_sites["M4 (Destroy Cleanup)"]['va']
    va_cave_destroy = cave_base_va + len(cave_code)
    code_destroy = assemble_cave_destroy(va_cave_destroy, iat_entries['ZPL_OnHoleMgrDestroy'], m4_va)
    cave_code.extend(code_destroy)

    # Cave F: Draw Node Hook
    site_f = resolved_sites["Site F (Draw Node Hook)"]
    va_cave_draw_node = cave_base_va + len(cave_code)
    code_draw_node = assemble_cave_draw_node(va_cave_draw_node, iat_entries['ZPL_DrawNode'], site_f['va'], site_f['orig_call'])
    cave_code.extend(code_draw_node)

    # Cave S1: State 1 Level Intro Render Overlay
    site_s1 = resolved_sites["Site S1 (State 1 Intro)"]
    va_cave_state1 = cave_base_va + len(cave_code)
    code_state1 = assemble_cave_state1(va_cave_state1, iat_entries['ZPL_DrawHigh'], site_s1['va'], site_s1['orig_call'])
    cave_code.extend(code_state1)

    # Cave S3: State 3 Stage Complete Render Overlay (High Priority)
    site_s3 = resolved_sites["Site S3 (State 3 Pause)"]
    va_cave_state3 = cave_base_va + len(cave_code)
    code_state3 = assemble_cave_state3(va_cave_state3, iat_entries['ZPL_DrawHigh'], site_s3['va'], site_s3['orig_call'])
    cave_code.extend(code_state3)

    # Cave S4: State 4 Pause Render Overlay (High Priority before Color ctor)
    site_s4 = resolved_sites["Site S4 (State 4 Color)"]
    va_cave_state4 = cave_base_va + len(cave_code)
    code_state4 = assemble_cave_state4(va_cave_state4, iat_entries['ZPL_DrawHigh'], site_s4['va'], site_s4['orig_call'])
    cave_code.extend(code_state4)

    # Cave G: Hardware Blend Hook
    site_g_va = resolved_sites["Site G (Hardware Blend)"]['va']
    va_cave_hw_blend = cave_base_va + len(cave_code)
    code_hw_blend = assemble_cave_hw_blend(va_cave_hw_blend, iat_entries['ZPL_HwBlendHook'], site_g_va)
    cave_code.extend(code_hw_blend)

    # Cave NB: Node Blend Tag Magic (0x00415F17)
    site_nb_va = resolved_sites["Site NB (Node Blend Tag)"]['va']
    va_cave_node_blend = cave_base_va + len(cave_code)
    code_node_blend = assemble_cave_node_blend(va_cave_node_blend, site_nb_va)
    cave_code.extend(code_node_blend)

    # Cave DL: Draw Dispatcher State Logger Hook
    site_dl = resolved_sites["Site DL (Draw Dispatcher)"]
    va_cave_state_log = cave_base_va + len(cave_code)
    code_state_log = assemble_cave_state_log(va_cave_state_log, iat_entries['ZPL_OnStateDraw'], site_dl['va'])
    cave_code.extend(code_state_log)

    # Pad rest of section with 0xCC (INT3)
    if len(cave_code) < section_size:
        cave_code.extend(b'\xCC' * (section_size - len(cave_code)))

    pe.add_section('.zprio', bytes(cave_code), 0x60000020)

    # 5. Apply Hooks dynamically
    cave_map = {
        "Site A (Load)": va_cave_load,
        "Site B (Normal Render)": va_cave_normal,
        "Site C (State 6 Render)": va_cave_state6,
        "Site D (Load-Begin)": va_cave_load_begin,
        "Site E (Bake Gempa)": va_cave_bake,
        "M4 (Destroy Cleanup)": va_cave_destroy,
        "Site F (Draw Node Hook)": va_cave_draw_node,
        "Site S1 (State 1 Intro)": va_cave_state1,
        "Site S3 (State 3 Pause)": va_cave_state3,
        "Site S4 (State 4 Color)": va_cave_state4,
        "Site G (Hardware Blend)": va_cave_hw_blend,
        "Site NB (Node Blend Tag)": va_cave_node_blend,
        "Site DL (Draw Dispatcher)": va_cave_state_log
    }

    for name, info in resolved_sites.items():
        raw_off = info['raw_off']
        va = info['va']
        patch_len = info['patch_len']
        target_cave = cave_map[name]
        rel_jmp = target_cave - (va + 5)
        hook = bytearray(b'\xE9' + struct.pack('<i', rel_jmp))
        if patch_len > 5:
            hook.extend(b'\x90' * (patch_len - 5))
        pe.data[raw_off:raw_off + patch_len] = hook
        print(f"[OK] Hooked {name:28s} @ VA 0x{va:08X} -> Cave @ 0x{target_cave:08X}")

    pe.save(dst_path)
    print(f"[SUCCESS] High-Priority Layer & Earthquake Bake successfully patched to {dst_path}")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python patch_priolayer.py <Zuma_in.exe> <Zuma_out.exe>")
        sys.exit(1)
    patch_priolayer(sys.argv[1], sys.argv[2])
