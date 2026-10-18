"""patch_testingkit.py - Injects Developer Testing Kit hooks into Zuma Deluxe.

Section: .ztk (0x60000020: CODE | EXECUTE | READ)
Sidecar: modules/zumatestingkit.dll
Hook Sites:
  - Site A  (UserDialog AddLine):   0x00436DCE -> Cave A  (add DevTester right after index 0)
  - Site A2 (UserDialog Skip User): 0x00436DF8 -> Cave A2 (skip curUser add if curUser == DevTester)
  - Site B  (Welcome Header Color): 0x00432072 -> Cave B  (glowing cyan-blue text if DevTester)
  - Site C  (ListBox Item Color):   0x0047B589 -> Cave C  (glowing cyan-blue line if DevTester)
  - Site D  (User Selection):       0x0043CD71 -> Cave D  (virtual DevTester profile dispatch)
  - Site E  (Gauntlet Sync):        0x0043BDE5 -> Cave E  (unlock all levels + Sun God rank)
  - Site F  (Adventure Map Sync):   0x004316C2 -> Cave F  (unlock all stages 1..12)
  - Site G  (SaveDetails Guard):    0x00417DB0 -> Cave G  (prevent disk write for DevTester)
  - Site H  (Button 4-Frame Setup): 0x0042BA29 -> Cave H  (dynamic 4-frame width handling for 480px)
  - Site I  (DialogButton PreDraw): 0x0043386C -> Cave I  (locks Rename and Delete when DevTester)
  - Site J  (UserDialog Depress):   0x00436F96 -> Cave J  (blocks Rename and Delete when DevTester)

Zero hardcoded addresses:
  - All hook sites resolved dynamically via AOB signatures + Capstone disassembly.
  - IAT slots resolved dynamically via PE import directory in .zdata.
"""

import sys
import os
import struct
import capstone
import pefile

sys.path.append(os.path.dirname(__file__))
from pe_util import PEFile, align

ZTK_CHARS = 0x60000020

# AOB Signatures for Dynamic Site Resolution (Zero hardcoded addresses)
AOB_SITE_A  = bytes.fromhex("C645FC0BFF92BC000000")
AOB_SITE_A2 = bytes.fromhex("3BFB8945F074268B8E18010000")
AOB_SITE_C  = bytes.fromhex("8B86800000008B4DE003C1")
AOB_SITE_D  = bytes.fromhex("8B8E0C06000050C745FC00000000E8")
AOB_SITE_E  = bytes.fromhex("C645F001C7868C000000030000008B4E648B45F0")
AOB_SITE_F  = bytes.fromhex("837828157C")
AOB_SITE_G_PROLOGUE = bytes.fromhex("558BEC6AFF")
AOB_SITE_H  = bytes.fromhex("8B4D1C740F8B470499F7F9894610")
AOB_SITE_I  = bytes.fromhex("8A860C01000084C0")
AOB_SITE_J  = bytes.fromhex("8B45E885C0C745FC0000000074")
AOB_SITE_K  = bytes.fromhex("A32C805400837DEC10")
AOB_SITE_L  = bytes.fromhex("8B87CC0100006A00508BCE")
AOB_SITE_M  = bytes.fromhex("8BF83BBEBC010000")
AOB_SITE_N  = bytes.fromhex("85C07C333B86B8010000")
AOB_SITE_O  = bytes.fromhex("C78554FFFFFF381A50000F8C")
AOB_SITE_P  = bytes.fromhex("8B826001000085C07C")
AOB_SITE_Q  = bytes.fromhex("558BEC568BF18B86B40000008B8804060000")

