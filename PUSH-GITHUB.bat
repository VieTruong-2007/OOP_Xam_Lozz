@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

REM Push project len GitHub - double-click hoac chay trong CMD
cd /d "%~dp0"

set "GIT="
if exist "C:\Program Files\Git\bin\git.exe" set "GIT=C:\Program Files\Git\bin\git.exe"
if exist "C:\Program Files\Git\cmd\git.exe" set "GIT=C:\Program Files\Git\cmd\git.exe"
if not defined GIT where git >nul 2>&1 && for /f "delims=" %%G in ('where git 2^>nul') do set "GIT=%%G"

if not defined GIT (
    echo ============================================================
    echo  CHUA CO GIT - Dang cai Git...
    echo ============================================================
    winget install -e --id Git.Git --accept-package-agreements --accept-source-agreements
    if exist "C:\Program Files\Git\bin\git.exe" set "GIT=C:\Program Files\Git\bin\git.exe"
)
if not defined GIT (
    echo.
    echo LOI: Chua cai duoc Git. Hay cai Git for Windows roi chay lai file nay:
    echo   https://git-scm.com/download/win
    pause
    exit /b 1
)

set "REMOTE=git@github.com:VieTruong-2007/OOP_Xam_Lozz.git"

echo Using: !GIT!
echo Remote: !REMOTE!
echo.

if not exist ".git" (
    echo [1/5] git init...
    "!GIT!" init
    "!GIT!" branch -M main
)

echo [2/5] git remote...
"!GIT!" remote remove origin 2>nul
"!GIT!" remote add origin "!REMOTE!"

echo [3/5] git add...
"!GIT!" add -A

echo [4/5] git commit...
"!GIT!" diff --cached --quiet
if !errorlevel! equ 0 (
    echo Khong co thay doi moi de commit.
) else (
    "!GIT!" commit -m "Update: tro choi pha gach C++ (C.cpp, build scripts, theme)"
)

set "GIT_SSH_COMMAND=C:\Windows\System32\OpenSSH\ssh.exe -o StrictHostKeyChecking=accept-new"
echo [5/5] git push -u origin main...
"!GIT!" -c core.sshCommand="C:/Windows/System32/OpenSSH/ssh.exe -o StrictHostKeyChecking=accept-new" push -u origin main
set "RC=!errorlevel!"

if !RC! neq 0 (
    echo.
    echo ============================================================
    echo  PUSH THAT BAI
    echo  - Them SSH key vao GitHub (xem file SSH-PUBLIC-KEY.txt)
    echo  - https://github.com/settings/ssh/new
    echo  - Tao repo (neu chua co): https://github.com/new  ten: OOP_Xam_Lozz
    echo  - Test: ssh -T git@github.com
    echo ============================================================
) else (
    echo.
    echo ============================================================
    echo  DA PUSH LEN GITHUB THANH CONG!
    echo  https://github.com/VieTruong-2007/OOP_Xam_Lozz
    echo ============================================================
)

pause
exit /b !RC!
