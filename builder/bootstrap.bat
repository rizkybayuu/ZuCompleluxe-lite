@echo off
REM =======================================================================
REM ZuCompleluxe Toolchain Bootstrap & Engine Manager
REM Shared across build_lite.bat and build_full.bat
REM =======================================================================
if "!BUILD_TMP!"=="" set "BUILD_TMP=.build"
if not exist "!BUILD_TMP!" mkdir "!BUILD_TMP!" >nul 2>&1

set "PATH=%PATH%;%SystemRoot%\System32\WindowsPowerShell\v1.0"
set "PS_EXE=powershell.exe"
if exist "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" set "PS_EXE=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"

if "%~1"=="ensure_python" goto EnsurePython
if "%~1"=="ensure_pydeps" goto EnsurePyDeps
if "%~1"=="detect_compiler" goto DetectCompiler
if "%~1"=="probe_webp" goto ProbeWebp
if "%~1"=="compile_dll" goto CompileDll
if "%~1"=="deploy_prebuilt" goto DeployPrebuiltDll
if "%~1"=="print_summary" goto PrintBuildSummary

echo [ERROR] Unknown bootstrap command: %~1
exit /b 1

REM =======================================================================
REM 1. Python Environment Detection & Bootstrap (Minimum Python 3.11+)
REM =======================================================================
:EnsurePython
echo [INFO] Scanning adaptively for Python 3.11+ runtime...
set "PYTHON_BIN="

REM Check local tools\python\python.exe first
if exist "tools\python\python.exe" (
    "tools\python\python.exe" -c "import sys; assert sys.version_info >= (3, 11)" >nul 2>&1
    if not errorlevel 1 set "PYTHON_BIN=tools\python\python.exe"
)

REM Check System PATH (filter out dead WindowsApps stubs)
if "!PYTHON_BIN!"=="" (
    for /f "delims=" %%p in ('where python.exe py.exe 2^>nul') do (
        if "!PYTHON_BIN!"=="" (
            echo "%%p" | findstr /i "WindowsApps" >nul
            if errorlevel 1 (
                "%%p" -c "import sys; assert sys.version_info >= (3, 11)" >nul 2>&1
                if not errorlevel 1 set "PYTHON_BIN=%%p"
            )
        )
    )
)

REM Check standard directories on active drives
if "!PYTHON_BIN!"=="" (
    for %%d in (C: D: E: F: G:) do (
        if exist "%%d\" if "!PYTHON_BIN!"=="" (
            for %%p in (
                "Python314\python.exe" "Python313\python.exe" "Python312\python.exe" "Python311\python.exe"
                "Program Files\Python314\python.exe" "Program Files\Python313\python.exe"
                "Program Files\Python312\python.exe" "Program Files\Python311\python.exe"
            ) do (
                if "!PYTHON_BIN!"=="" if exist "%%d\%%~p" (
                    "%%d\%%~p" -c "import sys; assert sys.version_info >= (3, 11)" >nul 2>&1
                    if not errorlevel 1 set "PYTHON_BIN=%%d\%%~p"
                )
            )
            if "!PYTHON_BIN!"=="" if exist "%%d\Users" (
                for /d %%u in ("%%d\Users\*") do (
                    if exist "%%u\AppData\Local\Programs\Python" if "!PYTHON_BIN!"=="" (
                        for /d %%w in ("%%u\AppData\Local\Programs\Python\Python31*") do (
                            if exist "%%w\python.exe" if "!PYTHON_BIN!"=="" (
                                "%%w\python.exe" -c "import sys; assert sys.version_info >= (3, 11)" >nul 2>&1
                                if not errorlevel 1 set "PYTHON_BIN=%%w\python.exe"
                            )
                        )
                    )
                )
            )
        )
    )
)

if not "!PYTHON_BIN!"=="" goto PythonDetected

