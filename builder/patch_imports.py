"""patch_imports.py - Injects static PE imports for sidecar DLLs into section .zdata.

Adds a pure data section '.zdata' (characteristics 0xC0000040: READ | WRITE, non-executable).
Constructs a new Import Directory containing:
  - All existing descriptors from vanilla Zuma.exe
  - New descriptor for zumaextrapath.dll (ZEP_BindExtras, ZEP_AddHole, ZEP_SetHolePercent, ZEP_SetHoleSomething, ZEP_LevelStart)
  - New descriptors for zumawebpbg.dll, zumawebpsprite.dll, and zumadisplay.dll
  - Null terminator descriptor
  - ILT, IAT, Hint/Name tables, and DLL name strings
  - Reserved space for 11-ball color data cave relocation
Points DataDirectory[1] to the new Import Directory.
Zeros out Bound Import Directory (DataDirectory[11]).
Keeps original Entry Point intact (NO entry point hijacking).
"""

import sys
import os
import struct
import capstone

sys.path.append(os.path.dirname(__file__))
from pe_util import PEFile, align
from site_resolver import resolve_all_sites, HOLEMGR_MAX_HOLES

NEW_DLLS = [
    ('zumadllchecker.dll', [
        'ZDC_RunChecker'
    ]),
    ('zumaextrapath.dll', [
        'ZEP_BindExtras',
        'ZEP_AddHole',
        'ZEP_SetHolePercent',
        'ZEP_SetHoleSomething',
        'ZEP_ExtraCount',
        'ZEP_GetCurveDesc',
        'ZEP_CheckHolePriority'
    ]),
    ('zumawebpbg.dll', [
        'Anim_PreDraw',
        'Anim_LoadImage'
    ]),
    ('zumawebpsprite.dll', [
        'ZSP_AnimateSprites',
        'ZSP_IsActive'
    ]),
    ('zumaextracolor.dll', [
        'ZEC_ExtractAll',
        'ZEC_GetBallImage',
        'ZEC_GetBombImage',
        'ZEC_GetLightImage',
        'ZEC_GetPowerupImage',
        'ZEC_ApplyColors'
    ]),
    ('zumacustommap.dll', [
        'zcm_AdjustCount',
        'zcm_GetSrcId',
        'zcm_GetSrcName',
        'zcm_GetVPtr',
        'zcm_GetV',
        'zcm_PickRandomLevel',
        'zcm_PickGauntletLevel'
    ]),
    ('zumacplxcredit.dll', [
        'ZCC_GetCreditsTable',
        'ZCC_PreDrawCreditLine',
        'ZCC_OnMouseDown'
    ]),
    ('zumatestingkit.dll', [
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
    ]),
    ('zumadisplay.dll', [
        'ZDISP_IsActive',
        'ZDISP_ScreenToGameCoords'
    ])
]

LITE_EXCLUDED = {
    'zumaextrapath.dll',
    'zumawebpbg.dll',
    'zumawebpsprite.dll',
    'zumaextracolor.dll'
}

# Section characteristics: IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_CNT_INITIALIZED_DATA
ZDATA_CHARS = 0xC0000040
ZDATA_RESERVE_SIZE = 0x2000  # 8192 bytes (leaves >7KB for data cave relocation)

