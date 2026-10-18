"""patch_zccredit.py - Injects exclusive developer credits and interactive button into Zuma Deluxe.

Section: .zcc (0x60000020: CODE | EXECUTE | READ)
Sidecar: zumacplxcredit.dll
Hook Sites:
  - Site A (Credits Table): STAGE1_CREDITS_TABLE (0x00430A0F) -> Cave A (call ZCC_GetCreditsTable)
  - Site B (Credits Draw Hook): STAGE1_CREDITS_DRAW_HOOK (0x00430A4A) -> Cave B (dispatch ZCC_PreDrawCreditLine)
  - Site C (Credits Click Hook): STAGE1_CREDITS_CLICK_HOOK (0x00477560) -> Cave C (dispatch ZCC_OnMouseDown)

Zero hardcoded addresses:
  - All hook sites resolved dynamically via AOB signatures + Capstone disassembly.
  - Graphics::SetFont resolved dynamically from call operand at Site B.
  - IAT slots resolved dynamically via PE import directory in .zdata.
"""

import sys
import os
import struct
import capstone
import pefile

sys.path.append(os.path.dirname(__file__))
from pe_util import PEFile, align

ZCC_CHARS = 0x60000020

# AOB Signatures for Dynamic Site Resolution (Zero hardcoded addresses)
AOB_CREDITS_TABLE = bytes.fromhex("8B7F5C897DEC895DE40F8E") # followed by disp32 and B8 (mov eax, table)
AOB_CREDITS_DRAW  = bytes.fromhex("8B4D0856E8")             # mov ecx, [ebp+8]; push esi; call SetFont
AOB_CREDITS_CLICK = bytes.fromhex("C6411F01C20C00909090909090909090") # mov byte ptr [ecx+1Fh], 1; ret 0Ch; 9x nop

def resolve_zccredit_sites(pe_raw):
    p = pefile.PE(data=pe_raw)
    image_base = p.OPTIONAL_HEADER.ImageBase

    text_sec = next((s for s in p.sections if s.Name.rstrip(b'\x00') == b'.text'), None)
    if not text_sec:
        raise RuntimeError(".text section not found in PE")

    text_data = text_sec.get_data()
    text_va = image_base + text_sec.VirtualAddress
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    # 1. Resolve Site A: STAGE1_CREDITS_TABLE
    idx_a = text_data.find(AOB_CREDITS_TABLE)
    if idx_a == -1:
        # Check if already patched with call (0xE8)
        idx_a2 = text_data.find(bytes.fromhex("8B7F5C897DEC895DE4"))
        if idx_a2 != -1:
            site_a = text_va + idx_a2 + 15
        else:
            raise RuntimeError("Could not locate Site A (Credits Table) signature")
    else:
        site_a = text_va + idx_a + 15

    # Verify Site A with Capstone: must be 5 bytes (mov eax, imm32 or call)
    off_a = site_a - text_va
    insns_a = list(md.disasm(text_data[off_a : off_a + 8], site_a))
    if not insns_a:
        raise RuntimeError(f"Failed to disassemble Site A at 0x{site_a:08X}")
    table_orig_val = insns_a[0].operands[1].imm if insns_a[0].mnemonic == 'mov' else 0

    # 2. Resolve Site B: STAGE1_CREDITS_DRAW_HOOK
    # Find mov ecx, [ebp+8]; push esi; call SetFont; mov edx, [ebp-0x10]
    draw_anchor = bytes.fromhex("8B4D0856E8")
    matches_b = []
    pos = 0
    while True:
        pos = text_data.find(draw_anchor, pos)
        if pos == -1:
            break
        # Verify succeeding bytes: after 5-byte instruction (mov, push, call = 9 bytes), next is 8B 55 F0
        if pos + 12 <= len(text_data):
            if text_data[pos + 9 : pos + 12] == bytes.fromhex("8B55F0"):
                matches_b.append(text_va + pos)
        pos += 1

    if len(matches_b) != 1:
        raise RuntimeError(f"Expected exactly 1 match for Site B (Credits Draw), got {len(matches_b)}")
    site_b = matches_b[0]

    # Disassemble Site B to extract Graphics::SetFont destination
    off_b = site_b - text_va
    insns_b = list(md.disasm(text_data[off_b : off_b + 16], site_b))
    if len(insns_b) < 3 or insns_b[2].mnemonic != 'call':
        raise RuntimeError(f"Site B at 0x{site_b:08X} does not match expected call Graphics::SetFont")
    set_font_va = insns_b[2].operands[0].imm

    # 3. Resolve Site C: STAGE1_CREDITS_CLICK_HOOK
    idx_c = text_data.find(AOB_CREDITS_CLICK)
    if idx_c == -1:
        # Check if already patched with jmp (0xE9)
        idx_c2 = text_data.find(bytes.fromhex("C6411F00C20800"))
        if idx_c2 != -1 and idx_c2 >= 16:
            site_c = text_va + idx_c2 - 16
        else:
            raise RuntimeError("Could not locate Site C (Credits Click) signature")
    else:
        site_c = text_va + idx_c

    return {
        'site_a': site_a,
        'table_orig': table_orig_val,
        'site_b': site_b,
        'set_font_va': set_font_va,
        'site_c': site_c
    }