REM Portable Embedded AMD64 Python Fallback (Zero-Dependency)
echo [INFO] First run downloads ~120 MB (portable Python + Zig toolchain). Please wait...
echo [INFO] Downloading portable embedded Python 3.11.9 amd64...
if not exist "tools\python" mkdir "tools\python"
set "PY_ZIP=!BUILD_TMP!\python_embed_amd64.zip"
set "PY_URL=https://www.python.org/ftp/python/3.11.9/python-3.11.9-embed-amd64.zip"
set "PY_SHA256=009d6bf7e3b2ddca3d784fa09f90fe54336d5b60f0e0f305c37f400bf83cfd3b"

if exist "%SystemRoot%\System32\curl.exe" (
    "%SystemRoot%\System32\curl.exe" -L --fail --retry 3 -# -o "!PY_ZIP!" "!PY_URL!"
) else (
    "%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '!PY_URL!' -OutFile '!PY_ZIP!'"
)

if exist "!PY_ZIP!" (
    certutil -hashfile "!PY_ZIP!" SHA256 | findstr /i "!PY_SHA256!" >nul
    if errorlevel 1 (
        echo [ERROR] SHA-256 verification failed for embedded Python package!
        del /f /q "!PY_ZIP!" >nul 2>&1
        exit /b 1
    )
    echo [OK] SHA-256 verified for embedded Python package.
    echo [EXTRACT] Extracting embedded Python via tar.exe...
    if exist "%SystemRoot%\System32\tar.exe" (
        "%SystemRoot%\System32\tar.exe" -xf "!PY_ZIP!" -C "tools\python"
    ) else (
        "%PS_EXE%" -NoProfile -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('!PY_ZIP!', 'tools\python')"
    )
    del /f /q "!PY_ZIP!" >nul 2>&1
    if exist "tools\python\python.exe" set "PYTHON_BIN=tools\python\python.exe"
)

:PythonDetected
if "!PYTHON_BIN!"=="" (
    echo [ERROR] Python 3.11+ could not be located or installed.
    echo         Please install Python manually from https://www.python.org/
    exit /b 1
)

for %%F in ("!PYTHON_BIN!") do set "PATH=%%~dpF;!PATH!"
for %%P in ("!PYTHON_BIN!") do set "PY_DIR=%%~dpP"
for %%F in ("!PY_DIR!python*._pth") do call :PatchPth "%%F"
set "PY_VER_STR=unknown"
for /f "tokens=*" %%V in ('"!PYTHON_BIN!" --version 2^>^&1') do set "PY_VER_STR=%%V"
echo [FOUND] Python Runtime    : "!PYTHON_BIN!" - !PY_VER_STR!
"!PYTHON_BIN!" -c "import sys; print('[INFO] Python Mode       : isolated=' + str(sys.flags.isolated) + ', sys.path=' + str(sys.path))"
exit /b 0

REM =======================================================================
REM 2. Python Dependencies Verification & Pip Bootstrapping
REM =======================================================================
:EnsurePyDeps
set "PIP_OPTS=--disable-pip-version-check --no-warn-script-location --retries 3 --timeout 30"

REM Always patch embedded Python ._pth first so site-packages & builder are visible
for %%P in ("!PYTHON_BIN!") do set "PY_DIR=%%~dpP"
for %%F in ("!PY_DIR!python*._pth") do call :PatchPth "%%F"

"!PYTHON_BIN!" -c "import capstone,pefile,PIL,numpy,ziglang" >nul 2>&1
if not errorlevel 1 (
    echo [OK] Python build libraries verified: capstone, pefile, pillow, numpy, ziglang.
    exit /b 0
)

"!PYTHON_BIN!" -m pip --version >nul 2>&1
if errorlevel 1 (
    echo [INFO] pip missing, embedded Python detected. Bootstrapping pip...
    set "GET_PIP=!BUILD_TMP!\get-pip.py"
    if exist "%SystemRoot%\System32\curl.exe" (
        "%SystemRoot%\System32\curl.exe" -L --fail --retry 3 -# -o "!GET_PIP!" https://bootstrap.pypa.io/get-pip.py
    ) else (
        "%PS_EXE%" -NoProfile -Command "Invoke-WebRequest -Uri 'https://bootstrap.pypa.io/get-pip.py' -OutFile '!GET_PIP!'"
    )
    if errorlevel 1 (
        echo [ERROR] Failed to download get-pip.py payload.
        del /f /q "!GET_PIP!" >nul 2>&1
        exit /b 1
    )
    if not exist "!GET_PIP!" (
        echo [ERROR] Failed to download get-pip.py payload.
        exit /b 1
    )
    echo [OK] Downloaded get-pip.py. Installing pip...
    "!PYTHON_BIN!" "!GET_PIP!" --no-warn-script-location
)

