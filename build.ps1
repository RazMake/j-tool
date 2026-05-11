# Build the project.
& "$PSScriptRoot\build_env.ps1" cmake --build build
exit $LASTEXITCODE
