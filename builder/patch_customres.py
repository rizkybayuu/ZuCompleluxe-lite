#!/usr/bin/env python3
"""patch_customres.py - Inject section .zres and apply dynamic resolution PE hooks.
Zero hardcoded RVAs: Dynamic AOB pattern scanning, idempotent architecture.
Target module: modules\\zumareshandler.dll
"""
import argparse, json, os, re, struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR) if os.path.basename(SCRIPT_DIR) == "builder" else SCRIPT_DIR

u16 = lambda b, o: struct.unpack_from("<H", b, o)[0]
u32 = lambda b, o: struct.unpack_from("<I", b, o)[0]
p32 = lambda v: struct.pack("<I", v & 0xFFFFFFFF)
align = lambda v, a: (v + a - 1) // a * a
md = Cs(CS_ARCH_X86, CS_MODE_32)
CC = dict(e=4, ne=5, l=0xC, ge=0xD, le=0xE, g=0xF)

class Asm:
    def __init__(s, va): s.b = bytearray(); s.v = va; s.lab = {}; s.fix = []
    def va(s): return s.v + len(s.b)
    def raw(s, *xs):
        for x in xs: s.b += bytes(x) if isinstance(x, (bytes, bytearray)) else bytes([x])
    def dw(s, v): s.b += p32(v)
    def label(s, n):
        assert n not in s.lab, n
        s.lab[n] = s.va()
    def _rel(s, t):
        if isinstance(t, str): s.fix.append((len(s.b), t)); s.b += b"\0\0\0\0"
        else: s.b += struct.pack("<i", t - (s.va() + 4))
    def jmp(s, t): s.b += b"\xE9"; s._rel(t)
    def call(s, t): s.b += b"\xE8"; s._rel(t)
    def jcc(s, cc, t): s.b += bytes([0x0F, 0x80 + CC[cc]]); s._rel(t)
    def done(s):
        for pos, name in s.fix:
            struct.pack_into("<i", s.b, pos, s.lab[name] - (s.v + pos + 4))
        return bytes(s.b)

def load_pe(path):
    d = bytearray(open(path, "rb").read())
    pe = u32(d, 0x3C)
    assert d[pe:pe + 4] == b"PE\0\0" and u16(d, pe + 24) == 0x10B, "Not a PE32 executable"
    opt = pe + 24
    P = dict(d=d, pe=pe, opt=opt, nsec=u16(d, pe + 6), base=u32(d, opt + 28),
             sal=u32(d, opt + 32), fal=u32(d, opt + 36), secs=[])
    P["tab"] = opt + u16(d, pe + 20)
    for i in range(P["nsec"]):
        o = P["tab"] + 40 * i
        vs, va, rs, rp = struct.unpack_from("<IIII", d, o + 8)
        P["secs"].append(dict(name=bytes(d[o:o + 8]).rstrip(b"\0").decode("latin1"),
                              vs=vs, va=va, rs=rs, rp=rp, ch=u32(d, o + 36)))
    return P

def rva2off(P, rva):
    for s in P["secs"]:
        if s["va"] <= rva < s["va"] + max(s["vs"], s["rs"]):
            return s["rp"] + rva - s["va"]
    raise ValueError(f"rva 0x{rva:X} outside sections")

def foff(P, va): return rva2off(P, va - P["base"])

def insns_at(P, va, n):
    o = foff(P, va)
    return list(md.disasm(bytes(P["d"][o:o + 48]), va))[:n]

def call_target(P, va):
    o = foff(P, va)
    assert P["d"][o] == 0xE8, f"0x{va:08X} is not 0xE8 (call rel32)"
    return va + 5 + struct.unpack_from("<i", P["d"], o + 1)[0]

def rx(pat):
    out = b""
    for t in pat.split():
        out += b"." if t == "??" else re.escape(bytes([int(t, 16)]))
    return re.compile(b"(?=(" + out + b"))", re.S)

def cstr(d, o):
    e = d.index(b"\0", o); return bytes(d[o:e]).decode("latin1")