echo [AUTO-SETUP] Installing build dependencies: capstone, pefile, pillow, numpy, ziglang...
if exist "requirements.txt" (
    "!PYTHON_BIN!" -m pip install !PIP_OPTS! -r requirements.txt
    if errorlevel 1 "!PYTHON_BIN!" -m pip install --user !PIP_OPTS! -r requirements.txt
) else (
    "!PYTHON_BIN!" -m pip install !PIP_OPTS! capstone pefile pillow numpy ziglang
    if errorlevel 1 "!PYTHON_BIN!" -m pip install --user !PIP_OPTS! capstone pefile pillow numpy ziglang
)

set "DEPS_FAIL=0"
for %%M in (capstone pefile PIL numpy ziglang) do (
    "!PYTHON_BIN!" -c "import %%M" >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] Missing Python module: %%M
        set "DEPS_FAIL=1"
    )
)
if "!DEPS_FAIL!"=="1" (
    echo [ERROR] Dependencies not installed. This is NOT a Zuma.exe problem.
    echo         Check internet connection or proxy settings and retry.
    exit /b 1
)
echo [OK] All Python dependencies installed and verified.
exit /b 0

:PatchPth
"!PYTHON_BIN!" "builder\prepare_embed.py" "%~1" "%CD%" >nul 2>&1
exit /b 0

REM =======================================================================
REM 3. Compiler Detection (Probe Native GCC 32-bit, Zig, Portable Fallback)
REM =======================================================================
:ProbeGcc32
"!PYTHON_BIN!" "builder\probe_compiler.py" "%~1" "!BUILD_TMP!" >nul 2>&1
if not errorlevel 1 (
    set "GCC32=%~1"
    for %%F in ("%~1") do set "PATH=%%~dpF;!PATH!"
    echo [FOUND] Verified 32-bit GCC : "%~1"
) else (
    echo [SKIP] %~1 cannot target 32-bit x86
)
exit /b 0

:DetectCompiler
set "GCC32="
set "ZIG_OK=0"
set "ZIG_GLOBAL_CACHE_DIR=%CD%\.build\zigcache"

REM 1. Probe PATH compilers
for %%c in (gcc.exe clang.exe) do (
    if "!GCC32!"=="" (
        where %%c >nul 2>nul
        if not errorlevel 1 for /f "delims=" %%p in ('where %%c 2^>nul') do (
            if "!GCC32!"=="" call :ProbeGcc32 "%%p"
        )
    )
)

REM 2. Fast-Path Standard Working Toolchains
if "!GCC32!"=="" (
    for %%d in (C: D: E:) do (
        if exist "%%d\" if "!GCC32!"=="" (
            for %%p in (
                "tools\w64devkit\bin\gcc.exe"
                "msys64\mingw32\bin\gcc.exe"
                "mingw32\bin\gcc.exe"
            ) do (
                if "!GCC32!"=="" if exist "%%d\%%~p" call :ProbeGcc32 "%%d\%%~p"
            )
        )
    )
)

REM 3. Python Zig Cross-Compiler Tier
"!PYTHON_BIN!" -m ziglang version >nul 2>&1
if not errorlevel 1 (
    set "ZIG_OK=1"
    echo [FOUND] Zig Cross-Compiler : "!PYTHON_BIN!" -m ziglang cc [32-bit target]
)

