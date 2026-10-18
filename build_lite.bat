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
echo    ZuCompleluxe v0.4.1 Lite - 1-Click Game Patcher ^& Builder
echo    Direct Installation Folder Patcher - Lite Edition
echo =======================================================
echo.

REM =======================================================================
REM [PRIORITY 0] Target Zuma Deluxe Folder Selection
REM =======================================================================
set "TARGET_ZUMA_DIR="

REM Check CLI argument first
if not "!CLI_ZUMA_DIR!"=="" (
    if exist "!CLI_ZUMA_DIR!\Zuma.exe" (
        set "TARGET_ZUMA_DIR=!CLI_ZUMA_DIR!"
    ) else if exist "!CLI_ZUMA_DIR!\*" (
        set "TARGET_ZUMA_DIR=!CLI_ZUMA_DIR!"
    ) else if exist "!CLI_ZUMA_DIR!" (
        for %%F in ("!CLI_ZUMA_DIR!") do set "TARGET_ZUMA_DIR=%%~dpF"
    )
)

REM If not provided via CLI, launch modern File Explorer dialog (interactive mode only)
if "!TARGET_ZUMA_DIR!"=="" if "!NON_INTERACTIVE!"=="0" (
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
echo [PROMPT] Enter Zuma Deluxe installation folder path or press Enter to cancel:
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
    if "!NON_INTERACTIVE!"=="0" pause
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
    if "!NON_INTERACTIVE!"=="0" pause
    exit /b 1
)

for %%A in ("!SRC_VANILLA!") do set "ZUMA_SIZE=%%~zA"
echo [INFO] Original executable confirmed: !ZUMA_SIZE! bytes.
echo.

REM =======================================================================
REM [PRIORITY 1] Adaptive Python Environment & Dependencies Setup
REM Reordered: Python & wheels checked FIRST before compiler detection
REM =======================================================================
set "BUILD_TMP=.build"
if not exist "!BUILD_TMP!" mkdir "!BUILD_TMP!" >nul 2>&1

call builder\bootstrap.bat ensure_python
if errorlevel 1 (
    echo [ERROR] Failed to prepare Python environment.
    if "!NON_INTERACTIVE!"=="0" pause
    exit /b 1
)

call builder\bootstrap.bat ensure_pydeps
if errorlevel 1 (
    echo [ERROR] Failed to verify Python build dependencies.
    if "!NON_INTERACTIVE!"=="0" pause
    exit /b 1
)
echo.

REM =======================================================================
REM [PRIORITY 2] Adaptive 32-bit Compiler Detection
REM Probes GCC 32-bit -> Zig CC fallback -> w64devkit i686 portable
REM =======================================================================
call builder\bootstrap.bat detect_compiler
if errorlevel 1 (
    echo [ERROR] Failed to locate or prepare a 32-bit compiler.
    if "!NON_INTERACTIVE!"=="0" pause
    exit /b 1
)
echo.

REM =======================================================================
REM [PRIORITY 3] Initialize Target Modules Directory
REM =======================================================================
if not exist "!TARGET_ZUMA_DIR!\modules" (
    echo [INFO] Creating directory: "!TARGET_ZUMA_DIR!\modules"
    mkdir "!TARGET_ZUMA_DIR!\modules"
)

REM =======================================================================
REM [PRIORITY 4] Building Sidecar DLLs (Lite: 4 Sidecars)
REM =======================================================================
REM Purge excluded sidecars from target modules directory so installation is strictly Lite
for %%D in (zumawebp.dll zumaextrapath.dll zumaextracolor.dll) do (
    if exist "!TARGET_ZUMA_DIR!\modules\%%D" del /f /q "!TARGET_ZUMA_DIR!\modules\%%D" >nul 2>&1
    if exist "!TARGET_ZUMA_DIR!\%%D" del /f /q "!TARGET_ZUMA_DIR!\%%D" >nul 2>&1
)
for %%D in (zumadllchecker.dll zumacustommap.dll zumacplxcredit.dll zumatestingkit.dll) do (
    if exist "!TARGET_ZUMA_DIR!\%%D" del /f /q "!TARGET_ZUMA_DIR!\%%D" >nul 2>&1
)

set "DLL_STATUS_zumawebp.dll=skipped (omitted in lite)"
set "DLL_STATUS_zumaextrapath.dll=skipped (omitted in lite)"
set "DLL_STATUS_zumaextracolor.dll=skipped (omitted in lite)"

