@echo off
chcp 65001 >nul
title Auto build + run game

cd /d "%~dp0"

echo ============================================================
echo  AUTO BUILD + RUN
echo  Luu C.cpp -^> build C.exe -^> mo lai game
echo  Nhan Ctrl+C de dung
echo ============================================================
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0watch_build.ps1" run
pause
