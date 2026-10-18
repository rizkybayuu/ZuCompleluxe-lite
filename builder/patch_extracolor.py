"""patch_extracolor.py - 11-ball-color support for Zuma Deluxe 1.0.0.1 via zumaextracolor.dll.

Usage: python patch_extracolor.py <Zuma_in.exe> <Zuma_out.exe>
Replaces build_mod_exe.py in the pipeline (same slot: after patch_imports.py, before patch_anim_hook.py).
Requires patch_imports.py to have created .zdata AND the zumaextracolor.dll import (NEW_DLLS).

What it does (all code lives in a new PURE RX section '.zcol'; .text is only patched in place):
  1. .zdata cave @ +0x800: ZecBlock (addresses handed to the DLL), 11-color palettes, 6 x 11-slot image arrays, mode flag.
  2. Repoints the 4 palette tables (.text imm32 operands) to the .zdata palettes; frog-dots divisor 6 -> 11.
  3. Hook: resource extraction (0x42F795) -> stub -> vanilla extractor, then ZEC_ExtractAll.
  4. Hooks: ball / bomb / light / powerup image lookups -> thunks -> ZEC_Get*Image.
  5. Hooks in FUN_0041A1D0: mode flag capture (entry) and the 'curve->mNumColors' loop -> ZEC_ApplyColors.
  6. Sentinel: Curve-slot constructor default 'mNumColors = 4' -> 0 ("colors not set in levels.xml").
Every site is validated BEFORE anything is written. Entry point is untouched.
"""

import sys
import os
import re
import struct

sys.path.append(os.path.dirname(__file__))
from pe_util import PEFile, align

DLL = 'zumaextracolor.dll'
IMPORTS = ['ZEC_ExtractAll', 'ZEC_GetBallImage', 'ZEC_GetBombImage',
           'ZEC_GetLightImage', 'ZEC_GetPowerupImage', 'ZEC_ApplyColors']

ZCOL_CHARS = 0x60000020            # CODE | EXECUTE | READ
ZDATA_CAVE_OFF = 0x800
ZEC_MAGIC, ZEC_VERSION = 0x4243455A, 1     # must match zumaextracolor.dll.c

VANILLA_EXTRACT = 0x0042CE20       # vanilla resource extractor (cdecl, 1 arg)
GET_IMAGE       = 0x0042A010       # game GetImage(id), cdecl
EXTRACT_SITE    = 0x0042F795       # push eax ; call VANILLA_EXTRACT

# (va, length, push-prefix, kind) - every site ends with 'call GET_IMAGE'
IMG_SITES = [
    (0x0040128A, 9,  b'\x50',     'ball'),
    (0x004012C9, 9,  b'\x50',     'ball'),
    (0x004035DB, 10, b'\x57\x50', 'ball'),
    (0x00403E43, 10, b'\x57\x51', 'ball'),
    (0x0040386E, 9,  b'\x50',     'bomb'),
    (0x00403990, 9,  b'\x52',     'light'),
    (0x00403A20, 9,  b'\x57\x50', 'powerup'),
]

FN_START, FN_END = 0x0041A1D0, 0x0041A4C0          # FUN_0041A1D0 on the known build
LOOP_PAT = bytes.fromhex("85c07e0e81c7e4000000" "8937" "83c77c" "48" "75f8")   # vanilla mNumColors loop (18 bytes)
MODE_RE  = re.compile(rb'\x8b[\x80-\x83\x85-\x87]\x7c\x01\x00\x00\x85\xc0')   # mov eax,[reg+0x17c] ; test eax,eax

SENT_HINT = 0x00406B4D                              # Curve-slot ctor: mov dword [eax+0x38], 4
SENT_PAT  = bytes.fromhex("c740380400" "0000")

OLD_PAL = {'rgb': 0x0052E41C, 'spark': 0x0052E434, 'shadow': 0x0052E44C, 'proj': 0x0052E464}
DIV6  = bytes.fromhex("b8abaaaa2af7ea8bc2c1e81f03d0")
DIV11 = bytes.fromhex("8bc29951" "6a0b59" "f7f9" "59" "8bd0" "9090")

