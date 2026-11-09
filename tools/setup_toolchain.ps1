# Installs the Windows toolchain Tera Recomp builds with (docs/building.md, "Toolchain").
# Run once from the repository root in PowerShell:  powershell -ExecutionPolicy Bypass -File tools\setup_toolchain.ps1
# Windows asks for administrator approval for some installers; accept those prompts.
# Safe to run again: every step skips what is already installed.
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Log = Join-Path $Root 'build\setup_toolchain.log'
New-Item -ItemType Directory -Force (Split-Path $Log) | Out-Null
Start-Transcript -Path $Log -Force | Out-Null

function Step($name) { Write-Host "`n=== $name ===" -ForegroundColor Cyan }
function Install-Pkg($id, $extra = @()) {
    $listed = winget.exe list --id $id --exact --accept-source-agreements 2>$null | Out-String
    if ($listed -match [regex]::Escape($id)) { Write-Host "$id already installed"; return }
    winget.exe install --id $id --exact --silent --accept-package-agreements --accept-source-agreements @extra
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne -1978335189) { throw "winget install $id failed ($LASTEXITCODE)" }
}

Step 'winget packages'
Install-Pkg 'MSYS2.MSYS2'
Install-Pkg 'Microsoft.DotNet.SDK.10'
Install-Pkg 'KhronosGroup.VulkanSDK'
Install-Pkg 'astral-sh.uv'
Install-Pkg 'Git.Git'
Install-Pkg 'Microsoft.VisualStudio.BuildTools' @('--override', '--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended')

Step 'PATH'
$vsInstaller = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer'
$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
if ($userPath -notlike "*$vsInstaller*") {
    [Environment]::SetEnvironmentVariable('Path', "$userPath;$vsInstaller", 'User')
    Write-Host "added $vsInstaller to the user PATH"
}
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')

Step 'MSYS2 packages'
$bash = 'C:\msys64\usr\bin\bash.exe'
if (-not (Test-Path $bash)) { throw "MSYS2 not found at C:\msys64" }
# The first -Syuu updates the core and may exit; the second updates the rest.
& $bash -lc 'pacman -Syuu --noconfirm' ; & $bash -lc 'pacman -Syuu --noconfirm'
$pkgs = 'mingw-w64-ucrt-x86_64-{clang,lld,ninja,gcc,make,pkgconf,sdl3,opus,libvpx,zstd,vulkan-devel,vulkan-memory-allocator,vulkan-validation-layers,shaderc,spirv-tools} base-devel git unzip'
& $bash -lc "pacman -S --needed --noconfirm $pkgs"
if ($LASTEXITCODE -ne 0) { throw 'pacman failed' }

Step 'Python environments'
Set-Location $Root
$uv = (Get-Command uv -ErrorAction SilentlyContinue).Source
if (-not $uv) { $uv = Join-Path $env:USERPROFILE '.local\bin\uv.exe' }
if (-not (Test-Path '.venv\Scripts\python.exe')) { & $uv venv --python 3.12 .venv }
& $uv pip install --python .venv\Scripts\python.exe -r tools\requirements.txt
if (-not (Test-Path '.venv-py314\Scripts\python.exe')) { & $uv venv --python 3.14 .venv-py314 }
& $uv pip install --python .venv-py314\Scripts\python.exe -r tools\requirements-py314.txt

Step 'Check'
$clang = 'C:\msys64\ucrt64\bin\clang.exe'
& $clang --version | Select-Object -First 1
dotnet --version
& .venv\Scripts\python.exe --version
& .venv-py314\Scripts\python.exe --version
$vk = Get-Command vulkaninfo -ErrorAction SilentlyContinue
# vulkaninfo writes loader warnings (an OBS capture layer, for example) to stderr; Windows PowerShell would stop on them.
if ($vk) {
    $ErrorActionPreference = 'Continue'
    cmd /c 'vulkaninfo --summary 2>nul' | Select-String -Pattern 'apiVersion|deviceName|driverName|driverInfo' | Select-Object -First 8
    $ErrorActionPreference = 'Stop'
}
Write-Host "`nTOOLCHAIN OK" -ForegroundColor Green
Stop-Transcript | Out-Null
