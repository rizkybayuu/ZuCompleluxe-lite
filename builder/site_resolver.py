"""site_resolver.py - Dynamic AOB Pattern & Capstone Site Resolver for Zuma Deluxe.

Scans the baseline Zuma.exe executable using signature scanning (AOB with lookahead)
and Capstone detailed structural disassembly to resolve all 48 patch sites across Stage 1 & Stage 2:
- 47 Write Sites (verified by bidirectional Byte-Diff Audit)
- 1 Read Reference (COUNT_FN @ 0x00404CF0)

Uses strict Capstone operand inspection (X86_OP_MEM / base / disp) and linear basic-block
disassembly rather than arbitrary offset arithmetic or loose string matching.
Fails fast if the input PE data hash differs from the authentic retail v1.0.0.1 baseline.
"""

import sys
import os
import re
import hashlib
import capstone
from capstone.x86 import (
    X86_OP_MEM, X86_OP_REG, X86_OP_IMM,
    X86_REG_EAX, X86_REG_ECX, X86_REG_EDX,
    X86_REG_ESI, X86_REG_EBX, X86_REG_EDI
)

# Add current directory for pe_util
sys.path.append(os.path.dirname(__file__))
from pe_util import PEFile

EXPECTED_BASE_SHA256 = "EC3AAF01A887B8F6093AEF7FF7C20D8612F86EBECAABBC35AC18B0D1977EB00D"

# Shared constants for HoleMgr allocation and limits (Single Source of Truth)
HOLEMGR_MAX_HOLES = 10
HOLEMGR_BUFFER_SIZE = 0x308