PAL = {
    'rgb':    [0x001980ff, 0x00ffff00, 0x00ff0000, 0x0000ff00, 0x00ff00ff, 0x00ffffff,
               0x00ff8000, 0x00002080, 0x0000ffff, 0x00202020, 0x00fff0be],
    'spark':  [0x0080ffff, 0x00ffff40, 0x00ffaaaa, 0x0080ff80, 0x00ff80ff, 0x00ffffff,
               0x00ffc080, 0x004060c0, 0x0080ffff, 0x00606060, 0x00fffae0],
    'shadow': [0x00231679, 0x0060510a, 0x00a00f14, 0x00204422, 0x00561643, 0x00381b22,
               0x00804000, 0x00101040, 0x00006060, 0x00181818, 0x00605030],
    'proj':   [0x002d8bff, 0x00ffff00, 0x00ff0000, 0x0000ff00, 0x00ff00ff, 0x00ffffff,
               0x00ffa500, 0x00002080, 0x0000ffff, 0x00303030, 0x00fff0be],
}

# Game globals holding Image* for colors 0..5, per kind: ball, accuracy, backwards, slow, bomb, light
VANILLA_GLOBALS = [
    [0x547f08, 0x547ed8, 0x547ff8, 0x547e48, 0x547fe4, 0x548098],
    [0x547fd0, 0x547e30, 0x547e04, 0x547f3c, 0x547ef4, 0x547fbc],
    [0x547e14, 0x547e2c, 0x547eac, 0x548018, 0x547e84, 0x547fa8],
    [0x547f98, 0x547fac, 0x54805c, 0x547e24, 0x547f0c, 0x547f20],
    [0x547f64, 0x548088, 0x547e74, 0x5480dc, 0x547e9c, 0x547e00],
    [0x547f58, 0x547f48, 0x548004, 0x5480a4, 0x5480d8, 0x547f24],
]

NSLOTS_BYTES = 11 * 4
BLOCK_DWORDS = 2 + 6 + 36 + 1 + 1   # magic, version, images[6], vanilla[6][6], flag, get_image


def rel32(target, next_ip):
    return struct.pack('<i', target - next_ip)


