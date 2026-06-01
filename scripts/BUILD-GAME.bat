@echo off
chcp 65001 >nul
title Build game C.cpp -^> C.exe

REM Double-click file nay - khong can cd thu muc
cd /d "%~dp0"

echo ============================================================
echo  BUILD GAME
echo  Thu muc: %CD%
echo ============================================================
echo.

call "%~dp0build.bat" %*

echo.
pause