echo -------------------------------------------------------
echo  [FILE 1/4] Building zumadllchecker.dll - Lite Gatekeeper ...
echo -------------------------------------------------------
set "OUT=zumadllchecker.dll"
set "SRC=src\zumadllchecker.dll.c"
set "EXPORT=ZDC_RunChecker"
set "XFLAGS=-DZUCOMPLELUXE_LITE=1"
set "LIBS=-lversion -luxtheme -ldwmapi -lgdi32 -luser32 -ladvapi32 -lshell32 -lkernel32"
call builder\bootstrap.bat compile_dll
if errorlevel 1 (
    echo [ERROR] zumadllchecker.dll build failed.
    if "!NON_INTERACTIVE!"=="0" pause
    exit /b 1
)
echo.

echo -------------------------------------------------------
echo  [FILE 2/4] Building zumacustommap.dll - Gauntlet custom map sidecar ...
echo -------------------------------------------------------
set "OUT=zumacustommap.dll"
set "SRC=src\zumacustommap.dll.c"
set "EXPORT=zcm_AdjustCount"
set "XFLAGS="
set "LIBS="
call builder\bootstrap.bat compile_dll
if errorlevel 1 (
    echo [ERROR] zumacustommap.dll build failed.
    if "!NON_INTERACTIVE!"=="0" pause
    exit /b 1
)
echo.

echo -------------------------------------------------------
echo  [FILE 3/4] Building zumacplxcredit.dll - Developer Credits sidecar ...
echo -------------------------------------------------------
set "OUT=zumacplxcredit.dll"
set "SRC=src\zumacplxcredit.dll.c"
set "EXPORT=ZCC_GetCreditsTable"
set "XFLAGS="
set "LIBS="
call builder\bootstrap.bat compile_dll
if errorlevel 1 (
    echo [ERROR] zumacplxcredit.dll build failed.
    if "!NON_INTERACTIVE!"=="0" pause
    exit /b 1
)
echo.

echo -------------------------------------------------------
echo  [FILE 4/4] Building zumatestingkit.dll - Developer Testing Kit sidecar ...
echo -------------------------------------------------------
set "OUT=zumatestingkit.dll"
set "SRC=src\zumatestingkit.dll.c"
set "EXPORT=ZTK_IsActive"
set "XFLAGS="
set "LIBS="
call builder\bootstrap.bat compile_dll
if errorlevel 1 (
    echo [ERROR] zumatestingkit.dll build failed.
    if "!NON_INTERACTIVE!"=="0" pause
    exit /b 1
)
echo.

echo [LITE] Skipping zumawebp.dll - Omitted in Lite Edition
echo [LITE] Skipping zumaextrapath.dll - Omitted in Lite Edition
echo [LITE] Skipping zumaextracolor.dll - Omitted in Lite Edition
echo.

REM =======================================================================
REM [PRIORITY 5] Synthesizing and Patching Zuma.exe
REM =======================================================================
echo -------------------------------------------------------
echo  Patching Zuma.exe into "!TARGET_ZUMA_DIR!\Zuma.exe" ...
echo -------------------------------------------------------
set "PATCH_LOG=!BUILD_TMP!\patch.log"
del /f /q "!PATCH_LOG!" >nul 2>&1
set "ALL_OK=1"

REM 1. Inject static sidecar imports into .zdata
echo [PATCHING] Injecting static sidecar imports into .zdata...
"!PYTHON_BIN!" "builder\patch_imports.py" "!SRC_VANILLA!" "!BUILD_TMP!\Zuma.exe" >"!PATCH_LOG!" 2>&1
if errorlevel 1 (
    set "FAILED_PATCH_STEP=builder\patch_imports.py"
    goto PatchFailed
)
echo [OK] Static sidecar imports injected.

REM 2. Apply 11-ball colors mod via .zcol and zumaextracolor.dll
echo [PATCHING] Applying 11-ball colors mod via .zcol and zumaextracolor.dll...
"!PYTHON_BIN!" "builder\patch_extracolor.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe" >"!PATCH_LOG!" 2>&1
if errorlevel 1 (
    set "FAILED_PATCH_STEP=builder\patch_extracolor.py"
    goto PatchFailed
)
echo [OK] Extra colors mod applied.

REM 3. Install animated WebP hooks into Zuma.exe
echo [PATCHING] Applying animated WebP hook...
"!PYTHON_BIN!" "builder\patch_anim_hook.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe" >"!PATCH_LOG!" 2>&1
if errorlevel 1 (
    set "FAILED_PATCH_STEP=builder\patch_anim_hook.py"
    goto PatchFailed
)
echo [OK] Animated WebP hook applied.

