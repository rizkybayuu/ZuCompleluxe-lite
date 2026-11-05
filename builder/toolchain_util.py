"""toolchain_util.py - Universal Toolchain Manager, Disk Monitor & Compile Engine.

Handles:
  - Pre-flight disk space verification on cache & build targets.
  - Recursive orphan lock file cleanup for Zig.
  - Interactive compiler C-Runtime warm-up probe with live progress timer.
  - Uniform DLL compilation with timing, byte statistics, live progress heartbeat, and root-cause error diagnosis.
  - Safe DLL deployment with process lock detection (Zuma.exe) and error reporting.
"""

import sys
import os
import time
import shutil
import subprocess

def clean_zig_locks(cache_dir):
    """Safely removes orphan *.lock files from Zig cache when zig.exe is not running."""
    if not cache_dir:
        return 0
    try:
        out = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq zig.exe'], capture_output=True, text=True).stdout
        if 'zig.exe' in out.lower():
            return 0
    except Exception:
        pass

    zig_cache = os.path.join(cache_dir, 'zig')
    if not os.path.isdir(zig_cache):
        return 0

    cleaned = 0
    for root, dirs, files in os.walk(zig_cache):
        for f in files:
            if f.endswith('.lock'):
                try:
                    os.remove(os.path.join(root, f))
                    cleaned += 1
                except Exception:
                    pass
    return cleaned

def cmd_disk_check():
    cache_dir = os.environ.get('LOCALAPPDATA', '')
    target_raw = os.environ.get('TARGET_ZUMA_DIR', '').strip('"')
    target_dir = target_raw if target_raw and os.path.exists(target_raw) else os.getcwd()

    c_free_mb = shutil.disk_usage(cache_dir).free // (1024 * 1024) if cache_dir else 0
    t_free_mb = shutil.disk_usage(target_dir).free // (1024 * 1024)

    c_free_gb = c_free_mb / 1024.0
    t_free_gb = t_free_mb / 1024.0

    c_drive = os.path.splitdrive(cache_dir)[0] if cache_dir else "C:"
    t_drive = os.path.splitdrive(target_dir)[0]

    print(f"[INFO] Disk Space Check   : Cache Drive ({c_drive}) = {c_free_gb:.1f} GB free | Target Drive ({t_drive}) = {t_free_gb:.1f} GB free")

    if c_free_mb < 500:
        print(f"[ERROR] DISK FULL / LOW DISK SPACE! Cache drive {c_drive} only has {c_free_mb} MB free (minimum 500 MB required).")
        sys.exit(1)

    if t_free_mb < 500:
        print(f"[ERROR] DISK FULL / LOW DISK SPACE! Target drive {t_drive} only has {t_free_mb} MB free (minimum 500 MB required).")
        sys.exit(1)

    # Recursive orphan lock cleanup in %LOCALAPPDATA%\zig
    cleaned = clean_zig_locks(cache_dir)
    if cleaned > 0:
        print(f"[INFO] Cleaned {cleaned} orphan compiler lock file(s) in {os.path.join(cache_dir, 'zig')}")

    sys.exit(0)

def cmd_warmup_probe():
    if len(sys.argv) < 5:
        sys.exit("Usage: toolchain_util.py warmup_probe <compiler_type> <build_tmp> <compiler_bin>")

    compiler_type = sys.argv[2].lower()
    build_tmp = sys.argv[3]
    compiler_bin = sys.argv[4]

    os.makedirs(build_tmp, exist_ok=True)
    src_file = os.path.join(build_tmp, "warmup_probe.c")
    dll_file = os.path.join(build_tmp, "warmup_probe.dll")

    with open(src_file, "w", encoding="utf-8") as f:
        f.write("int dummy_probe(void) { return 42; }\n")

    if compiler_type == 'gcc':
        cmd = [compiler_bin, "-m32", "-O2", "-shared", "-o", dll_file, src_file]
    else:
        cmd = [compiler_bin, "-m", "ziglang", "cc", "-target", "x86-windows-gnu", "-O2", "-shared", "-o", dll_file, src_file]

    start_t = time.time()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    warmup_shown = False
    last_tick = 0
    while proc.poll() is None:
        time.sleep(0.5)
        elapsed = int(time.time() - start_t)
        if elapsed >= 2 and not warmup_shown:
            warmup_shown = True
            print("============================================================")
            print("[WARMUP] Building 32-bit C Runtime cache (first-time setup)...")
            print("[WARMUP] Compiling C Runtime libraries, please wait...")
            print("============================================================")
        if warmup_shown and elapsed >= last_tick + 3:
            last_tick = elapsed
            print(f"[WARMUP] Still compiling C Runtime libraries... {elapsed}s elapsed", flush=True)

    out, err = proc.communicate()
    total_time = time.time() - start_t

    for p in [src_file, dll_file]:
        if os.path.exists(p):
            try: os.remove(p)
            except Exception: pass

    if proc.returncode != 0:
        print(f"[ERROR] Toolchain warm-up probe failed (exit code {proc.returncode})!")
        err_msg = err.decode('utf-8', errors='ignore')
        if err_msg:
            print(err_msg.strip())
        sys.exit(1)

    if warmup_shown:
        print(f"[OK] Compiler C Runtime cache ready (completed in {total_time:.1f}s).")
    else:
        print(f"[OK] Compiler C Runtime ready ({total_time:.2f}s).")

    sys.exit(0)

