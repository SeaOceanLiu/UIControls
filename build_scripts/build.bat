@echo off
call "d:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64

set UICONTROLS_ROOT=%~dp0..

echo Building UIControls...
mkdir build 2>nul
cd /d build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug

echo.
echo Build completed!
echo Executables in: %UICONTROLS_ROOT%\build\Debug

pause