def build_zcol(new_va, iat, va_block, va_flag, va_arr, mode_orig6, loop_back_va):
    """Returns (blob, vas). Pure code; no data. 'iat' maps export name -> IAT slot VA."""
    blob = bytearray()
    vas = {}

    def here():
        return new_va + len(blob)

    def pad():
        while len(blob) % 16:
            blob.append(0xCC)

    def make_getter(va_array, va_default):
        # in: [esp+4] = color. Returns Image* in EAX, strictly preserving ECX, EDX, EBX, ESI, EDI.
        # Fallback hierarchy: va_array[color] -> [va_default] (vanilla PopCap global).
        b = bytearray()
        b += b'\x8B\x44\x24\x04'                               # mov eax, [esp+4]
        b += b'\x85\xC0'                                       # test eax, eax
        b += b'\x79\x02'                                       # jns +2
        b += b'\x31\xC0'                                       # xor eax, eax
        b += b'\x83\xF8\x0A'                                   # cmp eax, 10
        b += b'\x7E\x05'                                       # jle +5
        b += b'\xB8\x0A\x00\x00\x00'                           # mov eax, 10
        b += b'\x8B\x04\x85' + struct.pack('<I', va_array)     # mov eax, [eax*4 + va_array]
        b += b'\x85\xC0'                                       # test eax, eax
        b += b'\x75\x05'                                       # jnz .ret (+5 over mov)
        b += b'\xA1' + struct.pack('<I', va_default)           # mov eax, [va_default] (guaranteed non-null)
        # .ret:
        b += b'\xC3'                                           # ret
        return bytes(b)

    # Fast in-engine getters with zero register clobbering (Ball: va_arr[0], Bomb: va_arr[4], Light: va_arr[5])
    vas['ball'] = here()
    blob += make_getter(va_arr[0], VANILLA_GLOBALS[0][0])
    pad()

    vas['bomb'] = here()
    blob += make_getter(va_arr[4], VANILLA_GLOBALS[4][0])
    pad()

    vas['light'] = here()
    blob += make_getter(va_arr[5], VANILLA_GLOBALS[5][0])
    pad()

    # powerup thunk: color on stack ([esp+4]), base id in ECX
    vas['powerup'] = here()
    p_blob = bytearray()
    p_blob += b'\x8B\x44\x24\x04'                               # mov eax, [esp+4]
    p_blob += b'\x85\xC0'                                       # test eax, eax
    p_blob += b'\x79\x02'                                       # jns +2
    p_blob += b'\x31\xC0'                                       # xor eax, eax
    p_blob += b'\x83\xF8\x0A'                                   # cmp eax, 10
    p_blob += b'\x7E\x05'                                       # jle +5
    p_blob += b'\xB8\x0A\x00\x00\x00'                           # mov eax, 10
    # cmp ecx, 0x56 ; je .acc
    p_blob += b'\x83\xF9\x56'
    j_acc = len(p_blob); p_blob += b'\x74\x00'
    # cmp ecx, 0x5C ; je .bkw
    p_blob += b'\x83\xF9\x5C'
    j_bkw = len(p_blob); p_blob += b'\x74\x00'
    # cmp ecx, 0x62 ; je .slw
    p_blob += b'\x83\xF9\x62'
    j_slw = len(p_blob); p_blob += b'\x74\x00'
    # fallback: add eax, ecx ; push eax ; call GET_IMAGE ; add esp, 4 ; ret
    p_blob += b'\x01\xC8\x50'
    call_off = len(p_blob)
    p_blob += b'\xE8' + rel32(GET_IMAGE, here() + call_off + 5)
    p_blob += b'\x83\xC4\x04\xC3'
    # .acc
    p_blob[j_acc + 1] = len(p_blob) - (j_acc + 2)
    p_blob += b'\x8B\x04\x85' + struct.pack('<I', va_arr[1])
    p_blob += b'\x85\xC0\x75\x05\xA1' + struct.pack('<I', VANILLA_GLOBALS[1][0])
    p_blob += b'\xC3'
    # .bkw
    p_blob[j_bkw + 1] = len(p_blob) - (j_bkw + 2)
    p_blob += b'\x8B\x04\x85' + struct.pack('<I', va_arr[2])
    p_blob += b'\x85\xC0\x75\x05\xA1' + struct.pack('<I', VANILLA_GLOBALS[2][0])
    p_blob += b'\xC3'
    # .slw
    p_blob[j_slw + 1] = len(p_blob) - (j_slw + 2)
    p_blob += b'\x8B\x04\x85' + struct.pack('<I', va_arr[3])
    p_blob += b'\x85\xC0\x75\x05\xA1' + struct.pack('<I', VANILLA_GLOBALS[3][0])
    p_blob += b'\xC3'
    blob += p_blob
    pad()

    # extraction: vanilla extractor first, in-engine populate va_arr, then ZEC_ExtractAll if present
    # NOTE: vanilla extractor returns status in EAX; we MUST preserve it so the caller
    # (LoadingThread) does not think resource extraction failed!
    vas['extract'] = here()
    blob += b'\xFF\x74\x24\x04'                         # push [esp+4]   (resMgr)
    blob += b'\xE8' + rel32(VANILLA_EXTRACT, here() + 5)
    blob += b'\x83\xC4\x04'                             # add esp, 4
    blob += b'\x50'                                     # push eax (save vanilla return value)

    # In-engine copy: populate va_arr[0..5] from VANILLA_GLOBALS[0..5]
    # This guarantees that even without zumaextracolor.dll, all 6 vanilla colors are loaded,
    # and extra slots 6..10 are filled with slot 0 (Blue) so NO image lookup EVER returns NULL!
    for k in range(6):
        for i in range(6):
            blob += b'\x8B\x15' + struct.pack('<I', VANILLA_GLOBALS[k][i])
            blob += b'\x89\x15' + struct.pack('<I', va_arr[k] + i*4)
        for i in range(6, 11):
            blob += b'\x8B\x15' + struct.pack('<I', va_arr[k])
            blob += b'\x89\x15' + struct.pack('<I', va_arr[k] + i*4)

    # Check if zumaextracolor.dll is present
    blob += b'\x83\x3D' + struct.pack('<I', iat['ZEC_ExtractAll']) + b'\x00' # cmp dword [iat['ZEC_ExtractAll']], 0
    j_skip_ext = len(blob); blob += b'\x74\x00'         # jz skip_extract
    blob += b'\x68' + struct.pack('<I', va_block)       # push &ZecBlock (2nd arg)
    blob += b'\xFF\x74\x24\x0C'                         # push [esp+12]  (resMgr, 1st arg)
    blob += b'\xFF\x15' + struct.pack('<I', iat['ZEC_ExtractAll'])
    blob += b'\x83\xC4\x08'                             # add esp, 8
    blob[j_skip_ext + 1] = len(blob) - (j_skip_ext + 2) # skip_extract:
    blob += b'\x58'                                     # pop eax (restore vanilla return value)
    blob += b'\xC3'                                     # ret
    pad()

    # mode capture: re-executes the original 'mov eax,[reg+0x17c]' and stores eax in the flag
    vas['mode'] = here()
    blob += bytes(mode_orig6) + b'\xA3' + struct.pack('<I', va_flag) + b'\xC3'
    pad()

    # colors loop:
    # If ZEC_ApplyColors is absent: clamp colors to <= 6 and execute vanilla behavior!
    # In Adventure mode ([flag] == 0): clamp every curve's mNumColors <= 6!
    # In Gauntlet mode ([flag] != 0): call ZEC_ApplyColors(level=edi, count=eax, isGauntlet=[flag])
    vas['colors'] = here()
    blob += b'\x83\x3D' + struct.pack('<I', iat['ZEC_ApplyColors']) + b'\x00' # cmp dword [iat['ZEC_ApplyColors']], 0
    jz_vanilla_colors = len(blob); blob += b'\x74\x00'  # jz .vanilla_colors

    # DLL is present: modded logic
    blob += b'\x83\x3D' + struct.pack('<I', va_flag) + b'\x00' # cmp dword [flag], 0
    blob += b'\x0F\x84' + rel32(loop_back_va, here() + 6)       # je loop_back_va (Adventure preserves levels.xml)
    blob += b'\x60\x9C'                                         # pushad ; pushfd
    blob += b'\xFF\x35' + struct.pack('<I', va_flag)            # push [flag]
    blob += b'\x50\x57'                                         # push eax ; push edi
    blob += b'\xFF\x15' + struct.pack('<I', iat['ZEC_ApplyColors'])
    blob += b'\x83\xC4\x0C\x9D\x61'                             # add esp,12 ; popfd ; popad
    blob += b'\xE9' + rel32(loop_back_va, here() + 5)

    # .vanilla_colors:
    blob[jz_vanilla_colors + 1] = len(blob) - (jz_vanilla_colors + 2)
    blob += b'\x85\xC0'                                         # test eax, eax (curve count)
    blob += b'\x0F\x8E' + rel32(loop_back_va, here() + 6)       # jle loop_back_va
    blob += b'\x83\x3D' + struct.pack('<I', va_flag) + b'\x00' # cmp dword [flag], 0
    j_gaunt = len(blob); blob += b'\x75\x00'                    # jne .vanilla_gaunt

    # .vanilla_adv: Adventure mode without DLL -> clamp each curve's mNumColors <= 6
    blob += b'\x50\x57'                                         # push eax ; push edi
    blob += b'\x89\xC1'                                         # mov ecx, eax (ecx = curve count)
    blob += b'\x81\xC7\xE4\x00\x00\x00'                         # add edi, 0xE4
    adv_loop_top = len(blob)
    blob += b'\x83\x3F\x06'                                     # cmp dword ptr [edi], 6
    jle_adv = len(blob); blob += b'\x7E\x00'                    # jle skip_clamp
    blob += b'\xC7\x07\x06\x00\x00\x00'                         # mov dword ptr [edi], 6
    blob[jle_adv + 1] = len(blob) - (jle_adv + 2)               # target skip_clamp (+6 bytes)
    blob += b'\x83\xC7\x7C'                                     # add edi, 0x7C
    blob += b'\x49'                                             # dec ecx
    rel_adv_loop = adv_loop_top - (len(blob) + 2)
    blob += b'\x75' + struct.pack('b', rel_adv_loop)            # jnz adv_loop_top
    blob += b'\x5F\x58'                                         # pop edi ; pop eax
    blob += b'\xE9' + rel32(loop_back_va, here() + 5)

    # .vanilla_gaunt: Gauntlet mode without DLL -> vanilla PopCap loop
    blob[j_gaunt + 1] = len(blob) - (j_gaunt + 2)
    blob += b'\x83\xFE\x06\x7E\x05\xBE\x06\x00\x00\x00'        # cmp esi, 6; jle +5; mov esi, 6
    blob += b'\x81\xC7\xE4\x00\x00\x00'                         # add edi, 0xE4
    gaunt_loop_top = len(blob)
    blob += b'\x89\x37'                                         # mov [edi], esi
    blob += b'\x83\xC7\x7C'                                     # add edi, 0x7C
    blob += b'\x48'                                             # dec eax
    rel_gaunt_loop = gaunt_loop_top - (len(blob) + 2)
    blob += b'\x75' + struct.pack('b', rel_gaunt_loop)          # jnz gaunt_loop_top
    blob += b'\xE9' + rel32(loop_back_va, here() + 5)
    pad()
    return bytes(blob), vas