def diagnose_compile_error(log_text, build_tmp):
    diagnoses = []
    low_log = log_text.lower()

    # 1. Disk full check
    try:
        cache_dir = os.environ.get('LOCALAPPDATA', '')
        c_free = shutil.disk_usage(cache_dir).free // (1024 * 1024) if cache_dir else 999
        t_free = shutil.disk_usage(build_tmp).free // (1024 * 1024)
        if 'no space left on device' in low_log or 'enospc' in low_log or 'disk full' in low_log or c_free < 50 or t_free < 50:
            diagnoses.append("Disk Full / Low Disk Space! Free up space on drive C: (compiler cache) or the target workspace drive.")
    except Exception:
        pass

    # 2. Permission / file lock
    if 'permission denied' in low_log or 'access is denied' in low_log or 'cannot open output file' in low_log:
        diagnoses.append("File Access Denied / File Locked! The output file is locked by another process, antivirus, or a running Zuma.exe instance.")

    # 3. File not found
    if 'no such file or directory' in low_log or ('fatal error:' in low_log and 'not found' in low_log):
        diagnoses.append("Missing Source or Toolchain File! A required source file, header, or library was not found or was quarantined by antivirus.")

    # 4. Cache corruption / lock conflict
    if 'cachecheckfailed' in low_log or 'unable to update cache' in low_log:
        diagnoses.append("Compiler Cache Conflict (CacheCheckFailed)! Close any background zig.exe processes or delete %LOCALAPPDATA%\\zig.")

    # 5. Unsupported linker flag
    if 'unsupported linker arg' in low_log:
        diagnoses.append("Unsupported Linker Argument! The active compiler does not support one of the passed linker flags.")

    return diagnoses

def run_compiler_with_progress(cmd, out_dll):
    start_t = time.time()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    last_tick = 0
    while proc.poll() is None:
        time.sleep(0.3)
        elapsed = int(time.time() - start_t)
        if elapsed >= 3 and elapsed >= last_tick + 3:
            last_tick = elapsed
            print(f"[COMPILING] Still compiling {out_dll}... {elapsed}s elapsed", flush=True)
    out, _ = proc.communicate()
    return proc.returncode, out, time.time() - start_t

