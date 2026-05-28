@echo off
setlocal

for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS_ROOT=%%I

if not defined VS_ROOT (
  echo Visual Studio VC toolchain not found.
  exit /b 1
)

call "%VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat"
cl.exe /EHsc /nologo /utf-8 /DUNICODE /D_UNICODE C.cpp /FeC.exe /link /subsystem:windows user32.lib gdi32.lib kernel32.lib gdiplus.lib comdlg32.lib
exit /b %errorlevel%