REM 4. Extra paths STAGE 1: Board curve heap array expansion
echo [PATCHING] Applying extra-path stage 1 - Board curve arrays...
"!PYTHON_BIN!" "builder\patch_extrapath.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe" "!SRC_VANILLA!" >"!PATCH_LOG!" 2>&1
if errorlevel 1 (
    set "FAILED_PATCH_STEP=builder\patch_extrapath.py"
    goto PatchFailed
)
echo [OK] Extra-path stage 1 applied.

REM 5. Extra paths STAGE 2: Curve4..10 runtime activation
echo [PATCHING] Applying extra-path stage 2 - Curve4..10 runtime activation...
"!PYTHON_BIN!" "builder\patch_extrapath_stage2.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe" "!SRC_VANILLA!" >"!PATCH_LOG!" 2>&1
if errorlevel 1 (
    set "FAILED_PATCH_STEP=builder\patch_extrapath_stage2.py"
    goto PatchFailed
)
echo [OK] Extra-path stage 2 applied.

REM 6. Custom Map Gauntlet support via .zcm and zumacustommap.dll
echo [PATCHING] Applying Custom Map Gauntlet support via .zcm and zumacustommap.dll...
"!PYTHON_BIN!" "builder\patch_custommap.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe" >"!PATCH_LOG!" 2>&1
if errorlevel 1 (
    set "FAILED_PATCH_STEP=builder\patch_custommap.py"
    goto PatchFailed
)
echo [OK] Custom Map Gauntlet support applied.

REM 7. Developer Credits via .zcc and zumacplxcredit.dll
echo [PATCHING] Applying Developer Credits support via .zcc and zumacplxcredit.dll...
"!PYTHON_BIN!" "builder\patch_zccredit.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe" >"!PATCH_LOG!" 2>&1
if errorlevel 1 (
    set "FAILED_PATCH_STEP=builder\patch_zccredit.py"
    goto PatchFailed
)
echo [OK] Developer Credits support applied.

REM 8. Developer Testing Kit via .ztk and zumatestingkit.dll
echo [PATCHING] Applying Developer Testing Kit support via .ztk and zumatestingkit.dll...
"!PYTHON_BIN!" "builder\patch_testingkit.py" "!BUILD_TMP!\Zuma.exe" "!BUILD_TMP!\Zuma.exe" >"!PATCH_LOG!" 2>&1
if errorlevel 1 (
    set "FAILED_PATCH_STEP=builder\patch_testingkit.py"
    goto PatchFailed
)
echo [OK] Developer Testing Kit support applied.

REM Deploy patched executable to target folder
copy /y "!BUILD_TMP!\Zuma.exe" "!TARGET_ZUMA_DIR!\Zuma.exe" >nul
if errorlevel 1 (
    set "FAILED_PATCH_STEP=copy Zuma.exe to !TARGET_ZUMA_DIR!\Zuma.exe"
    goto PatchFailed
)
echo [OK] Patched Zuma.exe successfully deployed to "!TARGET_ZUMA_DIR!\Zuma.exe"
echo.
goto PatchAssets

:PatchFailed
echo.
echo ===================================================================
echo  [ERROR] Binary patching failed at step: !FAILED_PATCH_STEP!
if exist "!PATCH_LOG!" (
    echo  Detailed log follows:
    echo ---------------------------------------------------------------
    type "!PATCH_LOG!"
    echo ---------------------------------------------------------------
)
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
    "!PYTHON_BIN!" "builder\patch_assets.py" "!TARGET_ZUMA_DIR!" >"!BUILD_TMP!\patch_assets.log" 2>&1
    if errorlevel 1 (
        echo [ERROR] Asset patch failed! Detailed log from !BUILD_TMP!\patch_assets.log:
        echo -------------------------------------------------------
        type "!BUILD_TMP!\patch_assets.log"
        echo -------------------------------------------------------
        set "ALL_OK=0"
    ) else (
        echo [OK] Game XMLs and color assets updated successfully.
    )
) else (
    echo [ERROR] builder\patch_assets.py not found!
    set "ALL_OK=0"
)
echo.

REM =======================================================================
REM [PRIORITY 7] Verification Summary & Integrity Check
REM =======================================================================
:BuildSummary
call builder\bootstrap.bat print_summary

if exist "!TARGET_ZUMA_DIR!\Zuma.exe" if exist "builder\verify_pe.py" (
    echo.
    echo [INFO] Running PE structural verification...
    "!PYTHON_BIN!" "builder\verify_pe.py" "!TARGET_ZUMA_DIR!\Zuma.exe" "!SRC_VANILLA!" "!TARGET_ZUMA_DIR!" --lite
    if errorlevel 1 set "ALL_OK=0"
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
