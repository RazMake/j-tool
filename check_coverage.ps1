# Run code coverage locally and check against the 85% gate.
# Requires: OpenCppCoverage (choco install opencppcoverage -y)
#
# Usage:  .\check_coverage.ps1                    (uses build\j_tests.exe)
#         .\check_coverage.ps1 -TestExe <path>    (custom test-exe path)

param(
    [string]$TestExe = "build\j_tests.exe"
)

$ErrorActionPreference = "Stop"

# --- Locate OpenCppCoverage ---
$occ = Get-Command OpenCppCoverage -ErrorAction SilentlyContinue |
       Select-Object -ExpandProperty Source
if (-not $occ) {
    $occ = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"
}
if (-not (Test-Path $occ)) {
    Write-Error "OpenCppCoverage not found. Install with:  choco install opencppcoverage -y"
    exit 1
}

# --- Verify test executable ---
if (-not (Test-Path $TestExe)) {
    Write-Error @"
Test executable not found: $TestExe
Build first:  .\build_env.ps1 cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
              .\build_env.ps1 cmake --build build
"@
    exit 1
}

# --- Run coverage ---
Write-Host "--- Running coverage ---"
& $occ `
    --sources src\ `
    --excluded_sources osd.c `
    --excluded_sources main.c `
    --excluded_sources main_win.c `
    --excluded_sources jump.c `
    --excluded_sources install.c `
    --excluded_sources log.c `
    --export_type cobertura:coverage.xml `
    -- $TestExe

if ($LASTEXITCODE -ne 0) {
    Write-Error "OpenCppCoverage failed."
    exit 1
}

# --- Parse coverage.xml and print summary table ---
Write-Host ""
Write-Host "--- Coverage Summary ---"

[xml]$xml = Get-Content coverage.xml
$rows = @()
foreach ($pkg in $xml.SelectNodes("//package")) {
    foreach ($cls in $pkg.SelectNodes(".//class")) {
        $filename = $cls.filename -replace '\\', '/'
        if ($filename -notmatch 'src/(config|core|platform|resolve)/') { continue }
        $name = $filename.Split('/')[-1]
        $lines = $cls.SelectNodes(".//line")
        $total = $lines.Count
        $hit = ($lines | Where-Object { [int]$_.hits -gt 0 }).Count
        if ($total -gt 0) {
            $rows += [PSCustomObject]@{ File = $name; Hit = $hit; Total = $total }
        }
    }
}

$rows = $rows | Sort-Object File
$allHit = ($rows | Measure-Object -Property Hit -Sum).Sum
$allTotal = ($rows | Measure-Object -Property Total -Sum).Sum
$w = ($rows | ForEach-Object { $_.File.Length } | Measure-Object -Maximum).Maximum
if ($w -lt 5) { $w = 5 }

$hdr = "  {0,-2} {1,-$w}  {2,7}  {3,5}  {4,5}  {5,7}" -f "", "File", "Lines", "Hit", "Miss", "Cover"
$sep = "  {0,-2} {1,-$w}  {2,7}  {3,5}  {4,5}  {5,7}" -f "--", ("-" * $w), "-------", "-----", "-----", "-------"
Write-Host $hdr
Write-Host $sep

foreach ($r in $rows) {
    $pct = $r.Hit / $r.Total * 100
    $mark = if ($pct -lt 85) { "!!" } else { "  " }
    $color = if ($pct -ge 85) { "Green" } else { "DarkYellow" }
    $line = "  {0,-2} {1,-$w}  {2,7}  {3,5}  {4,5}  {5,6:F1}%" -f $mark, $r.File, $r.Total, $r.Hit, ($r.Total - $r.Hit), $pct
    Write-Host $line -ForegroundColor $color
}

Write-Host $sep
$totalPct = if ($allTotal -gt 0) { $allHit / $allTotal * 100 } else { 0 }
$totalMark = if ($totalPct -lt 85) { "!!" } else { "  " }
$totalColor = if ($totalPct -ge 85) { "Green" } else { "DarkYellow" }
$totalLine = "  {0,-2} {1,-$w}  {2,7}  {3,5}  {4,5}  {5,6:F1}%" -f $totalMark, "TOTAL", $allTotal, $allHit, ($allTotal - $allHit), $totalPct
Write-Host $totalLine -ForegroundColor $totalColor
Write-Host ""

if ($totalPct -ge 85) {
    Write-Host "PASS: Coverage meets 85% threshold." -ForegroundColor Green
    exit 0
} else {
    Write-Host "FAIL: Coverage is below 85% threshold." -ForegroundColor DarkYellow
    exit 1
}
