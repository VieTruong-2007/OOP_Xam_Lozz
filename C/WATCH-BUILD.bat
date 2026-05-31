@echo off
chcp 65001 >nul
title Auto build C.cpp -^> C.exe

cd /d "%~dp0"

echo ============================================================
echo  AUTO BUILD (watch C.cpp)
echo  Luu C.cpp trong Cursor/VS Code -^> tu dong build C.exe
echo  Nhan Ctrl+C de dung
echo ============================================================
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0watch_build.ps1"
pause
