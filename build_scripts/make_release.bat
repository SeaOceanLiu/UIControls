@echo off
setlocal EnableDelayedExpansion

rem ============================================================
rem make_release.bat - 构建三后端 Release 并组装待发布 release 目录
rem
rem 用法:
rem   make_release.bat            → 输出到 <仓库根>/release
rem   make_release.bat D:\out     → 输出到指定目录
rem
rem 输出内容:
rem   UICornerstone.dll           核心库（任意后端构建产物，三端一致）
rem   UIBackend_{sdl3,sfml,raylib}.dll
rem   SDL3.dll / SDL3_ttf.dll / SDL3_image.dll      （subModules/libs，不含 DebugInfoX64.dll）
rem   sfml-graphics-3.dll / sfml-window-3.dll / sfml-system-3.dll   （subModules/SFML/bin）
rem   raylib.dll                                        （subModules/raylib/lib）
rem   assets/                      运行时资源（subModules/assets 全量）
rem   binding/                      C++ Binding 源码（含核心头 UICornerstoneAPI.h / PropertyNames.h）
rem ============================================================

set "UICORNERSTONE_ROOT=%~dp0.."
set "RELEASE_DIR=%~1"
if "%RELEASE_DIR%"=="" set "RELEASE_DIR=%UICORNERSTONE_ROOT%\release"
if /i "%~2"=="-y" set "NO_PAUSE=1"
set "PATH=%SystemRoot%\System32;%PATH%"   rem xcopy 等系统命令兜底

set "CORE_DLL=%UICORNERSTONE_ROOT%\build\sdl3_dll\Release\UICornerstone.dll"

rem ============================================================
rem 1. 构建三后端 Release（已有配置则增量）
rem ============================================================
for %%B in (sdl3 SFML RAYLIB) do call :build_backend %%B
if errorlevel 1 (
    echo [ERROR] build failed.
    if not defined NO_PAUSE pause
    exit /b 1
)

rem ============================================================
rem 2. 重建 release 目录（仅清理已知产物）
rem ============================================================
if exist "%RELEASE_DIR%" rd /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%" || (echo [ERROR] cannot create %RELEASE_DIR% & pause & exit /b 1)
mkdir "%RELEASE_DIR%\binding"

echo.
echo ============================================================
echo Assembling release into: %RELEASE_DIR%
echo ============================================================

rem ============================================================
rem 3. 核心库 + 三后端 DLL
rem ============================================================
copy /y "%CORE_DLL%" "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\build\sdl3_dll\Release\UIBackend_sdl3.dll"  "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\build\sfml_dll\Release\UIBackend_sfml.dll"  "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\build\raylib_dll\Release\UIBackend_raylib.dll" "%RELEASE_DIR%\" >nul || goto :fail

rem ============================================================
rem 4. 运行时 DLL（不拷贝 DebugInfoX64.dll）
rem ============================================================
copy /y "%UICORNERSTONE_ROOT%\subModules\libs\SDL3.dll"        "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\subModules\libs\SDL3_ttf.dll"    "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\subModules\libs\SDL3_image.dll"  "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\subModules\SFML\bin\sfml-graphics-3.dll" "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\subModules\SFML\bin\sfml-window-3.dll"   "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\subModules\SFML\bin\sfml-system-3.dll"   "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\subModules\raylib\lib\raylib.dll" "%RELEASE_DIR%\" >nul || goto :fail

rem ============================================================
rem 5. 运行时资源（assets 全量）
rem ============================================================
xcopy /y /e /i "%UICORNERSTONE_ROOT%\subModules\assets" "%RELEASE_DIR%\assets" >nul || goto :fail

rem ============================================================
rem 6. C++ Binding 源码（含构建所需的两个核心头）
rem     binding/include/ 附入 UICornerstoneAPI.h + PropertyNames.h，
rem     使发布包可独立编译 Binding（纯动态加载，无需核心构建树）；另附独立构建 CMakeLists 样例。
rem ============================================================
xcopy /y /e /i "%UICORNERSTONE_ROOT%\binding" "%RELEASE_DIR%\binding" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\include\UICornerstoneAPI.h" "%RELEASE_DIR%\binding\include\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\include\PropertyNames.h"    "%RELEASE_DIR%\binding\include\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\build_scripts\CMakeLists.binding_sample.txt" "%RELEASE_DIR%\binding\CMakeLists.example.txt" >nul || goto :fail

rem ============================================================
rem 7. 校验清单
rem ============================================================
set "MISSING="
for %%F in (UICornerstone.dll UIBackend_sdl3.dll UIBackend_sfml.dll UIBackend_raylib.dll SDL3.dll SDL3_ttf.dll SDL3_image.dll sfml-graphics-3.dll sfml-window-3.dll sfml-system-3.dll raylib.dll) do (
    if not exist "%RELEASE_DIR%\%%F" set "MISSING=!MISSING! %%F"
)
if not exist "%RELEASE_DIR%\assets"       set "MISSING=!MISSING! assets"
if not exist "%RELEASE_DIR%\binding\include\UICornerstone.h"   set "MISSING=!MISSING! binding/include/UICornerstone.h"
if not exist "%RELEASE_DIR%\binding\include\UICornerstoneAPI.h" set "MISSING=!MISSING! binding/include/UICornerstoneAPI.h"
if not exist "%RELEASE_DIR%\binding\include\PropertyNames.h"   set "MISSING=!MISSING! binding/include/PropertyNames.h"
if not exist "%RELEASE_DIR%\binding\src\UICornerstone.cpp"     set "MISSING=!MISSING! binding/src/UICornerstone.cpp"
if not exist "%RELEASE_DIR%\binding\CMakeLists.example.txt"    set "MISSING=!MISSING! binding/CMakeLists.example.txt"

if not "%MISSING%"=="" (
    echo [ERROR] missing:%MISSING%
    if not defined NO_PAUSE pause
    exit /b 1
)

echo.
echo ============================================================
echo Release ready: %RELEASE_DIR%
echo   核心库 + 三后端 DLL + 运行时 DLL + assets + binding 源码
echo ============================================================
if not defined NO_PAUSE pause
exit /b 0

:fail
echo [ERROR] copy failed.
if not defined NO_PAUSE pause
exit /b 1

:build_backend
set "BNAME=%~1"
if /i "%BNAME%"=="sdl3"  (set "BDIR=sdl3_dll"   & set "BOPT=SDL3")
if /i "%BNAME%"=="SFML"  (set "BDIR=sfml_dll"   & set "BOPT=SFML")
if /i "%BNAME%"=="RAYLIB" (set "BDIR=raylib_dll" & set "BOPT=RAYLIB")
set "BUILD_DIR=%UICORNERSTONE_ROOT%\build\%BDIR%"

echo.
echo Building UICornerstone [%BOPT% Release]...
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    cmake -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release -DUICORNERSTONE_BACKEND=%BOPT% >nul 2>&1
    if errorlevel 1 (echo [ERROR] cmake configure %BOPT% failed & exit /b 1)
)
cmake --build "%BUILD_DIR%" --config Release --target ALL_BUILD
if errorlevel 1 (echo [ERROR] cmake build %BOPT% failed & exit /b 1)
exit /b 0