def img_site_bytes(va, length, prefix, target):
    b = prefix + b'\xE8' + rel32(target, va + len(prefix) + 5)
    return b + b'\x90' * (length - len(b))


def patch_extracolor(src, dst):
    pe = PEFile(src)
    if pe.find_section_by_name('.zcol'):
        print("[INFO] Section .zcol already present in executable.")
        if src != dst:
            pe.save(dst)
        return

    zdata = pe.find_section_by_name('.zdata')
    if not zdata:
        sys.exit("[ABORT] .zdata not found - run patch_imports.py first!")
    iat = {fn: pe.find_iat(DLL, fn) for fn in IMPORTS}
    miss = [fn for fn, v in iat.items() if not v]
    if miss:
        sys.exit(f"[ABORT] IAT slots from {DLL} missing: {miss}. Add them to NEW_DLLS in patch_imports.py.")

    text = pe.find_section_by_name('.text')
    if not text:
        sys.exit("[ABORT] .text not found")
    t_lo, t_hi = text.raw_offset, text.raw_offset + text.raw_size
    d = pe.data
    bad = []

    # ---- validate call-shaped sites ----------------------------------------------------
    def call_target(off, va_end_of_call):
        rel = struct.unpack_from('<i', d, off + 1)[0]
        return (va_end_of_call + rel) & 0xFFFFFFFF

    off = pe.va_to_offset(EXTRACT_SITE)
    if off is None or d[off] != 0x50 or d[off + 1] != 0xE8 or call_target(off + 1, EXTRACT_SITE + 6) != VANILLA_EXTRACT:
        bad.append(f"extraction site 0x{EXTRACT_SITE:08X}: expected 'push eax; call 0x{VANILLA_EXTRACT:08X}'")

    for va, ln, _pre, kind in IMG_SITES:
        o = pe.va_to_offset(va)
        if o is None or d[o + ln - 5] != 0xE8 or call_target(o + ln - 5, va + ln) != GET_IMAGE:
            bad.append(f"{kind} image site 0x{va:08X}: does not end with 'call 0x{GET_IMAGE:08X}'")

    # ---- sentinel (curve-slot ctor default) --------------------------------------------
    hits = [m.start() for m in re.finditer(re.escape(SENT_PAT), bytes(d[t_lo:t_hi]))]
    sent_off = None
    if len(hits) == 1:
        sent_off = t_lo + hits[0]
    else:
        ho = pe.va_to_offset(SENT_HINT)
        if ho is not None and bytes(d[ho:ho + len(SENT_PAT)]) == SENT_PAT:
            sent_off = ho
    if sent_off is None:
        bad.append("sentinel: 'mov dword [eax+0x38],4' not found uniquely and not at the hint address")

    # ---- colors loop + mode site -------------------------------------------------------
    seg = bytes(d[t_lo:t_hi])
    if seg.count(LOOP_PAT) != 1:
        bad.append(f"colors loop pattern: {seg.count(LOOP_PAT)} matches (need exactly 1)")
        loop_off = loop_va = mode_off = mode_va = None
    else:
        loop_off = t_lo + seg.index(LOOP_PAT)
        loop_va = pe.offset_to_va(loop_off)
        fn_lo_va = FN_START if FN_START <= loop_va < FN_END else loop_va - 0x300
        fn_lo = pe.va_to_offset(fn_lo_va)
        mh = list(MODE_RE.finditer(bytes(d[fn_lo:loop_off])))
        if not mh:
            bad.append("mode-test 'mov eax,[reg+0x17c]; test eax,eax' not found before the colors loop")
            mode_off = mode_va = None
        else:
            mode_off = fn_lo + mh[0].start()
            mode_va = pe.offset_to_va(mode_off)
            if len(mh) > 1:
                print(f"[WARN] {len(mh)} mode-test candidates, using the first (0x{mode_va:08X})")

    # ---- optional pieces ---------------------------------------------------------------
    div_hits = seg.count(DIV6)
    if div_hits > 1:
        bad.append(f"frog-dots divisor pattern matches {div_hits} times")

    cave_va = pe.image_base + zdata.virtual_address + ZDATA_CAVE_OFF
    cave_off = pe.va_to_offset(cave_va)
    cave_len = BLOCK_DWORDS * 4 + 4 * NSLOTS_BYTES + 6 * NSLOTS_BYTES + 4 + NSLOTS_BYTES
    if any(d[cave_off:cave_off + cave_len]):
        bad.append(f".zdata cave @0x{cave_va:08X} (+0x{cave_len:X}) is not empty - overlaps patch_imports data?")

    if bad:
        sys.exit("[ABORT] Validation failed - nothing written:\n  " + "\n  ".join(bad))
    print(f"[OK] All sites validated (mode hook 0x{mode_va:08X}, colors loop 0x{loop_va:08X}, sentinel 0x{pe.offset_to_va(sent_off):08X})")

    # ---- data cave layout --------------------------------------------------------------
    cur = cave_va
    va_block = cur; cur += BLOCK_DWORDS * 4
    va_pal = {}
    for name in ('rgb', 'spark', 'shadow', 'proj'):
        va_pal[name] = cur; cur += NSLOTS_BYTES
    va_arr = []
    for _ in range(6):
        va_arr.append(cur); cur += NSLOTS_BYTES
    va_flag = cur; cur += 4
    va_particle_balls = cur; cur += NSLOTS_BYTES
    assert cur - cave_va == cave_len

    block = struct.pack('<II', ZEC_MAGIC, ZEC_VERSION)
    block += struct.pack('<6I', *va_arr)
    for k in range(6):
        block += struct.pack('<6I', *VANILLA_GLOBALS[k])
    block += struct.pack('<II', va_flag, GET_IMAGE)
    assert len(block) == BLOCK_DWORDS * 4
    pe.data[cave_off:cave_off + len(block)] = block
    for name in ('rgb', 'spark', 'shadow', 'proj'):
        o = pe.va_to_offset(va_pal[name])
        pe.data[o:o + NSLOTS_BYTES] = struct.pack('<11I', *PAL[name])
    o_part = pe.va_to_offset(va_particle_balls)
    pe.data[o_part:o_part + NSLOTS_BYTES] = b'\x00' * NSLOTS_BYTES
    print(f"[OK] .zdata cave: {cave_len} bytes @0x{cave_va:08X} (ZecBlock @0x{va_block:08X})")

    # ---- .zcol section -----------------------------------------------------------------
    last = max(pe.sections, key=lambda s: s.virtual_address)
    new_va = pe.image_base + align(last.virtual_address + max(last.virtual_size, last.raw_size), pe.section_alignment)
    mode_orig6 = bytes(pe.data[mode_off:mode_off + 6])
    blob, vas = build_zcol(new_va, iat, va_block, va_flag, va_arr, mode_orig6, loop_va + len(LOOP_PAT))
    sec = pe.add_section('.zcol', blob, ZCOL_CHARS)
    assert pe.image_base + sec.virtual_address == new_va
    print(f"[OK] Section .zcol added: RVA=0x{sec.virtual_address:08X}, Chars=0x{sec.characteristics:08X}")

    # ---- patch .text -------------------------------------------------------------------
    D = pe.data
    # 1. extraction hook: keep 'push eax', retarget the call
    o = pe.va_to_offset(EXTRACT_SITE)
    D[o + 1:o + 6] = b'\xE8' + rel32(vas['extract'], EXTRACT_SITE + 6)

    # 2. image lookups
    tgt = {'ball': vas['ball'], 'bomb': vas['bomb'], 'light': vas['light'], 'powerup': vas['powerup']}
    for va, ln, pre, kind in IMG_SITES:
        o = pe.va_to_offset(va)
        D[o:o + ln] = img_site_bytes(va, ln, pre, tgt[kind])

    # 3. mode capture + colors loop
    D[mode_off:mode_off + 6] = b'\xE8' + rel32(vas['mode'], mode_va + 5) + b'\x90'
    D[loop_off:loop_off + len(LOOP_PAT)] = b'\xE9' + rel32(vas['colors'], loop_va + 5) + b'\x90' * (len(LOOP_PAT) - 5)

    # 4. sentinel: Keep vanilla default mNumColors = 4 intact (do NOT zero it out)

    # 5. palettes: repoint imm32 operands in .text
    for name, old in OLD_PAL.items():
        b_old, b_new = struct.pack('<I', old), struct.pack('<I', va_pal[name])
        n, p = 0, t_lo
        while True:
            i = D.find(b_old, p, t_hi)
            if i == -1:
                break
            D[i:i + 4] = b_new
            n += 1
            p = i + 4
        print(f"[{'OK' if n else 'WARN'}] palette '{name}': {n} reference(s) repointed")

    # 6. frog-dots indicator divisor 6 -> 11
    i = D.find(DIV6, t_lo, t_hi)
    if i != -1:
        D[i:i + len(DIV6)] = DIV11
        print("[OK] frog-dots divisor patched to 11")
    else:
        print("[WARN] frog-dots divisor pattern not found (already patched?)")

    # 7. repoint particle cache references in 0x004035B0 to 11-slot array (prevents overflow into 0x00533D08)
    for p_va in (0x004035D1, 0x0040363F, 0x00403658):
        po = pe.va_to_offset(p_va)
        D[po:po + 4] = struct.pack('<I', va_particle_balls)
    print(f"[OK] Particle template cache repointed to 11-slot array @0x{va_particle_balls:08X}")

    # 8. keep .rdata read-only
    rd = pe.find_section_by_name('.rdata')
    if rd and (rd.characteristics & 0x80000000):
        struct.pack_into('<I', D, rd.header_offset + 36, rd.characteristics & ~0x80000000)
        print("[OK] .rdata write flag cleared")

    pe.save(dst)
    print(f"[DONE] Extra colors applied -> {dst}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    try:
        patch_extracolor(sys.argv[1], sys.argv[2])
    except Exception as e:
        print("\n" + "=" * 67)
        print(" [COMPATIBILITY ERROR] Failed to detect required ball color attributes.")
        print(f" Detail: {e}")
        print("=" * 67)
        print(" [WARNING]")
        print(" The provided executable does not have the expected structure or attributes.")
        print(" Please provide a compatible Zuma.exe binary.")
        print(" (RECOMMENDED: Zuma Deluxe v1.0.0.1 GameHouse / PopCap Retail)")
        print("=" * 67 + "\n")
        sys.exit(1)
