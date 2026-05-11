# Configure the CMake project for Debug with testing enabled.
& "$PSScriptRoot\build_env.ps1" cmake -B build -G "NMake Makefiles" `
    -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
exit $LASTEXITCODE
