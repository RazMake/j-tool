#Requires -RunAsAdministrator
# ============================================================
#  Jump — Developer Machine Setup
#  Run this from an elevated (Administrator) PowerShell prompt.
#  It installs all prerequisites, detects MSVC paths, generates
#  build_env.ps1, configures the project, builds, tests, and
#  runs the coverage gate.
# ============================================================

$ErrorActionPreference = "Stop"

Write-Host "============================================================"
Write-Host " Jump — Developer Environment Setup"
Write-Host "============================================================"
Write-Host ""

# ============================================================
#  1. Install prerequisites via Chocolatey
# ============================================================
Write-Host "[1/7] Checking prerequisites..."

if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
    Write-Host "  Installing Chocolatey..."
    [System.Net.ServicePointManager]::SecurityProtocol = 3072
    Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
    $env:PATH = "$env:ALLUSERSPROFILE\chocolatey\bin;$env:PATH"
}
Write-Host "  Chocolatey: OK"

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Host "  Installing Git..."
    choco install git -y --no-progress
} else {
    Write-Host "  Git: OK"
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "  Installing CMake..."
    choco install cmake --installargs '"ADD_CMAKE_TO_PATH=System"' -y --no-progress
} else {
    Write-Host "  CMake: OK"
}

$occPath = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"
if (-not (Get-Command OpenCppCoverage -ErrorAction SilentlyContinue) -and -not (Test-Path $occPath)) {
    Write-Host "  Installing OpenCppCoverage..."
    choco install opencppcoverage -y --no-progress
} else {
    Write-Host "  OpenCppCoverage: OK"
}

# Visual Studio Build Tools
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Host "  Installing Visual Studio Build Tools 2022..."
    choco install visualstudio2022buildtools -y --no-progress `
        --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
}
Write-Host "  Visual Studio Build Tools: OK"
Write-Host ""

# ============================================================
#  2. Verify MSVC and Windows SDK detection
# ============================================================
Write-Host "[2/5] Verifying MSVC and Windows SDK detection via build_env.ps1..."
& .\build_env.ps1
if ($LASTEXITCODE -ne 0) { Write-Error "build_env.ps1 detection failed."; exit 1 }
Write-Host "  Detection OK."
Write-Host ""

# ============================================================
#  3. Configure
# ============================================================
Write-Host "[3/5] Configuring project (CMake)..."
& .\build_env.ps1 cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed."; exit 1 }
Write-Host ""

# ============================================================
#  4. Build
# ============================================================
Write-Host "[4/5] Building (Debug)..."
& .\build_env.ps1 cmake --build build
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed."; exit 1 }
Write-Host ""

# ============================================================
#  5. Test & Coverage
# ============================================================
Write-Host "[5/5] Running tests..."
& .\build_env.ps1 ctest --test-dir build --output-on-failure
if ($LASTEXITCODE -ne 0) { Write-Error "Tests failed."; exit 1 }
Write-Host ""

# ============================================================
#  Coverage
# ============================================================
Write-Host "Running coverage check..."
& .\check_coverage.ps1
if ($LASTEXITCODE -ne 0) {
    Write-Host "WARNING: Coverage gate failed. See output above."
} else {
    Write-Host "  Coverage gate passed."
}
Write-Host ""

Write-Host "============================================================"
Write-Host " Setup complete!"
Write-Host ""
Write-Host "  Quick reference:"
Write-Host "    .\build_env.ps1 cmake --build build                          Build"
Write-Host "    .\build_env.ps1 ctest --test-dir build --output-on-failure   Test"
Write-Host "    .\check_coverage.ps1                                         Coverage"
Write-Host "============================================================"
