@echo off
setlocal EnableDelayedExpansion
set "SCRIPT_PATH=%~f0"

REM =======================================================================
REM 0. Check non-interactive / CI flag & parameters
REM =======================================================================
set "NON_INTERACTIVE=0"
set "CLI_ZUMA_DIR="

:ParseArgs
if "%~1"=="" goto DoneParseArgs
if /i "%~1"=="/yes" (
    set "NON_INTERACTIVE=1"
    shift
    goto ParseArgs
)
if /i "%~1"=="-y" (
    set "NON_INTERACTIVE=1"
    shift
    goto ParseArgs
)
if /i "%~1"=="/y" (
    set "NON_INTERACTIVE=1"
    shift
    goto ParseArgs
)
if "!CLI_ZUMA_DIR!"=="" (
    set "CLI_ZUMA_DIR=%~f1"
    shift
    goto ParseArgs
)
shift
goto ParseArgs
:DoneParseArgs

if defined CI set "NON_INTERACTIVE=1"
if "!NON_INTERACTIVE!"=="1" set "ZUMA_KEEP_OPEN=1"

REM =======================================================================
REM 0.1 Keep Console Open Check (for Explorer double-click only)
REM =======================================================================
if not defined ZUMA_KEEP_OPEN (
    set "ZUMA_KEEP_OPEN=1"
    cmd /k call "%SCRIPT_PATH%" %*
    exit /b
)

REM 1. Terminate game if running to prevent file locks
taskkill /f /im Zuma.exe >nul 2>&1

echo =======================================================
echo    ZuCompleluxe v0.4.0.1 Lite - 1-Click Game Patcher ^& Builder
echo    Direct Installation Folder Patcher (Lite Edition)
echo =======================================================
echo.

REM =======================================================================
REM [PRIORITY 0] Target Zuma Deluxe Folder Selection
REM =======================================================================
set "TARGET_ZUMA_DIR="

REM Check CLI argument first
if not "!CLI_ZUMA_DIR!"=="" (
    if exist "!CLI_ZUMA_DIR!\*" (
        set "TARGET_ZUMA_DIR=!CLI_ZUMA_DIR!"
    ) else if exist "!CLI_ZUMA_DIR!" (
        REM If user passed path directly to Zuma.exe
        for %%F in ("!CLI_ZUMA_DIR!") do set "TARGET_ZUMA_DIR=%%~dpF"
    )
)

REM If not provided via CLI, launch modern File Explorer dialog with "Patch Folder" button
if "!TARGET_ZUMA_DIR!"=="" (
    echo [PROMPT] Opening folder selection dialog...
    echo Please select your Zuma Deluxe installation folder in File Explorer and click "Patch Folder".
    echo.
    if exist "builder\select_folder.ps1" (
        for /f "usebackq delims=" %%f in (`powershell -NoProfile -ExecutionPolicy Bypass -File "builder\select_folder.ps1"`) do (
            set "TARGET_ZUMA_DIR=%%f"
        )
    ) else if exist "builder\select_folder.py" (
        for /f "usebackq delims=" %%f in (`python "builder\select_folder.py"`) do (
            set "TARGET_ZUMA_DIR=%%f"
        )
    )
)

if not "!TARGET_ZUMA_DIR!"=="" goto HaveTargetDir

REM Fallback prompt if GUI was closed or cancelled
if "!NON_INTERACTIVE!"=="1" (
    echo [ERROR] No Zuma directory specified in non-interactive mode.
    exit /b 1
)
echo [PROMPT] Enter Zuma Deluxe installation folder path (or press Enter to cancel):
set /p USER_INPUT_PATH="> "
if "!USER_INPUT_PATH!"=="" (
    echo [CANCELLED] Operation cancelled by user.
    pause
    exit /b 1
)
set "USER_INPUT_PATH=!USER_INPUT_PATH:"=!"
if "!USER_INPUT_PATH:~-1!"=="\" set "USER_INPUT_PATH=!USER_INPUT_PATH:~0,-1!"
if "!USER_INPUT_PATH:~-1!"=="/" set "USER_INPUT_PATH=!USER_INPUT_PATH:~0,-1!"
set "TARGET_ZUMA_DIR=!USER_INPUT_PATH!"

:HaveTargetDir
REM Strip trailing slashes safely
if "!TARGET_ZUMA_DIR:~-1!"=="\" set "TARGET_ZUMA_DIR=!TARGET_ZUMA_DIR:~0,-1!"
if "!TARGET_ZUMA_DIR:~-1!"=="/" set "TARGET_ZUMA_DIR=!TARGET_ZUMA_DIR:~0,-1!"

if not exist "!TARGET_ZUMA_DIR!\*" (
    echo.
    echo ===================================================================
    echo  [ERROR] Invalid folder or path not found:
    echo  "!TARGET_ZUMA_DIR!"
    echo ===================================================================
    echo.
    pause
    exit /b 1
)


echo [OK] Target Folder : "!TARGET_ZUMA_DIR!"
echo.

REM Validate or create original Zuma.exe backup in modules\bak\Zuma.exe
set "BAK_DIR=!TARGET_ZUMA_DIR!\modules\bak"
if not exist "!BAK_DIR!" mkdir "!BAK_DIR!"