REM 4. Portable w64devkit Download Tier (Only if neither GCC nor Zig is ready)
if "!GCC32!"=="" if "!ZIG_OK!"=="0" (
    echo [INFO] No native 32-bit compiler found. Downloading portable w64devkit i686...
    set "W64_URL=https://github.com/skeeto/w64devkit/releases/download/v1.23.0/w64devkit-i686-1.23.0.zip"
    set "W64_ZIP=!BUILD_TMP!\w64devkit.zip"
    if not exist "tools" mkdir "tools"
    if exist "%SystemRoot%\System32\curl.exe" (
        "%SystemRoot%\System32\curl.exe" -L --fail --retry 3 -# -o "!W64_ZIP!" "!W64_URL!"
    ) else (
        "%PS_EXE%" -NoProfile -Command "Invoke-WebRequest -Uri '!W64_URL!' -OutFile '!W64_ZIP!'"
    )
    if exist "!W64_ZIP!" (
        echo [EXTRACT] Extracting w64devkit via tar.exe...
        if exist "%SystemRoot%\System32\tar.exe" (
            "%SystemRoot%\System32\tar.exe" -xf "!W64_ZIP!" -C tools
        ) else (
            "%PS_EXE%" -NoProfile -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('!W64_ZIP!', 'tools')"
        )
        del /f /q "!W64_ZIP!" >nul 2>&1
        if exist "tools\w64devkit\bin\gcc.exe" call :ProbeGcc32 "%CD%\tools\w64devkit\bin\gcc.exe"
    )
)

if "!GCC32!"=="" if "!ZIG_OK!"=="0" (
    echo [ERROR] No 32-bit C compiler could be prepared.
    exit /b 1
)
exit /b 0

REM =======================================================================
REM 3.5 Probe libwebp compilation capabilities
REM =======================================================================
:ProbeWebp
set "WEBP_OK=0"
if "!GCC32!"=="" exit /b 0
> "!BUILD_TMP!\probe_webp.c" echo #include ^<webp/demux.h^>
>> "!BUILD_TMP!\probe_webp.c" echo int test_webp^(void^){WebPAnimDecoderOptions opt;return WebPAnimDecoderOptionsInit^(^&opt^);}
"!PYTHON_BIN!" -c "import ctypes, subprocess, sys; ctypes.windll.kernel32.SetErrorMode(0x8003); res = subprocess.run([r'!GCC32!', '-m32', '-shared', '-o', r'!BUILD_TMP!\probe_webp.dll', r'!BUILD_TMP!\probe_webp.c', '-static', '-lwebpdemux', '-lwebp', '-lsharpyuv'], capture_output=True); sys.exit(res.returncode)" >nul 2>&1
if not errorlevel 1 (
    set "WEBP_OK=1"
    echo [FOUND] libwebp static libraries detected for GCC
) else (
    echo [INFO] libwebp static libraries not present in GCC toolchain
)
exit /b 0

REM =======================================================================
REM 4. Uniform DLL Compiler Engine
REM Variables: OUT, SRC, EXPORT, XFLAGS, LIBS
REM =======================================================================
:CompileDll
set "LOG=!BUILD_TMP!\!OUT!.log"
del /f /q "!BUILD_TMP!\!OUT!" >nul 2>&1
set "DLL_STATUS_!OUT!=failed"

if not "!GCC32!"=="" (
    echo [COMPILING] !OUT! with GCC: !GCC32!
    "!GCC32!" -m32 -O2 -s -shared !XFLAGS! -o "!BUILD_TMP!\!OUT!" !SRC! -static !LIBS! -Wl,--kill-at -Wl,--no-insert-timestamp >"!LOG!" 2>&1
    if not errorlevel 1 set "DLL_STATUS_!OUT!=built (gcc)"
) else if "!ZIG_OK!"=="1" (
    echo [COMPILING] !OUT! with Zig CC
    "!PYTHON_BIN!" -m ziglang cc -target x86-windows-gnu -O2 -s -shared !XFLAGS! -o "!BUILD_TMP!\!OUT!" !SRC! !LIBS! >"!LOG!" 2>&1
    if not errorlevel 1 set "DLL_STATUS_!OUT!=built (zig)"
) else (
    echo [ERROR] No compiler available for !OUT! >"!LOG!"
    exit /b 1
)

if "!DLL_STATUS_!OUT!"=="failed" (
    echo [ERROR] Compile failed for !OUT!. Build log follows:
    echo -------------------------------------------------------
    type "!LOG!"
    echo -------------------------------------------------------
    exit /b 1
)

