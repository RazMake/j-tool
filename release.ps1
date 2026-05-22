param(
    [string]$Version
)

$ErrorActionPreference = "Stop"

# If no version provided, read from CMakeLists.txt
if (-not $Version) {
    $cmakeContent = Get-Content "$PSScriptRoot\CMakeLists.txt" -Raw
    if ($cmakeContent -match 'project\s*\(\s*Jump\s+VERSION\s+(\d+\.\d+\.\d+)') {
        $Version = $Matches[1]
    } else {
        Write-Error "Could not parse version from CMakeLists.txt"
        exit 1
    }
}

Write-Host "Building Jump v$Version (Release)" -ForegroundColor Cyan

# Update version in CMakeLists.txt so it flows into version.h
$cmakePath = "$PSScriptRoot\CMakeLists.txt"
$cmakeText = Get-Content $cmakePath -Raw
$cmakeText = $cmakeText -replace 'project\s*\(\s*Jump\s+VERSION\s+[\d.]+', "project(Jump VERSION $Version"
Set-Content $cmakePath $cmakeText -NoNewline

# Configure CMake for Release
Write-Host "`nConfiguring CMake..." -ForegroundColor Yellow
& "$PSScriptRoot\build_env.ps1" cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Build
Write-Host "`nBuilding..." -ForegroundColor Yellow
& "$PSScriptRoot\build_env.ps1" cmake --build build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Create dist directory
$distDir = "$PSScriptRoot\dist"
New-Item -ItemType Directory -Path $distDir -Force | Out-Null

# Create zip
$zipName = "jump-$Version-win-x64.zip"
$zipPath = Join-Path $distDir $zipName
Write-Host "`nCreating $zipName..." -ForegroundColor Yellow

if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path "$PSScriptRoot\build\j.exe", "$PSScriptRoot\build\jc.exe" -DestinationPath $zipPath
if (-not (Test-Path $zipPath)) {
    Write-Error "Failed to create zip archive"
    exit 1
}

# Build Inno Setup installer if ISCC is available
$iscc = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
if (-not $iscc) {
    $defaultPath = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
    if (Test-Path $defaultPath) {
        $iscc = $defaultPath
    }
}

if ($iscc) {
    Write-Host "`nBuilding installer with Inno Setup..." -ForegroundColor Yellow
    & $iscc "/DMyAppVersion=$Version" "$PSScriptRoot\installer\jump.iss"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host "`nInno Setup (ISCC.exe) not found, skipping installer build." -ForegroundColor DarkYellow
}

# Summary
Write-Host "`n--- Release artifacts ---" -ForegroundColor Green
Get-ChildItem $distDir | ForEach-Object {
    $size = if ($_.Length -ge 1MB) { "{0:N1} MB" -f ($_.Length / 1MB) }
           elseif ($_.Length -ge 1KB) { "{0:N1} KB" -f ($_.Length / 1KB) }
           else { "$($_.Length) B" }
    Write-Host "  $($_.Name)  ($size)"
}
