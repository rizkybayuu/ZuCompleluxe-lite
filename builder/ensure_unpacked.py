#!/usr/bin/env python3
"""ensure_unpacked.py - Adaptive Executable Unpacker and Layout Normalizer for ZuCompleluxe.

Inspects target executable, classifies its packing state, and ensures an authentic,
uncompressed baseline PE exists with valid .text section and valid entry point.

CLI Usage:
    python ensure_unpacked.py <exe> --out <path> [--upx <upx.exe>]

Exit Codes:
    0: Success - Baseline ready (plain retail or cleanly unpacked UPX).
    1: Usage error, file I/O failure, or file is not a valid PE.
    2: UPX packer detected but decompressor failed or upx.exe is missing.
    3: Unknown/unsupported packer layout or invalid post-unpack structure.

Output:
    Outputs single machine-readable status line on stdout:
    STATE=plain
    STATE=upx-unpacked
"""

import sys
import os
import shutil
import subprocess
import argparse
import pefile


def _name(s):
    return s.Name.rstrip(b"\0").decode("latin1")


def sniff(path):
    """Parses PE section names, locates entry point section, and checks UPX magic."""
    pe = pefile.PE(path, fast_load=True)
    names = [_name(s) for s in pe.sections]
    ep = pe.OPTIONAL_HEADER.AddressOfEntryPoint
    ep_sec = next(
        (
            _name(s)
            for s in pe.sections
            if s.VirtualAddress <= ep < s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData)
        ),
        None,
    )
    with open(path, "rb") as f:
        magic = b"UPX!" in f.read(0x2000)
    pe.close()
    return names, ep_sec, magic


def classify(path):
    """Classifies PE layout into 'upx', 'plain', or 'unknown'."""
    names, ep_sec, magic = sniff(path)
    if magic or "UPX0" in names or "UPX1" in names:
        return "upx"
    if ".text" in names:
        return "plain"
    return "unknown"


def main():
    ap = argparse.ArgumentParser(description="Ensure unpacked baseline executable for ZuCompleluxe.")
    ap.add_argument("exe", help="Path to input executable")
    ap.add_argument("--out", required=True, help="Destination path for plain uncompressed executable")
    ap.add_argument(
        "--upx",
        default=os.path.join(os.path.dirname(__file__), "tools", "upx.exe"),
        help="Path to upx.exe binary",
    )
    a = ap.parse_args()

    if not os.path.isfile(a.exe):
        print(f"[ERROR] Input executable not found: {a.exe}")
        return 1

    try:
        kind = classify(a.exe)
    except Exception as e:
        print(f"[ERROR] Not a readable PE: {e}")
        return 1

    out_dir = os.path.dirname(os.path.abspath(a.out))
    if out_dir and not os.path.exists(out_dir):
        os.makedirs(out_dir, exist_ok=True)

    if kind == "plain":
        if os.path.abspath(a.exe) != os.path.abspath(a.out):
            shutil.copyfile(a.exe, a.out)
        print("STATE=plain")
        return 0

    if kind == "unknown":
        print(
            "[ERROR] Unsupported executable structure: Section .text not found and no UPX signature detected.\n"
            "        The file may be packed with an unsupported protector or heavily modified."
        )
        return 3

    # kind == "upx"
    if not os.path.isfile(a.upx):
        print(
            f"[ERROR] Target is compressed with UPX, but decompressor was not found at:\n"
            f"        {a.upx}\n"
            f"        Please run toolchain bootstrap or place upx.exe manually into builder\\tools\\."
        )
        return 2

    tmp_out = a.out + ".tmp_unpack"
    if os.path.exists(tmp_out):
        try:
            os.remove(tmp_out)
        except OSError:
            pass

    # Unpack to temporary file
    r = subprocess.run([a.upx, "-d", "-q", f"-o{tmp_out}", a.exe], capture_output=True, text=True)
    if r.returncode != 0:
        err_msg = (r.stdout + "\n" + r.stderr).strip()
        print(
            f"[ERROR] UPX decompression failed (header may be modified, scrubbed, or anti-unpack protected):\n"
            f"        {err_msg}"
        )
        if os.path.exists(tmp_out):
            try:
                os.remove(tmp_out)
            except OSError:
                pass
        return 2

    # Validate unpacked output
    try:
        names, ep_sec, _ = sniff(tmp_out)
    except Exception as e:
        print(f"[ERROR] Decompressed output is not a valid PE: {e}")
        if os.path.exists(tmp_out):
            try:
                os.remove(tmp_out)
            except OSError:
                pass
        return 3

    # Valid Zuma Deluxe executables must contain .text, and Entry Point must reside in .text or GARR
    if ".text" not in names or ep_sec not in (".text", "GARR"):
        print(
            f"[ERROR] Decompressed executable layout is invalid for Zuma Deluxe.\n"
            f"        Detected sections: {names}, EntryPoint section: {ep_sec}"
        )
        if os.path.exists(tmp_out):
            try:
                os.remove(tmp_out)
            except OSError:
                pass
        return 3

    # Atomically move temporary file to final target
    if os.path.exists(a.out):
        try:
            os.remove(a.out)
        except OSError:
            pass
    os.replace(tmp_out, a.out)

    print("STATE=upx-unpacked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