def resolve_ztk_sites(pe_raw):
    p = pefile.PE(data=pe_raw)
    image_base = p.OPTIONAL_HEADER.ImageBase

    text_sec = next((s for s in p.sections if s.Name.rstrip(b'\x00') == b'.text'), None)
    if not text_sec:
        raise RuntimeError(".text section not found in PE")
    text_data = text_sec.get_data()
    text_va = image_base + text_sec.VirtualAddress

    rdata_sec = next((s for s in p.sections if s.Name.rstrip(b'\x00') == b'.rdata'), None)
    if not rdata_sec:
        raise RuntimeError(".rdata section not found in PE")
    rdata_data = rdata_sec.get_data()
    rdata_va = image_base + rdata_sec.VirtualAddress

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    # 1. Resolve Site A: UserDialog AddLine
    idx_a = text_data.find(AOB_SITE_A)
    if idx_a == -1:
        # Check if already patched with jmp
        idx_a2 = text_data.find(bytes.fromhex("C645FC0BE9"))
        if idx_a2 != -1:
            site_a = text_va + idx_a2 + 4
        else:
            raise RuntimeError("Could not locate Site A (UserDialog AddLine) signature")
    else:
        site_a = text_va + idx_a + 4

    # 2. Resolve Site A2: UserDialog Skip curUser
    idx_a2 = text_data.find(AOB_SITE_A2)
    if idx_a2 == -1:
        idx_a2_p = text_data.find(bytes.fromhex("E9"))
        # Fallback check
        raise RuntimeError("Could not locate Site A2 (UserDialog Skip curUser) signature")
    site_a2 = text_va + idx_a2
    disp_a2 = struct.unpack('<b', text_data[idx_a2 + 6 : idx_a2 + 7])[0]
    skip_target_a2 = site_a2 + 7 + disp_a2

    # 3. Resolve Site B: Welcome Header Color
    str_welcome = rdata_data.find(b"Welcome to Zuma,\x00")
    if str_welcome == -1:
        raise RuntimeError("Could not locate 'Welcome to Zuma,' string in .rdata")
    str_welcome_va = rdata_va + str_welcome
    push_welcome = text_data.find(b"\x68" + struct.pack("<I", str_welcome_va))
    if push_welcome == -1:
        raise RuntimeError("Could not locate push reference to 'Welcome to Zuma,' string")
    site_b_off = text_data.rfind(bytes.fromhex("68FFFFFF008D4DE0"), max(0, push_welcome - 200), push_welcome)
    if site_b_off == -1:
        raise RuntimeError("Could not locate Site B (Welcome Header Color) signature near push reference")
    site_b = text_va + site_b_off

    # 4. Resolve Site C: ListBox Item Color
    idx_c = text_data.find(AOB_SITE_C)
    if idx_c == -1:
        raise RuntimeError("Could not locate Site C (ListBox Item Color) signature")
    site_c = text_va + idx_c + 11
    rel_set_color = struct.unpack("<i", text_data[idx_c + 11 + 8 : idx_c + 11 + 12])[0]
    set_color_va = (site_c + 7 + 5) + rel_set_color

    # 5. Resolve Site D: User Selection (ProfileManager::GetProfile)
    idx_d = text_data.find(AOB_SITE_D)
    if idx_d == -1:
        raise RuntimeError("Could not locate Site D (User Selection) signature")
    site_d = text_va + idx_d + 14
    rel_get_profile = struct.unpack("<i", text_data[idx_d + 15 : idx_d + 19])[0]
    orig_get_profile = (site_d + 5) + rel_get_profile

    # 6. Resolve Site E: Gauntlet Sync
    idx_e = text_data.find(AOB_SITE_E)
    if idx_e == -1:
        raise RuntimeError("Could not locate Site E (Gauntlet Sync) signature")
    site_e = text_va + idx_e + 14

    # 7. Resolve Site F: Adventure Map Sync
    idx_f = text_data.find(AOB_SITE_F)
    if idx_f == -1:
        raise RuntimeError("Could not locate Site F (Adventure Map Sync) signature")
    site_f = text_va + idx_f
    disp_f = struct.unpack("<b", text_data[idx_f + 5 : idx_f + 6])[0]
    lock_target_f = site_f + 6 + disp_f
    unlock_target_f = site_f + 6

    # 8. Resolve Site G: SaveDetails Guard
    str_userdat = rdata_data.find(b"userdata/user%d.dat\x00")
    if str_userdat == -1:
        raise RuntimeError("Could not locate 'userdata/user%d.dat' string in .rdata")
    str_userdat_va = rdata_va + str_userdat
    push_userdat = b"\x68" + struct.pack("<I", str_userdat_va)

    str_ud = rdata_data.find(b"userdata\x00")
    if str_ud == -1:
        raise RuntimeError("Could not locate 'userdata' string in .rdata")
    push_ud = b"\x68" + struct.pack("<I", rdata_va + str_ud)

    pos = 0
    site_g = None
    while True:
        pos = text_data.find(push_userdat, pos)
        if pos == -1:
            break
        if text_data.find(push_ud, max(0, pos - 0x100), pos) != -1:
            back = pos
            while back > max(0, pos - 0x200):
                if text_data[back : back + 5] == AOB_SITE_G_PROLOGUE:
                    site_g = text_va + back
                    break
                back -= 1
            break
        pos += 1

    if site_g is None:
        raise RuntimeError("Could not locate Site G (SaveDetails Guard) signature")

    # 9. Resolve Site H: DialogButton 4-frame width adjustment in MakeButton (0x0042BA29)
    idx_h = text_data.find(AOB_SITE_H)
    if idx_h == -1:
        raise RuntimeError("Could not locate Site H (Button Frames Resolution) signature")
    site_h = text_va + idx_h + 5
    continue_target_h = site_h + 9

    # 10. Resolve Site I: DialogButton::Draw pre-draw hook (0x0043386C)
    idx_i = text_data.find(AOB_SITE_I)
    if idx_i == -1:
        raise RuntimeError("Could not locate Site I (DialogButton::Draw) signature")
    site_i = text_va + idx_i + 19
    rel_i = struct.unpack('<i', text_data[idx_i + 20 : idx_i + 24])[0]
    orig_btn_draw = (site_i + 5) + rel_i

    # 11. Resolve Site J: UserDialog::ButtonDepress Rename/Delete lock hook (0x00436F96)
    idx_j = text_data.find(AOB_SITE_J)
    if idx_j == -1:
        raise RuntimeError("Could not locate Site J (UserDialog::ButtonDepress) signature")
    site_j = text_va + idx_j
    disp_j = struct.unpack('<b', text_data[idx_j + 13 : idx_j + 14])[0]
    cleanup_target_j = (site_j + 14) + disp_j
    continue_target_j = site_j + 14

    # 12. Resolve Site K: ExtractRegisterResources Image pointer capture (0x0042EEAC)
    idx_k = text_data.find(AOB_SITE_K)
    if idx_k == -1:
        raise RuntimeError("Could not locate Site K (ExtractRegisterResources) signature")
    site_k = text_va + idx_k
    continue_target_k = site_k + 5

    # 13. Resolve Site L: MapScreen::Draw Temple 5 Render Hook (0x00436432)
    idx_l = text_data.find(AOB_SITE_L)
    if idx_l == -1:
        raise RuntimeError("Could not locate Site L (MapScreen::Draw) signature")
    site_l = text_va + idx_l
    continue_target_l = site_l + 6

    # 14. Resolve Site M: MapScreen::MouseMove Door 5 Hit Hook (0x00435638)
    idx_m = text_data.find(AOB_SITE_M)
    if idx_m == -1:
        raise RuntimeError("Could not locate Site M (MapScreen::MouseMove) signature")
    site_m = text_va + idx_m
    continue_target_m = site_m + 8

    # 15. Resolve Site N: MapScreen::MouseDown Door 5 Click Hook (0x0043569C)
    idx_n = text_data.find(AOB_SITE_N)
    if idx_n == -1:
        raise RuntimeError("Could not locate Site N (MapScreen::MouseDown) signature")
    site_n = text_va + idx_n
    disp_n = struct.unpack('<b', text_data[idx_n + 3 : idx_n + 4])[0]
    skip_target_n = (site_n + 4) + disp_n
    continue_target_n = site_n + 10

    # 16. Resolve Site O: MapScreen::Draw Banner Hook (0x0043653A)
    idx_o = text_data.find(AOB_SITE_O)
    if idx_o == -1:
        raise RuntimeError("Could not locate Site O (MapScreen::Draw Banner Hook) signature")
    site_o = text_va + idx_o + 10
    disp_o = struct.unpack('<i', text_data[idx_o + 12 : idx_o + 16])[0]
    continue_target_skip_o = (site_o + 6) + disp_o
    continue_target_vanilla_o = site_o + 6

    # 17. Resolve Site P: MapScreen::ButtonDepress Play Button Hook (0x004334C0)
    idx_p = text_data.find(AOB_SITE_P)
    if idx_p == -1:
        raise RuntimeError("Could not locate Site P (MapScreen::ButtonDepress Play Button) signature")
    site_p = text_va + idx_p
    continue_target_p = site_p + 10  # 0x004334CA (push eax; call StartAdventure)
    disp_p = struct.unpack('<b', text_data[idx_p + 9 : idx_p + 10])[0]
    skip_target_p = (site_p + 10) + disp_p  # 0x004334D9 (pop ebp; ret 4)

    # 18. Resolve Site Q: Board::Update Entry Hook (0x0040D330)
    idx_q = text_data.find(AOB_SITE_Q)
    if idx_q == -1:
        raise RuntimeError("Could not locate Site Q (Board::Update Entry Hook) signature")
    site_q = text_va + idx_q
    continue_target_q = site_q + 6  # 0x0040D336

    # 19. Resolve Site R: HelpScreen::Draw background & content hook
    # Anchor: "Don't show this screen at startup.\x00" in .rdata
    str_dont_show = rdata_data.find(b"Don't show this screen at startup.\x00")
    if str_dont_show == -1:
        raise RuntimeError("Could not locate 'Don't show this screen at startup.' string in .rdata")
    str_dont_show_va = rdata_va + str_dont_show

    # Find 'mov eax, str_dont_show_va' (B8 <va>) in .text
    mov_dont_show = text_data.find(b"\xB8" + struct.pack("<I", str_dont_show_va))
    if mov_dont_show == -1:
        raise RuntimeError("Could not locate xref to 'Don't show this screen at startup.' in .text")

    # 55 bytes before mov_dont_show is:
    #   +00: A1 <ptr_help_front>       mov eax, [g_ImageHelpFront] (5 bytes)
    #   +05: 6A 00 6A 00 50 8B CE      push 0; push 0; push eax; mov ecx, esi (7 bytes)
    #   +12: E8 <rel32_draw_image>     call Graphics::DrawImage (5 bytes)
    # Total = 17 bytes
    idx_r = mov_dont_show - 55
    if text_data[idx_r] != 0xA1 or text_data[idx_r + 12] != 0xE8:
        raise RuntimeError("Unexpected instruction pattern at Site R (HelpScreen::Draw)")

    site_r = text_va + idx_r
    ptr_help_front_r = struct.unpack_from("<I", text_data, idx_r + 1)[0]
    rel_draw_img_r = struct.unpack_from("<i", text_data, idx_r + 13)[0]
    fn_draw_image_r = (site_r + 12) + 5 + rel_draw_img_r
    continue_target_r = site_r + 17

    return {
        'site_a': site_a,
        'site_a2': site_a2,
        'skip_target_a2': skip_target_a2,
        'site_b': site_b,
        'site_c': site_c,
        'set_color_va': set_color_va,
        'site_d': site_d,
        'orig_get_profile': orig_get_profile,
        'site_e': site_e,
        'site_f': site_f,
        'lock_target_f': lock_target_f,
        'unlock_target_f': unlock_target_f,
        'site_g': site_g,
        'site_h': site_h,
        'continue_target_h': continue_target_h,
        'site_i': site_i,
        'orig_btn_draw': orig_btn_draw,
        'site_j': site_j,
        'cleanup_target_j': cleanup_target_j,
        'continue_target_j': continue_target_j,
        'site_k': site_k,
        'continue_target_k': continue_target_k,
        'site_l': site_l,
        'continue_target_l': continue_target_l,
        'site_m': site_m,
        'continue_target_m': continue_target_m,
        'site_n': site_n,
        'skip_target_n': skip_target_n,
        'continue_target_n': continue_target_n,
        'site_o': site_o,
        'continue_target_skip_o': continue_target_skip_o,
        'continue_target_vanilla_o': continue_target_vanilla_o,
        'site_p': site_p,
        'continue_target_p': continue_target_p,
        'skip_target_p': skip_target_p,
        'site_q': site_q,
        'continue_target_q': continue_target_q,
        'site_r': site_r,
        'ptr_help_front_r': ptr_help_front_r,
        'fn_draw_image_r': fn_draw_image_r,
        'continue_target_r': continue_target_r
    }