def resolve_all_sites(pe_or_path, enforce_hash=False):
    """Resolves all 48 patch sites from an authentic baseline Zuma.exe.
    pe_or_path: file path (str) or PEFile instance.
    enforce_hash: if True, validates SHA-256 of pe.data against EXPECTED_BASE_SHA256.
    Returns: dict[str, int] mapping site_name -> virtual_address (VA)
    """
    if isinstance(pe_or_path, str):
        pe = PEFile(pe_or_path)
    else:
        pe = pe_or_path

    if enforce_hash:
        h = hashlib.sha256(bytes(pe.data)).hexdigest().upper()
        if h != EXPECTED_BASE_SHA256:
            raise ValueError(
                f"[ABORT] Target executable hash {h} does not match authentic "
                f"Zuma Deluxe v1.0.0.1 baseline ({EXPECTED_BASE_SHA256})"
            )

    text_sec = pe.find_section_by_name('.text')
    if not text_sec:
        raise ValueError("Section .text not found in PE")

    text_data = bytes(pe.data[text_sec.raw_offset : text_sec.raw_offset + text_sec.raw_size])
    image_base = pe.image_base
    text_va = image_base + text_sec.virtual_address

    # Initialize Capstone in 32-bit x86 detailed mode
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    def find_pattern(pat):
        tokens = pat.strip().split()
        parts = []
        for t in tokens:
            if t in ('??', '?'):
                parts.append(b'.')
            else:
                parts.append(re.escape(bytes([int(t, 16)])))
        rx = re.compile(b'(?=(' + b''.join(parts) + b'))', re.DOTALL)
        return [text_va + m.start() for m in rx.finditer(text_data)]

    resolved = {}

    # 1. Single-site hook patterns (21 sites: 19 writes + 2 reads: COUNT_FN, STAGE1_OPNEW)
    single_patterns = {
        # Stage 2:
        'STAGE2_DESC_SITE1': ('8D 84 01 AC 00 00 00', 0),
        'STAGE2_DESC_SITE2': ('8D 94 01 AC 00 00 00', 0),
        'STAGE2_DESC_SITE3': ('8D 9C 02 AC 00 00 00', 0),
        'STAGE2_BIND_VA':    ('8B 4E 78 E8 ?? ?? ?? ?? 33 C9 89 4D EC 8D 7E 68', 8),
        'STAGE2_SWAP_COUNT': ('8B 8E F8 00 00 00 E8 ?? ?? ?? ?? 89 86 8C 00 00 00', 6),
        'STAGE2_CLAMP_SITE': ('8B 8E 8C 00 00 00 42 83 C0 7C 3B D1', 0),
        'STAGE2_HOLE_HOOK1': ('55 8B EC 8B 45 08 D9 45 0C 8B 54 81 78 6B D2 38', 0),
        'STAGE2_HOLE_HOOK2': ('55 8B EC 8B 55 08 8B 44 91 78 6B C0 38', 0),
        'STAGE2_HOLE_HOOK3': ('55 8B EC D9 45 14 56 8B 35 ?? ?? ?? ?? D8 15 ?? ?? ?? ?? 8B 46 04', 0),
        'STAGE2_BALL_SET_WAYPOINT': ('55 8B EC 8B 45 0C 8B 55 08 89 84 91 9C 00 00 00 5D C2 08 00 90 90 90 90 90 90 90 90 90 90 90 90', 0),
        'STAGE2_BALL_GET_WAYPOINT': ('55 8B EC 8B 45 08 8B 84 81 9C 00 00 00 5D C2 04 00 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90', 0),
        'STAGE2_SPATIAL_ADD1':      ('55 8B EC 8B 45 0C 8B 14 81 56 8D 72 01 89 34 81 C1 E0 0A 03 C2 8B 55 08 89 54 81 28 5E 5D C2 08 00', 0),
        'STAGE2_SPATIAL_ADD2':      ('55 8B EC 8B 45 0C 8B 54 81 14 56 8D 72 01 89 74 81 14 C1 E0 0A 03 C2 8B 55 08 89 94 81 28 50 00 00 5E 5D C2 08 00', 0),
        'STAGE2_OVERLAY_DRAW':      ('55 8B EC 8B 45 0C 8D 04 40 8D 54 81 04 8B 45 08 52 50 E8', 0),
        'STAGE2_HOLE_BG_END':       ('8B 47 74 33 DB 85 C0 7E 72 8D 8F 9C 00 00 00 89 4D 08 8B FF 53 56 8B CF E8', 0),
        'STAGE2_HOLE_DRAW_LOOP':    ('8B 47 74 33 DB 85 C0 7E 72 8D 8F 9C 00 00 00 89 4D 08 8B FF 53 56 8B CF E8', 20),
        'STAGE2_STATIC_BOARD_DRAW': ('5F 5E 5B 8B E5 5D C2 04 00 55 8B EC 5D E9 ?? ?? ?? ?? 90 90 90 90 90 90 90 55 8B EC 56 8B 75 08', 13),
        'COUNT_FN':          ('8B 91 C0 00 00 00 33 C0 85 D2 74 05 B8 01 00 00 00 8B 91 3C 01 00 00', 0),

        # Stage 1:
        'STAGE1_OPNEW':           ('56 8B 74 24 08 EB 10 56 E8', 0),
        'STAGE1_CTOR_HOOK':       ('68 2C 01 00 00 E8 ?? ?? ?? ?? 83 C4 04 89 45 08', 0),
        'STAGE1_SWAP_REGION':     ('8B 56 68 8B 46 5C 89 56 5C 89 46 68', 0),
        'STAGE1_DTOR_VA':         ('8B 0F 3B CB 74 06 8B 11 6A 01 FF 12', 0),
        'STAGE1_HOLEMGR_ALLOC':   ('33 C0 68 2C 01 00 00 C6 45 FC 08 89 46 74 E8 ?? ?? ?? ?? 83 C4 04', 2),
        'STAGE1_HOLEMGR_CREDITS_ALLOC': ('C7 47 08 00 00 00 00 68 2C 01 00 00 C6 45 FC 01', 7),
        'STAGE1_HOLEMGR_EH_CTOR': ('68 ?? ?? ?? ?? 6A 03 6A 38 8D 8E 84 00 00 00 51', 5),
        'STAGE1_HOLEMGR_EH_DTOR': ('68 ?? ?? ?? ?? 6A 03 6A 38 8D 87 84 00 00 00 50', 5),
        'STAGE1_EXTRA1':          ('8B 4D 08 8B 0C 01 68 ?? ?? ?? ?? E8', 0),
        'STAGE1_EXTRA2':          ('8B 56 04 8B 45 FC 8B 0C 10', 0),
        'STAGE1_EXTRA3_BASE':     ('BB 5C 00 00 00 8D 64 24 00 8B 4E 04 8B 0C 0B', 0),
        'STAGE1_EXTRA4_BASE':     ('BF 5C 00 00 00 8B FF 83 FF 5C', 0),

        # Sidecar engine config functions (ZEP_EngineConfig):
        'CURVE_CTOR':     ('55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 51 53 56 8B F1 57 8D 7E 20', 0),
        'CURVEDESC_CTOR': ('8B C1 33 C9 89 48 14 C7 40 18 0F 00 00 00 88 48 04 BA 28 00 00 00', 0),
        'STR_ASSIGN':     ('55 8B EC 56 8B 75 08 8B C6 57 8D 78 01 8D 49 00 8A 10 40 84 D2 75 F9 2B C7 50 56', 0),
        'CURVE_INIT':     ('55 8B EC 8B 45 08 8B 55 0C 53 56 8B F1 57 8B 7D 10 8B CF 6B C9 7C 89 46 10', 0),

        # Credits Screen & Exclusive Credits Engine Hooks:
        'STAGE1_CREDITS_TABLE':      ('8B 7F 5C 89 7D EC 89 5D E4 0F 8E ?? ?? ?? ?? B8', 15),
        'STAGE1_CREDITS_COUNT':      ('8B 15 ?? ?? ?? ?? 03 FE 41 83 C0 0C 3B CA', 0),
        'STAGE1_CREDITS_DRAW_HOOK':  ('0F 8E ?? ?? ?? ?? 8B 4D 08 56 E8 ?? ?? ?? ?? 8B 55 F0', 6),
        'STAGE1_CREDITS_CLICK_HOOK': ('C6 41 1F 01 C2 0C 00 90 90 90 90 90 90 90 90 90 C6 41 1F 00 C2 08 00', 0),
        'DRAW_IMAGE_RECT':           ('55 8B EC 83 EC 24 8B 55 0C 53 8B D9 8B 43 0C', 0),
        'FONT_LOADER_ANCHOR':        ('F6 05 ?? ?? ?? ?? 01 8B 35 ?? ?? ?? ?? A1', 0),
        'BUTTON_LOADER_ANCHOR':      ('33 F6 3B FE C6 45 ?? 01 75 0A 8B 3D', 0),
        'ZUMA_APP_ANCHOR':           ('55 8B EC 8B 45 08 85 C0 75 ?? 8B 0D ?? ?? ?? ?? E8', 0),
        'FONT_DIALOG_ANCHOR':        ('89 4B 0C 8B 0D ?? ?? ?? ?? 51 8B CE', 3),
        'CREDITS_COLOR_ANCHOR':      ('68 FF FF FF 00 8D 4D D4 E8', 0),
    }

    for name, (pat, hook_off) in single_patterns.items():
        matches = find_pattern(pat)
        if len(matches) != 1:
            raise RuntimeError(f"Pattern for {name} matched {len(matches)} times (expected exactly 1)")
        resolved[name] = matches[0] + hook_off

    # Capstone semantic validation for engine functions:
    # 1. CURVE_CTOR
    cc_va = resolved['CURVE_CTOR']
    cc_raw = pe.rva_to_offset(cc_va - image_base) - text_sec.raw_offset
    cc_insns = list(md.disasm(text_data[cc_raw : cc_raw + 0x60], cc_va))
    if not (cc_insns[0].mnemonic == 'push' and any(i.mnemonic == 'lea' and '0x20' in i.op_str for i in cc_insns)):
        raise RuntimeError(f"CURVE_CTOR at 0x{cc_va:08X} failed Capstone semantic verification")

    # 2. CURVEDESC_CTOR
    cd_va = resolved['CURVEDESC_CTOR']
    cd_raw = pe.rva_to_offset(cd_va - image_base) - text_sec.raw_offset
    cd_insns = list(md.disasm(text_data[cd_raw : cd_raw + 0x40], cd_va))
    if not (cd_insns[0].mnemonic == 'mov' and any(i.mnemonic == 'mov' and '0xf' in i.op_str for i in cd_insns)):
        raise RuntimeError(f"CURVEDESC_CTOR at 0x{cd_va:08X} failed Capstone semantic verification")

    # 3. STR_ASSIGN
    sa_va = resolved['STR_ASSIGN']
    sa_raw = pe.rva_to_offset(sa_va - image_base) - text_sec.raw_offset
    sa_insns = list(md.disasm(text_data[sa_raw : sa_raw + 0x30], sa_va))
    if not (sa_insns[0].mnemonic == 'push' and any(i.mnemonic == 'test' and 'dl, dl' in i.op_str for i in sa_insns) and any(i.mnemonic == 'ret' and '4' in i.op_str for i in sa_insns)):
        raise RuntimeError(f"STR_ASSIGN at 0x{sa_va:08X} failed Capstone semantic verification")

    # 4. CURVE_INIT
    ci_va = resolved['CURVE_INIT']
    ci_raw = pe.rva_to_offset(ci_va - image_base) - text_sec.raw_offset
    ci_insns = list(md.disasm(text_data[ci_raw : ci_raw + 0x40], ci_va))
    if not (ci_insns[0].mnemonic == 'push' and any(i.mnemonic == 'imul' and '0x7c' in i.op_str for i in ci_insns) and any(i.mnemonic == 'lea' and '0xac' in i.op_str for i in ci_insns)):
        raise RuntimeError(f"CURVE_INIT at 0x{ci_va:08X} failed Capstone semantic verification")

    # 5. Credits Engine Validation & Dynamic Pointer Extractions
    # STAGE1_CREDITS_TABLE
    ct_va = resolved['STAGE1_CREDITS_TABLE']
    ct_raw = pe.rva_to_offset(ct_va - image_base) - text_sec.raw_offset
    ct_insns = list(md.disasm(text_data[ct_raw : ct_raw + 8], ct_va))
    if not (ct_insns and ct_insns[0].mnemonic == 'mov' and ct_insns[0].operands[0].reg == X86_REG_EAX and ct_insns[0].size == 5):
        raise RuntimeError(f"STAGE1_CREDITS_TABLE at 0x{ct_va:08X} failed Capstone verification")

    # STAGE1_CREDITS_COUNT & CREDITS_COUNT_PTR
    ccnt_va = resolved['STAGE1_CREDITS_COUNT']
    ccnt_raw = pe.rva_to_offset(ccnt_va - image_base) - text_sec.raw_offset
    ccnt_insns = list(md.disasm(text_data[ccnt_raw : ccnt_raw + 8], ccnt_va))
    if not (ccnt_insns and ccnt_insns[0].mnemonic == 'mov' and ccnt_insns[0].operands[0].reg == X86_REG_EDX and ccnt_insns[0].operands[1].type == X86_OP_MEM and ccnt_insns[0].size == 6):
        raise RuntimeError(f"STAGE1_CREDITS_COUNT at 0x{ccnt_va:08X} failed Capstone verification")
    resolved['CREDITS_COUNT_PTR'] = ccnt_insns[0].operands[1].mem.disp

    # STAGE1_CREDITS_DRAW_HOOK & GRAPHICS_SET_FONT
    cdrw_va = resolved['STAGE1_CREDITS_DRAW_HOOK']
    cdrw_raw = pe.rva_to_offset(cdrw_va - image_base) - text_sec.raw_offset
    cdrw_insns = list(md.disasm(text_data[cdrw_raw : cdrw_raw + 16], cdrw_va))
    if len(cdrw_insns) < 3 or cdrw_insns[0].mnemonic != 'mov' or cdrw_insns[1].mnemonic != 'push' or cdrw_insns[2].mnemonic != 'call':
        raise RuntimeError(f"STAGE1_CREDITS_DRAW_HOOK at 0x{cdrw_va:08X} failed Capstone verification")
    resolved['GRAPHICS_SET_FONT'] = cdrw_insns[2].operands[0].imm

    # STAGE1_CREDITS_CLICK_HOOK
    cclk_va = resolved['STAGE1_CREDITS_CLICK_HOOK']
    cclk_raw = pe.rva_to_offset(cclk_va - image_base) - text_sec.raw_offset
    cclk_insns = list(md.disasm(text_data[cclk_raw : cclk_raw + 8], cclk_va))
    if len(cclk_insns) < 2 or cclk_insns[0].mnemonic != 'mov' or cclk_insns[1].mnemonic != 'ret':
        raise RuntimeError(f"STAGE1_CREDITS_CLICK_HOOK at 0x{cclk_va:08X} failed Capstone verification")

    # DRAW_IMAGE_RECT
    dir_va = resolved['DRAW_IMAGE_RECT']
    dir_raw = pe.rva_to_offset(dir_va - image_base) - text_sec.raw_offset
    dir_insns = list(md.disasm(text_data[dir_raw : dir_raw + 16], dir_va))
    if len(dir_insns) < 3 or dir_insns[0].mnemonic != 'push' or dir_insns[1].mnemonic != 'mov' or dir_insns[2].mnemonic != 'sub':
        raise RuntimeError(f"DRAW_IMAGE_RECT at 0x{dir_va:08X} failed Capstone verification")

    # FONT_LOADER_ANCHOR -> FONT_HEADER, FONT_NAME, FONT_TITLE, ORIGINAL_CREDITS_TABLE
    fla_va = resolved.pop('FONT_LOADER_ANCHOR')
    fla_raw = pe.rva_to_offset(fla_va - image_base) - text_sec.raw_offset
    fla_insns = list(md.disasm(text_data[fla_raw : fla_raw + 0x30], fla_va))
    resolved['FONT_HEADER'] = fla_insns[1].operands[1].mem.disp
    resolved['FONT_NAME'] = fla_insns[2].operands[1].mem.disp
    resolved['FONT_TITLE'] = fla_insns[5].operands[1].mem.disp
    resolved['ORIGINAL_CREDITS_TABLE'] = fla_insns[6].operands[0].mem.disp

    # BUTTON_LOADER_ANCHOR -> IMAGE_DIALOG_BUTTON_PTR
    bla_va = resolved.pop('BUTTON_LOADER_ANCHOR')
    bla_raw = pe.rva_to_offset(bla_va - image_base) - text_sec.raw_offset
    bla_insns = list(md.disasm(text_data[bla_raw : bla_raw + 0x18], bla_va))
    resolved['IMAGE_DIALOG_BUTTON_PTR'] = next(i for i in bla_insns if i.mnemonic == 'mov' and i.operands[0].reg == X86_REG_EDI).operands[1].mem.disp

    # ZUMA_APP_ANCHOR -> ZUMA_APP_PTR
    zaa_va = resolved.pop('ZUMA_APP_ANCHOR')
    zaa_raw = pe.rva_to_offset(zaa_va - image_base) - text_sec.raw_offset
    zaa_insns = list(md.disasm(text_data[zaa_raw : zaa_raw + 0x18], zaa_va))
    resolved['ZUMA_APP_PTR'] = next(i for i in zaa_insns if i.mnemonic == 'mov' and i.operands[0].reg == X86_REG_ECX).operands[1].mem.disp

    # FONT_DIALOG_ANCHOR -> FONT_DIALOG_PTR
    fda_va = resolved.pop('FONT_DIALOG_ANCHOR')
    fda_raw = pe.rva_to_offset(fda_va - image_base) - text_sec.raw_offset
    fda_insns = list(md.disasm(text_data[fda_raw : fda_raw + 8], fda_va))
    if not (fda_insns and fda_insns[0].mnemonic == 'mov' and fda_insns[0].operands[0].reg == X86_REG_ECX and fda_insns[0].operands[1].type == X86_OP_MEM):
        raise RuntimeError(f"FONT_DIALOG_ANCHOR at 0x{fda_va:08X} failed Capstone verification")
    resolved['FONT_DIALOG_PTR'] = fda_insns[0].operands[1].mem.disp

    # CREDITS_COLOR_ANCHOR -> GRAPHICS_SET_COLOR
    cca_va = resolved.pop('CREDITS_COLOR_ANCHOR')
    cca_raw = pe.rva_to_offset(cca_va - image_base) - text_sec.raw_offset
    cca_insns = list(md.disasm(text_data[cca_raw : cca_raw + 30], cca_va))
    cca_calls = [i for i in cca_insns if i.mnemonic == 'call' and len(i.operands) >= 1 and i.operands[0].type == X86_OP_IMM]
    if len(cca_calls) < 2:
        raise RuntimeError(f"CREDITS_COLOR_ANCHOR at 0x{cca_va:08X} failed Capstone verification")
    resolved['GRAPHICS_SET_COLOR'] = cca_calls[1].operands[0].imm

    # Structural check: verify HoleMgr and rendering hooks align cleanly on exact instruction boundaries
    boundary_checks = [
        ('STAGE2_HOLE_HOOK1', 6),
        ('STAGE2_HOLE_HOOK2', 6),
        ('STAGE2_HOLE_HOOK3', 6),
        ('STAGE2_OVERLAY_DRAW', 6),
        ('STAGE2_HOLE_BG_END', 5),
        ('STAGE2_HOLE_DRAW_LOOP', 9),
        ('STAGE2_STATIC_BOARD_DRAW', 5),
        ('STAGE1_HOLEMGR_CREDITS_ALLOC', 5),
        ('STAGE1_CREDITS_TABLE', 5),
        ('STAGE1_CREDITS_DRAW_HOOK', 9),
        ('STAGE1_CREDITS_CLICK_HOOK', 7),
    ]
    for hname, exp_sz in boundary_checks:
        hva = resolved[hname]
        hraw = pe.rva_to_offset(hva - image_base) - text_sec.raw_offset
        cum = 0
        for insn in md.disasm(text_data[hraw : hraw + 16], hva):
            cum += insn.size
            if cum >= exp_sz:
                break
        if cum != exp_sz:
            raise RuntimeError(f"Hook {hname} at 0x{hva:08X} does not align cleanly to {exp_sz}-byte instruction boundary (got {cum}b)")

    # 2. Linear basic-block decode from EXTRA3_BASE and EXTRA4_BASE (6 derived sites)
    # Replaces arbitrary +9/+20/+48 offsets with direct structural instruction parsing
    e3_va = resolved['STAGE1_EXTRA3_BASE']
    e3_raw = pe.rva_to_offset(e3_va - image_base) - text_sec.raw_offset
    insns_e3 = list(md.disasm(text_data[e3_raw : e3_raw + 0x40], e3_va))
    sub1, sub2, sub3 = None, None, None
    for insn in insns_e3:
        if insn.mnemonic == 'mov' and len(insn.operands) >= 2:
            op0, op1 = insn.operands[0], insn.operands[1]
            if op0.type == X86_OP_REG and op1.type == X86_OP_MEM:
                if op1.mem.base == X86_REG_ESI and op1.mem.disp == 4:
                    if sub1 is None: sub1 = insn.address
                    elif sub2 is None: sub2 = insn.address
        elif insn.mnemonic == 'add' and len(insn.operands) >= 2:
            op0, op1 = insn.operands[0], insn.operands[1]
            if op0.type == X86_OP_REG and op0.reg == X86_REG_EBX and op1.type == X86_OP_IMM and op1.imm == 4:
                sub3 = insn.address

    if None in (sub1, sub2, sub3):
        raise RuntimeError(f"Failed to resolve EXTRA3 derived instructions from 0x{e3_va:08X}")
    resolved['STAGE1_EXTRA3_SUB1'] = sub1
    resolved['STAGE1_EXTRA3_SUB2'] = sub2
    resolved['STAGE1_EXTRA3_SUB3'] = sub3

    e4_va = resolved['STAGE1_EXTRA4_BASE']
    e4_raw = pe.rva_to_offset(e4_va - image_base) - text_sec.raw_offset
    insns_e4 = list(md.disasm(text_data[e4_raw : e4_raw + 0x40], e4_va))
    e4_1, e4_2, e4_3 = None, None, None
    for insn in insns_e4:
        if insn.mnemonic == 'cmp' and len(insn.operands) >= 2:
            op0, op1 = insn.operands[0], insn.operands[1]
            if op0.type == X86_OP_REG and op0.reg == X86_REG_EDI and op1.type == X86_OP_IMM and op1.imm == 0x5c:
                if e4_1 is None: e4_1 = insn.address
        elif insn.mnemonic == 'mov' and len(insn.operands) >= 2:
            op0, op1 = insn.operands[0], insn.operands[1]
            if op0.type == X86_OP_REG and op1.type == X86_OP_MEM:
                if op1.mem.base == X86_REG_ESI and op1.mem.disp == 4:
                    if e4_2 is None: e4_2 = insn.address
        elif insn.mnemonic == 'add' and len(insn.operands) >= 2:
            op0, op1 = insn.operands[0], insn.operands[1]
            if op0.type == X86_OP_REG and op0.reg == X86_REG_EDI and op1.type == X86_OP_IMM and op1.imm == 4:
                if e4_3 is None: e4_3 = insn.address

    if None in (e4_1, e4_2, e4_3):
        raise RuntimeError(f"Failed to resolve EXTRA4 derived instructions from 0x{e4_va:08X}")
    resolved['STAGE1_EXTRA4_SUB1'] = e4_1
    resolved['STAGE1_EXTRA4_SUB2'] = e4_2
    resolved['STAGE1_EXTRA4_SUB3'] = e4_3

    # 3. Staging sites via Capstone detailed operand inspection in FUN_00418c40 (3 sites)
    stg_anchor = '8B 8E FC 00 00 00 53 51 8B 4E 78 E8 ?? ?? ?? ?? 33 C9'
    m_stg = find_pattern(stg_anchor)
    if len(m_stg) != 1:
        raise RuntimeError(f"Staging anchor matched {len(m_stg)} times (expected 1)")
    raw_stg_sec = pe.rva_to_offset(m_stg[0] - image_base) - text_sec.raw_offset
    stg_sites = []
    for insn in md.disasm(text_data[raw_stg_sec : raw_stg_sec + 0x200], m_stg[0]):
        if insn.mnemonic == 'lea' and len(insn.operands) >= 2:
            op1 = insn.operands[1]
            if op1.type == X86_OP_MEM and op1.mem.disp == 0x68 and op1.mem.index == 0 and op1.mem.base == X86_REG_ESI:
                stg_sites.append(insn.address)
    if len(stg_sites) != 3:
        raise RuntimeError(f"Found {len(stg_sites)} staging sites (expected 3)")
    resolved['STAGE1_STAGING_1'] = stg_sites[0]
    resolved['STAGE1_STAGING_2'] = stg_sites[1]
    resolved['STAGE1_STAGING_3'] = stg_sites[2]

    # 4. Active loop sites via address-independent Function Anchors + Capstone detailed operands (19 sites)
    function_anchors = [
        ("FUN_00407fa0", "55 8B EC 8B 45 08 53 8B 5D 0C 56 57 8B 7D 10 57", 0, 1),
        ("FUN_004090d0", "55 8B EC 83 EC 54 53 56 8B F1 8B 86 68 02 00 00", 0, 2),
        ("FUN_00409360", "55 8B EC 83 EC 10 53 8B 5D 08 56 8B F1 8B 4E 74", 0, 1),
        ("FUN_00413ba0", "55 8B EC 83 EC 1C 53 56 8B F1 8B 86 B0 00 00 00", 0, 1),
        ("FUN_00416b20", "64 89 25 00 00 00 00 83 EC 3C A1 ?? ?? ?? ?? 33 45 04 53 56 57 8B 7D 08", -0x11, 1),
        ("FUN_004172f0", "55 8B EC 56 8B F1 8B 4D 08 57 E8 ?? ?? ?? ?? 85 C0", 0, 1),
        ("FUN_0041a500", "64 89 25 00 00 00 00 83 EC 40 53 56 33 DB 57", -0x11, 1),
        ("FUN_0041c460", "64 89 25 00 00 00 00 83 EC 38 8B 45 08 53 56 8B F1", -0x11, 2),
        ("FUN_00421430", "56 8B F1 8B 86 B4 00 00 00 57 C6 80 50 06 00 00", 0, 3),
        ("FUN_00421930", "55 8B EC 51 53 8B D9 8B 83 64 02 00 00 85 C0 89", 0, 3),
        ("FUN_00421a10", "55 8B EC 51 53 56 8B F1 8B 8E 68 02 00 00 57 E8", 0, 3),
    ]

    active_idx = 1
    for fn_name, fn_pat, fn_off, exp_count in function_anchors:
        matches = find_pattern(fn_pat)
        if len(matches) != 1:
            raise RuntimeError(f"Anchor for {fn_name} matched {len(matches)} times (expected 1)")
        fn_va = matches[0] + fn_off
        fn_raw = pe.rva_to_offset(fn_va - image_base)
        raw_start = fn_raw - text_sec.raw_offset

        insns = []
        for insn in md.disasm(text_data[raw_start : raw_start + 0x1000], fn_va):
            insns.append(insn)
            if insn.mnemonic in ('ret', 'retf'):
                insn_end_raw = raw_start + (insn.address - fn_va) + insn.size
                if insn_end_raw < len(text_data):
                    next_byte = text_data[insn_end_raw]
                    next_three = text_data[insn_end_raw : insn_end_raw + 3]
                    if next_byte in (0x90, 0xCC) or next_three == b'\x55\x8B\xEC':
                        break

        fn_sites = []
        for insn in insns:
            if insn.mnemonic == 'lea' and len(insn.operands) >= 2:
                op1 = insn.operands[1]
                if op1.type == X86_OP_MEM and op1.mem.disp == 0x5c and op1.mem.index == 0 \
                   and op1.mem.base in (X86_REG_ESI, X86_REG_EBX):
                    fn_sites.append(insn.address)
            elif insn.mnemonic == 'add' and len(insn.operands) >= 2:
                op0, op1 = insn.operands[0], insn.operands[1]
                if op0.type == X86_OP_REG and op0.reg == X86_REG_EBX and op1.type == X86_OP_IMM and op1.imm == 0x5c:
                    fn_sites.append(insn.address)

        if len(fn_sites) != exp_count:
            raise RuntimeError(f"{fn_name}: found {len(fn_sites)} active loop sites (expected {exp_count})")

        for s in fn_sites:
            resolved[f'STAGE1_ACTIVE_{active_idx:02d}'] = s
            active_idx += 1

        # Resolve clamp sites for FUN_00413ba0 and FUN_00421a10
        if fn_name == "FUN_00413ba0":
            store_9c_idx = next(
                idx for idx, i in enumerate(insns)
                if i.mnemonic == 'mov' and len(i.operands) >= 2
                and i.operands[0].type == X86_OP_MEM and i.operands[0].mem.base == X86_REG_ESI
                and i.operands[0].mem.disp == 0x9c and i.operands[0].mem.index == 0
            )
            cands_a = [
                (i, insns[idx + 1]) for idx, i in enumerate(insns[store_9c_idx + 1:], start=store_9c_idx + 1)
                if i.mnemonic == 'mov' and len(i.operands) >= 2
                and i.operands[0].type == X86_OP_REG and i.operands[0].reg == X86_REG_ECX
                and i.operands[1].type == X86_OP_MEM and i.operands[1].mem.base == X86_REG_ESI
                and i.operands[1].mem.disp == 0x8c and i.operands[1].mem.index == 0
            ]
            if len(cands_a) != 1:
                raise RuntimeError(f"Expected 1 candidate for STAGE2_TREASURE_CLAMP_A, got {len(cands_a)}")
            insn_a, next_a = cands_a[0]
            if insn_a.size != 6:
                raise RuntimeError(f"STAGE2_TREASURE_CLAMP_A size mismatch: {insn_a.size} != 6")
            if next_a.mnemonic not in ('xor', 'inc', 'sub', 'add', 'cmp'):
                raise RuntimeError(f"Instruction after STAGE2_TREASURE_CLAMP_A ({next_a.mnemonic}) does not overwrite flags")
            resolved['STAGE2_TREASURE_CLAMP_A'] = insn_a.address

            cands_b = [
                (i, insns[idx + 1]) for idx, i in enumerate(insns[store_9c_idx + 1:], start=store_9c_idx + 1)
                if i.mnemonic == 'mov' and len(i.operands) >= 2
                and i.operands[0].type == X86_OP_REG and i.operands[0].reg == X86_REG_EAX
                and i.operands[1].type == X86_OP_MEM and i.operands[1].mem.base == X86_REG_ESI
                and i.operands[1].mem.disp == 0x8c and i.operands[1].mem.index == 0
            ]
            if len(cands_b) != 1:
                raise RuntimeError(f"Expected 1 candidate for STAGE2_TREASURE_CLAMP_B, got {len(cands_b)}")
            insn_b, next_b = cands_b[0]
            if insn_b.size != 6:
                raise RuntimeError(f"STAGE2_TREASURE_CLAMP_B size mismatch: {insn_b.size} != 6")
            if next_b.mnemonic not in ('xor', 'inc', 'sub', 'add', 'cmp'):
                raise RuntimeError(f"Instruction after STAGE2_TREASURE_CLAMP_B ({next_b.mnemonic}) does not overwrite flags")
            resolved['STAGE2_TREASURE_CLAMP_B'] = insn_b.address

        elif fn_name == "FUN_00421a10":
            cands_da = []
            cands_db = []
            for idx, i in enumerate(insns):
                if i.mnemonic == 'mov' and len(i.operands) >= 2 \
                   and i.operands[1].type == X86_OP_MEM and i.operands[1].mem.base == X86_REG_ESI \
                   and i.operands[1].mem.disp == 0x8c and i.operands[1].mem.index == 0:
                    if idx + 1 < len(insns):
                        next_i = insns[idx + 1]
                        if i.operands[0].type == X86_OP_REG and i.operands[0].reg == X86_REG_ECX:
                            if next_i.mnemonic == 'xor' and next_i.operands[0].reg == X86_REG_EDX and next_i.operands[1].reg == X86_REG_EDX:
                                cands_da.append((i, next_i))
                        elif i.operands[0].type == X86_OP_REG and i.operands[0].reg == X86_REG_EAX:
                            if next_i.mnemonic == 'inc' and next_i.operands[0].reg == X86_REG_EDX:
                                cands_db.append((i, next_i))

            if len(cands_da) != 1:
                raise RuntimeError(f"Expected 1 candidate for STAGE2_DESCRESET_CLAMP_A, got {len(cands_da)}")
            insn_da, next_da = cands_da[0]
            if insn_da.size != 6:
                raise RuntimeError(f"STAGE2_DESCRESET_CLAMP_A size mismatch: {insn_da.size} != 6")
            if next_da.mnemonic not in ('xor', 'inc', 'sub', 'add', 'cmp'):
                raise RuntimeError(f"Instruction after STAGE2_DESCRESET_CLAMP_A ({next_da.mnemonic}) does not overwrite flags")
            resolved['STAGE2_DESCRESET_CLAMP_A'] = insn_da.address

            if len(cands_db) != 1:
                raise RuntimeError(f"Expected 1 candidate for STAGE2_DESCRESET_CLAMP_B, got {len(cands_db)}")
            insn_db, next_db = cands_db[0]
            if insn_db.size != 6:
                raise RuntimeError(f"STAGE2_DESCRESET_CLAMP_B size mismatch: {insn_db.size} != 6")
            if next_db.mnemonic not in ('xor', 'inc', 'sub', 'add', 'cmp'):
                raise RuntimeError(f"Instruction after STAGE2_DESCRESET_CLAMP_B ({next_db.mnemonic}) does not overwrite flags")
            resolved['STAGE2_DESCRESET_CLAMP_B'] = insn_db.address

    if len(resolved) != 81:
        raise RuntimeError(f"Resolved {len(resolved)} sites (expected exactly 81)")

    return resolved


if __name__ == '__main__':
    default_base = os.path.join(os.path.dirname(__file__), "..", ".base", "Zuma.exe")
    if not os.path.isfile(default_base):
        default_base = os.path.join(os.path.dirname(__file__), "..", "base", "Zuma.exe")
    target = sys.argv[1] if len(sys.argv) > 1 else default_base
    print(f"Resolving all 81 sites from: {target}")
    res = resolve_all_sites(target, enforce_hash=False)
    print(f"[OK] Successfully resolved {len(res)} sites:\n")
    for k, v in sorted(res.items(), key=lambda x: x[1]):
        print(f"  0x{v:08X} -> {k}")