def find_import(P, dll, func):
    """Find VA of IAT slot for dll!func."""
    d = P["d"]; o = rva2off(P, u32(d, P["opt"] + 104))
    while bytes(d[o:o + 20]) != b"\0" * 20:
        oft, _, _, name, ft = struct.unpack_from("<5I", d, o)
        if cstr(d, rva2off(P, name)).lower() == dll.lower():
            so, i = rva2off(P, oft or ft), 0
            while u32(d, so + 4 * i):
                v = u32(d, so + 4 * i)
                if not v & 0x80000000 and cstr(d, rva2off(P, v) + 2) == func:
                    return P["base"] + ft + 4 * i
                i += 1
        o += 20
    return None

def compute_pe_checksum(data, pe_offset):
    checksum_offset = pe_offset + 24 + 64
    checksum = 0
    length = len(data)
    remainder = length % 4
    limit = length - remainder
    for i in range(0, limit, 4):
        if i == checksum_offset:
            continue
        dword = struct.unpack_from('<I', data, i)[0]
        checksum += dword
        if checksum > 0xFFFFFFFF:
            checksum = (checksum & 0xFFFFFFFF) + (checksum >> 32)
    if remainder:
        val = 0
        for j in range(remainder):
            val |= (data[limit + j] << (j * 8))
        checksum += val
        if checksum > 0xFFFFFFFF:
            checksum = (checksum & 0xFFFFFFFF) + (checksum >> 32)
    checksum = (checksum & 0xFFFF) + (checksum >> 16)
    checksum = checksum + (checksum >> 16)
    checksum = checksum & 0xFFFF
    return (checksum + length) & 0xFFFFFFFF

