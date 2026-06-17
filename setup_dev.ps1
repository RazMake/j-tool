#Requires -RunAsAdministrator
# ============================================================
#  Jump — Developer Machine Setup
#  Run this from an elevated (Administrator) PowerShell prompt.
#  It installs all prerequisites, detects MSVC paths, generates
#  build_env.ps1, configures the project, builds, tests, and
#  runs the coverage gate.
# ============================================================

$ErrorActionPreference = "Stop"

# Returns $true only if an installation with MSVC C++ headers is present.
# Mirrors the detection logic in build_env.ps1 so we install the C++ workload
# when the VS Installer exists but the MSVC headers are missing.
function Test-MsvcHeaders {
    param([string]$VswherePath)
    if (-not (Test-Path $VswherePath)) { return $false }
    $roots = & $VswherePath -all -products * -property installationPath 2>$null
    foreach ($root in $roots) {
        $base = "$root\VC\Tools\MSVC"
        if (-not (Test-Path $base)) { continue }
        $ver = Get-ChildItem $base -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
        if ($ver -and (Test-Path "$base\$ver\include")) { return $true }
    }
    return $false
}

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

# Visual Studio Build Tools + MSVC C++ workload
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-MsvcHeaders $vswhere)) {
    if (Test-Path $vswhere) {
        Write-Host "  Visual Studio is installed but the MSVC C++ headers are missing — adding the C++ build tools workload..."
    } else {
        Write-Host "  Installing Visual Studio Build Tools 2022 with the C++ workload..."
    }
    # The workload package depends on visualstudio2022buildtools, so it installs
    # the Build Tools when absent and adds the VCTools workload when only the
    # installer/shell is present.
    choco install visualstudio2022-workload-vctools -y --no-progress `
        --package-parameters "--includeRecommended"
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

    if (-not (Test-MsvcHeaders $vswhere)) {
        Write-Error @"
The MSVC C++ build tools workload could not be installed automatically.
Install it manually via the Visual Studio Installer:
  - Open 'Visual Studio Installer'
  - Modify your Build Tools / Visual Studio installation
  - Enable 'Desktop development with C++' (VCTools workload)
Then re-run .\setup_dev.ps1
"@
        exit 1
    }
}
Write-Host "  Visual Studio Build Tools (MSVC C++): OK"
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
