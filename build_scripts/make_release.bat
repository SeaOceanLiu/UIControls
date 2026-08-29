@echo off
set "PATH=%SystemRoot%\System32;%PATH%"
chcp 65001 >nul
setlocal EnableDelayedExpansion

rem ============================================================
rem make_release.bat - 生成 UICornerstone Release 发布包
rem
rem 用法:
rem   make_release.bat              -> 生成 <仓库>/release
rem   make_release.bat D:\out       -> 生成指定目录
rem   make_release.bat [dir] -y     -> 跳过交互暂停
rem
rem 产物:
rem   UICornerstone.dll / UIBackend_{sdl3,sfml,raylib}.dll
rem   SDL3.dll / SDL3_ttf.dll / SDL3_image.dll
rem   sfml-graphics-3.dll / sfml-window-3.dll / sfml-system-3.dll
rem   raylib.dll
rem   assets/ / binding/ / tools/(validate_layout.exe + schema)
rem ============================================================

set "UICORNERSTONE_ROOT=%~dp0.."
set "RELEASE_DIR=%~1"
if "%RELEASE_DIR%"=="" set "RELEASE_DIR=%UICORNERSTONE_ROOT%\release"
if /i "%~2"=="-y" set "NO_PAUSE=1"

echo [%date% %time%] ============================================
echo [%date% %time%] make_release start
echo [%date% %time%] root: %UICORNERSTONE_ROOT%
echo [%date% %time%] release dir: %RELEASE_DIR%
echo [%date% %time%] ============================================

set "CORE_DLL=%UICORNERSTONE_ROOT%\build\sdl3_dll\Release\UICornerstone.dll"

rem ============================================================
rem 0. 清理 build 目录（递归删除非空，强制全新构建，防旧版本残留）
rem ============================================================
echo [%time%] [0/6] Cleaning build dirs (sdl3_dll sfml_dll raylib_dll tools)...
for %%D in (sdl3_dll sfml_dll raylib_dll tools) do (
    if exist "%UICORNERSTONE_ROOT%\build\%%D" (
        rd /s /q "%UICORNERSTONE_ROOT%\build\%%D"
        if exist "%UICORNERSTONE_ROOT%\build\%%D" (
            echo [ERROR] failed to remove build\%%D
            goto :fail
        )
    )
)
echo [%time%] [0/6] Clean done.

rem ============================================================
rem 1. 构建三后端 Release DLL（仅核心 DLL + 后端插件，不编译 test/CABI/sample）
rem ============================================================
call :build_backend sdl3
if errorlevel 1 goto :fail
call :build_backend SFML
if errorlevel 1 goto :fail
call :build_backend RAYLIB
if errorlevel 1 goto :fail
echo [%time%] [1/6] Backends (sdl3/sfml/raylib) DLL built.

rem ============================================================
rem 2. 构建 tools（validate_layout Release）
rem ============================================================
call :build_tools
if errorlevel 1 goto :fail
echo [%time%] [2/6] tools (validate_layout) built.

rem ============================================================
rem 3. 重建 release 目录
rem ============================================================
echo [%time%] [3/6] Assembling release into: %RELEASE_DIR%
if exist "%RELEASE_DIR%" rd /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%" 2>nul || (echo [ERROR] cannot create %RELEASE_DIR% & goto :fail)
mkdir "%RELEASE_DIR%\binding"
mkdir "%RELEASE_DIR%\tools" 2>nul

rem ============================================================
rem 4. 复制 核心 DLL + 后端 DLL
rem ============================================================
copy /y "%CORE_DLL%" "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\build\sdl3_dll\Release\UIBackend_sdl3.dll"   "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\build\sfml_dll\Release\UIBackend_sfml.dll"   "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\build\raylib_dll\Release\UIBackend_raylib.dll" "%RELEASE_DIR%\" >nul || goto :fail
echo [%time%] [4/6] Core + backend DLLs copied.

rem ============================================================
rem 5. 复制 运行时 DLL
rem ============================================================
copy /y "%UICORNERSTONE_ROOT%\subModules\libs\SDL3.dll"        "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\subModules\libs\SDL3_ttf.dll"    "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\subModules\libs\SDL3_image.dll"  "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\subModules\SFML\bin\sfml-graphics-3.dll" "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\subModules\SFML\bin\sfml-window-3.dll"   "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\subModules\SFML\bin\sfml-system-3.dll"   "%RELEASE_DIR%\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\subModules\raylib\lib\raylib.dll" "%RELEASE_DIR%\" >nul || goto :fail
echo [%time%] [5/6] Runtime DLLs copied.

rem ============================================================
rem 6. 复制 assets + binding + tools
rem ============================================================
rem assets: 仅运行时所需子集（排除 background/config/music/saved/sounds）
mkdir "%RELEASE_DIR%\assets" 2>nul
copy /y "%UICORNERSTONE_ROOT%\subModules\assets\Windows.jsonc" "%RELEASE_DIR%\assets\" >nul || goto :fail
for %%A in (animations fonts images) do (
    xcopy /y /e /i "%UICORNERSTONE_ROOT%\subModules\assets\%%A" "%RELEASE_DIR%\assets\%%A" >nul || goto :fail
)

rem 用户手册 docs 站点（跟随发布；含 docs/assets、css、controls/appendix 等全部页面）
xcopy /y /e /i "%UICORNERSTONE_ROOT%\docs" "%RELEASE_DIR%\docs" >nul || goto :fail

