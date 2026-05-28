@echo off
REM Chay build tu thu muc goc project -> tu dong vao folder C
cd /d "%~dp0C"
if not exist "build.bat" (
    echo ERROR: Khong tim thay C\build.bat
    echo Thu muc hien tai: %CD%
    pause
    exit /b 1
)
call "%~dp0C\build.bat" %*
