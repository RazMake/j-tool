# Setup MSVC x64 environment — auto-detects VS and Windows SDK paths.

$ErrorActionPreference = "Stop"

# --- Detect MSVC via vswhere ---
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere not found — install Visual Studio or Build Tools 2022."
    exit 1
}

$vsRoots = & $vswhere -all -products * -property installationPath 2>$null
$MSVC_DIR = $null
foreach ($root in $vsRoots) {
    $base = "$root\VC\Tools\MSVC"
    if (-not (Test-Path $base)) { continue }
    $ver = Get-ChildItem $base -Directory | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
    if ($ver -and (Test-Path "$base\$ver\include")) {
        $MSVC_DIR = "$base\$ver"
        break
    }
}
if (-not $MSVC_DIR) {
    Write-Error "No Visual Studio installation with MSVC C++ headers found."
    exit 1
}

# --- Detect Windows SDK ---
$WINKITS_DIR = "${env:ProgramFiles(x86)}\Windows Kits\10"
$WINKITS_VER = Get-ChildItem "$WINKITS_DIR\Include" -Directory |
    Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
if (-not $WINKITS_VER) {
    Write-Error "No Windows SDK found under $WINKITS_DIR\Include"
    exit 1
}

# --- Set environment ---
$env:PATH    = "$MSVC_DIR\bin\Hostx64\x64;$WINKITS_DIR\bin\$WINKITS_VER\x64;C:\Program Files\CMake\bin;$env:PATH"
$env:INCLUDE = "$MSVC_DIR\include;$WINKITS_DIR\include\$WINKITS_VER\ucrt;$WINKITS_DIR\include\$WINKITS_VER\um;$WINKITS_DIR\include\$WINKITS_VER\shared;$WINKITS_DIR\include\$WINKITS_VER\winrt"
$env:LIB     = "$MSVC_DIR\lib\x64;$WINKITS_DIR\lib\$WINKITS_VER\ucrt\x64;$WINKITS_DIR\lib\$WINKITS_VER\um\x64"

# Execute whatever is passed as arguments
if ($args.Count -gt 0) {
    & $args[0] $args[1..($args.Count - 1)]
    exit $LASTEXITCODE
}