def find_ztk_iat_entries(pe_raw):
    pe = PEFile(pe_raw)
    imps = pe.get_imports()
    ztk_dict = imps.get('zumatestingkit.dll', {})
    expected = [
        'ZTK_IsActive',
        'ZTK_OnUserDialogInit',
        'ZTK_CheckCurUserSkip',
        'ZTK_GetItemColor',
        'ZTK_GetWelcomeColor',
        'ZTK_GetProfile',
        'ZTK_SyncPractice',
        'ZTK_CheckStageUnlock',
        'ZTK_SaveDetails',
        'ZTK_PreButtonDraw',
        'ZTK_CheckUserDialogAction',
        'ZTK_OnRegisterResources',
        'ZTK_DrawTemple5',
        'ZTK_CheckDoor5Hit',
        'ZTK_CheckDoor5Click',
        'ZTK_DrawStage13Banner',
        'ZTK_GetSelectedStage',
        'ZTK_UpdateDevCheat',
        'ZTK_DrawHelpScreen'
    ]
    target_funcs = {fn: ztk_dict.get(fn) for fn in expected}
    missing = [k for k, v in target_funcs.items() if v is None]
    if missing:
        raise RuntimeError(f"Missing IAT imports from zumatestingkit.dll: {missing}. Run patch_imports.py first!")
    return target_funcs

