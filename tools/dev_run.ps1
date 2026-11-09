# Rebuild build\v400\totk.exe (only what changed) and run it with boot tracing. Development loop for Tera Recomp.
#   powershell -ExecutionPolicy Bypass -File tools\dev_run.ps1            # rebuild, then run
#   powershell -ExecutionPolicy Bypass -File tools\dev_run.ps1 -NoBuild   # run only
# The build log goes to build\dev-build.log, the game log to build\v400\logs\totk.log.
param([switch]$NoBuild, [string]$Trace = 'host,ipc,fs', [int]$Jobs = 6)
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
if (-not $NoBuild) {
    Write-Host 'building...' -ForegroundColor Cyan
    cmd /c ".venv\Scripts\python tools\build_game.py --version v400 --romfs build\extracted\v400\romfs-image --jobs $Jobs > build\dev-build.log 2>&1"
    if ($LASTEXITCODE -ne 0) {
        Get-Content build\dev-build.log -Tail 30
        Write-Host 'BUILD FAILED (build\dev-build.log)' -ForegroundColor Red
        exit 1
    }
}
$env:TOTK_TRACE = $Trace
Write-Host 'running...' -ForegroundColor Cyan
& .\build\v400\totk.exe
Write-Host "exit code $LASTEXITCODE; log: build\v400\logs\totk.log" -ForegroundColor Cyan
