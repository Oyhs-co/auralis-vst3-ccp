# Installs the built Auralis VST3 into the system folder that FL Studio scans.
# Usage: powershell -File tools\install.ps1 [-Vst3Path <path-to-Auralis.vst3>]
param(
    [string]$Vst3Path = "build\Auralis_artefacts\Release\VST3\Auralis.vst3"
)

if (-not (Test-Path $Vst3Path)) {
    Write-Error "VST3 not found at '$Vst3Path'. Build first: cmake --preset msvc; cmake --build --preset release"
    exit 1
}

$dest = Join-Path $env:COMMONPROGRAMFILES "VST3"
Copy-Item -Recurse -Force $Vst3Path $dest
Write-Host "Installed to $dest\Auralis.vst3 - rescan plugins in FL Studio."