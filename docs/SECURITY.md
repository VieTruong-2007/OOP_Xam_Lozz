# Security review — OOP_Xam_Lozz / D:\C

Last reviewed: 2026-05-30

## Virus / malware scan (static + Defender)

| Check | Result |
|-------|--------|
| Suspicious network/download code | **None** in source |
| `eval`, base64 payloads, remote shells | **None** |
| Scripts only run local `build.bat` / `C.exe` | **Yes** |
| `winget` package | `MartinStorsjo.LLVM-MinGW.UCRT` (official ID) |
| `C.exe` SHA256 (local build) | `F5C6BBABF05C41FB5CE3E1E3E9D9945BBB11CE2E6734EEDEB3FED3E36BAD4A58` |

Re-run antivirus on your PC:

```powershell
Start-MpScan -ScanType CustomScan -ScanPath "D:\C"
```

Or: **Windows Security → Virus & threat protection → Quick scan** (full scan recommended after downloading from GitHub).

## File inventory

| File | Role | Risk |
|------|------|------|
| `src/C.cpp` | Game source | Low — local GUI only |
| `dist/C.exe` | Compiled game | Rebuild from `src/C.cpp` to trust |
| `scripts/build.bat` | Compile / optional winget install | Low — fixed commands |
| `scripts/watch_build.ps1` | Auto-build watcher | Low — only calls `build.bat` |
| `*.bat` | Wrappers | Low |
| `assets/theme_setup.txt` | Colors / background filename | Low — validated in code |
| `assets/background_cache.jpg` | User image cache | Only image files in game folder |

## Hardening applied in code

- Theme config: only `theme_setup.txt` / `theme.txt`
- `BACKGROUND_IMAGE`: filename only (no `..`, no paths, no `C:\`)
- RGB values clamped to 0–255
- PowerShell watcher uses `-LiteralPath` for `build.bat` and `C.exe`

## Residual notes

- `ExecutionPolicy Bypass` in watch scripts is for local dev only; do not run unknown `.ps1` from the internet.
- `C.exe` is a generic name — Windows may flag unknown unsigned EXEs; building locally from source is safest.
- Do not commit `C.exe` to git; use `build.bat` on each machine.

## If you cloned from GitHub

1. Compare `src/C.cpp` with the repo you trust.
2. Delete `dist/C.exe`, run `scripts\build.bat` to rebuild.
3. Run Defender scan on `D:\C`.