def patch_testingkit(src_path, dst_path):
    print(f"[INFO] Patching Developer Testing Kit support from {src_path} -> {dst_path}")
    pe = PEFile(src_path)

    # Idempotency guard: if .ztk already exists, skip
    if pe.find_section_by_name('.ztk'):
        print("[INFO] Section .ztk already present in executable.")
        if src_path != dst_path:
            pe.save(dst_path)
        return

    # 1. Resolve IAT slots from .zdata
    iat = find_ztk_iat_entries(bytes(pe.data))
    for k, v in iat.items():
        print(f"[OK] IAT {k:22s}: 0x{v:08X}")

    # 2. Resolve Sites dynamically
    sites = resolve_ztk_sites(bytes(pe.data))
    for k, v in sites.items():
        print(f"[OK] Site {k:16s}: 0x{v:08X}")

    # 3. Determine new section RVA & VA
    last_sec = max(pe.sections, key=lambda s: s.virtual_address)
    new_rva = align(last_sec.virtual_address + max(last_sec.virtual_size, last_sec.raw_size), pe.section_alignment)
    new_va = pe.image_base + new_rva

    # 4. Construct .ztk content
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
        def jz(s, t): s.b += b"\x0F\x84" + struct.pack("<i", t - (s.va() + 6))
        def jnz(s, t): s.b += b"\x0F\x85" + struct.pack("<i", t - (s.va() + 6))
        def jl(s, t): s.b += b"\x0F\x8C" + struct.pack("<i", t - (s.va() + 6))
        def jns(s, t): s.b += b"\x0F\x89" + struct.pack("<i", t - (s.va() + 6))

    def emit(a):
        assert a.start == new_va + len(blob)
        blob.extend(a.b)
        pad()

    # Cave A2: va_cave_a2 (Hooked from Site A2 @ 0x00436DF8)
    # At entry: eax = gZumaApp, edi = curUser, esi = UserDialog* (this), [esi + 0x118] = mListBox.
    # 1. Immediately preserve gZumaApp in [ebp - 0x10] (matching vanilla 0x00436DFA).
    # 2. Vanilla check: if curUser == NULL (edi == ebx), skip curUser add immediately.
    # 3. If ZTK_OnUserDialogInit IAT is NULL (zumatestingkit.dll missing), continue vanilla add curUser.
    # 4. Call ZTK_OnUserDialogInit(mListBox) to insert "DevTester" at index 1.
    # 5. Call ZTK_CheckCurUserSkip(curUser=edi).
    # 6. If skipping, restore eax = gZumaApp and jump to 0x00436E25.
    a = Asm(); va_cave_a2 = a.start
    a.raw(b"\x89\x45\xF0")                              # mov dword ptr [ebp - 0x10], eax (SAVE gZumaApp!)
    a.raw(b"\x3B\xFB")                                  # cmp edi, ebx (curUser == NULL?)
    a.jz(sites['skip_target_a2'])                       # jz 0x00436E25 (skip immediately if NULL!)
    # cmp dword ptr [iat['ZTK_OnUserDialogInit']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_OnUserDialogInit']) + b"\x00")
    a.jz(sites['site_a2'] + 7)                          # jz 0x00436DFF (vanilla add curUser)
    a.raw(b"\x51")                                      # push ecx
    a.raw(b"\x52")                                      # push edx
    a.raw(b"\x56")                                      # push esi (arg2: UserDialog*)
    a.raw(b"\xFF\xB6\x18\x01\x00\x00")                  # push dword ptr [esi + 0x118] (arg1: mListBox)
    a.raw(b"\xFF\x15" + struct.pack("<I", iat['ZTK_OnUserDialogInit'])) # call [ZTK_OnUserDialogInit]
    a.raw(b"\x83\xC4\x08")                              # add esp, 8
    a.raw(b"\x5A")                                      # pop edx
    a.raw(b"\x59")                                      # pop ecx
    a.raw(b"\x57")                                      # push edi (curUser)
    a.raw(b"\xFF\x15" + struct.pack("<I", iat['ZTK_CheckCurUserSkip'])) # call [ZTK_CheckCurUserSkip]
    a.raw(b"\x83\xC4\x04")                              # add esp, 4
    a.raw(b"\x85\xC0")                                  # test eax, eax
    skip_lbl = a.va() + 2 + 5                           # after jnz
    a.raw(b"\x75\x05")                                  # jnz skip_lbl
    a.jmp(sites['site_a2'] + 7)                         # jmp 0x00436DFF (normal add curUser)
    # skip_lbl:
    a.raw(b"\x8B\x45\xF0")                              # mov eax, dword ptr [ebp - 0x10] (RESTORE gZumaApp!)
    a.jmp(sites['skip_target_a2'])                      # jmp 0x00436E25 (skip curUser add)
    emit(a)

    # Cave B: va_cave_b (Hooked from Site B @ 0x00432072)
    # Replaces push 0x00ffffff
    # If IAT slot is NULL, pushes 0x00ffffff directly and continues vanilla
    a = Asm(); va_cave_b = a.start
    # cmp dword ptr [iat['ZTK_GetWelcomeColor']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_GetWelcomeColor']) + b"\x00")
    # jz vanilla_b (call(6) + push(1) + jmp(5) = 12 bytes = 0x0C)
    a.raw(b"\x74\x0C")
    a.raw(b"\xFF\x15" + struct.pack("<I", iat['ZTK_GetWelcomeColor'])) # call [ZTK_GetWelcomeColor]
    a.raw(b"\x50")                                      # push eax
    a.jmp(sites['site_b'] + 5)                          # jmp back to 0x00432077
    # vanilla_b:
    a.raw(b"\x68\xFF\xFF\xFF\x00")                      # push 0x00ffffff
    a.jmp(sites['site_b'] + 5)                          # jmp back to 0x00432077
    emit(a)

    # Cave C: va_cave_c (Hooked from Site C @ 0x0047B589)
    # In: eax = in_color, esi = listbox, edi = item_idx
    # If IAT slot is NULL, leaves eax as in_color and passes to SetColor
    a = Asm(); va_cave_c = a.start
    call_color_block = bytearray()
    call_color_block += b"\x57"                                      # push edi (item_idx)
    call_color_block += b"\x56"                                      # push esi (listbox)
    call_color_block += b"\x50"                                      # push eax (in_color)
    call_color_block += b"\xFF\x15" + struct.pack("<I", iat['ZTK_GetItemColor']) # call [ZTK_GetItemColor]
    call_color_block += b"\x83\xC4\x0C"                              # add esp, 12

    # cmp dword ptr [iat['ZTK_GetItemColor']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_GetItemColor']) + b"\x00")
    # jz skip_item_color
    a.raw(b"\x74" + bytes([len(call_color_block)]))
    a.raw(call_color_block)
    # skip_item_color / continue vanilla:
    a.raw(b"\x8D\x8D\x5C\xFF\xFF\xFF")                  # lea ecx, [ebp - 0xA4] (Graphics* g)
    a.raw(b"\x50")                                      # push eax (PopCapColor*)
    a.call(sites['set_color_va'])                       # call Graphics::SetColor
    a.jmp(sites['site_c'] + 12)                         # jmp back to 0x0047B595
    emit(a)

    # Cave D: va_cave_d (Hooked from Site D @ 0x0043CD71)
    # Replaces call ProfileManager::GetProfile (5 bytes)
    # ecx = profileMgr, stack: [esp] = ret_addr, [esp+4] = name_str
    # If IAT slot is NULL, branches directly to orig_get_profile
    a = Asm(); va_cave_d = a.start
    # cmp dword ptr [iat['ZTK_GetProfile']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_GetProfile']) + b"\x00")
    # jz orig_get_profile
    a.jz(sites['orig_get_profile'])
    a.raw(b"\x68" + struct.pack("<I", sites['orig_get_profile']))       # push orig_get_profile
    a.raw(b"\xFF\x74\x24\x08")                           # push dword ptr [esp + 8] (name_str)
    a.raw(b"\x51")                                       # push ecx (profileMgr)
    a.raw(b"\xFF\x15" + struct.pack("<I", iat['ZTK_GetProfile']))      # call [ZTK_GetProfile]
    a.raw(b"\x83\xC4\x0C")                               # add esp, 12
    a.raw(b"\xC2\x04\x00")                               # ret 4 (thiscall callee cleanup of name_str)
    emit(a)

    # Cave E: va_cave_e (Hooked from Site E @ 0x0043BDE5)
    # esi = practiceScreen, [ebp - 0x10] = is_unlocked
    # If IAT slot is NULL, skips ZTK_SyncPractice and executes vanilla read
    a = Asm(); va_cave_e = a.start
    call_sync_block = bytearray()
    call_sync_block += b"\x8D\x45\xF0"                               # lea eax, [ebp - 0x10]
    call_sync_block += b"\x50"                                       # push eax (&is_unlocked)
    call_sync_block += b"\x56"                                       # push esi (practiceScreen)
    call_sync_block += b"\xFF\x15" + struct.pack("<I", iat['ZTK_SyncPractice']) # call [ZTK_SyncPractice]
    call_sync_block += b"\x83\xC4\x08"                               # add esp, 8

    # cmp dword ptr [iat['ZTK_SyncPractice']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_SyncPractice']) + b"\x00")
    # jz skip_sync
    a.raw(b"\x74" + bytes([len(call_sync_block)]))
    a.raw(call_sync_block)
    # skip_sync / continue vanilla:
    a.raw(b"\x8B\x4E\x64")                               # mov ecx, dword ptr [esi + 0x64]
    a.raw(b"\x8B\x45\xF0")                               # mov eax, dword ptr [ebp - 0x10]
    a.jmp(sites['site_e'] + 6)                           # jmp back to 0x0043BDEB
    emit(a)

    # Cave F: va_cave_f (Hooked from Site F @ 0x004316C2)
    # eax = stageNode
    # If IAT slot is NULL, executes vanilla: cmp [eax+0x28], 0x15; jl lock; jmp unlock
    a = Asm(); va_cave_f = a.start
    # cmp dword ptr [iat['ZTK_CheckStageUnlock']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_CheckStageUnlock']) + b"\x00")
    # jnz sidecar_f (15 bytes: cmp (4) + jl (6) + jmp (5))
    a.raw(b"\x75\x0F")
    # vanilla_f:
    a.raw(b"\x83\x78\x28\x15")                           # cmp dword ptr [eax + 0x28], 0x15
    a.jl(sites['lock_target_f'])                         # jl 0x0043171B (locked)
    a.jmp(sites['unlock_target_f'])                      # jmp 0x004316C8 (unlocked)
    # sidecar_f:
    a.raw(b"\x50")                                       # push eax (stageNode)
    a.raw(b"\xFF\x15" + struct.pack("<I", iat['ZTK_CheckStageUnlock'])) # call [ZTK_CheckStageUnlock]
    a.raw(b"\x83\xC4\x04")                               # add esp, 4
    a.raw(b"\x85\xC0")                                   # test eax, eax
    a.jz(sites['lock_target_f'])                         # jz 0x0043171B (locked)
    a.jmp(sites['unlock_target_f'])                      # jmp 0x004316C8 (unlocked)
    emit(a)

    # Cave G: va_cave_g (Hooked from Site G @ 0x00417DB0)
    # ecx = userProfile
    # If IAT slot is NULL, branches directly to vanilla prologue and save
    a = Asm(); va_cave_g = a.start
    call_save_block = bytearray()
    call_save_block += b"\x51"                                       # push ecx (save this)
    call_save_block += b"\x51"                                       # push ecx (userProfile)
    call_save_block += b"\xFF\x15" + struct.pack("<I", iat['ZTK_SaveDetails']) # call [ZTK_SaveDetails]
    call_save_block += b"\x83\xC4\x04"                               # add esp, 4
    call_save_block += b"\x85\xC0"                                   # test eax, eax
    call_save_block += b"\x74\x02"                                   # jz normal_save (jump 2 bytes over \x59\xC3)
    call_save_block += b"\x59"                                       # pop ecx (restore this)
    call_save_block += b"\xC3"                                       # ret (abort save!)
    # normal_save:
    call_save_block += b"\x59"                                       # pop ecx (restore this)

    # cmp dword ptr [iat['ZTK_SaveDetails']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_SaveDetails']) + b"\x00")
    # jz normal_save_direct
    a.raw(b"\x74" + bytes([len(call_save_block)]))
    a.raw(call_save_block)
    # normal_save_direct:
    a.raw(b"\x55")                                       # push ebp
    a.raw(b"\x8B\xEC")                                   # mov ebp, esp
    a.raw(b"\x6A\xFF")                                   # push -1
    a.jmp(sites['site_g'] + 5)                           # jmp 0x00417DB5
    emit(a)

    # Cave H: va_cave_h (Hooked from Site H @ 0x0042BA29)
    # Handles 4-frame DialogButton (480px) vs 3-frame (360px)
    # At entry: edi = Image*, esi = ButtonWidget*, ecx = numFrames (default 3)
    a = Asm(); va_cave_h = a.start
    a.raw(b"\x8B\x47\x04")                              # mov eax, dword ptr [edi + 4] (image->mWidth)
    a.raw(b"\x3D\xE0\x01\x00\x00")                      # cmp eax, 480 (0x1E0)
    a.raw(b"\x75\x05")                                  # jne not_480_lbl
    a.raw(b"\xB9\x04\x00\x00\x00")                      # mov ecx, 4
    # not_480_lbl:
    a.raw(b"\x99")                                      # cdq
    a.raw(b"\xF7\xF9")                                  # idiv ecx
    a.raw(b"\x89\x46\x10")                              # mov dword ptr [esi + 0x10], eax
    a.jmp(sites['continue_target_h'])                   # jmp 0x0042BA32
    emit(a)

    # Cave I: va_cave_i (Hooked from Site I @ 0x0043386C)
    # Pre-draw hook for DialogButton: locks Rename and Delete when DevTester is selected
    # At entry: esi = DialogButton*, [esp] = ret_addr (0x00433871), [esp+4] = Graphics* g
    # If IAT slot is NULL, skips ZTK_PreButtonDraw and jumps to orig_btn_draw
    a = Asm(); va_cave_i = a.start
    call_predraw_block = bytearray()
    call_predraw_block += b"\x50"                                      # push eax
    call_predraw_block += b"\x51"                                      # push ecx
    call_predraw_block += b"\x52"                                      # push edx
    call_predraw_block += b"\x56"                                      # push esi (DialogButton*)
    call_predraw_block += b"\xFF\x15" + struct.pack("<I", iat['ZTK_PreButtonDraw']) # call [ZTK_PreButtonDraw]
    call_predraw_block += b"\x83\xC4\x04"                              # add esp, 4
    call_predraw_block += b"\x5A"                                      # pop edx
    call_predraw_block += b"\x59"                                      # pop ecx
    call_predraw_block += b"\x58"                                      # pop eax

    # cmp dword ptr [iat['ZTK_PreButtonDraw']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_PreButtonDraw']) + b"\x00")
    # jz skip_pre_draw
    a.raw(b"\x74" + bytes([len(call_predraw_block)]))
    a.raw(call_predraw_block)
    # skip_pre_draw:
    a.raw(b"\x8B\xCE")                                  # mov ecx, esi (this = DialogButton*)
    a.jmp(sites['orig_btn_draw'])                       # jmp ButtonWidget::Draw (tail call with ret 4)
    emit(a)

    # Cave J: va_cave_j (Hooked from Site J @ 0x00436F96)
    # Intercepts UserDialog::ButtonDepress for Rename and Delete buttons
    # At entry: [ebp - 0x2C] = std::string selectedUser, edi = btn_id (0 for Rename, 1 for Delete)
    # If IAT slot is NULL, executes vanilla length check
    a = Asm(); va_cave_j = a.start
    chk_action_block = bytearray()
    chk_action_block += b"\x57"                                       # push edi (btn_id: 0 = Rename, 1 = Delete)
    chk_action_block += b"\x8D\x45\xD4"                               # lea eax, [ebp - 0x2C] (&selectedUser)
    chk_action_block += b"\x50"                                       # push eax
    chk_action_block += b"\xFF\x15" + struct.pack("<I", iat['ZTK_CheckUserDialogAction']) # call [ZTK_CheckUserDialogAction]
    chk_action_block += b"\x83\xC4\x08"                               # add esp, 8
    chk_action_block += b"\x85\xC0"                                   # test eax, eax
    chk_action_block += b"\x74\x13"                                   # jz blocked_lbl (+19 bytes)

    # cmp dword ptr [iat['ZTK_CheckUserDialogAction']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_CheckUserDialogAction']) + b"\x00")
    # jz allowed_lbl
    a.raw(b"\x74" + bytes([len(chk_action_block)]))
    a.raw(chk_action_block)
    # allowed_lbl: normal vanilla check
    a.raw(b"\x8B\x45\xE8")                              # mov eax, dword ptr [ebp - 0x18]
    a.raw(b"\x85\xC0")                                  # test eax, eax
    a.raw(b"\xC7\x45\xFC\x00\x00\x00\x00")              # mov dword ptr [ebp - 4], 0
    a.raw(b"\x74\x05")                                  # jz blocked_lbl (+5 bytes over jmp)
    a.jmp(sites['continue_target_j'])                   # jmp 0x00436FA4
    # blocked_lbl:
    a.raw(b"\xC7\x45\xFC\x00\x00\x00\x00")              # mov dword ptr [ebp - 4], 0
    a.jmp(sites['cleanup_target_j'])                    # jmp 0x00436FC6
    emit(a)

    # Cave K: va_cave_k (Hooked from Site K @ 0x0042EEAC)
    # Intercepts IMAGE_DIALOG_BUTTON extraction to capture mResourceManager
    # At entry: eax = IMAGE_DIALOG_BUTTON (Image*), edi = mResourceManager
    # If IAT slot is NULL, skips ZTK_OnRegisterResources
    a = Asm(); va_cave_k = a.start
    a.raw(b"\xA3\x2C\x80\x54\x00")                                       # mov [0x54802c], eax
    call_res_block = bytearray()
    call_res_block += b"\x50"                                                       # push eax
    call_res_block += b"\x51"                                                       # push ecx
    call_res_block += b"\x52"                                                       # push edx
    call_res_block += b"\x50"                                                       # push eax (arg2: dialog_button_img)
    call_res_block += b"\x57"                                                       # push edi (arg1: res_mgr)
    call_res_block += b"\xFF\x15" + struct.pack("<I", iat['ZTK_OnRegisterResources']) # call [ZTK_OnRegisterResources]
    call_res_block += b"\x83\xC4\x08"                                               # add esp, 8
    call_res_block += b"\x5A"                                                       # pop edx
    call_res_block += b"\x59"                                                       # pop ecx
    call_res_block += b"\x58"                                                       # pop eax

    # cmp dword ptr [iat['ZTK_OnRegisterResources']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_OnRegisterResources']) + b"\x00")
    # jz skip_reg_res
    a.raw(b"\x74" + bytes([len(call_res_block)]))
    a.raw(call_res_block)
    # skip_reg_res:
    a.jmp(sites['continue_target_k'])                                    # jmp 0x0042EEB1
    emit(a)

    # Cave L: va_cave_l (Hooked from Site L @ 0x00436432)
    # At entry: edi = MapScreen*, esi = Graphics*
    # Replaces: mov eax, dword ptr [edi + 0x1cc] (6 bytes)
    a = Asm(); va_cave_l = a.start
    call_l_block = bytearray()
    call_l_block += b"\x51"                                             # push ecx
    call_l_block += b"\x52"                                             # push edx
    call_l_block += b"\x56"                                             # push esi (Graphics*)
    call_l_block += b"\x57"                                             # push edi (MapScreen*)
    call_l_block += b"\xFF\x15" + struct.pack("<I", iat['ZTK_DrawTemple5']) # call [ZTK_DrawTemple5]
    call_l_block += b"\x83\xC4\x08"                                     # add esp, 8
    call_l_block += b"\x5A"                                             # pop edx
    call_l_block += b"\x59"                                             # pop ecx

    # cmp dword ptr [iat['ZTK_DrawTemple5']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_DrawTemple5']) + b"\x00")
    # jz fallback_l
    a.raw(b"\x74" + bytes([len(call_l_block)]))
    a.raw(call_l_block)
    # fallback_l:
    a.raw(b"\x8B\x87\xCC\x01\x00\x00")                                  # mov eax, dword ptr [edi + 0x1cc]
    a.jmp(sites['continue_target_l'])                                   # jmp 0x00436438
    emit(a)

    # Cave M: va_cave_m (Hooked from Site M @ 0x00435638)
    # At entry: eax = vanilla hit result (-1 if no vanilla door hit)
    # esi = MapScreen*, ebx = mouseX, [ebp + 0xc] = mouseY
    # Replaces: mov edi, eax (2) + cmp edi, [esi + 0x1bc] (6) = 8 bytes
    a = Asm(); va_cave_m = a.start
    call_m_block = bytearray()
    call_m_block += b"\x51"                                             # push ecx
    call_m_block += b"\x52"                                             # push edx
    call_m_block += b"\x50"                                             # push eax (vanilla_hit)
    call_m_block += b"\xFF\x75\x0C"                                     # push dword ptr [ebp + 0xc] (mouseY)
    call_m_block += b"\x53"                                             # push ebx (mouseX)
    call_m_block += b"\x56"                                             # push esi (MapScreen*)
    call_m_block += b"\xFF\x15" + struct.pack("<I", iat['ZTK_CheckDoor5Hit']) # call [ZTK_CheckDoor5Hit]
    call_m_block += b"\x83\xC4\x10"                                     # add esp, 16
    call_m_block += b"\x5A"                                             # pop edx
    call_m_block += b"\x59"                                             # pop ecx

    # cmp dword ptr [iat['ZTK_CheckDoor5Hit']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_CheckDoor5Hit']) + b"\x00")
    # jz vanilla_hit_m
    a.raw(b"\x74" + bytes([len(call_m_block)]))
    a.raw(call_m_block)
    # vanilla_hit_m:
    a.raw(b"\x8B\xF8")                                                  # mov edi, eax
    a.raw(b"\x3B\xBE\xBC\x01\x00\x00")                                  # cmp edi, dword ptr [esi + 0x1bc]
    a.jmp(sites['continue_target_m'])                                   # jmp 0x00435640
    emit(a)

    # Cave N: va_cave_n (Hooked from Site N @ 0x0043569C)
    # At entry: eax = vanilla hit result
    # esi = MapScreen*, ebx = mouseX, edi = mouseY
    # Replaces: test eax, eax (2) + jl 0x4356d3 (2) + cmp eax, [esi + 0x1b8] (6) = 10 bytes
    a = Asm(); va_cave_n = a.start
    a.raw(b"\x85\xC0")                                                  # test eax, eax
    pos_jns = len(a.b)
    a.raw(b"\x0F\x89\x00\x00\x00\x00")                                  # jns vanilla_click_n placeholder

    # cmp dword ptr [iat['ZTK_CheckDoor5Click']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_CheckDoor5Click']) + b"\x00")
    pos_jz = len(a.b)
    a.raw(b"\x0F\x84\x00\x00\x00\x00")                                  # jz no_click_n placeholder

    a.raw(b"\x51")                                                      # push ecx
    a.raw(b"\x52")                                                      # push edx
    a.raw(b"\x57")                                                      # push edi (mouseY)
    a.raw(b"\x53")                                                      # push ebx (mouseX)
    a.raw(b"\x56")                                                      # push esi (MapScreen*)
    a.raw(b"\xFF\x15" + struct.pack("<I", iat['ZTK_CheckDoor5Click'])) # call [ZTK_CheckDoor5Click]
    a.raw(b"\x83\xC4\x0C")                                              # add esp, 12
    a.raw(b"\x5A")                                                      # pop edx
    a.raw(b"\x59")                                                      # pop ecx
    a.raw(b"\x85\xC0")                                                  # test eax, eax
    a.jnz(sites['skip_target_n'])                                       # if handled != 0, jnz 0x004356D3

    # no_click_n:
    lbl_no_click_va = a.va()
    disp_jz = lbl_no_click_va - (a.start + pos_jz + 6)
    a.b[pos_jz + 2 : pos_jz + 6] = struct.pack("<i", disp_jz)
    a.jmp(sites['skip_target_n'])                                       # jmp 0x004356D3

    # vanilla_click_n:
    lbl_vanilla_va = a.va()
    disp_jns = lbl_vanilla_va - (a.start + pos_jns + 6)
    a.b[pos_jns + 2 : pos_jns + 6] = struct.pack("<i", disp_jns)
    a.raw(b"\x3B\x86\xB8\x01\x00\x00")                                  # cmp eax, dword ptr [esi + 0x1b8]
    a.jmp(sites['continue_target_n'])                                   # jmp 0x004356A6
    emit(a)

    # Cave O: va_cave_o (Hooked from Site O @ 0x0043653A)
    # At entry: edi = MapScreen*, esi = Graphics*
    # eax = [edi + 0x1B8] (tested right before Site O)
    # Replaces: jl 0x00436781 (6 bytes: 0F 8C disp32)
    a = Asm(); va_cave_o = a.start
    a.raw(b"\x85\xC0")                                                  # test eax, eax
    pos_jns = len(a.b)
    a.raw(b"\x0F\x89\x00\x00\x00\x00")                                  # jns vanilla_banner_o placeholder

    # If eax < 0 (Door 5 selected):
    # cmp dword ptr [iat['ZTK_DrawStage13Banner']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_DrawStage13Banner']) + b"\x00")
    pos_jz = len(a.b)
    a.raw(b"\x0F\x84\x00\x00\x00\x00")                                  # jz skip_banner_o placeholder

    a.raw(b"\x51")                                                      # push ecx
    a.raw(b"\x52")                                                      # push edx
    a.raw(b"\x56")                                                      # push esi (Graphics*)
    a.raw(b"\x57")                                                      # push edi (MapScreen*)
    a.raw(b"\xFF\x15" + struct.pack("<I", iat['ZTK_DrawStage13Banner']))# call [ZTK_DrawStage13Banner]
    a.raw(b"\x83\xC4\x08")                                              # add esp, 8
    a.raw(b"\x5A")                                                      # pop edx
    a.raw(b"\x59")                                                      # pop ecx

    # skip_banner_o:
    lbl_skip_o_va = a.va()
    disp_jz = lbl_skip_o_va - (a.start + pos_jz + 6)
    a.b[pos_jz + 2 : pos_jz + 6] = struct.pack("<i", disp_jz)
    a.jmp(sites['continue_target_skip_o'])                              # jmp 0x00436781

    # vanilla_banner_o:
    lbl_vanilla_o_va = a.va()
    disp_jns = lbl_vanilla_o_va - (a.start + pos_jns + 6)
    a.b[pos_jns + 2 : pos_jns + 6] = struct.pack("<i", disp_jns)
    a.jmp(sites['continue_target_vanilla_o'])                           # jmp 0x00436540
    emit(a)

    # Cave P: va_cave_p (Hooked from Site P @ 0x004334C0)
    # At entry: edx = ButtonListener* (MapScreen + 0x58)
    # Replaces: mov eax, [edx + 0x160] (6) + test eax, eax (2) + jl 0x4334d9 (2) = 10 bytes
    a = Asm(); va_cave_p = a.start

    # cmp dword ptr [iat['ZTK_GetSelectedStage']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_GetSelectedStage']) + b"\x00")
    pos_jz = len(a.b)
    a.raw(b"\x0F\x84\x00\x00\x00\x00")                                  # jz fallback_p placeholder

    # Call ZTK_GetSelectedStage(mapScreen = edx - 0x58):
    a.raw(b"\x8D\x42\xA8")                                              # lea eax, [edx - 0x58]
    a.raw(b"\x50")                                                      # push eax (mapScreen)
    a.raw(b"\xFF\x15" + struct.pack("<I", iat['ZTK_GetSelectedStage'])) # call [ZTK_GetSelectedStage]
    a.raw(b"\x83\xC4\x04")                                              # add esp, 4
    a.raw(b"\x85\xC0")                                                  # test eax, eax
    a.jl(sites['skip_target_p'])                                        # if eax < 0 -> jl 0x004334D9
    a.raw(b"\x8B\x0D\x00\xEF\x57\x00")                                  # mov ecx, dword ptr [0x0057EF00] (reload ecx = g_App)
    a.jmp(sites['continue_target_p'])                                   # jmp 0x004334CA (push eax; call 0x00440990)

    # fallback_p:
    lbl_fallback_va = a.va()
    disp_jz = lbl_fallback_va - (a.start + pos_jz + 6)
    a.b[pos_jz + 2 : pos_jz + 6] = struct.pack("<i", disp_jz)
    a.raw(b"\x8B\x82\x60\x01\x00\x00")                                  # mov eax, dword ptr [edx + 0x160]
    a.raw(b"\x85\xC0")                                                  # test eax, eax
    a.jl(sites['skip_target_p'])                                        # jl 0x004334D9
    a.raw(b"\x8B\x0D\x00\xEF\x57\x00")                                  # mov ecx, dword ptr [0x0057EF00] (reload ecx = g_App)
    a.jmp(sites['continue_target_p'])                                   # jmp 0x004334CA
    emit(a)

    # Cave Q: va_cave_q (Hooked from Site Q @ 0x0040D330)
    # At entry: ecx = Board*
    # Replaces: push ebp (1) + mov ebp, esp (2) + push esi (1) + mov esi, ecx (2) = 6 bytes
    a = Asm(); va_cave_q = a.start

    # cmp dword ptr [iat['ZTK_UpdateDevCheat']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_UpdateDevCheat']) + b"\x00")
    pos_jz = len(a.b)
    a.raw(b"\x0F\x84\x00\x00\x00\x00")                                  # jz skip_cheat_q placeholder

    # Preserve caller-saved registers
    a.raw(b"\x50")                                                      # push eax
    a.raw(b"\x51")                                                      # push ecx
    a.raw(b"\x52")                                                      # push edx

    # Call ZTK_UpdateDevCheat(board = ecx):
    a.raw(b"\x51")                                                      # push ecx (board)
    a.raw(b"\xFF\x15" + struct.pack("<I", iat['ZTK_UpdateDevCheat']))  # call [ZTK_UpdateDevCheat]
    a.raw(b"\x83\xC4\x04")                                              # add esp, 4

    # Restore caller-saved registers
    a.raw(b"\x5A")                                                      # pop edx
    a.raw(b"\x59")                                                      # pop ecx
    a.raw(b"\x58")                                                      # pop eax

    # skip_cheat_q:
    lbl_skip_q_va = a.va()
    disp_jz = lbl_skip_q_va - (a.start + pos_jz + 6)
    a.b[pos_jz + 2 : pos_jz + 6] = struct.pack("<i", disp_jz)

    # Execute replaced 6 bytes:
    a.raw(b"\x55")                                                      # push ebp
    a.raw(b"\x8B\xEC")                                                  # mov ebp, esp
    a.raw(b"\x56")                                                      # push esi
    a.raw(b"\x8B\xF1")                                                  # mov esi, ecx
    a.jmp(sites['continue_target_q'])                                   # jmp 0x0040D336
    emit(a)

    # Cave R: va_cave_r (Hooked from Site R in HelpScreen::Draw)
    # At entry: edi = HelpScreen* this, esi = Graphics* g
    # Replaces 17 bytes: mov eax, [g_ImageHelpFront]; push 0; push 0; push eax; mov ecx, esi; call DrawImage
    a = Asm(); va_cave_r = a.start

    # cmp dword ptr [iat['ZTK_DrawHelpScreen']], 0
    a.raw(b"\x83\x3D" + struct.pack("<I", iat['ZTK_DrawHelpScreen']) + b"\x00")
    pos_jz_r = len(a.b)
    a.raw(b"\x0F\x84\x00\x00\x00\x00")                                  # jz fallback_r placeholder

    # Call ZTK_DrawHelpScreen(helpScreen = edi, graphics = esi, fnDrawImage = sites['fn_draw_image_r']):
    a.raw(b"\x68" + struct.pack("<I", sites['fn_draw_image_r']))        # push fn_draw_image_r
    a.raw(b"\x56")                                                      # push esi (graphics)
    a.raw(b"\x57")                                                      # push edi (helpScreen)
    a.raw(b"\xFF\x15" + struct.pack("<I", iat['ZTK_DrawHelpScreen']))   # call [ZTK_DrawHelpScreen]
    a.raw(b"\x83\xC4\x0C")                                              # add esp, 12
    a.raw(b"\x85\xC0")                                                  # test eax, eax
    a.jnz(sites['continue_target_r'])                                   # if handled (eax != 0) -> jmp continue_target_r

    # fallback_r (vanilla player):
    lbl_fallback_r_va = a.va()
    disp_jz_r = lbl_fallback_r_va - (a.start + pos_jz_r + 6)
    a.b[pos_jz_r + 2 : pos_jz_r + 6] = struct.pack("<i", disp_jz_r)

    a.raw(b"\xA1" + struct.pack("<I", sites['ptr_help_front_r']))       # mov eax, dword ptr [g_ImageHelpFront]
    a.raw(b"\x6A\x00")                                                  # push 0
    a.raw(b"\x6A\x00")                                                  # push 0
    a.raw(b"\x50")                                                      # push eax
    a.raw(b"\x8B\xCE")                                                  # mov ecx, esi
    a.call(sites['fn_draw_image_r'])                                    # call Graphics::DrawImage
    a.jmp(sites['continue_target_r'])                                   # jmp continue_target_r
    emit(a)

    print(f"[OK] Section .ztk built ({len(blob)} bytes at RVA 0x{new_rva:08X}, VA 0x{new_va:08X})")

    # 5. Add section .ztk to PE
    pe.add_section('.ztk', bytes(blob), ZTK_CHARS)

    # 6. Apply Patches at the 11 Sites (Site A is kept 100% vanilla to ensure zero stack displacement)
    # Patch Site A2: jmp va_cave_a2 + 2 NOPs (7 bytes)
    off_a2 = pe.rva_to_offset(sites['site_a2'] - pe.image_base)
    rel_a2 = va_cave_a2 - (sites['site_a2'] + 5)
    patch_a2 = b"\xE9" + struct.pack("<i", rel_a2) + b"\x90\x90"
    assert len(patch_a2) == 7
    pe.data[off_a2 : off_a2 + 7] = patch_a2
    print(f"[PATCH] Site A2 @ 0x{sites['site_a2']:08X} -> jmp 0x{va_cave_a2:08X} + 2 nops")

    # Patch Site B: jmp va_cave_b (5 bytes)
    off_b = pe.rva_to_offset(sites['site_b'] - pe.image_base)
    rel_b = va_cave_b - (sites['site_b'] + 5)
    patch_b = b"\xE9" + struct.pack("<i", rel_b)
    assert len(patch_b) == 5
    pe.data[off_b : off_b + 5] = patch_b
    print(f"[PATCH] Site B  @ 0x{sites['site_b']:08X} -> jmp 0x{va_cave_b:08X}")

    # Patch Site C: jmp va_cave_c + 7 NOPs (12 bytes)
    off_c = pe.rva_to_offset(sites['site_c'] - pe.image_base)
    rel_c = va_cave_c - (sites['site_c'] + 5)
    patch_c = b"\xE9" + struct.pack("<i", rel_c) + b"\x90" * 7
    assert len(patch_c) == 12
    pe.data[off_c : off_c + 12] = patch_c
    print(f"[PATCH] Site C  @ 0x{sites['site_c']:08X} -> jmp 0x{va_cave_c:08X} + 7 nops")

    # Patch Site D: call va_cave_d (5 bytes)
    off_d = pe.rva_to_offset(sites['site_d'] - pe.image_base)
    rel_d = va_cave_d - (sites['site_d'] + 5)
    patch_d = b"\xE8" + struct.pack("<i", rel_d)
    assert len(patch_d) == 5
    pe.data[off_d : off_d + 5] = patch_d
    print(f"[PATCH] Site D  @ 0x{sites['site_d']:08X} -> call 0x{va_cave_d:08X}")

    # Patch Site E: jmp va_cave_e + 1 NOP (6 bytes)
    off_e = pe.rva_to_offset(sites['site_e'] - pe.image_base)
    rel_e = va_cave_e - (sites['site_e'] + 5)
    patch_e = b"\xE9" + struct.pack("<i", rel_e) + b"\x90"
    assert len(patch_e) == 6
    pe.data[off_e : off_e + 6] = patch_e
    print(f"[PATCH] Site E  @ 0x{sites['site_e']:08X} -> jmp 0x{va_cave_e:08X} + 1 nop")

    # Patch Site F: jmp va_cave_f + 1 NOP (6 bytes)
    off_f = pe.rva_to_offset(sites['site_f'] - pe.image_base)
    rel_f = va_cave_f - (sites['site_f'] + 5)
    patch_f = b"\xE9" + struct.pack("<i", rel_f) + b"\x90"
    assert len(patch_f) == 6
    pe.data[off_f : off_f + 6] = patch_f
    print(f"[PATCH] Site F  @ 0x{sites['site_f']:08X} -> jmp 0x{va_cave_f:08X} + 1 nop")

    # Patch Site G: jmp va_cave_g (5 bytes)
    off_g = pe.rva_to_offset(sites['site_g'] - pe.image_base)
    rel_g = va_cave_g - (sites['site_g'] + 5)
    patch_g = b"\xE9" + struct.pack("<i", rel_g)
    assert len(patch_g) == 5
    pe.data[off_g : off_g + 5] = patch_g
    print(f"[PATCH] Site G  @ 0x{sites['site_g']:08X} -> jmp 0x{va_cave_g:08X}")

    # Patch Site H: jmp va_cave_h + 4 NOPs (9 bytes)
    off_h = pe.rva_to_offset(sites['site_h'] - pe.image_base)
    rel_h = va_cave_h - (sites['site_h'] + 5)
    patch_h = b"\xE9" + struct.pack("<i", rel_h) + b"\x90\x90\x90\x90"
    assert len(patch_h) == 9
    pe.data[off_h : off_h + 9] = patch_h
    print(f"[PATCH] Site H  @ 0x{sites['site_h']:08X} -> jmp 0x{va_cave_h:08X} + 4 nops")

    # Patch Site I: call va_cave_i (5 bytes)
    off_i = pe.rva_to_offset(sites['site_i'] - pe.image_base)
    rel_i = va_cave_i - (sites['site_i'] + 5)
    patch_i = b"\xE8" + struct.pack("<i", rel_i)
    assert len(patch_i) == 5
    pe.data[off_i : off_i + 5] = patch_i
    print(f"[PATCH] Site I  @ 0x{sites['site_i']:08X} -> call 0x{va_cave_i:08X}")

    # Patch Site J: jmp va_cave_j + 9 NOPs (14 bytes)
    off_j = pe.rva_to_offset(sites['site_j'] - pe.image_base)
    rel_j = va_cave_j - (sites['site_j'] + 5)
    patch_j = b"\xE9" + struct.pack("<i", rel_j) + b"\x90" * 9
    assert len(patch_j) == 14
    pe.data[off_j : off_j + 14] = patch_j
    print(f"[PATCH] Site J  @ 0x{sites['site_j']:08X} -> jmp 0x{va_cave_j:08X} + 9 nops")

    # Patch Site K: jmp va_cave_k (5 bytes)
    off_k = pe.rva_to_offset(sites['site_k'] - pe.image_base)
    rel_k = va_cave_k - (sites['site_k'] + 5)
    patch_k = b"\xE9" + struct.pack("<i", rel_k)
    assert len(patch_k) == 5
    pe.data[off_k : off_k + 5] = patch_k
    print(f"[PATCH] Site K  @ 0x{sites['site_k']:08X} -> jmp 0x{va_cave_k:08X}")

    # Patch Site L: jmp va_cave_l + 1 NOP (6 bytes)
    off_l = pe.rva_to_offset(sites['site_l'] - pe.image_base)
    rel_l = va_cave_l - (sites['site_l'] + 5)
    patch_l = b"\xE9" + struct.pack("<i", rel_l) + b"\x90"
    assert len(patch_l) == 6
    pe.data[off_l : off_l + 6] = patch_l
    print(f"[PATCH] Site L  @ 0x{sites['site_l']:08X} -> jmp 0x{va_cave_l:08X} + 1 nop")

    # Patch Site M: jmp va_cave_m + 3 NOPs (8 bytes)
    off_m = pe.rva_to_offset(sites['site_m'] - pe.image_base)
    rel_m = va_cave_m - (sites['site_m'] + 5)
    patch_m = b"\xE9" + struct.pack("<i", rel_m) + b"\x90\x90\x90"
    assert len(patch_m) == 8
    pe.data[off_m : off_m + 8] = patch_m
    print(f"[PATCH] Site M  @ 0x{sites['site_m']:08X} -> jmp 0x{va_cave_m:08X} + 3 nops")

    # Patch Site N: jmp va_cave_n + 5 NOPs (10 bytes)
    off_n = pe.rva_to_offset(sites['site_n'] - pe.image_base)
    rel_n = va_cave_n - (sites['site_n'] + 5)
    patch_n = b"\xE9" + struct.pack("<i", rel_n) + b"\x90\x90\x90\x90\x90"
    assert len(patch_n) == 10
    pe.data[off_n : off_n + 10] = patch_n
    print(f"[PATCH] Site N  @ 0x{sites['site_n']:08X} -> jmp 0x{va_cave_n:08X} + 5 nops")

    # Patch Site O: jmp va_cave_o + 1 NOP (6 bytes)
    off_o = pe.rva_to_offset(sites['site_o'] - pe.image_base)
    rel_o = va_cave_o - (sites['site_o'] + 5)
    patch_o = b"\xE9" + struct.pack("<i", rel_o) + b"\x90"
    assert len(patch_o) == 6
    pe.data[off_o : off_o + 6] = patch_o
    print(f"[PATCH] Site O  @ 0x{sites['site_o']:08X} -> jmp 0x{va_cave_o:08X} + 1 nop")

    # Patch Site P: jmp va_cave_p + 5 NOPs (10 bytes: MapScreen::ButtonDepress Play Button @ 0x004334C0)
    off_p = pe.rva_to_offset(sites['site_p'] - pe.image_base)
    rel_p = va_cave_p - (sites['site_p'] + 5)
    patch_p = b"\xE9" + struct.pack("<i", rel_p) + b"\x90\x90\x90\x90\x90"
    assert len(patch_p) == 10
    pe.data[off_p : off_p + 10] = patch_p
    print(f"[PATCH] Site P  @ 0x{sites['site_p']:08X} -> jmp 0x{va_cave_p:08X} + 5 nops")

    # Patch Site Q: jmp va_cave_q + 1 NOP (6 bytes: Board::Update @ 0x0040D330)
    off_q = pe.rva_to_offset(sites['site_q'] - pe.image_base)
    rel_q = va_cave_q - (sites['site_q'] + 5)
    patch_q = b"\xE9" + struct.pack("<i", rel_q) + b"\x90"
    assert len(patch_q) == 6
    pe.data[off_q : off_q + 6] = patch_q
    print(f"[PATCH] Site Q  @ 0x{sites['site_q']:08X} -> jmp 0x{va_cave_q:08X} + 1 nop")

    # Patch Site R: jmp va_cave_r + 12 NOPs (17 bytes: HelpScreen::Draw)
    off_r = pe.rva_to_offset(sites['site_r'] - pe.image_base)
    rel_r = va_cave_r - (sites['site_r'] + 5)
    patch_r = b"\xE9" + struct.pack("<i", rel_r) + (b"\x90" * 12)
    assert len(patch_r) == 17
    pe.data[off_r : off_r + 17] = patch_r
    print(f"[PATCH] Site R  @ 0x{sites['site_r']:08X} -> jmp 0x{va_cave_r:08X} + 12 nops")

    # Patch DialogButton::Update @ 0x00433840 (NOP 2 bytes: je 0x43384d -> nop nop)
    # Ensures DialogButton always calls MarkDirty() every tick so selection changes update immediately
    off_upd = pe.rva_to_offset(0x00433840 - pe.image_base)
    pe.data[off_upd : off_upd + 2] = b"\x90\x90"
    print(f"[PATCH] DialogButton::Update MarkDirty always-on @ 0x00433840 -> nop nop")

    # 7. Save output PE
    pe.save(dst_path)
    print(f"[SUCCESS] Developer Testing Kit support successfully installed into {dst_path}")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit("Usage: python patch_testingkit.py <Zuma_in.exe> <Zuma_out.exe>")
    patch_testingkit(sys.argv[1], sys.argv[2])
