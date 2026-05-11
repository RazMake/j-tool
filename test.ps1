# Run the test suite.
& "$PSScriptRoot\build_env.ps1" ctest --test-dir build --output-on-failure
exit $LASTEXITCODE