def cmd_compile_dll():
    # Args: compile_dll <compiler_type> <compiler_bin> <build_tmp> <out_dll> <export_sym> <src_files...> [--xflags ...] [--libs ...]
    if len(sys.argv) < 7:
        sys.exit("Usage: toolchain_util.py compile_dll <compiler_type> <compiler_bin> <build_tmp> <out_dll> <export_sym> <srcs...> [--xflags ...] [--libs ...]")

    compiler_type = sys.argv[2].lower()
    compiler_bin = sys.argv[3]
    build_tmp = sys.argv[4]
    out_dll = sys.argv[5]
    export_sym = sys.argv[6]

    # Parse remaining args: sources, xflags, libs
    raw_args = sys.argv[7:]
    src_files = []
    xflags = []
    libs = []

    mode = 'src'
    for a in raw_args:
        if a == '--xflags':
            mode = 'xflags'
        elif a == '--libs':
            mode = 'libs'
        else:
            if mode == 'src':
                src_files.append(a)
            elif mode == 'xflags':
                xflags.extend(a.split())
            elif mode == 'libs':
                libs.extend(a.split())

    out_path = os.path.join(build_tmp, out_dll)
    log_path = os.path.join(build_tmp, f"{out_dll}.log")

    if os.path.exists(out_path):
        try: os.remove(out_path)
        except Exception: pass

    compiler_label = f"GCC ({compiler_bin})" if compiler_type == 'gcc' else "Zig CC"
    src_display = " ".join(src_files)
    print(f"[COMPILING] {out_dll} ({src_display}) via {compiler_label}...")

    if compiler_type == 'gcc':
        cmd = [compiler_bin, "-m32", "-O2", "-s", "-shared", "-Wall", "-Werror=int-conversion"]
        cmd.extend(xflags)
        cmd.extend(["-o", out_path])
        cmd.extend(src_files)
        cmd.extend(["-static"])
        cmd.extend(libs)
        cmd.extend(["-Wl,--kill-at", "-Wl,--no-insert-timestamp", "-Wl,--enable-stdcall-fixup"])
    else:
        cmd = [compiler_bin, "-m", "ziglang", "cc", "-target", "x86-windows-gnu", "-O2", "-s", "-shared", "-Wall", "-Werror=int-conversion"]
        cmd.extend(xflags)
        cmd.extend(["-o", out_path])
        cmd.extend(src_files)
        cmd.extend(libs)

    retcode, out_text, elapsed = run_compiler_with_progress(cmd, out_dll)

    # Automatic 1-time retry on transient Zig cache conflict
    if retcode != 0 and 'cachecheckfailed' in out_text.lower():
        print(f"[WARN] Transient Zig cache conflict detected while building {out_dll}; cleaning orphan locks and retrying once...")
        clean_zig_locks(os.environ.get('LOCALAPPDATA', ''))
        retcode, out_text, elapsed = run_compiler_with_progress(cmd, out_dll)

    with open(log_path, "w", encoding="utf-8") as f:
        f.write(out_text)

    if retcode != 0 or not os.path.exists(out_path):
        print(f"[ERROR] Compile failed for {out_dll} (exit code {retcode})!")
        print("-------------------------------------------------------")
        print(out_text.strip())
        print("-------------------------------------------------------")
        diagnoses = diagnose_compile_error(out_text, build_tmp)
        if diagnoses:
            print("[DIAGNOSIS]")
            for d in diagnoses:
                print(f"  * {d}")
        sys.exit(1)

    # Check for warnings in log
    if out_text.strip():
        print(f"[COMPILER WARNINGS] {out_dll}:")
        print("-------------------------------------------------------")
        print(out_text.strip())
        print("-------------------------------------------------------")

    # Verify export symbol via pefile
    try:
        import pefile
        pe = pefile.PE(out_path)
        try:
            exp_names = [e.name.decode() for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name] if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT') else []
            if export_sym not in exp_names:
                print(f"[ERROR] Export '{export_sym}' missing or decorated in {out_dll}!")
                print(f"        Available exports: {', '.join(exp_names[:10])}")
                sys.exit(1)
        finally:
            pe.close()
    except Exception as ex:
        print(f"[ERROR] PE export inspection failed for {out_dll}: {ex}")
        sys.exit(1)

    file_size = os.path.getsize(out_path)
    print(f"[OK] {out_dll} built in {elapsed:.1f}s ({file_size:,} bytes), export '{export_sym}' verified.")
    sys.exit(0)

def cmd_deploy_dll():
    if len(sys.argv) < 4:
        sys.exit("Usage: toolchain_util.py deploy_dll <src_path> <dst_path>")

    src_path = sys.argv[2]
    dst_path = sys.argv[3]
    dll_name = os.path.basename(src_path)

    os.makedirs(os.path.dirname(dst_path), exist_ok=True)

    try:
        shutil.copyfile(src_path, dst_path)
        print(f"[OK] {dll_name} deployed successfully to {dst_path}")
        sys.exit(0)
    except PermissionError:
        print(f"[ERROR] Failed to deploy {dll_name} to '{dst_path}'!")
        print("[DIAGNOSIS] File Access Denied / Target File Locked!")
        print("            The destination file is currently in use by another process (close Zuma.exe if running) or locked by antivirus.")
        sys.exit(1)
    except OSError as oe:
        print(f"[ERROR] Failed to deploy {dll_name} to '{dst_path}': {oe}")
        if 'space' in str(oe).lower() or getattr(oe, 'errno', None) == 28:
            print("[DIAGNOSIS] Disk Full / No Space Left on Device! Free up space on the target drive.")
        else:
            print("[DIAGNOSIS] File Access Denied or I/O Error! Ensure Zuma.exe is closed and you have write permissions to the target folder.")
        sys.exit(1)

def main():
    if len(sys.argv) < 2:
        sys.exit("toolchain_util.py: missing command.")

    cmd = sys.argv[1].lower()
    if cmd == 'disk_check':
        cmd_disk_check()
    elif cmd == 'warmup_probe':
        cmd_warmup_probe()
    elif cmd == 'compile_dll':
        cmd_compile_dll()
    elif cmd == 'deploy_dll':
        cmd_deploy_dll()
    else:
        sys.exit(f"toolchain_util.py: unknown command '{cmd}'")

def entry():
    main()

if __name__ == '__main__':
    main()
