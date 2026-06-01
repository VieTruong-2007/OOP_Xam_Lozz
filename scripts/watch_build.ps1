# Auto-rebuild C.exe when C.cpp is saved. Started by build.bat watch / WATCH-BUILD.bat
$ErrorActionPreference = "Continue"
$root = $PSScriptRoot
Set-Location $root
$cpp = Join-Path $root "src\C.cpp"

if (-not (Test-Path $cpp)) {
    Write-Host "ERROR: src\C.cpp not found in $root"
    exit 1
}

$buildBat = Join-Path $root "build.bat"
$gameExe = Join-Path $root "dist\C.exe"

function Invoke-GameBuild {
    if (-not (Test-Path -LiteralPath $buildBat)) {
        Write-Host "ERROR: build.bat not found."
        return 1
    }
    $p = Start-Process -FilePath "cmd.exe" -ArgumentList @("/c", "`"$buildBat`"") -WorkingDirectory $root -Wait -NoNewWindow -PassThru
    return $p.ExitCode
}

function Invoke-GameRun {
    if (-not (Test-Path -LiteralPath $gameExe)) { return }
    Get-Process -Name "C" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Process -LiteralPath $gameExe -WorkingDirectory $root
}

$doRun = ($args -contains "run")

Write-Host ""
Write-Host "============================================================================"
Write-Host "  Auto-build ON: save C.cpp -> C.exe  (Ctrl+C to stop)"
if ($doRun) { Write-Host "  Mode: rebuild + restart game after each successful build" }
Write-Host "============================================================================"
Write-Host ""

$code = Invoke-GameBuild
if ($code -eq 0) {
    Write-Host "[OK] C.exe ready."
    if ($doRun) { Invoke-GameRun }
} else {
    Write-Host "[FAIL] Fix C.cpp and save again."
}
Write-Host ""

$lastStamp = (Get-Item $cpp).LastWriteTimeUtc.Ticks
$debounceMs = 400
$quietUntil = [datetime]::MinValue

while ($true) {
    Start-Sleep -Milliseconds 250
    $stamp = (Get-Item $cpp).LastWriteTimeUtc.Ticks
    if ($stamp -eq $lastStamp) { continue }
    if ([datetime]::UtcNow -lt $quietUntil) { continue }

    $lastStamp = $stamp
    $quietUntil = [datetime]::UtcNow.AddMilliseconds($debounceMs)

    Start-Sleep -Milliseconds $debounceMs
    $stamp = (Get-Item $cpp).LastWriteTimeUtc.Ticks
    if ($stamp -ne $lastStamp) {
        $lastStamp = $stamp
        continue
    }

    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] C.cpp changed -> building..."
    $code = Invoke-GameBuild
    if ($code -eq 0) {
        Write-Host "[OK] C.exe updated."
        if ($doRun) { Invoke-GameRun }
    } else {
        Write-Host "[FAIL] Fix C.cpp and save again."
    }
    Write-Host ""
}
