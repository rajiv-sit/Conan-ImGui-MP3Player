@ECHO ON
setlocal

set BASEDIR=%~dp0
set BUILD_DIR=build_debug_latest
PUSHD %BASEDIR%

REM Clean previous build dir (ignore errors if locked)
IF EXIST "%BUILD_DIR%" RMDIR /Q /S "%BUILD_DIR%" 2>nul

conan install . -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True --output-folder="%BUILD_DIR%" --build=missing --settings=build_type=Debug
cd "%BUILD_DIR%"
cmake .. -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=./build/generators/conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
cmake --build . --config Debug
robocopy ../assets/visualizer/webfonts Debug fa-brands-400.ttf /z
robocopy ../assets/visualizer/webfonts Debug fa-regular-400.ttf /z
robocopy ../assets/visualizer/webfonts Debug fa-solid-900.ttf /z
robocopy ../assets/visualizer/webfonts Debug fa-v4compatibility.ttf /z
robocopy ../test Debug Oryza.mp3 /z
Start "" Debug\mp3player.exe
endlocal