REM Exact Export Verification via pefile
"!PYTHON_BIN!" -c "import pefile,sys;p=pefile.PE(sys.argv[1]);n=[e.name.decode() for e in p.DIRECTORY_ENTRY_EXPORT.symbols if e.name];sys.exit(0 if sys.argv[2] in n else 1)" "!BUILD_TMP!\!OUT!" "!EXPORT!"
if errorlevel 1 (
    echo [ERROR] Export '!EXPORT!' missing or decorated in !OUT!
    exit /b 1
)

copy /y "!BUILD_TMP!\!OUT!" "!TARGET_ZUMA_DIR!\modules\!OUT!" >nul
for %%S in ("!OUT!") do echo [OK] !OUT! deployed successfully [!DLL_STATUS_%%~S!], export !EXPORT! verified.
exit /b 0

REM =======================================================================
REM 5. Prebuilt DLL Fallback Deployer
REM Variables: OUT, EXPORT, PREBUILT_PATH
REM =======================================================================
:DeployPrebuiltDll
set "DLL_STATUS_!OUT!=missing"
if exist "!PREBUILT_PATH!" (
    if exist "prebuilt\zumawebp.sha256" (
        "!PYTHON_BIN!" -c "import hashlib,sys; h1=hashlib.sha256(open('src/zumawebp.dll.c','rb').read()).hexdigest(); h2=hashlib.sha256(open('src/zumawebp_player.c','rb').read()).hexdigest(); sys.exit(0 if h1=='873a43f500f049e1679c1b67479c2b2ef33ba2f2ca2df2efc21bb60c431cd9b8' and h2=='40fb2e35ebe3423052306ad07e328a812f7c4a7bdb33de7cac0746a0adb703ea' else 1)" >nul 2>&1
        if errorlevel 1 echo [WARNING] Source files for zumawebp have changed! Prebuilt DLL may be stale.
    )
    echo [INFO] Deploying prebuilt !OUT! from !PREBUILT_PATH!...
    copy /y "!PREBUILT_PATH!" "!BUILD_TMP!\!OUT!" >nul
    "!PYTHON_BIN!" -c "import pefile,sys;p=pefile.PE(sys.argv[1]);n=[e.name.decode() for e in p.DIRECTORY_ENTRY_EXPORT.symbols if e.name];sys.exit(0 if sys.argv[2] in n else 1)" "!BUILD_TMP!\!OUT!" "!EXPORT!"
    if not errorlevel 1 (
        copy /y "!BUILD_TMP!\!OUT!" "!TARGET_ZUMA_DIR!\modules\!OUT!" >nul
        set "DLL_STATUS_!OUT!=prebuilt"
        echo [OK] !OUT! deployed successfully [prebuilt], export !EXPORT! verified.
        exit /b 0
    ) else (
        echo [WARNING] Prebuilt !OUT! failed export check for !EXPORT!
    )
)
echo [SKIP] !OUT! omitted - compiler missing webp libraries and prebuilt not found.
set "DLL_STATUS_!OUT!=skipped (no libwebp)"
exit /b 0

REM =======================================================================
REM 6. Build Summary Table
REM =======================================================================
:PrintBuildSummary
echo.
echo =======================================================
echo    BUILD VERIFICATION SUMMARY
echo =======================================================
if exist "!TARGET_ZUMA_DIR!\Zuma.exe" (
    for %%A in ("!TARGET_ZUMA_DIR!\Zuma.exe") do echo  [OK] Zuma.exe                  - %%~zA bytes
) else (
    echo  [MISSING] Zuma.exe
)

for %%D in (zumadllchecker.dll zumawebp.dll zumaextrapath.dll zumaextracolor.dll zumacustommap.dll zumacplxcredit.dll zumatestingkit.dll) do (
    if defined DLL_STATUS_%%D (
        if exist "!TARGET_ZUMA_DIR!\modules\%%D" (
            for %%A in ("!TARGET_ZUMA_DIR!\modules\%%D") do echo  [OK] modules\%%D - !DLL_STATUS_%%D! - %%~zA bytes
        ) else (
            echo  [--] modules\%%D - !DLL_STATUS_%%D!
        )
    )
)
echo =======================================================
exit /b 0