REM Migrate legacy Zuma.exe.bak if present
if exist "!TARGET_ZUMA_DIR!\Zuma.exe.bak" (
    if not exist "!BAK_DIR!\Zuma.exe" move /y "!TARGET_ZUMA_DIR!\Zuma.exe.bak" "!BAK_DIR!\Zuma.exe" >nul
    if exist "!TARGET_ZUMA_DIR!\Zuma.exe.bak" del /f /q "!TARGET_ZUMA_DIR!\Zuma.exe.bak" >nul
)

set "SRC_VANILLA="
if exist "!BAK_DIR!\Zuma.exe" (
    echo [BACKUP] Using existing original backup: modules\bak\Zuma.exe
    set "SRC_VANILLA=!BAK_DIR!\Zuma.exe"
) else if exist "!TARGET_ZUMA_DIR!\Zuma.exe" (
    echo [BACKUP] Creating original backup: Zuma.exe -^> modules\bak\Zuma.exe ...
    copy /y "!TARGET_ZUMA_DIR!\Zuma.exe" "!BAK_DIR!\Zuma.exe" >nul
    set "SRC_VANILLA=!BAK_DIR!\Zuma.exe"
) else (
    echo.
    echo ===================================================================
    echo  [ERROR] Zuma.exe not found in folder:
    echo  "!TARGET_ZUMA_DIR!"
    echo ===================================================================
    echo.
    pause
    exit /b 1
)

for %%A in ("!SRC_VANILLA!") do set "ZUMA_SIZE=%%~zA"
echo [INFO] Original executable confirmed (!ZUMA_SIZE! bytes).
echo.

REM =======================================================================
REM [PRIORITY 1] Check Computer Specs & Active Drives
REM =======================================================================
set "SYS_ARCH=x64"
if "%PROCESSOR_ARCHITECTURE%"=="x86" (
    if not defined PROCESSOR_ARCHITEW6432 set "SYS_ARCH=x86"
)
if "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "SYS_ARCH=ARM64"

set "TARGET_DRIVE=%SystemDrive%"
if "%TARGET_DRIVE%"=="" set "TARGET_DRIVE=C:"

echo [INFO] System Architecture : %SYS_ARCH%
echo [INFO] Target Binary Arch  : x86 (32-bit Native PE)
echo [INFO] System Drive        : %TARGET_DRIVE%
echo.

set "COMPILER_TYPE="
set "COMPILER_BIN="
set "VCVARS_BAT="

echo [INFO] Detecting active drives on this system...
set "ACTIVE_DRIVES="

for /f "tokens=1*" %%a in ('fsutil fsinfo drives 2^>nul') do (
    for %%d in (%%b) do (
        set "drv=%%d"
        set "drv=!drv:\=!"
        if not "!drv!"=="" (
            if exist "!drv!\" set "ACTIVE_DRIVES=!ACTIVE_DRIVES! !drv!"
        )
    )
)

if "%ACTIVE_DRIVES%"=="" (
    for %%d in (C D E F G H I J K L M N O P Q R S T U V W X Y Z A B) do (
        if exist "%%d:\" set "ACTIVE_DRIVES=!ACTIVE_DRIVES! %%d:"
    )
)

echo [INFO] Active drives found : %ACTIVE_DRIVES%
echo [INFO] Scanning adaptively for available 32-bit compilers...
echo.

REM =======================================================================
REM [PRIORITY 2] Adaptive 32-bit Compiler Search
REM =======================================================================