def patch_customres(input_exe, output_exe, width=800, height=600, sigs_path=None, moddir="modules"):
    MIN_W = 480
    MIN_H = 320
    if width < MIN_W or height < MIN_H:
        print(f"[!] WARNING: Target resolution {width}x{height} is below minimum allowed ({MIN_W}x{MIN_H}).")
        print(f"[!] Strictly clamping resolution to {max(width, MIN_W)}x{max(height, MIN_H)}.")
    w = max(width, MIN_W)
    h = max(height, MIN_H)
    dllfile = "zumareshandler.dll"

    if not sigs_path or not os.path.isfile(sigs_path):
        sigs_path = os.path.join(SCRIPT_DIR, "customres_sigs.json")
    if not os.path.isfile(sigs_path):
        sigs_path = os.path.join(SCRIPT_DIR, "sigs.json")

    print(f"[*] Patching Custom Resolution ({w}x{h}) into {output_exe}...")
    print(f"[*] Using signatures: {sigs_path}")

    P = load_pe(input_exe)
    d = P["d"]
    base = P["base"]

    if any(s["name"] == ".zres" for s in P["secs"]):
        print("[!] Input executable already contains section .zres. Skipping re-patching.")
        if input_exe != output_exe:
            open(output_exe, "wb").write(d)
        return True

    text = next(s for s in P["secs"] if s["name"] == ".text")
    tdata = bytes(d[text["rp"]:text["rp"] + text["rs"]])
    S = {}
    with open(sigs_path, "r", encoding="utf-8") as f:
        sigs_data = json.load(f)

    for n, sg in sigs_data.items():
        ms = [m.start() for m in rx(sg["pat"]).finditer(tdata)]
        if len(ms) != 1:
            raise RuntimeError(f"[{n}] signature pattern matched {len(ms)} times (expected 1).")
        S[n] = base + text["va"] + ms[0] + sg["anchor"]

    need = ["initdx_fn", "fill_fn", "dd_call_a", "dd_call_b", "dd_call_c", "gfx_call", "ds_call",
            "h4", "h6_move", "h6_drag", "bullet_l", "bullet_t", "bullet_r", "bullet_b", "drag_clamp", "board_h", "board_resize"]
    miss = [n for n in need if n not in S]
    if miss:
        raise RuntimeError(f"Missing required signatures in {sigs_path}: {miss}")

    def chk_call(n):
        i = insns_at(P, S[n], 1)[0]
        if i.mnemonic != "call" or i.size != 5:
            raise RuntimeError(f"[{n}] 0x{S[n]:08X} is not call rel32")
        return call_target(P, S[n])

    dd = {chk_call(n) for n in ("dd_call_a", "dd_call_b", "dd_call_c")}
    if len(dd) != 1:
        raise RuntimeError("3 DDInterface::Init call sites point to different targets: " + ", ".join(hex(x) for x in dd))
    DDINIT, GFX, DS = dd.pop(), chk_call("gfx_call"), chk_call("ds_call")
    INITDX, FILL = S["initdx_fn"], S["fill_fn"]

    i = insns_at(P, S["h4"], 2)
    if [x.mnemonic for x in i] != ["mov", "mov"] or "0x5c" not in i[0].op_str or "0x60" not in i[1].op_str:
        raise RuntimeError("[h4] structure mismatch")

    H6 = {}
    for n in ("h6_move", "h6_drag"):
        i = insns_at(P, S[n], 3)
        if [x.mnemonic for x in i] != ["push", "push", "call"] or i[0].op_str != "esi" or i[1].op_str != "edi":
            raise RuntimeError(f"[{n}] structure mismatch")
        H6[n] = call_target(P, S[n] + 2)

    FC_OPERAND = {}
    for n in ("bullet_l", "bullet_t"):
        o = foff(P, S[n])
        if bytes(d[o:o + 2]) != b"\xD8\x1D":
            raise RuntimeError(f"[{n}] is not fcomp dword ptr [abs]")
        FC_OPERAND[n] = u32(d, o + 2)
    if FC_OPERAND["bullet_l"] != FC_OPERAND["bullet_t"]:
        raise RuntimeError("[bullet_l/t] operand fcomp mismatch")
    if u32(d, foff(P, FC_OPERAND["bullet_l"])) != 0:
        raise RuntimeError("[bullet_l/t] operand is not float 0.0")

    for n, disp in (("bullet_r", 0x10), ("bullet_b", 0x14)):
        o = foff(P, S[n])
        if bytes(d[o:o + 5]) != bytes([0xDB, 0x46, disp, 0xDE, 0xD9]):
            raise RuntimeError(f"[{n}] is not 'fild [esi+0x{disp:02X}]; fcompp'")

    i = insns_at(P, S["drag_clamp"], 2)
    if (i[0].mnemonic, i[0].op_str, i[0].size) != ("cmp", "eax, -8", 3) or i[1].mnemonic != "jge":
        raise RuntimeError("[drag_clamp] is not 'cmp eax,-8; jge'")
    DRAG_JGE = int(i[1].op_str, 16)
    DRAG_NEXT = S["drag_clamp"] + 5
    DRAG_TAIL = S["drag_clamp"] + 0x58
    if bytes(d[foff(P, DRAG_TAIL):foff(P, DRAG_TAIL) + 6]) != bytes([0x8B, 0x7D, 0x08, 0x8B, 0x5D, 0x0C]):
        raise RuntimeError("[drag_clamp] clamp tail not at +0x58")

    o = foff(P, S["board_h"])
    if bytes(d[o:o + 10]) != b"\x68\xE0\x01\x00\x00\x68\x80\x02\x00\x00" or bytes(d[o + 12:o + 15]) != b"\xFF\x50\x34":
        raise RuntimeError("[board_h] is not 'push 1E0; push 280; push ebx; push ebx; call [eax+34]'")

    BOARD_RESIZE_ORIG = bytes.fromhex("8B 4E 60 51 8B 4E 5C 51")
    o = foff(P, S["board_resize"])
    if bytes(d[o:o + 8]) != BOARD_RESIZE_ORIG:
        raise RuntimeError("[board_resize] is not 'mov ecx,[esi+60]; push ecx; mov ecx,[esi+5C]; push ecx'")

    slots = []
    for s in P["secs"]:
        if s["ch"] & 0x20000000: continue
        raw = bytes(d[s["rp"]:s["rp"] + s["rs"]])
        p = raw.find(p32(INITDX))
        while p >= 0:
            slots.append(s["rp"] + p)
            p = raw.find(p32(INITDX), p + 1)
    if len(slots) != 2:
        raise RuntimeError(f"Found {len(slots)} InitDirectX vtbl slots (expected 2)")

    LOADLIB = find_import(P, "kernel32.dll", "LoadLibraryA")
    if not LOADLIB:
        raise RuntimeError("kernel32!LoadLibraryA not found in import table")

    # Section layout
    zdata = next((s for s in P["secs"] if s["name"] == ".zdata"), None)
    if not zdata:
        raise RuntimeError("Section .zdata not found - run patch_imports.py first!")
    ZRES_DATA_OFF = 0x1800
    ztable_va = base + zdata["va"] + ZRES_DATA_OFF
    ztable_foff = zdata["rp"] + ZRES_DATA_OFF

    sec_rva = align(max(s["va"] + max(s["vs"], s["rs"]) for s in P["secs"]), P["sal"])
    sec_va = base + sec_rva
    SLOT, TRIED = ztable_va + 0x18, ztable_va + 0x1C
    NEGX, NEGY, RLIM, BLIM = ztable_va + 0x20, ztable_va + 0x24, ztable_va + 0x28, ztable_va + 0x2C
    DMINX, DMAXX, DMINY, DMAXY = ztable_va + 0x30, ztable_va + 0x34, ztable_va + 0x38, ztable_va + 0x3C
    BW, BH = ztable_va + 0x40, ztable_va + 0x44
    OFF_NAME = 0x50
    name = (moddir + "\\" + dllfile).encode("latin1") + b"\0"
    OFF_CODE = align(OFF_NAME + len(name), 16)
    NAME = sec_va + OFF_NAME
    A = Asm(sec_va + OFF_CODE)
    T = {}

    A.label("resolve")
    A.raw(0x9C, 0x60)                            # pushfd; pushad
    A.raw(0xC6, 0x05); A.dw(TRIED); A.raw(1)     # mov byte [TRIED],1
    A.raw(0x68); A.dw(NAME)                      # push offset name
    A.raw(0xFF, 0x15); A.dw(LOADLIB)             # call [LoadLibraryA] (stdcall)
    A.raw(0x61, 0x9D, 0xC3)                      # popad; popfd; ret

    def thunk(key, orig, api_off):
        T[key] = A.va()
        A.raw(0xA1); A.dw(SLOT)                  # mov eax,[SLOT]
        A.raw(0x85, 0xC0); A.jcc("ne", key + ".go")
        A.raw(0x80, 0x3D); A.dw(TRIED); A.raw(0) # cmp byte [TRIED],0
        A.jcc("ne", key + ".orig")
        A.call("resolve")
        A.raw(0xA1); A.dw(SLOT); A.raw(0x85, 0xC0); A.jcc("e", key + ".orig")
        A.label(key + ".go");   A.raw(0xFF, 0x60, api_off)   # jmp [eax+off]
        A.label(key + ".orig"); A.jmp(orig)

    thunk("initdx", INITDX, 0x00); thunk("dd", DDINIT, 0x04)
    thunk("gfx", GFX, 0x08);       thunk("ds", DS, 0x0C)

    T["h4"] = A.va()
    A.raw(b"\x8B\x53\x5C\x8B\x43\x60")           # mov edx,[ebx+5C]; mov eax,[ebx+60]
    A.raw(0x8B, 0x0D); A.dw(SLOT); A.raw(b"\x85\xC9\x74\x06")   # mov ecx,[SLOT]; test; jz +6
    A.raw(b"\x8B\x51\x10\x8B\x41\x14")           # mov edx,[ecx+10]; mov eax,[ecx+14]
    A.jmp(S["h4"] + 6)

    for n in ("h6_move", "h6_drag"):
        T[n] = A.va()
        A.raw(0x50, 0xA1); A.dw(SLOT); A.raw(b"\x85\xC0\x74\x06")  # push eax; mov eax,[SLOT]; test; jz +6
        A.raw(b"\x2B\x78\x18\x2B\x70\x1C")       # sub edi,[eax+18]; sub esi,[eax+1C]
        A.raw(0x58, 0x56, 0x57)                  # pop eax; push esi; push edi
        A.call(H6[n]); A.jmp(S[n] + 7)

    for key, disp, cell in (("bullet_r", 0x10, RLIM), ("bullet_b", 0x14, BLIM)):
        T[key] = A.va()
        A.raw(0x83, 0x3D); A.dw(SLOT); A.raw(0)  # cmp dword [SLOT],0
        A.jcc("e", key + ".van")
        A.raw(0xDB, 0x05); A.dw(cell)            # fild dword [cell]
        A.jmp(key + ".end")
        A.label(key + ".van"); A.raw(0xDB, 0x46, disp)   # fild dword [esi+disp]
        A.label(key + ".end"); A.raw(0xDE, 0xD9)         # fcompp
        A.jmp(S[key] + 5)

    T["drag"] = A.va()                           # eax=newX edx=newY ecx=Dialog* ; ebx/edi free
    A.raw(0x83, 0x3D); A.dw(SLOT); A.raw(0); A.jcc("e", "d.van")
    A.raw(0x3B, 0x05); A.dw(DMINX); A.jcc("ge", "d.xmax")        # cmp eax,[DMINX]
    A.raw(0xA1); A.dw(DMINX); A.jmp("d.y")                       # mov eax,[DMINX]
    A.label("d.xmax")
    A.raw(0x8B, 0x59, 0x10, 0x03, 0xD8)                          # mov ebx,[ecx+10]; add ebx,eax
    A.raw(0x8B, 0x3D); A.dw(DMAXX)                               # mov edi,[DMAXX]
    A.raw(0x3B, 0xDF); A.jcc("le", "d.y")                        # cmp ebx,edi
    A.raw(0x8B, 0xC7, 0x2B, 0x41, 0x10)                          # mov eax,edi; sub eax,[ecx+10]
    A.label("d.y")
    A.raw(0x3B, 0x15); A.dw(DMINY); A.jcc("ge", "d.ymax")        # cmp edx,[DMINY]
    A.raw(0x8B, 0x15); A.dw(DMINY); A.jmp("d.end")               # mov edx,[DMINY]
    A.label("d.ymax")
    A.raw(0x8B, 0x59, 0x14, 0x03, 0xDA)                          # mov ebx,[ecx+14]; add ebx,edx
    A.raw(0x8B, 0x3D); A.dw(DMAXY)                               # mov edi,[DMAXY]
    A.raw(0x3B, 0xDF); A.jcc("le", "d.end")
    A.raw(0x8B, 0xD7, 0x2B, 0x51, 0x14)                          # mov edx,edi; sub edx,[ecx+14]
    A.label("d.end"); A.jmp(DRAG_TAIL)
    A.label("d.van")
    A.raw(0x83, 0xF8, 0xF8); A.jcc("ge", DRAG_JGE); A.jmp(DRAG_NEXT)

    for key, cell, site in (("board_h", BH, S["board_h"]), ("board_w", BW, S["board_h"] + 5)):
        T[key] = A.va()
        A.raw(0xFF, 0x35); A.dw(cell)            # push dword [cell]
        A.jmp(site + 5)

    T["board_resize"] = A.va()
    A.raw(0xFF, 0x35); A.dw(BH)                  # push dword [BH]
    A.raw(0xFF, 0x35); A.dw(BW)                  # push dword [BW]
    A.jmp(S["board_resize"] + 8)

    code = A.done()
    size = OFF_CODE + len(code)
    blob = bytearray(size)
    # Initialize mutable ZTable inside section .zdata (RW)
    struct.pack_into("<6I", d, ztable_foff + 0x00, 0x53455A52, INITDX, DDINIT, GFX, FILL, DS)
    struct.pack_into("<2I", d, ztable_foff + 0x40, 640, 480)
    struct.pack_into("<2I", d, ztable_foff + 0x48, w, h)

    # Read-only metadata in .zres (RX)
    struct.pack_into("<6I", blob, 0, 0x53455A52, INITDX, DDINIT, GFX, FILL, DS)
    struct.pack_into("<I", blob, 4, ztable_va)       # Pointer to active ZTable in .zdata
    struct.pack_into("<2I", blob, 0x40, 640, 480)   # default vanilla when DLL not active
    struct.pack_into("<2I", blob, 0x48, w, h)         # target custom resolution metadata for checker
    blob[OFF_NAME:OFF_NAME + len(name)] = name
    blob[OFF_CODE:] = code

    # Add section header
    hdr = P["tab"] + 40 * P["nsec"]
    first_raw = min(s["rp"] for s in P["secs"] if s["rp"])
    if hdr + 40 > first_raw or any(d[hdr:hdr + 40]):
        raise RuntimeError("No free section header slot available for .zres")

    fo = align(len(d), P["fal"])
    d.extend(b"\0" * (fo - len(d)))
    rawsz = align(size, P["fal"])
    d.extend(bytes(blob) + b"\0" * (rawsz - size))
    struct.pack_into("<8sIIIIIIHHI", d, hdr, b".zres\0\0\0", size, sec_rva, rawsz, fo, 0, 0, 0, 0, 0x60000020)
    struct.pack_into("<H", d, P["pe"] + 6, P["nsec"] + 1)
    struct.pack_into("<I", d, P["opt"] + 56, align(sec_rva + size, P["sal"]))   # SizeOfImage

    # Apply patches
    def patch_call(va, tgt):
        o = foff(P, va)
        d[o + 1:o + 5] = struct.pack("<i", tgt - (va + 5))

    def patch_jmp(va, tgt, total):
        o = foff(P, va)
        d[o] = 0xE9
        d[o + 1:o + 5] = struct.pack("<i", tgt - (va + 5))
        d[o + 5:o + total] = b"\x90" * (total - 5)

    for n in ("dd_call_a", "dd_call_b", "dd_call_c"): patch_call(S[n], T["dd"])
    patch_call(S["gfx_call"], T["gfx"])
    patch_call(S["ds_call"], T["ds"])
    for off in slots: d[off:off + 4] = p32(T["initdx"])
    patch_jmp(S["h4"], T["h4"], 6)
    patch_jmp(S["h6_move"], T["h6_move"], 7)
    patch_jmp(S["h6_drag"], T["h6_drag"], 7)
    d[foff(P, S["bullet_l"]) + 2:foff(P, S["bullet_l"]) + 6] = p32(NEGX)
    d[foff(P, S["bullet_t"]) + 2:foff(P, S["bullet_t"]) + 6] = p32(NEGY)
    patch_jmp(S["bullet_r"], T["bullet_r"], 5)
    patch_jmp(S["bullet_b"], T["bullet_b"], 5)
    patch_jmp(S["drag_clamp"], T["drag"], 5)
    patch_jmp(S["board_h"], T["board_h"], 5)
    patch_jmp(S["board_h"] + 5, T["board_w"], 5)
    patch_jmp(S["board_resize"], T["board_resize"], 8)

    # Compute valid PE checksum
    new_checksum = compute_pe_checksum(d, P["pe"])
    struct.pack_into("<I", d, P["opt"] + 64, new_checksum)

    os.makedirs(os.path.dirname(os.path.abspath(output_exe)), exist_ok=True)
    open(output_exe, "wb").write(d)
    print(f"[+] Section .zres successfully injected into {output_exe} (Size: {size} bytes, Chksum: 0x{new_checksum:08X})")
    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="ZuCompleluxe Custom Resolution PE Injector")
    parser.add_argument("input_exe", help="Input Zuma.exe")
    parser.add_argument("output_exe", help="Output Zuma.exe")
    parser.add_argument("--width", "-W", type=int, default=800, help="Target resolution width")
    parser.add_argument("--height", "-H", type=int, default=600, help="Target resolution height")
    parser.add_argument("--sigs", default=None, help="Signatures JSON file")
    parser.add_argument("--moddir", default="modules", help="Relative directory for DLL")
    args = parser.parse_args()

    patch_customres(args.input_exe, args.output_exe, width=args.width, height=args.height, sigs_path=args.sigs, moddir=args.moddir)