rem binding: 仅源码（排除 build 编译产物）
copy /y "%UICORNERSTONE_ROOT%\binding\CMakeLists.txt" "%RELEASE_DIR%\binding\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\binding\LICENSE"        "%RELEASE_DIR%\binding\" >nul || goto :fail
for %%B in (src include cmake samples) do (
    xcopy /y /e /i "%UICORNERSTONE_ROOT%\binding\%%B" "%RELEASE_DIR%\binding\%%B" >nul || goto :fail
)
copy /y "%UICORNERSTONE_ROOT%\include\UICornerstoneAPI.h" "%RELEASE_DIR%\binding\include\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\include\PropertyNames.h"    "%RELEASE_DIR%\binding\include\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\build_scripts\CMakeLists.binding_sample.txt" "%RELEASE_DIR%\binding\CMakeLists.example.txt" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\build\tools\Release\validate_layout.exe" "%RELEASE_DIR%\tools\" >nul || goto :fail
copy /y "%UICORNERSTONE_ROOT%\include\PropertyNames.h"    "%RELEASE_DIR%\tools\" >nul || goto :fail
xcopy /y /e /i "%UICORNERSTONE_ROOT%\docs\schema" "%RELEASE_DIR%\tools\schema" >nul || goto :fail
echo [%time%] [6/6] assets + binding + tools copied.

rem ============================================================
rem 7. 校验清单
rem ============================================================
set "MISSING="
if not exist "%RELEASE_DIR%\tools\validate_layout.exe" set "MISSING=!MISSING! tools/validate_layout.exe"
if not exist "%RELEASE_DIR%\tools\PropertyNames.h" set "MISSING=!MISSING! tools/PropertyNames.h"
if not exist "%RELEASE_DIR%\tools\schema\declarative-ui.schema.json" set "MISSING=!MISSING! tools/schema"
for %%F in (UICornerstone.dll UIBackend_sdl3.dll UIBackend_sfml.dll UIBackend_raylib.dll SDL3.dll SDL3_ttf.dll SDL3_image.dll sfml-graphics-3.dll sfml-window-3.dll sfml-system-3.dll raylib.dll) do (
    if not exist "%RELEASE_DIR%\%%F" set "MISSING=!MISSING! %%F"
)
if not exist "%RELEASE_DIR%\assets"         set "MISSING=!MISSING! assets"
if not exist "%RELEASE_DIR%\docs\index.html" set "MISSING=!MISSING! docs/ (用户手册)"
if not exist "%RELEASE_DIR%\binding\include\UICornerstone.h"     set "MISSING=!MISSING! binding/include/UICornerstone.h"
if not exist "%RELEASE_DIR%\binding\include\UICornerstoneAPI.h" set "MISSING=!MISSING! binding/include/UICornerstoneAPI.h"
if not exist "%RELEASE_DIR%\binding\include\PropertyNames.h"     set "MISSING=!MISSING! binding/include/PropertyNames.h"
if not exist "%RELEASE_DIR%\binding\src\UICornerstone.cpp"       set "MISSING=!MISSING! binding/src/UICornerstone.cpp"
if not exist "%RELEASE_DIR%\binding\CMakeLists.example.txt"      set "MISSING=!MISSING! binding/CMakeLists.example.txt"

if not "%MISSING%"=="" (
    echo [ERROR] missing:%MISSING%
    goto :fail
)

echo [%date% %time%] ============================================
echo [%date% %time%] Release ready: %RELEASE_DIR%
echo [%date% %time%]   core + backend DLLs + runtime DLLs + assets + binding + tools
echo [%date% %time%]   tools: validate_layout.exe + PropertyNames.h + schema
echo [%date% %time%]   usage(in release dir): tools\validate_layout.exe layout.json --strict   (缺省从 exe 同目录找 PropertyNames.h、下一级 schema/ 找 schema)
echo [%date% %time%] make_release done
echo [%date% %time%] ============================================
if not defined NO_PAUSE pause
exit /b 0

:fail
echo [%time%] [ERROR] make_release failed.
if not defined NO_PAUSE pause
exit /b 1

:build_backend
set "BNAME=%~1"
set "BLOWER="
if /i "%BNAME%"=="sdl3"   (set "BDIR=sdl3_dll"   & set "BOPT=SDL3"   & set "BLOWER=sdl3")
if /i "%BNAME%"=="SFML"   (set "BDIR=sfml_dll"   & set "BOPT=SFML"   & set "BLOWER=sfml")
if /i "%BNAME%"=="RAYLIB" (set "BDIR=raylib_dll" & set "BOPT=RAYLIB" & set "BLOWER=raylib")
set "BUILD_DIR=%UICORNERSTONE_ROOT%\build\%BDIR%"

echo [%time%] Building UICornerstone [%BOPT% Release DLL]...
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    cmake -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release -DUICORNERSTONE_BACKEND=%BOPT% -DUICORNERSTONE_BUILD_DLL=ON >nul 2>&1
    if errorlevel 1 (echo [ERROR] cmake configure %BOPT% failed & exit /b 1)
)
cmake --build "%BUILD_DIR%" --config Release --target UICornerstone_dll UIBackend_%BLOWER%
if errorlevel 1 (echo [ERROR] cmake build %BOPT% failed & exit /b 1)
echo [%time%]   %BOPT% DLL built.
exit /b 0

:build_tools
set "TOOLS_DIR=%UICORNERSTONE_ROOT%\build\tools"
echo [%time%] Building validate_layout [Release]...
if not exist "%TOOLS_DIR%\CMakeCache.txt" (
    cmake -S "%UICORNERSTONE_ROOT%\tools" -B "%TOOLS_DIR%" -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release >nul 2>&1
    if errorlevel 1 (echo [ERROR] cmake configure tools failed & exit /b 1)
)
cmake --build "%TOOLS_DIR%" --config Release --target validate_layout
if errorlevel 1 (echo [ERROR] cmake build validate_layout failed & exit /b 1)
echo [%time%]   validate_layout built.
exit /b 0
