@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM  build.bat  -  Auto build C.cpp -> C.exe
REM
REM  Usage:
REM    build.bat              Build C.cpp -> C.exe
REM    build.bat watch        Auto-rebuild when C.cpp is saved (~0.4s)
REM    build.bat run          Build then launch C.exe
REM    build.bat watch run    Watch + rebuild + restart game
REM    build.bat install      Install free C++ compiler (LLVM MinGW via winget)
REM ============================================================================

cd /d "%~dp0"

if /i "%~1"=="install" goto :do_install

set "DO_WATCH=0"
set "DO_RUN=0"
if /i "%~1"=="watch" set "DO_WATCH=1"
if /i "%~2"=="watch" set "DO_WATCH=1"
if /i "%~1"=="run" set "DO_RUN=1"
if /i "%~2"=="run" set "DO_RUN=1"

if not exist "C.cpp" (
    echo ERROR: C.cpp not found in %CD%
    exit /b 1
)

call :find_compiler
if errorlevel 1 (
    call :no_compiler_help
    exit /b 1
)

if "%DO_WATCH%"=="1" goto :watch_mode

call :compile_cpp
set "RC=!errorlevel!"
if "%DO_RUN%"=="1" if !RC! equ 0 call :launch_exe
exit /b !RC!

:watch_mode
if "%DO_RUN%"=="1" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0watch_build.ps1" run
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0watch_build.ps1"
)
exit /b !errorlevel!

REM ---------------------------------------------------------------------------
:do_install
echo Installing LLVM MinGW (g++) - free C++ compiler, ~180 MB...
echo.
where winget >nul 2>&1
if errorlevel 1 (
    echo ERROR: winget not found. Install "App Installer" from Microsoft Store.
    exit /b 1
)
winget install -e --id MartinStorsjo.LLVM-MinGW.UCRT --accept-package-agreements --accept-source-agreements
if errorlevel 1 (
    echo.
    echo Install failed. Try running this terminal as Administrator, then:
    echo   build.bat install
    exit /b 1
)
echo.
echo Install finished. Close and reopen the terminal, then run:
echo   build.bat
exit /b 0

REM ---------------------------------------------------------------------------
:find_compiler
set "COMPILER_CMD="
set "COMPILER_NAME="
set "USE_MSVC=0"

REM --- Try MSVC (cl.exe) ---
call :init_msvc_quiet
if not errorlevel 1 (
    where cl >nul 2>&1
    if !errorlevel! equ 0 (
        set "COMPILER_CMD=cl"
        set "COMPILER_NAME=MSVC"
        set "USE_MSVC=1"
        exit /b 0
    )
)

REM --- Try g++ (MinGW / LLVM-MinGW) ---
call :find_gpp
if defined GPP (
    set "COMPILER_CMD=!GPP!"
    set "COMPILER_NAME=G++ (MinGW)"
    set "USE_MSVC=0"
    exit /b 0
)

exit /b 1

:find_gpp
set "GPP="
where g++ >nul 2>&1
if !errorlevel! equ 0 (
    for /f "delims=" %%G in ('where g++ 2^>nul') do (
        set "GPP=%%G"
        goto :find_gpp_done
    )
)

REM WinGet package folder (LLVM MinGW)
if exist "%LOCALAPPDATA%\Microsoft\WinGet\Packages" (
    for /d %%D in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW*") do (
        for /f "delims=" %%G in ('dir /s /b "%%D\g++.exe" 2^>nul') do (
            set "GPP=%%G"
            goto :find_gpp_done
        )
    )
)

REM Common manual install paths
for %%P in (
    "%ProgramFiles%\llvm-mingw\bin\g++.exe"
    "%ProgramFiles(x86)%\llvm-mingw\bin\g++.exe"
    "C:\llvm-mingw\bin\g++.exe"
    "%LOCALAPPDATA%\Programs\LLVM-MinGW\bin\g++.exe"
) do (
    if exist %%P (
        set "GPP=%%~fP"
        goto :find_gpp_done
    )
)

:find_gpp_done
exit /b 0

REM ---------------------------------------------------------------------------
:compile_cpp
echo [%date% %time%] C.cpp -^> C.exe  ^(!COMPILER_NAME!^)

if "!USE_MSVC!"=="1" (
    cl.exe /EHsc /nologo /utf-8 /DUNICODE /D_UNICODE "C.cpp" /Fe"C.exe" /Fo"C.obj" /link /subsystem:windows user32.lib gdi32.lib kernel32.lib gdiplus.lib comdlg32.lib
    set "RC=!errorlevel!"
    goto :compile_done
)

"!COMPILER_CMD!" -std=c++17 -O2 -DUNICODE -D_UNICODE -mwindows -municode "C.cpp" -o "C.exe" -lgdi32 -luser32 -lkernel32 -lgdiplus -lcomdlg32
set "RC=!errorlevel!"

:compile_done
if !RC! equ 0 (
    echo Build successful: %CD%\C.exe
) else (
    echo Build FAILED.
)
exit /b !RC!

:get_cpp_stamp
for %%F in ("C.cpp") do set "%~1=%%~tF"
exit /b 0

:launch_exe
if not exist "C.exe" (
    echo Cannot run: C.exe does not exist.
    exit /b 1
)
taskkill /IM C.exe /F >nul 2>&1
start "" "%~dp0C.exe"
exit /b 0

REM ---------------------------------------------------------------------------
:init_msvc_quiet
where cl >nul 2>&1
if !errorlevel! equ 0 exit /b 0

set "VCVARS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
        if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" (
            set "VCVARS=%%I\VC\Auxiliary\Build\vcvars64.bat"
            goto :apply_vcvars
        )
    )
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -property installationPath 2^>nul`) do (
        if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" (
            set "VCVARS=%%I\VC\Auxiliary\Build\vcvars64.bat"
            goto :apply_vcvars
        )
    )
)

for %%R in ("%ProgramFiles%" "%ProgramFiles(x86)%") do (
    if exist %%~R (
        for /d %%Y in ("%%~R\Microsoft Visual Studio\20*") do (
            for /d %%E in ("%%Y\*") do (
                if exist "%%E\VC\Auxiliary\Build\vcvars64.bat" (
                    set "VCVARS=%%E\VC\Auxiliary\Build\vcvars64.bat"
                    goto :apply_vcvars
                )
            )
        )
        for /d %%V in ("%%~R\Microsoft Visual Studio\1*") do (
            for /d %%E in ("%%V\*") do (
                if exist "%%E\VC\Auxiliary\Build\vcvars64.bat" (
                    set "VCVARS=%%E\VC\Auxiliary\Build\vcvars64.bat"
                    goto :apply_vcvars
                )
            )
        )
    )
)
exit /b 1

:apply_vcvars
call "!VCVARS!" >nul 2>&1
exit /b 0

:no_compiler_help
echo.
echo ============================================================================
echo  No C++ compiler found (no cl.exe, no g++.exe)
echo ============================================================================
echo.
echo OPTION A - Quick install (recommended, ~180 MB, no Visual Studio needed):
echo   build.bat install
echo   (Then close terminal, open again, run: build.bat)
echo.
echo OPTION B - Full Microsoft compiler (larger download):
echo   1. Open "Visual Studio Installer"
echo   2. Modify your VS install
echo   3. Check: "Desktop development with C++"
echo   4. Install, then run: build.bat
echo.
echo OPTION C - winget Build Tools (run terminal as Administrator):
echo   winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools"
echo ============================================================================
exit /b 0
