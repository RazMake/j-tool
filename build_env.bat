@echo off
REM Setup MSVC x64 environment manually
set "MSVC_DIR=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.44.35207"
set "WINKITS_DIR=C:\Program Files (x86)\Windows Kits\10"
set "WINKITS_VER=10.0.26100.0"

set "PATH=%MSVC_DIR%\bin\Hostx64\x64;%WINKITS_DIR%\bin\%WINKITS_VER%\x64;C:\Program Files\CMake\bin;%PATH%"
set "SCOPECPP=%MSVC_DIR%\..\..\..\..\SDK\ScopeCppSDK\vc15\VC"
set "INCLUDE=%SCOPECPP%\include;%WINKITS_DIR%\include\%WINKITS_VER%\ucrt;%WINKITS_DIR%\include\%WINKITS_VER%\um;%WINKITS_DIR%\include\%WINKITS_VER%\shared;%WINKITS_DIR%\include\%WINKITS_VER%\winrt"
set "LIB=%MSVC_DIR%\lib\onecore\x64;%WINKITS_DIR%\lib\%WINKITS_VER%\ucrt\x64;%WINKITS_DIR%\lib\%WINKITS_VER%\um\x64"

cd /d "c:\Data\My Code\Jump"

REM Execute whatever is passed as arguments
%*
