"""patch_anim_hook.py - Installs animated WebP hooks into Zuma.exe using static IAT imports.

Uses IAT slots from zumawebp.dll (injected by patch_imports.py in section .zdata).
Section .anim is PURE CODE with characteristics 0x60000020 (CODE | EXECUTE | READ).
No entry point hijacking or dynamic loader stubs.
"""

import sys
import os
import struct

sys.path.append(os.path.dirname(__file__))
from pe_util import PEFile, align

HOOK_VA      = 0x0040726D
HOOK_EXPECT  = bytes.fromhex("8B474C" "6A00" "6A00" "50" "8BCE" "E8")
HOOK_RET_VA  = HOOK_VA + 5
BITS_VA      = 0x0044CD10
BITS_EXPECT  = bytes.fromhex("56" "8BF1" "E8")
LOADIMG_VA   = 0x0049A660          # ImageLib::GetImage(std::string*, char alpha) - cdecl
EXPECT_SITES = 6

ANIM_CHARS   = 0x60000020          # IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ

def patch_anim_hook(src_path, dst_path):
    pe = PEFile(src_path)

    # Check idempotency guard
    if pe.find_section_by_name('.anim'):
        print("[INFO] Section .anim already present in executable.")
        if src_path != dst_path:
            pe.save(dst_path)
        return

    # 1. Lookup IAT slots for zumawebp.dll
    iat_predraw = pe.find_iat('zumawebp.dll', 'Anim_PreDraw')
    iat_loadimage = pe.find_iat('zumawebp.dll', 'Anim_LoadImage')
    if not iat_predraw or not iat_loadimage:
        sys.exit("[ABORT] IAT slots for zumawebp.dll not found. Run patch_imports.py first!")
    print(f"[OK] Found IAT Anim_PreDraw=0x{iat_predraw:08X}, Anim_LoadImage=0x{iat_loadimage:08X}")

    # 2. Validate hook site and BitsChanged
    hook_off = pe.va_to_offset(HOOK_VA)
    bits_off = pe.va_to_offset(BITS_VA)
    if hook_off is None or bits_off is None:
        sys.exit("[ABORT] Cannot resolve hook addresses in PE.")

    got = bytes(pe.data[hook_off:hook_off + len(HOOK_EXPECT)])
    if got != HOOK_EXPECT:
        sys.exit(f"[ABORT] Byte mismatch at 0x{HOOK_VA:08X}.\n  Got     : {got.hex(' ')}\n  Expected: {HOOK_EXPECT.hex(' ')}")

    got_bits = bytes(pe.data[bits_off:bits_off + len(BITS_EXPECT)])
    if got_bits != BITS_EXPECT:
        sys.exit(f"[ABORT] Prolog BitsChanged at 0x{BITS_VA:08X} mismatch: {got_bits.hex(' ')}")
    print("[OK] Hook site & BitsChanged validated")

    # 3. Find all 'call GetImage' in code sections
    sites = []
    for sec in pe.sections:
        if not (sec.characteristics & 0x20) or not sec.raw_size:
            continue
        n = min(sec.virtual_size, sec.raw_size) if sec.virtual_size else sec.raw_size
        seg = bytes(pe.data[sec.raw_offset : sec.raw_offset + n])
        i = seg.find(b"\xE8")
        while i != -1:
            if i + 5 <= n:
                rel = struct.unpack_from("<i", seg, i + 1)[0]
                va = pe.image_base + sec.virtual_address + i
                if ((va + 5 + rel) & 0xFFFFFFFF) == LOADIMG_VA:
                    sites.append((sec.raw_offset + i, va))
            i = seg.find(b"\xE8", i + 1)

    if not sites:
        sys.exit(f"[ABORT] No 'call 0x{LOADIMG_VA:08X}' found")
    print(f"[OK] {len(sites)} GetImage call sites found")

    # 4. Construct .anim section blob (Pure RX Code)
    last_sec = max(pe.sections, key=lambda s: s.virtual_address)
    new_rva = align(last_sec.virtual_address + max(last_sec.virtual_size, last_sec.raw_size), pe.section_alignment)
    new_va = pe.image_base + new_rva

    blob = bytearray()

    # Cave PreDraw
    va_cave = new_va + len(blob)
    c = bytearray()
    c += b"\x60\x9C"                                                        # pushad; pushfd
    c += b"\x8B\x47\x4C"                                                    # mov eax,[edi+4Ch]  (DDImage*)
    c += b"\x50\x57"                                                        # push eax; push edi (Level*)
    c += b"\xA1" + struct.pack("<I", iat_predraw)                           # mov eax,[iat_predraw]
    c += b"\x85\xC0\x74\x02\xFF\xD0"                                        # test eax,eax; jz +2; call eax
    c += b"\x83\xC4\x08"                                                    # add esp,8 (cdecl)
    c += b"\x9D\x61"                                                        # popfd; popad
    c += b"\x8B\x47\x4C\x6A\x00"                                            # original overwritten instructions
    c += b"\xE9" + struct.pack("<i", HOOK_RET_VA - (va_cave + len(c) + 5))
    blob += c
    while len(blob) % 16:
        blob += b"\xCC"

    # Cave LoadImage: cdecl, [esp+4]=string*, [esp+8]=alpha
    va_cave2 = new_va + len(blob)
    k = bytearray()
    k += b"\xA1" + struct.pack("<I", iat_loadimage)                         # mov eax,[iat_loadimage]
    k += b"\x85\xC0"                                                        # test eax,eax
    jz2 = len(k); k += b"\x74\x00"                                          # jz orig
    k += b"\xFF\x74\x24\x08"                                                # push [esp+8]  (alpha)
    k += b"\xFF\x74\x24\x08"                                                # push [esp+8]  (string*)
    k += b"\xFF\xD0"                                                        # call eax
    k += b"\x83\xC4\x08"                                                    # add esp,8
    k += b"\x85\xC0"                                                        # test eax,eax
    k += b"\x75\x05"                                                        # jnz done (skip 5-byte jmp)
    k[jz2 + 1] = len(k) - (jz2 + 2)                                         # orig: here
    k += b"\xE9" + struct.pack("<i", LOADIMG_VA - (va_cave2 + len(k) + 5))  # jmp original GetImage
    k += b"\xC3"                                                            # done: ret
    blob += k

    # 5. Add section .anim (0x60000020: CODE | EXECUTE | READ)
    new_sec = pe.add_section('.anim', blob, ANIM_CHARS)
    print(f"[OK] Section .anim added: RVA=0x{new_sec.virtual_address:08X}, "
          f"VSz=0x{new_sec.virtual_size:X}, Chars=0x{new_sec.characteristics:08X}")

    # 6. Patch call sites and hook site in PE data
    o = pe.va_to_offset(HOOK_VA)
    pe.data[o:o + 5] = b"\xE9" + struct.pack("<i", va_cave - (HOOK_VA + 5))

    for off, va in sites:
        pe.data[off + 1:off + 5] = struct.pack("<i", va_cave2 - (va + 5))

    print(f"[OK] Cave PreDraw @0x{va_cave:08X}, Cave LoadImage @0x{va_cave2:08X}")

    # 7. Save output (entry point remains unchanged!)
    pe.save(dst_path)
    print(f"[DONE] Anim hook successfully applied -> {dst_path}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("Usage: python patch_anim_hook.py <input_exe> <output_exe>")
    try:
        patch_anim_hook(sys.argv[1], sys.argv[2])
    except Exception as e:
        print("\n" + "=" * 67)
        print(" [COMPATIBILITY ERROR] Failed to detect required WebP hook sites.")
        print(f" Detail: {e}")
        print("=" * 67)
        print(" [WARNING]")
        print(" The provided executable does not have the expected structure or attributes.")
        print(" Please provide a compatible Zuma.exe binary.")
        print(" (RECOMMENDED: Zuma Deluxe v1.0.0.1 GameHouse / PopCap Retail)")
        print("=" * 67 + "\n")
        sys.exit(1)
