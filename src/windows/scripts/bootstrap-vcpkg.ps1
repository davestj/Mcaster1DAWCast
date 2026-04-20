# Mcaster1DAWCast — vcpkg bootstrap for Windows port
# Clones and builds vcpkg under src/windows/vcpkg/ so the build is self-contained.

$ErrorActionPreference = "Stop"

$WindowsRoot = Split-Path -Parent $PSScriptRoot
$VcpkgRoot   = Join-Path $WindowsRoot "vcpkg"
$VcpkgRepo   = "https://github.com/microsoft/vcpkg.git"
$VcpkgRef    = "2026.03.18"

Write-Host "Mcaster1DAWCast vcpkg bootstrap"
Write-Host "  Windows root: $WindowsRoot"
Write-Host "  vcpkg target: $VcpkgRoot"
Write-Host "  pinned ref:   $VcpkgRef"
Write-Host ""

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "git not found in PATH — install Git for Windows and retry."
}

if (Test-Path $VcpkgRoot) {
    Write-Host "vcpkg already present, fetching + checking out $VcpkgRef ..."
    git -C $VcpkgRoot fetch --tags --quiet
    git -C $VcpkgRoot checkout --quiet $VcpkgRef
} else {
    Write-Host "Cloning vcpkg ..."
    git clone --quiet $VcpkgRepo $VcpkgRoot
    git -C $VcpkgRoot checkout --quiet $VcpkgRef
}

$Bootstrap = Join-Path $VcpkgRoot "bootstrap-vcpkg.bat"
if (-not (Test-Path $Bootstrap)) {
    throw "bootstrap-vcpkg.bat missing — vcpkg clone is corrupted."
}

Write-Host "Running bootstrap-vcpkg.bat ..."
& $Bootstrap -disableMetrics

$VcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"
if (-not (Test-Path $VcpkgExe)) {
    throw "vcpkg.exe not produced by bootstrap."
}

Write-Host ""
Write-Host "vcpkg ready: $VcpkgExe"
Write-Host "Next steps:"
Write-Host "  cd $WindowsRoot"
Write-Host "  cmake --preset vs2022-x64-debug       # generates Mcaster1DAWCast.sln"
Write-Host "  cmake --build --preset vs2022-x64-debug"
Write-Host ""
Write-Host "Or open $WindowsRoot in Visual Studio 2022 (File > Open > Folder)"
Write-Host "and VS will pick up CMakePresets.json automatically."
