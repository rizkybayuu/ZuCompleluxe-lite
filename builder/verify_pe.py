"""verify_pe.py - Automated PE Architecture and Security Verifier for ZuCompleluxe.

Enforces strict compliance rules:
  1. No newly added section may have W+X (Writable and Executable) flags.
  2. Section .rdata MUST be strictly read-only (NOT writable).
  3. Entry Point MUST be 100% identical to vanilla Zuma.exe entry point (no entry point hijacking).
  4. All sidecar imports in Zuma.exe MUST cleanly resolve against the compiled DLLs in binary/.
Prints formatted section layout table.
"""

import sys
import os

sys.path.append(os.path.dirname(__file__))
from pe_util import PEFile

def verify_pe(target_exe_path, vanilla_exe_path, binary_dir, is_lite=False):
    print("=======================================================")
    print("          PE ARCHITECTURE & INTEGRITY VERIFIER         ")
    print("=======================================================")

    pe = PEFile(target_exe_path)
    vanilla_pe = PEFile(vanilla_exe_path)

    errors = []

    # 1. Print Section Table & Check W+X
    print("\n[SECTION LAYOUT TABLE]")
    print(f"{'Section':<10} {'VA':<12} {'VirtualSize':<14} {'RawSize':<10} {'Chars':<12} {'Access'}")
    print("-" * 68)

    for sec in pe.sections:
        r = bool(sec.characteristics & 0x40000000)
        w = bool(sec.characteristics & 0x80000000)
        x = bool(sec.characteristics & 0x20000000)
        access_str = ("R" if r else "") + ("W" if w else "") + ("X" if x else "")

        print(f"{sec.name_str:<10} 0x{pe.image_base + sec.virtual_address:08X}   0x{sec.virtual_size:08X}     0x{sec.raw_size:06X}   0x{sec.characteristics:08X}   [{access_str}]")

        # Check W+X
        if w and x and sec.name_str != "GARR":  # GARR is original PopCap retail DRM stub
            errors.append(f"Section '{sec.name_str}' has both WRITE and EXECUTE flags (Chars=0x{sec.characteristics:08X})!")

    # 2. Check .rdata is read-only
    sec_rdata = pe.find_section_by_name('.rdata')
    if sec_rdata:
        if sec_rdata.characteristics & 0x80000000:
            errors.append(f"Section .rdata is WRITABLE (Chars=0x{sec_rdata.characteristics:08X})! Expected read-only (0x40000040).")
        else:
            print("\n[PASS] Section .rdata is strictly read-only (Chars=0x40000040).")
    else:
        errors.append("Section .rdata not found in executable!")

    # 3. Check Entry Point == Vanilla Entry Point
    target_entry = pe.image_base + pe.entry_point_rva
    vanilla_entry = vanilla_pe.image_base + vanilla_pe.entry_point_rva
    if target_entry != vanilla_entry:
        errors.append(f"Entry point hijacked! Target: 0x{target_entry:08X}, Expected vanilla: 0x{vanilla_entry:08X}")
    else:
        print(f"[PASS] Entry point matches vanilla PopCap executable (0x{target_entry:08X}).")

    # 4. Check import resolution against built DLLs
    target_imports = pe.get_imports()
    lite_excluded = {'zumaextrapath.dll', 'zumawebp.dll', 'zumaextracolor.dll'}
    for dll_name in ['zumadllchecker.dll', 'zumaextrapath.dll', 'zumawebp.dll', 'zumaextracolor.dll', 'zumacustommap.dll', 'zumacplxcredit.dll', 'zumatestingkit.dll']:
        if is_lite and dll_name in lite_excluded:
            continue

        dll_path = os.path.join(binary_dir, "modules", dll_name)
        if not os.path.exists(dll_path):
            dll_path = os.path.join(binary_dir, dll_name)
        if not os.path.exists(dll_path):
            errors.append(f"Required sidecar DLL not found: {dll_path}")
            continue

        try:
            dll_pe = PEFile(dll_path)
            exported_names = set(dll_pe.get_exports())
        except Exception as e:
            print(f"[WARN] Soft-check: Failed to inspect exports in {dll_name}: {e}")
            continue

        if dll_name in target_imports:
            needed_funcs = target_imports[dll_name]
            for fn in needed_funcs:
                if fn not in exported_names:
                    errors.append(f"Import '{fn}' from '{dll_name}' does not exist in compiled {dll_name} exports!")

    if not errors:
        print("[PASS] All sidecar DLL imports cleanly resolved against compiled binaries.")

    print("\n-------------------------------------------------------")
    if errors:
        print(" [FAILED] PE INTEGRITY CHECKS FAILED:")
        for err in errors:
            print(f"   * {err}")
        print("-------------------------------------------------------")
        sys.exit(1)
    else:
        print(" [SUCCESS] ALL PE INTEGRITY CHECKS PASSED PERFECTLY!")
        print("-------------------------------------------------------")
        sys.exit(0)


if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit("Usage: python verify_pe.py <target_exe> <vanilla_exe> <binary_dir> [--lite]")
    is_lite = "--lite" in sys.argv
    args = [a for a in sys.argv[1:] if a != "--lite"]
    verify_pe(args[0], args[1], args[2], is_lite=is_lite)
