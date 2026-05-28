@echo off
REM Shortcut build - tu thu muc OOP_Xam_Lozz-main
cd /d "%~dp0OOP_Xam_Lozz-main\C"
if not exist "build.bat" (
    echo ERROR: Khong tim thay folder C\build.bat
    echo Hay chay file trong: d:\vs\OOP_Xam_Lozz-main\OOP_Xam_Lozz-main\C
    pause
    exit /b 1
)
call "build.bat" %*