def find_zcc_iat_entries(pe_raw):
    pe = PEFile(pe_raw)
    imps = pe.get_imports()
    zcc_dict = imps.get('zumacplxcredit.dll', {})
    target_funcs = {
        'ZCC_GetCreditsTable': zcc_dict.get('ZCC_GetCreditsTable'),
        'ZCC_PreDrawCreditLine': zcc_dict.get('ZCC_PreDrawCreditLine'),
        'ZCC_OnMouseDown': zcc_dict.get('ZCC_OnMouseDown')
    }

    missing = [k for k, v in target_funcs.items() if v is None]
    if missing:
        raise RuntimeError(f"Missing IAT imports from zumacplxcredit.dll: {missing}. Run patch_imports.py first!")

    return target_funcs

def patch_zccredit(src_path, dst_path):
    print(f"[INFO] Patching Developer Credits support from {src_path} -> {dst_path}")
    pe = PEFile(src_path)

    # Idempotency guard: if .zcc already exists, abort or skip
    if pe.find_section_by_name('.zcc'):
        print("[INFO] Section .zcc already present in executable.")
        if src_path != dst_path:
            pe.save(dst_path)
        return

    # 1. Resolve IAT slots from .zdata
    iat = find_zcc_iat_entries(bytes(pe.data))
    va_iat_get_table = iat['ZCC_GetCreditsTable']
    va_iat_draw      = iat['ZCC_PreDrawCreditLine']
    va_iat_click     = iat['ZCC_OnMouseDown']

    print(f"[OK] IAT ZCC_GetCreditsTable:   0x{va_iat_get_table:08X}")
    print(f"[OK] IAT ZCC_PreDrawCreditLine: 0x{va_iat_draw:08X}")
    print(f"[OK] IAT ZCC_OnMouseDown:       0x{va_iat_click:08X}")

    # 2. Resolve Sites dynamically via Capstone
    sites = resolve_zccredit_sites(bytes(pe.data))
    site_a = sites['site_a']
    site_b = sites['site_b']
    set_font_va = sites['set_font_va']
    site_c = sites['site_c']

    print(f"[OK] Site A (Credits Table): 0x{site_a:08X}")
    print(f"[OK] Site B (Credits Draw):  0x{site_b:08X} (SetFont: 0x{set_font_va:08X})")
    print(f"[OK] Site C (Credits Click): 0x{site_c:08X}")

    # 3. Determine new section RVA & VA
    last_sec = max(pe.sections, key=lambda s: s.virtual_address)
    new_rva = align(last_sec.virtual_address + max(last_sec.virtual_size, last_sec.raw_size), pe.section_alignment)
    new_va = pe.image_base + new_rva

    # 4. Construct .zcc content
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

    # Cave A: va_cave_a (Hooked from Site A @ 0x00430A0F)
    # Calls ZCC_GetCreditsTable via IAT, returning &g_extended_credits[0].text in eax
    # If IAT slot is NULL (zumacplxcredit.dll missing), falls back to vanilla credits table (0x00503A08)
    orig_table = sites['table_orig'] if sites['table_orig'] != 0 else 0x00503A08
    a = Asm(); va_cave_a = a.start
    call_get_table_block = bytearray()
    call_get_table_block += b"\xFF\x15" + struct.pack("<I", va_iat_get_table) # call [ZCC_GetCreditsTable]
    call_get_table_block += b"\xC3"                                            # ret
    # cmp dword ptr [va_iat_get_table], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", va_iat_get_table) + b"\x00")
    # jz fallback (skip over call and ret: 6 + 1 = 7 bytes)
    a.raw(b"\x74" + bytes([len(call_get_table_block)]))
    a.raw(call_get_table_block)
    # fallback:
    a.raw(b"\xB8" + struct.pack("<I", orig_table))            # mov eax, orig_table (0x00503A08)
    a.raw(b"\xC3")                                            # ret
    emit(a)

    # Cave B: va_cave_b (Hooked from Site B @ 0x00430A4A)
    # Passes this, g, font, text, lineY to ZCC_PreDrawCreditLine, then calls Graphics::SetFont
    # If IAT slot is NULL, skips ZCC_PreDrawCreditLine and executes vanilla SetFont
    a = Asm(); va_cave_b = a.start
    call_draw_block = bytearray()
    call_draw_block += b"\x50\x51\x52"                               # push eax ; push ecx ; push edx
    call_draw_block += b"\xFF\x75\xEC"                               # push dword ptr [ebp - 0x14] (lineY)
    call_draw_block += b"\x8B\x55\xF0"                               # mov edx, dword ptr [ebp - 0x10] (&entry->text)
    call_draw_block += b"\xFF\x32"                                   # push dword ptr [edx]        (const char *text)
    call_draw_block += b"\x56"                                       # push esi                    (Font *font)
    call_draw_block += b"\xFF\x75\x08"                               # push dword ptr [ebp + 8]    (Graphics *g)
    call_draw_block += b"\xFF\x75\xE8"                               # push dword ptr [ebp - 0x18] (CreditsScreen *this)
    call_draw_block += b"\xFF\x15" + struct.pack("<I", va_iat_draw)  # call [ZCC_PreDrawCreditLine]
    call_draw_block += b"\x83\xC4\x14"                               # add esp, 20
    call_draw_block += b"\x5A\x59\x58"                               # pop edx ; pop ecx ; pop eax

    # cmp dword ptr [va_iat_draw], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", va_iat_draw) + b"\x00")
    # jz skip_draw
    a.raw(b"\x74" + bytes([len(call_draw_block)]))
    a.raw(call_draw_block)
    # skip_draw / continue vanilla:
    a.raw(b"\x8B\x4D\x08")                               # mov ecx, dword ptr [ebp + 8]
    a.raw(b"\x56")                                       # push esi
    a.call(set_font_va)                                  # call Graphics::SetFont
    a.jmp(site_b + 9)                                    # jmp back to 0x00430A53
    emit(a)

    # Cave C: va_cave_c (Hooked from Site C @ 0x00477560)
    # Intercepts Widget::MouseDown, dispatches to ZCC_OnMouseDown
    # If IAT slot is NULL, branches directly to vanilla (mov byte ptr [ecx + 0x1f], 1; ret 0xc)
    a = Asm(); va_cave_c = a.start
    dispatch_click_block = bytearray()
    dispatch_click_block += b"\xFF\x74\x24\x08"                           # push dword ptr [esp + 8] (y)
    dispatch_click_block += b"\xFF\x74\x24\x08"                           # push dword ptr [esp + 8] (x, was [esp+4])
    dispatch_click_block += b"\x51"                                       # push ecx                 (this)
    dispatch_click_block += b"\xFF\x15" + struct.pack("<I", va_iat_click) # call [ZCC_OnMouseDown]
    dispatch_click_block += b"\x83\xC4\x0C"                               # add esp, 12
    dispatch_click_block += b"\x85\xC0"                                   # test eax, eax
    dispatch_click_block += b"\x74\x03"                                   # jz vanilla_default (skip ret 0xc)
    dispatch_click_block += b"\xC2\x0C\x00"                               # ret 0xc (handled)

    # cmp dword ptr [va_iat_click], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", va_iat_click) + b"\x00")
    # jz vanilla_default
    a.raw(b"\x74" + bytes([len(dispatch_click_block)]))
    a.raw(dispatch_click_block)
    # vanilla_default:
    a.raw(b"\xC6\x41\x1F\x01")                           # mov byte ptr [ecx + 0x1f], 1
    a.raw(b"\xC2\x0C\x00")                               # ret 0xc
    emit(a)

    print(f"[OK] Section .zcc built ({len(blob)} bytes at RVA 0x{new_rva:08X}, VA 0x{new_va:08X})")

    # 5. Add section .zcc to PE
    pe.add_section('.zcc', bytes(blob), ZCC_CHARS)

    # 6. Apply Patches at the 3 Sites
    # Patch Site A: call va_cave_a (5 bytes)
    off_a = pe.rva_to_offset(site_a - pe.image_base)
    rel_a = va_cave_a - (site_a + 5)
    patch_a = b"\xE8" + struct.pack("<i", rel_a)
    assert len(patch_a) == 5
    pe.data[off_a : off_a + 5] = patch_a
    print(f"[PATCH] Site A @ 0x{site_a:08X} -> call 0x{va_cave_a:08X}")

    # Patch Site B: jmp va_cave_b + 4 NOPs (9 bytes)
    off_b = pe.rva_to_offset(site_b - pe.image_base)
    rel_b = va_cave_b - (site_b + 5)
    patch_b = b"\xE9" + struct.pack("<i", rel_b) + b"\x90\x90\x90\x90"
    assert len(patch_b) == 9
    pe.data[off_b : off_b + 9] = patch_b
    print(f"[PATCH] Site B @ 0x{site_b:08X} -> jmp 0x{va_cave_b:08X} + 4 nops")

    # Patch Site C: jmp va_cave_c + ret 0xc + 8 NOPs (16 bytes)
    off_c = pe.rva_to_offset(site_c - pe.image_base)
    rel_c = va_cave_c - (site_c + 5)
    patch_c = b"\xE9" + struct.pack("<i", rel_c) + b"\xC2\x0C\x00" + b"\x90" * 8
    assert len(patch_c) == 16
    pe.data[off_c : off_c + 16] = patch_c
    print(f"[PATCH] Site C @ 0x{site_c:08X} -> jmp 0x{va_cave_c:08X} + ret 0xc + 8 nops")

    # 7. Save output PE
    pe.save(dst_path)
    print(f"[SUCCESS] Developer Credits support successfully installed into {dst_path}")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit("Usage: python patch_zccredit.py <Zuma_in.exe> <Zuma_out.exe>")
    patch_zccredit(sys.argv[1], sys.argv[2])