def patch_imports(src_path, dst_path, is_lite=False):
    pe = PEFile(src_path)

    active_dlls = [d for d in NEW_DLLS if not (is_lite and d[0] in LITE_EXCLUDED)]
    if is_lite:
        print(f"[INFO] Lite Edition: {len(active_dlls)} sidecars enabled (excluded: {', '.join(sorted(LITE_EXCLUDED))})")

    # Check idempotency guard
    if pe.find_section_by_name('.zdata'):
        # Already patched
        print("[INFO] Section .zdata already present in executable.")
        if src_path != dst_path:
            pe.save(dst_path)
        return

    # 1. Parse existing import descriptors
    imp_dir = pe.data_directories[1]
    imp_rva = imp_dir['rva']
    imp_off = pe.rva_to_offset(imp_rva)
    if imp_off is None:
        sys.exit("[ABORT] Cannot locate existing import directory in PE.")

    old_descs = []
    while True:
        desc_bytes = bytes(pe.data[imp_off + len(old_descs) * 20 : imp_off + (len(old_descs) + 1) * 20])
        oft, ts, fc, name_rva, ft = struct.unpack_from('<IIIII', desc_bytes)
        if oft == 0 and ts == 0 and fc == 0 and name_rva == 0 and ft == 0:
            break
        old_descs.append(desc_bytes)

    print(f"[OK] Found {len(old_descs)} existing import descriptors.")

    # Calculate bounds of original primary IAT (preserving existing DataDirectory[12] if valid)
    existing_iat = pe.data_directories[12] if len(pe.data_directories) > 12 else None
    if existing_iat and existing_iat['rva'] != 0 and existing_iat['size'] != 0:
        orig_iat_rva = existing_iat['rva']
        orig_iat_size = existing_iat['size']
    else:
        # Fallback: calculate IAT bounds strictly within the primary IAT section (e.g. .rdata)
        # to prevent spanning across intermediate writable sections (like .data or GARR).
        primary_sec = None
        for d in old_descs:
            oft, ts, fc, name_rva, ft = struct.unpack_from('<IIIII', d)
            if ft != 0:
                for s in pe.sections:
                    if s.virtual_address <= ft < s.virtual_address + max(s.virtual_size, s.raw_size):
                        primary_sec = s
                        break
                if primary_sec:
                    break

        orig_iat_rvas = []
        for d in old_descs:
            oft, ts, fc, name_rva, ft = struct.unpack_from('<IIIII', d)
            if ft != 0 and primary_sec:
                if primary_sec.virtual_address <= ft < primary_sec.virtual_address + max(primary_sec.virtual_size, primary_sec.raw_size):
                    orig_iat_rvas.append(ft)
                    ft_off = pe.rva_to_offset(ft)
                    if ft_off is not None:
                        cur_off = ft_off
                        while True:
                            entry = struct.unpack_from('<I', pe.data, cur_off)[0]
                            cur_off += 4
                            if entry == 0:
                                break
                        orig_iat_rvas.append(ft + (cur_off - ft_off))

        if orig_iat_rvas:
            orig_iat_rva = min(orig_iat_rvas)
            orig_iat_size = max(orig_iat_rvas) - orig_iat_rva
        else:
            orig_iat_rva, orig_iat_size = 0, 0

    # 2. Determine new section RVA
    last_sec = max(pe.sections, key=lambda s: s.virtual_address)
    new_rva = align(last_sec.virtual_address + max(last_sec.virtual_size, last_sec.raw_size), pe.section_alignment)

    # 3. Build .zdata content
    blob = bytearray()

    # Descriptors: old_descs (11) + null terminator (1) + new_dlls (5) + null terminator (1)
    num_descs = len(old_descs) + 1 + len(active_dlls) + 1
    desc_table_size = num_descs * 20
    blob.extend(b'\x00' * desc_table_size)

    # Copy old descriptors into blob
    for i, d in enumerate(old_descs):
        blob[i * 20 : (i + 1) * 20] = d
    # Descriptor at index len(old_descs) is 20 zeros (null terminator for Windows loader)

    # Place DLL names
    dll_name_rvas = {}
    for dll_name, _ in active_dlls:
        dll_name_rvas[dll_name] = new_rva + len(blob)
        blob.extend(dll_name.encode('ascii') + b'\x00')
    while len(blob) % 4 != 0:
        blob.append(0)

    # Place Hint/Name entries
    hint_name_rvas = {}
    for dll_name, funcs in active_dlls:
        hint_name_rvas[dll_name] = {}
        for fn in funcs:
            hint_name_rvas[dll_name][fn] = new_rva + len(blob)
            hn = struct.pack('<H', 0) + fn.encode('ascii') + b'\x00'
            if len(hn) % 2 != 0:
                hn += b'\x00'
            blob.extend(hn)
    while len(blob) % 4 != 0:
        blob.append(0)

    # Place ILT and IAT entries
    ilt_rvas = {}
    iat_rvas = {}
    for dll_name, funcs in active_dlls:
        ilt_rvas[dll_name] = new_rva + len(blob)
        for fn in funcs:
            blob.extend(struct.pack('<I', hint_name_rvas[dll_name][fn]))
        blob.extend(struct.pack('<I', 0))  # null terminator

        iat_rvas[dll_name] = new_rva + len(blob)
        for fn in funcs:
            blob.extend(struct.pack('<I', 0))  # zero-init IAT slot for optional dynamic loading
        blob.extend(struct.pack('<I', 0))  # null terminator

    # Fill new descriptors in the descriptor table starting after the null terminator
    cur_desc_idx = len(old_descs) + 1
    for dll_name, _ in active_dlls:
        desc = struct.pack('<IIIII',
                           ilt_rvas[dll_name],
                           0, 0,
                           dll_name_rvas[dll_name],
                           iat_rvas[dll_name])
        blob[cur_desc_idx * 20 : (cur_desc_idx + 1) * 20] = desc
        cur_desc_idx += 1

    # Pad blob to reserve size
    if len(blob) < ZDATA_RESERVE_SIZE:
        blob.extend(b'\x00' * (ZDATA_RESERVE_SIZE - len(blob)))

    # 3.5 Write ZEP_EngineConfig into .zdata @ offset 0xC00 (safe margin after imports and before ExtraColor cave @ 0x1000)
    ZEP_CFG_OFFSET = 0xC00
    eng_sites = resolve_all_sites(src_path, enforce_hash=False)
    image_base = pe.image_base

    # ZEP_EngineConfig layout (21 dwords = 84 bytes):
    #   uint32_t magic;              // 0x4350455A ("ZEPC")
    #   uint32_t version;            // 3
    #   uint32_t struct_size;        // 84 (0x54)
    #   uint32_t rva_curve_ctor;     // Curve::Curve
    #   uint32_t rva_curvedesc_ctor; // CurveDesc ctor
    #   uint32_t rva_str_assign;     // std::string::assign
    #   uint32_t rva_curve_init;     // Curve::Init
    #   uint32_t rva_op_new;         // op_new
    #   uint32_t rva_add_hole;       // AddHole (STAGE2_HOLE_HOOK3)
    #   uint32_t rva_set_percent;    // SetHolePercent (STAGE2_HOLE_HOOK1)
    #   uint32_t rva_set_something;  // SetHoleSomething (STAGE2_HOLE_HOOK2)
    #   uint32_t sizeof_curve;       // 0x84
    #   uint32_t max_holes;          // 10 (HOLEMGR_MAX_HOLES)
    #   uint32_t rva_draw_image_rect;    // Graphics::DrawImage(rect)
    #   uint32_t rva_img_dialog_button;  // Pointer to IMAGE_DIALOG_BUTTON
    #   uint32_t rva_font_title;         // Pointer to FONT_TITLE
    #   uint32_t rva_font_header;        // Pointer to FONT_HEADER
    #   uint32_t rva_font_name;          // Pointer to FONT_NAME
    #   uint32_t rva_credits_count;      // Pointer to credits line count
    #   uint32_t rva_credits_table_orig; // Original credits table
    #   uint32_t rva_zuma_app;           // ZumaApp instance pointer
    #   uint32_t rva_font_dialog;        // Pointer to FONT_DIALOG
    #   uint32_t rva_graphics_set_color; // Graphics::SetColor
    cfg_bytes = struct.pack(
        '<23I',
        0x4350455A,                                  # magic "ZEPC"
        4,                                           # version (v4)
        92,                                          # struct_size (0x5C)
        eng_sites['CURVE_CTOR'] - image_base,        # rva_curve_ctor
        eng_sites['CURVEDESC_CTOR'] - image_base,    # rva_curvedesc_ctor
        eng_sites['STR_ASSIGN'] - image_base,        # rva_str_assign
        eng_sites['CURVE_INIT'] - image_base,        # rva_curve_init
        eng_sites['STAGE1_OPNEW'] - image_base,      # rva_op_new
        eng_sites['STAGE2_HOLE_HOOK3'] - image_base, # rva_add_hole (0x00402860)
        eng_sites['STAGE2_HOLE_HOOK1'] - image_base, # rva_set_percent (0x00402790)
        eng_sites['STAGE2_HOLE_HOOK2'] - image_base, # rva_set_something (0x00402810)
        0x84,                                        # sizeof_curve
        HOLEMGR_MAX_HOLES,                           # max_holes (10)
        eng_sites['DRAW_IMAGE_RECT'] - image_base,   # rva_draw_image_rect
        eng_sites['IMAGE_DIALOG_BUTTON_PTR'] - image_base, # rva_img_dialog_button
        eng_sites['FONT_TITLE'] - image_base,        # rva_font_title
        eng_sites['FONT_HEADER'] - image_base,       # rva_font_header
        eng_sites['FONT_NAME'] - image_base,         # rva_font_name
        eng_sites['CREDITS_COUNT_PTR'] - image_base, # rva_credits_count
        eng_sites['ORIGINAL_CREDITS_TABLE'] - image_base, # rva_credits_table_orig
        eng_sites['ZUMA_APP_PTR'] - image_base,      # rva_zuma_app
        eng_sites['FONT_DIALOG_PTR'] - image_base,   # rva_font_dialog
        eng_sites['GRAPHICS_SET_COLOR'] - image_base # rva_graphics_set_color
    )
    blob[ZEP_CFG_OFFSET : ZEP_CFG_OFFSET + len(cfg_bytes)] = cfg_bytes
    print(f"[OK] ZEP_EngineConfig (v4, {len(cfg_bytes)} bytes, max_holes={HOLEMGR_MAX_HOLES}) written to .zdata @ +0x{ZEP_CFG_OFFSET:X}")

    # 4. Add section .zdata
    new_sec = pe.add_section('.zdata', blob, ZDATA_CHARS, virtual_size=ZDATA_RESERVE_SIZE)
    print(f"[OK] Section .zdata added: RVA=0x{new_sec.virtual_address:08X}, "
          f"VSz=0x{new_sec.virtual_size:X}, Chars=0x{new_sec.characteristics:08X}")

    # 5. Update DataDirectories:
    # Size given to DataDirectory[1] is (len(old_descs) + 1) * 20 so Windows loader stops at the null descriptor!
    pe.set_data_directory(1, new_rva, (len(old_descs) + 1) * 20)
    if len(pe.data_directories) > 11:
        pe.set_data_directory(11, 0, 0)  # Bound Import Directory
    if len(pe.data_directories) > 12 and orig_iat_size > 0:
        # Crucial for UPX-unpacked executables (e.g. Aizad): UPX zeroes DataDirectory[12].
        # When Import Directory is relocated to .zdata, the Windows loader requires
        # DataDirectory[12] (IAT) to be set so it temporarily unprotects the original IAT pages during loading.
        pe.set_data_directory(12, orig_iat_rva, orig_iat_size)
        print(f"[OK] Ensured DataDirectory[12] (IAT): RVA=0x{orig_iat_rva:08X}, Size=0x{orig_iat_size:X}")

    # 6. Build section .zldr for dynamic loading from modules/
    iat_loadlib = pe.find_iat('kernel32.dll', 'LoadLibraryA')
    iat_getproc = pe.find_iat('kernel32.dll', 'GetProcAddress')
    iat_exitproc = pe.find_iat('kernel32.dll', 'ExitProcess')
    iat_msgbox = pe.find_iat('user32.dll', 'MessageBoxA')
    if not iat_loadlib or not iat_getproc or not iat_exitproc:
        raise RuntimeError("Could not dynamically resolve LoadLibraryA, GetProcAddress, or ExitProcess from kernel32.dll IAT")

    ep_rva = pe.entry_point_rva
    ep_va = pe.image_base + ep_rva
    off_ep = pe.rva_to_offset(ep_rva)
    if off_ep is None:
        raise RuntimeError("Cannot resolve Entry Point offset in PE")

    # Decode instructions at Entry Point until at least 5 bytes are covered
    cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    ep_sample = bytes(pe.data[off_ep : off_ep + 32])
    displaced_len = 0
    for ins in cs.disasm(ep_sample, ep_va):
        displaced_len += ins.size
        if displaced_len >= 5:
            break
    if displaced_len < 5:
        raise RuntimeError(f"Could not decode >= 5 bytes from Entry Point (got {displaced_len})")

    disp_ep_bytes = bytes(pe.data[off_ep : off_ep + displaced_len])
    print(f"[OK] Entry Point displaced: {displaced_len} bytes ({disp_ep_bytes.hex(' ')})")

    last_sec = max(pe.sections, key=lambda s: s.virtual_address)
    zldr_rva = align(last_sec.virtual_address + max(last_sec.virtual_size, last_sec.raw_size), pe.section_alignment)
    zldr_va = pe.image_base + zldr_rva

    raw_zldr = bytearray()
    raw_zldr.extend(b"\xE9\x00\x00\x00\x00")  # jmp to code_start

    str_kernel32 = zldr_va + len(raw_zldr); raw_zldr.extend(b"kernel32.dll\x00")
    str_setdlldir = zldr_va + len(raw_zldr); raw_zldr.extend(b"SetDllDirectoryA\x00")
    str_modules = zldr_va + len(raw_zldr); raw_zldr.extend(b"modules\x00")
    str_checker_dll = zldr_va + len(raw_zldr); raw_zldr.extend(b"modules\\zumadllchecker.dll\x00")
    str_checker_func = zldr_va + len(raw_zldr); raw_zldr.extend(b"ZDC_RunChecker\x00")
    str_checker_err_title = zldr_va + len(raw_zldr); raw_zldr.extend(b"ZuCompleluxe Integrity Error\x00")
    str_checker_err_msg = zldr_va + len(raw_zldr); raw_zldr.extend(b"Fatal Error: modules\\zumadllchecker.dll is missing or corrupt!\nZuma Deluxe (ZuCompleluxe) cannot run without this required core module.\x00")

    dll_str_vas = {}
    func_str_vas = {}
    for dll, funcs in active_dlls:
        dll_rel_path = f"modules\\{dll}".encode('ascii') + b"\x00"
        dll_str_vas[dll] = zldr_va + len(raw_zldr)
        raw_zldr.extend(dll_rel_path)
        func_str_vas[dll] = {}
        for f in funcs:
            func_str_vas[dll][f] = zldr_va + len(raw_zldr)
            raw_zldr.extend(f.encode('ascii') + b"\x00")

    while len(raw_zldr) % 4 != 0: raw_zldr.append(0)

    # DLL Table:
    table_va = zldr_va + len(raw_zldr)
    for dll, funcs in active_dlls:
        raw_zldr.extend(struct.pack('<II', dll_str_vas[dll], len(funcs)))
        for idx, f in enumerate(funcs):
            slot_va = pe.image_base + iat_rvas[dll] + idx * 4
            raw_zldr.extend(struct.pack('<II', func_str_vas[dll][f], slot_va))
    raw_zldr.extend(struct.pack('<I', 0))  # null terminator
    while len(raw_zldr) % 4 != 0: raw_zldr.append(0)

    code_start_offset = len(raw_zldr)
    code_start_va = zldr_va + code_start_offset
    struct.pack_into('<i', raw_zldr, 1, code_start_offset - 5)

    code_b = bytearray()
    # pushad (0x60), pushfd (0x9C) - Preserve all registers and flags
    code_b.append(0x60)
    code_b.append(0x9C)

    # 1. Call SetDllDirectoryA("modules")
    code_b += b"\x68" + struct.pack("<I", str_kernel32)
    code_b += b"\xFF\x15" + struct.pack("<I", iat_loadlib)
    code_b += b"\x68" + struct.pack("<I", str_setdlldir)
    code_b += b"\x50"
    code_b += b"\xFF\x15" + struct.pack("<I", iat_getproc)
    code_b += b"\x85\xC0"  # test eax, eax
    jz_skip_set = len(code_b); code_b += b"\x74\x00"
    code_b += b"\x68" + struct.pack("<I", str_modules)
    code_b += b"\xFF\xD0"  # call eax (SetDllDirectoryA("modules"))
    code_b[jz_skip_set + 1] = len(code_b) - (jz_skip_set + 2)

    # 2. Hard Gatekeeper: modules\zumadllchecker.dll MUST exist and succeed!
    code_b += b"\x68" + struct.pack("<I", str_checker_dll)
    code_b += b"\xFF\x15" + struct.pack("<I", iat_loadlib)
    code_b += b"\x85\xC0"  # test eax, eax
    jz_fatal_checker = len(code_b); code_b += b"\x0F\x84\x00\x00\x00\x00"  # jz .fatal_checker

    code_b += b"\x89\xC3"  # mov ebx, eax (hChecker)
    code_b += b"\x68" + struct.pack("<I", str_checker_func)
    code_b += b"\x53"      # push ebx
    code_b += b"\xFF\x15" + struct.pack("<I", iat_getproc)
    code_b += b"\x85\xC0"  # test eax, eax
    jz_fatal_proc = len(code_b); code_b += b"\x0F\x84\x00\x00\x00\x00"     # jz .fatal_checker

    code_b += b"\xFF\xD0"  # call eax (ZDC_RunChecker())
    code_b += b"\x85\xC0"  # test eax, eax
    jz_user_quit = len(code_b); code_b += b"\x0F\x84\x00\x00\x00\x00"      # jz .user_quit

    # 3. User chose RUN! Proceed with module table loading
    # mov esi, offset table_va
    code_b += b"\xBE" + struct.pack("<I", table_va)

    # .dll_loop:
    dll_loop_off = len(code_b)
    code_b += b"\x8B\x06"  # mov eax, [esi]
    code_b += b"\x85\xC0"  # test eax, eax
    jz_done = len(code_b); code_b += b"\x74\x00"
    code_b += b"\x50"      # push eax (dll_path)
    code_b += b"\xFF\x15" + struct.pack("<I", iat_loadlib)  # call LoadLibraryA
    code_b += b"\x89\xC3"  # mov ebx, eax (hModule)
    code_b += b"\x8B\x4E\x04"  # mov ecx, [esi + 4] (func_count)
    code_b += b"\x83\xC6\x08"  # add esi, 8

    # .func_loop:
    func_loop_off = len(code_b)
    code_b += b"\x85\xC9"  # test ecx, ecx
    jz_next_dll = len(code_b); code_b += b"\x74\x00"
    code_b += b"\x85\xDB"  # test ebx, ebx
    jz_skip_func = len(code_b); code_b += b"\x74\x00"

    code_b += b"\x51"      # push ecx
    code_b += b"\xFF\x36"  # push dword ptr [esi] (func_name)
    code_b += b"\x53"      # push ebx (hModule)
    code_b += b"\xFF\x15" + struct.pack("<I", iat_getproc)  # call GetProcAddress
    code_b += b"\x59"      # pop ecx
    code_b += b"\x8B\x56\x04"  # mov edx, [esi + 4] (iat_slot_va)
    code_b += b"\x89\x02"  # mov [edx], eax (*iat_slot_va = proc)

    # .skip_func:
    code_b[jz_skip_func + 1] = len(code_b) - (jz_skip_func + 2)
    code_b += b"\x83\xC6\x08"  # add esi, 8
    code_b += b"\x49"          # dec ecx
    rel_func = func_loop_off - (len(code_b) + 2)
    code_b += b"\xEB" + struct.pack("b", rel_func)

    # .next_dll target
    code_b[jz_next_dll + 1] = len(code_b) - (jz_next_dll + 2)
    rel_dll = dll_loop_off - (len(code_b) + 2)
    code_b += b"\xEB" + struct.pack("b", rel_dll)

    # .done target
    code_b[jz_done + 1] = len(code_b) - (jz_done + 2)
    code_b += b"\x9D"  # popfd
    code_b += b"\x61"  # popad
    code_b += disp_ep_bytes  # execute displaced instruction(s) from Entry Point
    # jmp ep_va + displaced_len:
    rel_back = (ep_va + displaced_len) - (code_start_va + len(code_b) + 5)
    code_b += b"\xE9" + struct.pack("<i", rel_back)

    # .fatal_checker target
    off_fatal_checker = len(code_b)
    struct.pack_into('<i', code_b, jz_fatal_checker + 2, off_fatal_checker - (jz_fatal_checker + 6))
    struct.pack_into('<i', code_b, jz_fatal_proc + 2, off_fatal_checker - (jz_fatal_proc + 6))

    if iat_msgbox:
        # MessageBoxA(NULL, str_checker_err_msg, str_checker_err_title, MB_ICONERROR | MB_OK = 0x10)
        code_b += b"\x6A\x10"                                         # push 0x10
        code_b += b"\x68" + struct.pack("<I", str_checker_err_title)  # push title
        code_b += b"\x68" + struct.pack("<I", str_checker_err_msg)    # push msg
        code_b += b"\x6A\x00"                                         # push 0 (hWnd)
        code_b += b"\xFF\x15" + struct.pack("<I", iat_msgbox)        # call MessageBoxA

    # ExitProcess(1)
    code_b += b"\x6A\x01"                                             # push 1
    code_b += b"\xFF\x15" + struct.pack("<I", iat_exitproc)          # call ExitProcess

    # .user_quit target
    off_user_quit = len(code_b)
    struct.pack_into('<i', code_b, jz_user_quit + 2, off_user_quit - (jz_user_quit + 6))
    # ExitProcess(0)
    code_b += b"\x6A\x00"                                             # push 0
    code_b += b"\xFF\x15" + struct.pack("<I", iat_exitproc)          # call ExitProcess

    raw_zldr.extend(code_b)
    pe.add_section('.zldr', bytes(raw_zldr), 0x60000020)
    print(f"[OK] Section .zldr added ({len(raw_zldr)} bytes, loader entry @ 0x{code_start_va:08X})")

    # Patch Entry Point @ ep_va:
    rel_ep = code_start_va - (ep_va + 5)
    patch_ep = bytearray(b"\xE9" + struct.pack("<i", rel_ep))
    if displaced_len > 5:
        patch_ep.extend(b"\x90" * (displaced_len - 5))
    pe.data[off_ep : off_ep + displaced_len] = patch_ep
    print(f"[PATCH] Entry Point @ 0x{ep_va:08X} -> jmp 0x{code_start_va:08X} ({displaced_len} bytes patched)")

    # Save output
    pe.save(dst_path)
    print(f"[DONE] Imports and .zldr module loader successfully injected -> {dst_path}")


if __name__ == "__main__":
    is_lite = "--lite" in sys.argv
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 2:
        sys.exit("Usage: python patch_imports.py <input_exe> <output_exe> [--lite]")
    try:
        patch_imports(args[0], args[1], is_lite=is_lite)
    except Exception as e:
        print("\n" + "=" * 67)
        print(" [COMPATIBILITY ERROR] Failed to detect required import structure.")
        print(f" Detail: {e}")
        print("=" * 67)
        print(" [WARNING]")
        if "Section .text not found" in str(e):
            print(" The binary appears to be packed, compressed, or heavily obfuscated.")
            print(" Note: ZuCompleluxe automatically decompresses standard UPX-packed binaries.")
            print(" If this is a packed binary, ensure builder\\ensure_unpacked.py ran properly.")
        else:
            print(" The provided executable does not have the expected structure or attributes.")
        print(" Please provide a compatible Zuma.exe binary.")
        print(" (RECOMMENDED: Zuma Deluxe v1.0.0.1 GameHouse / PopCap Retail)")
        print("=" * 67 + "\n")
        sys.exit(1)
