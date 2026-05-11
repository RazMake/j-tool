# Setup MSVC x64 environment and execute the given command.
# Adjust MSVC_DIR and WINKITS_VER if your installed versions differ.

$MSVC_DIR    = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
$WINKITS_DIR = "C:\Program Files (x86)\Windows Kits\10"
$WINKITS_VER = "10.0.26100.0"

$env:PATH    = "$MSVC_DIR\bin\Hostx64\x64;$WINKITS_DIR\bin\$WINKITS_VER\x64;C:\Program Files\CMake\bin;$env:PATH"
$env:INCLUDE = "$MSVC_DIR\include;$WINKITS_DIR\include\$WINKITS_VER\ucrt;$WINKITS_DIR\include\$WINKITS_VER\um;$WINKITS_DIR\include\$WINKITS_VER\shared;$WINKITS_DIR\include\$WINKITS_VER\winrt"
$env:LIB     = "$MSVC_DIR\lib\x64;$WINKITS_DIR\lib\$WINKITS_VER\ucrt\x64;$WINKITS_DIR\lib\$WINKITS_VER\um\x64"

# Execute whatever is passed as arguments
if ($args.Count -gt 0) {
    & $args[0] $args[1..($args.Count - 1)]
    exit $LASTEXITCODE
}