REM --- 1. Check MSVC 32-bit via vswhere ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -prerelease -property installationPath 2^>nul`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvars32.bat" (
            set "VCVARS_BAT=%%i\VC\Auxiliary\Build\vcvars32.bat"
            set "COMPILER_TYPE=MSVC"
        ) else if exist "%%i\VC\Auxiliary\Build\vcvarsall.bat" (
            set "VCVARS_BAT=%%i\VC\Auxiliary\Build\vcvarsall.bat x86"
            set "COMPILER_TYPE=MSVC"
        )
    )
)

REM --- 2. Check System PATH ---
if "!COMPILER_TYPE!"=="" (
    for %%c in (gcc.exe g++.exe clang.exe cl.exe) do (
        if "!COMPILER_TYPE!"=="" (
            where %%c >nul 2>nul
            if !ERRORLEVEL! equ 0 (
                %%c -dumpmachine >nul 2>nul
                if !ERRORLEVEL! equ 0 (
                    set "COMPILER_BIN=%%c"
                    set "COMPILER_TYPE=GCC"
                ) else (
                    set "COMPILER_BIN=%%c"
                    set "COMPILER_TYPE=C_COMPILER"
                )
            )
        )
    )
)

REM --- 3. Fast-Path Check (Active Drives) ---
if "!COMPILER_TYPE!"=="" (
    for %%d in (%ACTIVE_DRIVES%) do (
        if "!COMPILER_TYPE!"=="" (
            for %%p in (
                "apps-github\SkyRetail\v0.1\tools\w64devkit\bin\gcc.exe"
                "apps-github\SkyRetail\v0.1\tools\w64devkit\bin\g++.exe"
                "tools\w64devkit\bin\gcc.exe"
                "tools\w64devkit\bin\g++.exe"
                "Program Files\CodeBlocks\MinGW\bin\gcc.exe"
                "Program Files (x86)\CodeBlocks\MinGW\bin\gcc.exe"
                "Program Files\LLVM\bin\clang.exe"
                "msys64\mingw32\bin\gcc.exe"
                "msys64\ucrt64\bin\gcc.exe"
                "msys64\mingw64\bin\gcc.exe"
                "msys64\clang32\bin\clang.exe"
                "mingw32\bin\gcc.exe"
                "mingw64\bin\gcc.exe"
                "MinGW\bin\gcc.exe"
                "TDM-GCC-64\bin\gcc.exe"
                "Strawberry\c\bin\gcc.exe"
                "w64devkit\bin\gcc.exe"
            ) do (
                if "!COMPILER_TYPE!"=="" (
                    if exist "%%d\%%~p" (
                        set "COMPILER_BIN=%%d\%%~p"
                        set "COMPILER_TYPE=GCC"
                    )
                )
            )

            if "!COMPILER_TYPE!"=="" (
                for /d %%w in ("%%d\winlibs*" "%%d\Qt\Tools\mingw*") do (
                    if exist "%%w\bin\gcc.exe" if "!COMPILER_TYPE!"=="" (
                        set "COMPILER_BIN=%%w\bin\gcc.exe"
                        set "COMPILER_TYPE=GCC"
                    )
                )
            )
        )
    )
)

if not "!COMPILER_BIN!"=="" goto CompilerFound
if "!COMPILER_TYPE!"=="MSVC" goto CompilerFound

REM --- 3.5 Package Manager Tier (winget / choco / scoop) ---
if "!COMPILER_TYPE!"=="" (
    echo -------------------------------------------------------
    echo    [WARNING] No 32-bit compiler found via PATH or common paths.
    echo -------------------------------------------------------
    set /p CONFIRM_INSTALL="[PROMPT] Try installing MinGW compiler automatically via winget/choco/scoop? (y/N): "
    if /i "!CONFIRM_INSTALL!"=="y" (
        set "PKG_MGR="
        where winget >nul 2>nul && set "PKG_MGR=winget"
        if "!PKG_MGR!"=="" (where choco >nul 2>nul && set "PKG_MGR=choco")
        if "!PKG_MGR!"=="" (where scoop >nul 2>nul && set "PKG_MGR=scoop")

        if "!PKG_MGR!"=="" (
            echo [SKIP] No package manager detected.
        ) else (
            echo [INFO] Detected package manager: !PKG_MGR!
            if "!PKG_MGR!"=="winget" (
                winget install --id BrechtSanders.WinLibs.POSIX.UCRT -e --silent --accept-source-agreements --accept-package-agreements
            ) else if "!PKG_MGR!"=="choco" (
                choco install mingw -y
            ) else (
                scoop install gcc
            )
            where gcc.exe >nul 2>nul && (set "COMPILER_BIN=gcc.exe" & set "COMPILER_TYPE=GCC")
        )
    )
)

if not "!COMPILER_TYPE!"=="" goto CompilerFound

REM --- 4. Recursive Deep Scan ---
echo [INFO] Fast-path missed. Executing DEEP SCAN on active drives...
for /f "usebackq delims=" %%f in (`powershell -NoProfile -Command "Get-PSDrive -PSProvider FileSystem | Where-Object { $_.Used -gt 0 -or $_.Free -gt 0 } | ForEach-Object { Get-ChildItem -Path $_.Root -Filter 'gcc.exe' -Recurse -ErrorAction SilentlyContinue } | Select-Object -First 1 -ExpandProperty FullName"`) do (
    if exist "%%f" (
        set "COMPILER_BIN=%%f"
        set "COMPILER_TYPE=GCC"
    )
)

if not "!COMPILER_TYPE!"=="" goto CompilerFound

REM --- 5. Portable Fallback Toolchain ---
echo [INFO] Downloading portable 32-bit toolchain (w64devkit)...
if not exist "tools" mkdir "tools"
set "DL_FILE=%TARGET_DRIVE%\w64devkit_dl.zip"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ProgressPreference = 'SilentlyContinue'; [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; try { $rel = Invoke-RestMethod -Uri 'https://api.github.com/repos/skeeto/w64devkit/releases/latest' -Headers @{'User-Agent'='zuma-builder'}; $asset = $rel.assets | Where-Object { $_.name -like 'w64devkit-i686-*.zip' -or $_.name -like 'w64devkit-*.zip' } | Select-Object -First 1; Invoke-WebRequest -Uri $asset.browser_download_url -OutFile '%DL_FILE%'; Expand-Archive -Path '%DL_FILE%' -DestinationPath 'tools' -Force; Remove-Item '%DL_FILE%' -ErrorAction SilentlyContinue } catch { exit 1 }"
if exist "tools\w64devkit\bin\gcc.exe" (
    set "COMPILER_BIN=tools\w64devkit\bin\gcc.exe"
    set "COMPILER_TYPE=GCC"
)

:CompilerFound
if not "!COMPILER_BIN!"=="" (
    for %%F in ("!COMPILER_BIN!") do set "PATH=%%~dpF;!PATH!"
)
if "!COMPILER_TYPE!"=="MSVC" (
    echo [FOUND] MSVC Environment : "!VCVARS_BAT!"
) else if not "!COMPILER_BIN!"=="" (
    echo [FOUND] !COMPILER_TYPE! Compiler    : "!COMPILER_BIN!"
) else (
    echo [INFO] Compiler not found - using Direct PE Synthesis Engine.
)
echo.

REM =======================================================================
REM [PRIORITY 2.5] Adaptive Python Environment Detection & Setup
REM =======================================================================
echo [INFO] Scanning adaptively for available Python 3.8+ runtime...
set "PYTHON_BIN="

REM --- 1. Check local tools\python\python.exe first ---
if exist "tools\python\python.exe" (
    "tools\python\python.exe" -c "import sys; assert sys.version_info >= (3, 8)" >nul 2>&1
    if !ERRORLEVEL! equ 0 (
        set "PYTHON_BIN=tools\python\python.exe"
    )
)

REM --- 2. Check System PATH (python.exe or py.exe), filtering WindowsApps stubs ---
if "!PYTHON_BIN!"=="" (
    for /f "delims=" %%p in ('where python.exe py.exe 2^>nul') do (
        if "!PYTHON_BIN!"=="" (
            echo "%%p" | findstr /i "WindowsApps" >nul
            if !ERRORLEVEL! neq 0 (
                "%%p" -c "import sys; assert sys.version_info >= (3, 8)" >nul 2>&1
                if !ERRORLEVEL! equ 0 (
                    set "PYTHON_BIN=%%p"
                )
            )
        )
    )
)

REM --- 3. Fast-Path Check on Active Drives & Common Directories ---
if "!PYTHON_BIN!"=="" (
    for %%d in (%ACTIVE_DRIVES%) do (
        if "!PYTHON_BIN!"=="" (
            for %%p in (
                "Python314\python.exe"
                "Python313\python.exe"
                "Python312\python.exe"
                "Python311\python.exe"
                "Python310\python.exe"
                "Python39\python.exe"
                "Python38\python.exe"
                "Program Files\Python314\python.exe"
                "Program Files\Python313\python.exe"
                "Program Files\Python312\python.exe"
                "Program Files\Python311\python.exe"
                "Program Files\Python310\python.exe"
                "Program Files\Python39\python.exe"
                "Program Files\Python38\python.exe"
            ) do (
                if "!PYTHON_BIN!"=="" (
                    if exist "%%d\%%~p" (
                        "%%d\%%~p" -c "import sys; assert sys.version_info >= (3, 8)" >nul 2>&1
                        if !ERRORLEVEL! equ 0 (
                            set "PYTHON_BIN=%%d\%%~p"
                        )
                    )
                )
            )

            if "!PYTHON_BIN!"=="" (
                if exist "%LocalAppData%\Programs\Python" (
                    for /d %%w in ("%LocalAppData%\Programs\Python\Python3*") do (
                        if "!PYTHON_BIN!"=="" (
                            if exist "%%w\python.exe" (
                                "%%w\python.exe" -c "import sys; assert sys.version_info >= (3, 8)" >nul 2>&1
                                if !ERRORLEVEL! equ 0 (
                                    set "PYTHON_BIN=%%w\python.exe"
                                )
                            )
                        )
                    )
                )
            )
        )
    )
)

if not "!PYTHON_BIN!"=="" goto PythonFound

REM --- 4. Package Manager Auto-Installation (winget / choco / scoop) ---
set "PKG_MGR="
where winget >nul 2>nul && set "PKG_MGR=winget"
if "!PKG_MGR!"=="" (where choco >nul 2>nul && set "PKG_MGR=choco")
if "!PKG_MGR!"=="" (where scoop >nul 2>nul && set "PKG_MGR=scoop")

if not "!PKG_MGR!"=="" (
    echo -------------------------------------------------------
    echo    [WARNING] Python 3.8+ was not found on your system.
    echo -------------------------------------------------------
    set "CONFIRM_PY=y"
    if "!NON_INTERACTIVE!"=="0" (
        set /p CONFIRM_PY="[PROMPT] Try installing Python automatically via !PKG_MGR!? (Y/n): "
    )
    if not "!CONFIRM_PY!"=="n" if not "!CONFIRM_PY!"=="N" (
        echo [INFO] Installing Python via !PKG_MGR! ...
        if "!PKG_MGR!"=="winget" (
            winget install --id Python.Python.3.11 -e --silent --accept-source-agreements --accept-package-agreements
        ) else if "!PKG_MGR!"=="choco" (
            choco install python -y
        ) else (
            scoop install python
        )

        for /f "delims=" %%p in ('where python.exe py.exe 2^>nul') do (
            if "!PYTHON_BIN!"=="" (
                echo "%%p" | findstr /i "WindowsApps" >nul
                if !ERRORLEVEL! neq 0 (
                    "%%p" -c "import sys; assert sys.version_info >= (3, 8)" >nul 2>&1
                    if !ERRORLEVEL! equ 0 (
                        set "PYTHON_BIN=%%p"
                    )
                )
            )
        )
        if "!PYTHON_BIN!"=="" if exist "%LocalAppData%\Programs\Python" (
            for /d %%w in ("%LocalAppData%\Programs\Python\Python3*") do (
                if "!PYTHON_BIN!"=="" if exist "%%w\python.exe" (
                    "%%w\python.exe" -c "import sys; assert sys.version_info >= (3, 8)" >nul 2>&1
                    if !ERRORLEVEL! equ 0 set "PYTHON_BIN=%%w\python.exe"
                )
            )
        )
    )
)

if not "!PYTHON_BIN!"=="" goto PythonFound

REM --- 5. Portable Embedded Python Fallback Download ---
echo [INFO] Downloading portable embedded Python 3.11 (zero-dependency fallback)...
if not exist "tools\python" mkdir "tools\python"
set "PY_ZIP=%TEMP%\python_embed_%RANDOM%.zip"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ProgressPreference = 'SilentlyContinue'; [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; $url = 'https://www.python.org/ftp/python/3.11.9/python-3.11.9-embed-win32.zip'; $outFile = '%PY_ZIP%'; $dest = 'tools\python'; $success = $false; for ($i=0; $i -lt 3; $i++) { try { Invoke-WebRequest -Uri $url -OutFile $outFile -TimeoutSec 30; if (Test-Path $outFile) { Expand-Archive -Path $outFile -DestinationPath $dest -Force; $success = $true; break } } catch { Start-Sleep -Seconds 2 } } Remove-Item $outFile -Force -ErrorAction SilentlyContinue; if (-not $success -or -not (Test-Path 'tools\python\python.exe')) { exit 1 }"

if exist "tools\python\python.exe" (
    "tools\python\python.exe" -c "import sys; assert sys.version_info >= (3, 8)" >nul 2>&1
    if !ERRORLEVEL! equ 0 (
        set "PYTHON_BIN=tools\python\python.exe"
    )
)

if "!PYTHON_BIN!"=="" (
    if exist "tools\python" rmdir /s /q "tools\python" >nul 2>&1
)

:PythonFound
if "!PYTHON_BIN!"=="" goto PythonNotFound

echo "!PYTHON_BIN!" | findstr /c:"\\" >nul
if !ERRORLEVEL! equ 0 (
    for %%F in ("!PYTHON_BIN!") do set "PATH=%%~dpF;!PATH!"
)
set "PY_VER_STR=unknown"
for /f "tokens=*" %%V in ('"!PYTHON_BIN!" --version 2^>^&1') do set "PY_VER_STR=%%V"
echo [FOUND] Python Runtime    : "!PYTHON_BIN!" - !PY_VER_STR!
goto PythonReady

:PythonNotFound
echo [ERROR] Python 3.8+ could not be located or installed.
echo         Please install Python manually from https://www.python.org/
pause
exit /b 1

:PythonReady
echo.

REM =======================================================================
REM [PRIORITY 3] Initialize Staging & Target Modules Directory
REM =======================================================================
set "BUILD_TMP=.build"
if not exist "!BUILD_TMP!" mkdir "!BUILD_TMP!"

if not exist "!TARGET_ZUMA_DIR!\modules" (
    echo [INFO] Creating directory: "!TARGET_ZUMA_DIR!\modules"
    mkdir "!TARGET_ZUMA_DIR!\modules"
)

set "GCC32="
if exist "C:\msys64\mingw32\bin\gcc.exe" set "GCC32=C:\msys64\mingw32\bin\gcc.exe"
if "!GCC32!"=="" if not "!COMPILER_BIN!"=="" set "GCC32=!COMPILER_BIN!"
if not "!GCC32!"=="" for %%F in ("!GCC32!") do set "PATH=%%~dpF;!PATH!"

REM =======================================================================
REM =======================================================================
REM [PRIORITY 4] Building Sidecar DLLs
REM =======================================================================

if not exist "!TARGET_ZUMA_DIR!\modules" mkdir "!TARGET_ZUMA_DIR!\modules" >nul 2>&1
REM Purge excluded sidecars from target modules directory so installation is strictly Lite
for %%D in (zumawebp.dll zumaextrapath.dll zumaextracolor.dll) do (
    if exist "!TARGET_ZUMA_DIR!\modules\%%D" del /f /q "!TARGET_ZUMA_DIR!\modules\%%D" >nul 2>&1
    if exist "!TARGET_ZUMA_DIR!\%%D" del /f /q "!TARGET_ZUMA_DIR!\%%D" >nul 2>&1
)
for %%D in (zumadllchecker.dll zumacustommap.dll zumacplxcredit.dll zumatestingkit.dll) do (
    if exist "!TARGET_ZUMA_DIR!\%%D" del /f /q "!TARGET_ZUMA_DIR!\%%D" >nul 2>&1
)

echo -------------------------------------------------------
echo  [FILE 1/4] Building zumadllchecker.dll (Lite Gatekeeper) ...
echo -------------------------------------------------------
set "BUILD_CHECKER_SUCCESS=0"
del /f /q "!BUILD_TMP!\zumadllchecker.dll" >nul 2>&1
if "!GCC32!"=="" (
    echo [ERROR] 32-bit gcc not found for zumadllchecker.dll
) else (
    echo [COMPILING] zumadllchecker.dll [Lite] using !GCC32!
    "!GCC32!" -m32 -O2 -s -shared -DZUCOMPLELUXE_LITE=1 -o "!BUILD_TMP!\zumadllchecker.dll" src\zumadllchecker.dll.c -static -lversion -luxtheme -ldwmapi -lgdi32 -luser32 -ladvapi32 -lshell32 -lkernel32 -Wl,--kill-at -Wl,--no-insert-timestamp >nul 2>&1
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Compile zumadllchecker.dll failed
    ) else (
        findstr /c:"ZDC_RunChecker" "!BUILD_TMP!\zumadllchecker.dll" >nul
        if !ERRORLEVEL! equ 0 (
            set "BUILD_CHECKER_SUCCESS=1"
            echo [OK] zumadllchecker.dll compiled [Lite], export ZDC_RunChecker verified
            copy /y "!BUILD_TMP!\zumadllchecker.dll" "!TARGET_ZUMA_DIR!\modules\zumadllchecker.dll" >nul
        ) else (
            echo [ERROR] DLL generated but export ZDC_RunChecker is missing
        )
    )
)
echo.

echo -------------------------------------------------------
echo  [FILE 2/4] Building zumacustommap.dll (Gauntlet custom map sidecar) ...
echo -------------------------------------------------------
set "BUILD_CUSTOMMAP_SUCCESS=0"
del /f /q "!BUILD_TMP!\zumacustommap.dll" >nul 2>&1
if not "!GCC32!"=="" (
    echo [COMPILING] zumacustommap.dll using !GCC32!
    "!GCC32!" -m32 -O2 -s -shared -o "!BUILD_TMP!\zumacustommap.dll" src\zumacustommap.dll.c -static -Wl,--kill-at -Wl,--no-insert-timestamp >nul 2>&1
) else (
    echo [COMPILING] zumacustommap.dll using zig cc
    "!PYTHON_BIN!" -m ziglang cc -target x86-windows-gnu -O2 -shared -o "!BUILD_TMP!\zumacustommap.dll" src\zumacustommap.dll.c >nul 2>&1
)
if exist "!BUILD_TMP!\zumacustommap.dll" (
    findstr /c:"zcm_AdjustCount" "!BUILD_TMP!\zumacustommap.dll" >nul
    if !ERRORLEVEL! equ 0 (
        set "BUILD_CUSTOMMAP_SUCCESS=1"
        echo [OK] zumacustommap.dll compiled, export zcm_AdjustCount verified
        copy /y "!BUILD_TMP!\zumacustommap.dll" "!TARGET_ZUMA_DIR!\modules\zumacustommap.dll" >nul
    ) else (
        echo [ERROR] DLL generated but export zcm_AdjustCount is missing
    )
) else (
    echo [ERROR] Compile zumacustommap.dll failed
)
echo.

echo -------------------------------------------------------
echo  [FILE 3/4] Building zumacplxcredit.dll (Developer Credits sidecar) ...
echo -------------------------------------------------------
set "BUILD_ZCCREDIT_SUCCESS=0"
del /f /q "!BUILD_TMP!\zumacplxcredit.dll" >nul 2>&1
if not "!GCC32!"=="" (
    echo [COMPILING] zumacplxcredit.dll using !GCC32!
    "!GCC32!" -m32 -O2 -s -shared -o "!BUILD_TMP!\zumacplxcredit.dll" src\zumacplxcredit.dll.c -static -Wl,--kill-at -Wl,--no-insert-timestamp >nul 2>&1
) else (
    echo [COMPILING] zumacplxcredit.dll using zig cc
    "!PYTHON_BIN!" -m ziglang cc -target x86-windows-gnu -O2 -shared -o "!BUILD_TMP!\zumacplxcredit.dll" src\zumacplxcredit.dll.c >nul 2>&1
)
if exist "!BUILD_TMP!\zumacplxcredit.dll" (
    findstr /c:"ZCC_GetCreditsTable" "!BUILD_TMP!\zumacplxcredit.dll" >nul
    if !ERRORLEVEL! equ 0 (
        set "BUILD_ZCCREDIT_SUCCESS=1"
        echo [OK] zumacplxcredit.dll compiled, export ZCC_GetCreditsTable verified
        copy /y "!BUILD_TMP!\zumacplxcredit.dll" "!TARGET_ZUMA_DIR!\modules\zumacplxcredit.dll" >nul
    ) else (
        echo [ERROR] DLL generated but export ZCC_GetCreditsTable is missing
    )
) else (
    echo [ERROR] Compile zumacplxcredit.dll failed
)
echo.

echo -------------------------------------------------------
echo  [FILE 4/4] Building zumatestingkit.dll (Developer Testing Kit sidecar) ...
echo -------------------------------------------------------
set "BUILD_ZTK_SUCCESS=0"
del /f /q "!BUILD_TMP!\zumatestingkit.dll" >nul 2>&1
if not "!GCC32!"=="" (
    echo [COMPILING] zumatestingkit.dll using !GCC32!
    "!GCC32!" -m32 -O2 -s -shared -o "!BUILD_TMP!\zumatestingkit.dll" src\zumatestingkit.dll.c -static -Wl,--kill-at -Wl,--no-insert-timestamp >nul 2>&1
) else (
    echo [COMPILING] zumatestingkit.dll using zig cc
    "!PYTHON_BIN!" -m ziglang cc -target x86-windows-gnu -O2 -shared -o "!BUILD_TMP!\zumatestingkit.dll" src\zumatestingkit.dll.c >nul 2>&1
)
if exist "!BUILD_TMP!\zumatestingkit.dll" (
    findstr /c:"ZTK_IsActive" "!BUILD_TMP!\zumatestingkit.dll" >nul
    if !ERRORLEVEL! equ 0 (
        set "BUILD_ZTK_SUCCESS=1"
        echo [OK] zumatestingkit.dll compiled, export ZTK_IsActive verified
        copy /y "!BUILD_TMP!\zumatestingkit.dll" "!TARGET_ZUMA_DIR!\modules\zumatestingkit.dll" >nul
    ) else (
        echo [ERROR] DLL generated but export ZTK_IsActive is missing
    )
) else (
    echo [ERROR] Compile zumatestingkit.dll failed
)
echo.

REM =======================================================================
echo [LITE] Skipping zumawebp.dll (Omitted in Lite Edition)
echo [LITE] Skipping zumaextrapath.dll (Omitted in Lite Edition)
echo [LITE] Skipping zumaextracolor.dll (Omitted in Lite Edition)
echo.
REM [PRIORITY 5] Synthesizing and Patching Zuma.exe
REM =======================================================================
echo -------------------------------------------------------
echo  Patching Zuma.exe into "!TARGET_ZUMA_DIR!\Zuma.exe" ...
echo -------------------------------------------------------
set "BUILD_SUCCESS=0"
del /f /q "!BUILD_TMP!\Zuma.exe" >nul 2>&1

REM 1. Inject static sidecar imports into .zdata
if exist "builder\patch_imports.py" (
    echo [PATCHING] Injecting static sidecar imports into .zdata...
    "!PYTHON_BIN!" "builder\patch_imports.py" "!SRC_VANILLA!" "!BUILD_TMP!\Zuma.exe"
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] patch_imports.py failed
        goto PatchFailed
    )
) else (
    copy /y "!SRC_VANILLA!" "!BUILD_TMP!\Zuma.exe" >nul
)

REM 2. Automated Mod Binary Synthesis (apply 11-ball colors mod via .zcol and zumaextracolor.dll)
if exist "!BUILD_TMP!\Zuma.exe" (
    if exist "builder\patch_extracolor.py" (
        echo [PATCHING] Applying 11-ball colors mod via .zcol and zumaextracolor.dll...
        "!PYTHON_BIN!" "builder\patch_extracolor.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe"
        if !ERRORLEVEL! equ 0 (
            set "BUILD_SUCCESS=1"
            echo [OK] Extra colors mod applied
        ) else (
            echo [ERROR] patch_extracolor.py failed
            goto PatchFailed
        )
    ) else (
        echo [ERROR] builder\patch_extracolor.py not found
        goto PatchFailed
    )
)

REM 3. Install animated WebP hooks into Zuma.exe
set "PATCH_OK=0"
if "!BUILD_SUCCESS!"=="1" (
    if exist "builder\patch_anim_hook.py" (
        echo [PATCHING] Applying animated WebP hook...
        "!PYTHON_BIN!" "builder\patch_anim_hook.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe"
        if !ERRORLEVEL! equ 0 (
            set "PATCH_OK=1"
            echo [OK] Anim hook applied
        ) else (
            echo [ERROR] patch_anim_hook.py failed
            goto PatchFailed
        )
    ) else (
        echo [ERROR] builder\patch_anim_hook.py not found
        goto PatchFailed
    )
)

REM 4. Extra paths STAGE 1: Board curve heap array expansion
if "!PATCH_OK!"=="1" (
    if exist "builder\patch_extrapath.py" (
        echo [PATCHING] Applying extra-path stage 1 - Board curve arrays...
        "!PYTHON_BIN!" "builder\patch_extrapath.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe" "!SRC_VANILLA!"
        if !ERRORLEVEL! equ 0 (
            echo [OK] Extra-path stage 1 applied
        ) else (
            echo [ERROR] builder\patch_extrapath.py failed
            goto PatchFailed
        )
    ) else (
        echo [ERROR] builder\patch_extrapath.py not found
        goto PatchFailed
    )
)

REM 5. Extra paths STAGE 2: Curve4..10 runtime activation via zumaextrapath.dll
if "!PATCH_OK!"=="1" (
    if exist "builder\patch_extrapath_stage2.py" (
        echo [PATCHING] Applying extra-path stage 2 - Curve4..10 runtime activation...
        "!PYTHON_BIN!" "builder\patch_extrapath_stage2.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe" "!SRC_VANILLA!"
        if !ERRORLEVEL! equ 0 (
            echo [OK] Extra-path stage 2 applied
        ) else (
            echo [ERROR] builder\patch_extrapath_stage2.py failed
            goto PatchFailed
        )
    ) else (
        echo [ERROR] builder\patch_extrapath_stage2.py not found
        goto PatchFailed
    )
)

REM 6. Custom Map Gauntlet support via .zcm and zumacustommap.dll
if "!PATCH_OK!"=="1" (
    if exist "builder\patch_custommap.py" (
        echo [PATCHING] Applying Custom Map Gauntlet support via .zcm and zumacustommap.dll...
        "!PYTHON_BIN!" "builder\patch_custommap.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe"
        if !ERRORLEVEL! equ 0 (
            echo [OK] Custom Map Gauntlet support applied
        ) else (
            echo [ERROR] builder\patch_custommap.py failed
            goto PatchFailed
        )
    ) else (
        echo [ERROR] builder\patch_custommap.py not found
        goto PatchFailed
    )
)

REM 7. Developer Credits via .zcc and zumacplxcredit.dll
if "!PATCH_OK!"=="1" (
    if exist "builder\patch_zccredit.py" (
        echo [PATCHING] Applying Developer Credits support via .zcc and zumacplxcredit.dll...
        "!PYTHON_BIN!" "builder\patch_zccredit.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe"
        if !ERRORLEVEL! equ 0 (
            echo [OK] Developer Credits support applied
        ) else (
            echo [ERROR] builder\patch_zccredit.py failed
            goto PatchFailed
        )
    ) else (
        echo [ERROR] builder\patch_zccredit.py not found
        goto PatchFailed
    )
)

REM 8. Developer Testing Kit via .ztk and zumatestingkit.dll
if "!PATCH_OK!"=="1" (
    if exist "builder\patch_testingkit.py" (
        echo [PATCHING] Applying Developer Testing Kit support via .ztk and zumatestingkit.dll...
        "!PYTHON_BIN!" "builder\patch_testingkit.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe"
        if !ERRORLEVEL! equ 0 (
            echo [OK] Developer Testing Kit support applied
        ) else (
            echo [ERROR] builder\patch_testingkit.py failed
            goto PatchFailed
        )
    ) else (
        echo [ERROR] builder\patch_testingkit.py not found
        goto PatchFailed
    )
)

REM Copy final patched Zuma.exe into target installation directory
copy /y "!BUILD_TMP!\Zuma.exe" "!TARGET_ZUMA_DIR!\Zuma.exe" >nul
if !ERRORLEVEL! equ 0 (
    echo [OK] Patched Zuma.exe successfully deployed to "!TARGET_ZUMA_DIR!\Zuma.exe"
) else (
    echo [ERROR] Failed to deploy Zuma.exe to "!TARGET_ZUMA_DIR!\Zuma.exe"
    goto PatchFailed
)
echo.

goto PatchAssets

:PatchFailed
echo.
echo ===================================================================
echo  [COMPATIBILITY WARNING]
echo  Failed to apply patch to the provided executable.
echo  Please ensure Zuma.exe is compatible and not write-protected.
echo ===================================================================
echo.
set "ALL_OK=0"
goto BuildSummary

:PatchAssets
REM =======================================================================
REM [PRIORITY 6] Patching Assets, Colors & XML Definitions
REM =======================================================================
echo -------------------------------------------------------
echo  Patching Game XMLs and Generating 11-Color Assets ...
echo -------------------------------------------------------
if exist "builder\patch_assets.py" (
    "!PYTHON_BIN!" "builder\patch_assets.py" "!TARGET_ZUMA_DIR!"
    if !ERRORLEVEL! neq 0 (
        echo [WARNING] Some asset patches returned errors.
    )
) else (
    echo [ERROR] builder\patch_assets.py not found!
)
echo.

REM =======================================================================
REM [PRIORITY 7] Verification Summary & Integrity Check
REM =======================================================================
:BuildSummary
echo =======================================================
echo    BUILD VERIFICATION SUMMARY
echo =======================================================
if not defined ALL_OK set "ALL_OK=1"

if exist "!TARGET_ZUMA_DIR!\Zuma.exe" (
    for %%A in ("!TARGET_ZUMA_DIR!\Zuma.exe") do echo  [OK] Zuma.exe                  - %%~zA bytes
) else (
    echo  [MISSING] Zuma.exe
    set "ALL_OK=0"
)

if exist "!TARGET_ZUMA_DIR!\modules\zumadllchecker.dll" (
    for %%A in ("!TARGET_ZUMA_DIR!\modules\zumadllchecker.dll") do echo  [OK] modules\zumadllchecker.dll [Lite] - %%~zA bytes
) else (
    echo  [MISSING] modules\zumadllchecker.dll
    set "ALL_OK=0"
)

echo  [LITE: DISABLED] modules\zumawebp.dll       - (Omitted in Lite Edition)
echo  [LITE: DISABLED] modules\zumaextrapath.dll  - (Omitted in Lite Edition)
echo  [LITE: DISABLED] modules\zumaextracolor.dll - (Omitted in Lite Edition)

if exist "!TARGET_ZUMA_DIR!\modules\zumacustommap.dll" (
    for %%A in ("!TARGET_ZUMA_DIR!\modules\zumacustommap.dll") do echo  [OK] modules\zumacustommap.dll  - %%~zA bytes
) else (
    echo  [MISSING] modules\zumacustommap.dll
    set "ALL_OK=0"
)

if exist "!TARGET_ZUMA_DIR!\modules\zumacplxcredit.dll" (
    for %%A in ("!TARGET_ZUMA_DIR!\modules\zumacplxcredit.dll") do echo  [OK] modules\zumacplxcredit.dll - %%~zA bytes
) else (
    echo  [MISSING] modules\zumacplxcredit.dll
    set "ALL_OK=0"
)

if exist "!TARGET_ZUMA_DIR!\modules\zumatestingkit.dll" (
    for %%A in ("!TARGET_ZUMA_DIR!\modules\zumatestingkit.dll") do echo  [OK] modules\zumatestingkit.dll - %%~zA bytes
) else (
    echo  [MISSING] modules\zumatestingkit.dll
    set "ALL_OK=0"
)

if exist "!TARGET_ZUMA_DIR!\Zuma.exe" if exist "builder\verify_pe.py" (
    echo.
    "!PYTHON_BIN!" "builder\verify_pe.py" "!TARGET_ZUMA_DIR!\Zuma.exe" "!SRC_VANILLA!" "!TARGET_ZUMA_DIR!" --lite
    if !ERRORLEVEL! neq 0 (
        set "ALL_OK=0"
    )
)

if exist "!BUILD_TMP!" rmdir /s /q "!BUILD_TMP!" >nul 2>&1

echo =======================================================

if "!ALL_OK!"=="1" (
    echo.
    echo  [SUCCESS] Zuma Deluxe successfully patched in:
    echo          "!TARGET_ZUMA_DIR!"
    echo.
) else (
    echo.
    echo  [WARNING] Some files or checks failed. Check log above.
    echo.
)

if "!NON_INTERACTIVE!"=="0" pause
exit /b 0
