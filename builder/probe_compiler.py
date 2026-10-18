import sys
import os
import subprocess
import ctypes

SEM_FAILCRITICALERRORS = 0x0001
SEM_NOGPFAULTERRORBOX = 0x0002
SEM_NOOPENFILEERRORBOX = 0x8000
ctypes.windll.kernel32.SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX)

compiler = sys.argv[1]
build_tmp = sys.argv[2] if len(sys.argv) > 2 else ".build"
os.makedirs(build_tmp, exist_ok=True)
probe_c = os.path.join(build_tmp, "probe.c")
probe_dll = os.path.join(build_tmp, "probe.dll")

with open(probe_c, "w") as f:
    f.write("int __stdcall P(void){return 1;}\n")

env = os.environ.copy()
comp_dir = os.path.dirname(os.path.abspath(compiler))
env["PATH"] = comp_dir + os.pathsep + env.get("PATH", "")

try:
    res = subprocess.run([compiler, "-m32", "-shared", "-o", probe_dll, probe_c], env=env, capture_output=True, timeout=5)
    sys.exit(res.returncode)
except Exception:
    sys.exit(1)
