@echo off
chcp 65001 >nul
title Cai dat compiler C++ cho game

REM Luon chay tu dung thu muc chua file nay (double-click OK)
cd /d "%~dp0"

echo ============================================================
echo  CAI DAT COMPILER C++ (g++)
echo  Thu muc: %CD%
echo ============================================================
echo.

where winget >nul 2>&1
if errorlevel 1 (
    echo Loi: Khong co winget. Hay cai "App Installer" tu Microsoft Store.
    pause
    exit /b 1
)

echo Dang cai LLVM MinGW (~180 MB). Cho 5-15 phut...
echo Neu bi treo lau, dong het CMD/PowerShell roi chay lai file nay.
echo.

winget install -e --id MartinStorsjo.LLVM-MinGW.UCRT --accept-package-agreements --accept-source-agreements

if errorlevel 1 (
    echo.
    echo ============================================================
    echo  CAI DAT THAT BAI
    echo  Thu: Click phai file nay -^> Run as administrator
    echo  Hoac mo Visual Studio Installer -^> tick "Desktop development with C++"
    echo ============================================================
    pause
    exit /b 1
)

echo.
echo ============================================================
echo  CAI DAT XONG!
echo  1. Dong cua so nay
echo  2. Double-click: BUILD-GAME.bat
echo ============================================================
pause
