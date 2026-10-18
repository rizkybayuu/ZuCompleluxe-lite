#!/usr/bin/env python3
"""find_target_exe.py - Discover candidate Zuma Deluxe executables in a game folder.

Heuristics:
1. If Zuma.exe exists, it is the primary target.
2. Otherwise, inspects all *.exe files (>= 300 KB), excluding known installers/uninstallers.
3. Examines PE .rsrc VersionInfo (ProductName, FileDescription, OriginalFilename) and .rdata.
4. If exactly 1 candidate is found, selects it.
5. If multiple candidates are found, prompts the user interactively (or lists options).

Output:
    Prints selected executable filename (basename only, e.g. Zuma.exe) on stdout.
    Exit code 0 on success, 1 on not found or cancelled.
"""

import sys
import os
import pefile

EXCLUDED_EXES = {"uninstal.exe", "uninstall.exe", "unins000.exe", "setup.exe", "installer.exe"}


def is_zuma_pe(exe_path):
    """Checks if an executable has Zuma signatures in .rsrc or .rdata."""
    try:
        pe = pefile.PE(exe_path, fast_load=True)
        # Check VersionInfo in .rsrc
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
        if hasattr(pe, "FileInfo"):
            for fileinfo in pe.FileInfo:
                for sub in fileinfo:
                    if hasattr(sub, "StringTable"):
                        for st in sub.StringTable:
                            for k, v in st.entries.items():
                                val_str = v.decode("utf-8", errors="ignore").lower()
                                if "zuma" in val_str or "popcap" in val_str:
                                    pe.close()
                                    return True
        pe.close()
    except Exception:
        pass

    # Fallback: check file bytes for PopCap / Zuma signatures (in .rsrc or uncompressed sections)
    try:
        with open(exe_path, "rb") as f:
            header_sample = f.read(0x10000)
            if b"PopCap" in header_sample or b"Zuma" in header_sample:
                return True
            # Check tail resource sample
            f.seek(max(0, os.path.getsize(exe_path) - 0x20000))
            tail_sample = f.read(0x20000)
            if b"PopCap" in tail_sample or b"Zuma" in tail_sample:
                return True
    except Exception:
        pass

    return False


def find_target_exe(game_dir, non_interactive=False):
    primary = os.path.join(game_dir, "Zuma.exe")
    if os.path.isfile(primary):
        return "Zuma.exe"

    candidates = []
    try:
        for f in os.listdir(game_dir):
            if not f.lower().endswith(".exe"):
                continue
            if f.lower() in EXCLUDED_EXES:
                continue
            full_p = os.path.join(game_dir, f)
            if not os.path.isfile(full_p):
                continue
            # Size threshold >= 300 KB
            if os.path.getsize(full_p) < 300 * 1024:
                continue
            if is_zuma_pe(full_p):
                candidates.append(f)
    except Exception as e:
        sys.stderr.write(f"[ERROR] Failed to scan directory {game_dir}: {e}\n")
        return None

    if len(candidates) == 1:
        return candidates[0]

    if len(candidates) > 1:
        sys.stderr.write(f"\n[PROMPT] Multiple Zuma Deluxe executables detected in folder:\n")
        for i, c in enumerate(candidates, 1):
            sys.stderr.write(f"  [{i}] {c}\n")
        if non_interactive or not sys.stdin.isatty():
            # In non-interactive mode, default to candidate 1
            sys.stderr.write(f"[INFO] Non-interactive mode: selecting {candidates[0]}\n")
            return candidates[0]
        while True:
            try:
                sys.stderr.write(f"Select executable [1-{len(candidates)}] or 'q' to cancel: ")
                sys.stderr.flush()
                choice = sys.stdin.readline().strip()
                if choice.lower() == 'q':
                    return None
                idx = int(choice)
                if 1 <= idx <= len(candidates):
                    return candidates[idx - 1]
            except (ValueError, IndexError):
                sys.stderr.write("Invalid selection, please try again.\n")

    return None


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("Usage: python find_target_exe.py <game_dir> [--non-interactive]\n")
        sys.exit(1)

    game_dir = sys.argv[1]
    non_interactive = "--non-interactive" in sys.argv

    if not os.path.isdir(game_dir):
        sys.stderr.write(f"[ERROR] Directory does not exist: {game_dir}\n")
        sys.exit(1)

    selected = find_target_exe(game_dir, non_interactive=non_interactive)
    if not selected:
        sys.stderr.write(f"[ERROR] No valid Zuma Deluxe executable found in: {game_dir}\n")
        sys.exit(1)

    print(selected)
    sys.exit(0)


if __name__ == "__main__":
    main()
