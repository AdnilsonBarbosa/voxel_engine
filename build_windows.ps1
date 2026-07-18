# Voxel Engine — Windows Build Script (PowerShell + MSVC)
#
# Usage:
#   .\build_windows.ps1          — builds voxel_engine.exe
#   .\build_windows.ps1 clean    — removes build artifacts

param([string]$Action = "build")

$ErrorActionPreference = "Stop"

# ── Find VS 2022 ───────────────────────────────────────────────────────────
$vsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$vcvars = "$vsPath\Common7\Tools\VsDevCmd.bat"

if (-not (Test-Path $vcvars)) {
    Write-Host "ERROR: Visual Studio 2022 not found at $vsPath" -ForegroundColor Red
    exit 1
}

# ── SDL2 paths ──────────────────────────────────────────────────────────────
$SDL2_DIR = "C:\Users\barbo\SDL2\SDL2-2.30.12"
$SDL2_INC = "$SDL2_DIR\include"
$SDL2_LIB = "$SDL2_DIR\lib\x64"

if (-not (Test-Path "$SDL2_INC\SDL.h")) {
    Write-Host "ERROR: SDL2 not found at $SDL2_DIR" -ForegroundColor Red
    exit 1
}

# ── Handle clean ────────────────────────────────────────────────────────────
if ($Action -eq "clean") {
    Write-Host "Cleaning build artifacts..."
    Remove-Item -Force -ErrorAction SilentlyContinue voxel_engine.exe, *.obj, *.pdb, *.ilk
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue build
    Write-Host "Done."
    exit 0
}

# ── Build ───────────────────────────────────────────────────────────────────
Write-Host "Building Voxel Engine for Windows (SDL2 + OpenGL)..." -ForegroundColor Cyan

# Keep MSVC intermediates out of the project root
New-Item -ItemType Directory -Force "$PSScriptRoot\build" | Out-Null

$buildCmd = @"
call "$vcvars" -arch=amd64 >nul 2>&1
cl.exe /std:c++17 /O2 /W3 /EHsc /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
    /Fo"build\\" ^
    /I"$SDL2_INC" ^
    /I"src\core" /I"src\world" /I"src\rendering" /I"src\utils" /I"src\time" /I"src\weather" /I"src\craft" /I"src\ui" /I"src\physics" /I"src\seasons" ^
    src\main.cpp src\core\renderer.cpp src\rendering\sky.cpp ^
    src\rendering\texture_atlas.cpp src\rendering\debug_overlay.cpp ^
    src\rendering\weather_particles.cpp ^
    src\world\chunk.cpp src\world\chunk_manager.cpp ^
    /Fe:voxel_engine.exe ^
    /link /SUBSYSTEM:CONSOLE /LIBPATH:"$SDL2_LIB" ^
    SDL2main.lib SDL2.lib opengl32.lib user32.lib gdi32.lib kernel32.lib shell32.lib
"@

$tempBat = Join-Path $PSScriptRoot "_build_temp.bat"
Set-Content -Path $tempBat -Value $buildCmd -Encoding ASCII

try {
    cmd.exe /C $tempBat
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
} finally {
    Remove-Item -Force -ErrorAction SilentlyContinue $tempBat
}

# Copy SDL2.dll next to the exe
Copy-Item -Force "$SDL2_LIB\SDL2.dll" "$PSScriptRoot\SDL2.dll"

Write-Host ""
Write-Host "BUILD SUCCESSFUL: voxel_engine.exe" -ForegroundColor Green
Write-Host "Run with: .\voxel_engine.exe"
