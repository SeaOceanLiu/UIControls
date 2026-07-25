# 构建与测试

## Build library + all tests

```batch
build_scripts\build.bat sdl3    # SDL3 backend (default)
build_scripts\build.bat sfml    # SFML backend
build_scripts\build.bat raylib  # Raylib backend
```

## Build single test

```batch
build_scripts\build_test.bat test_label             # SDL3 (default) → build/sdl3/test/Debug/
build_scripts\build_test.bat test_label sfml        # SFML → build/sfml/test/Debug/
build_scripts\build_test.bat test_label raylib      # Raylib → build/raylib/test/Debug/
build_scripts\build_sdl3.bat test_label             # shortcut for SDL3
build_scripts\build_sfml.bat test_label             # shortcut for SFML
build_scripts\build_raylib.bat test_label           # shortcut for Raylib

# DLL 模式 C ABI 测试需从 *_dll 构建树编译：
cmake --build build/sdl3_dll --target test_treeview_cabi    # → build/sdl3_dll/test/Debug/
cmake --build build/sfml_dll --target test_treeview_cabi    # → build/sfml_dll/test/Debug/
cmake --build build/raylib_dll --target test_treeview_cabi  # → build/raylib_dll/test/Debug/
```

**Available tests (标准)**: test_menu, test_label, test_editbox, test_checkbox, test_progressbar, test_layout, test_layout_advanced, test_winframe, test_graphtool, test_button, test_slider, test_colorpicker, test_combobox, test_dialog, test_handlecontrol, test_numericupdown, test_splitter, test_treeview

**Available tests (DLL 模式 C ABI)**: test_fromsource_cabi, test_api, test_dialog_cabi, test_combobox_cabi, test_numericupdown_cabi, test_splitter_cabi, test_treeview_cabi

## Running Tests

Executables are backend-specific. Assets and DLLs are auto-copied by CMake POST_BUILD:

```batch
cd build\sdl3\test\Debug
test_label.exe

cd build\sfml\test\Debug
test_label.exe

cd build\sdl3_dll\test\Debug
test_treeview_cabi.exe
```

## Important Notes

- Clone with `--recursive` to get all submodules
- Build scripts automatically call VsDevCmd.bat - do not run from vanilla cmd.exe
- Output directory is `build\{sdl3|sfml}\test\Debug\`, not `build\Debug`
- SDL3 backend DLLs (SDL3.dll, SDL3_ttf.dll, SDL3_image.dll, DebugInfoX64.dll) and assets/layouts folders are auto-copied to output
- SFML backend DLLs and assets are auto-copied to `build\sfml\test\Debug\`
- `NOMINMAX` must appear before any `#include` in files using `std::min`/`std::max` because SDL3 headers pull in `windows.h` transitively

### Backend Compilation Order

- **先编译 SDL3 后端的测试用例**，确保功能正确后再编译其他后端。
- SDL3 编译通过且**经评估审核后**，再启动 SFML 和 Raylib 后端的编译和测试。
- 三后端的差异通常在 RenderDevice、InputBackend 等抽象层，而非控件逻辑本身，因此快速迭代期间只编译 SDL3 即可。

### 构建目录说明

- **标准测试**（非 DLL 模式）：使用 `build/{backend}/` 构建目录

  | 后端   | 构建目录          | 测试输出目录                 |
  | ------ | ----------------- | ---------------------------- |
  | SDL3   | `build/sdl3/`     | `build/sdl3/test/Debug/`     |
  | SFML   | `build/sfml/`     | `build/sfml/test/Debug/`     |
  | Raylib | `build/raylib/`   | `build/raylib/test/Debug/`   |
  | 构建   | `cmake --build build/sdl3 --target test_treeview` | |

- **DLL 模式集成测试**（C ABI fromsource 测试，如 `test_*_cabi`）：使用 `build/{backend}_dll/` 构建目录

  | 后端   | 构建目录              | 测试输出目录                         |
  | ------ | --------------------- | ------------------------------------ |
  | SDL3   | `build/sdl3_dll/`     | `build/sdl3_dll/test/Debug/`         |
  | SFML   | `build/sfml_dll/`     | `build/sfml_dll/test/Debug/`         |
  | Raylib | `build/raylib_dll/`   | `build/raylib_dll/test/Debug/`       |
  | 构建   | `cmake --build build/sdl3_dll --target test_treeview_cabi` | |

- `build/{backend}_dll/` 目录与 `build/{backend}/` 是**两个独立的 CMake 构建树**，不可混用。
- 构建命令示例：

  ```batch
  build_scripts\build.bat sdl3     # → build/sdl3/       （标准测试，正确）
  build_scripts\build.bat sfml     # → build/sfml/       （标准测试，正确）
  build_scripts\build.bat raylib   # → build/raylib/     （标准测试，正确）
  ```
- 如果不确定当前构建输出目录，检查 `build/` 下各子目录中的 `test/Debug/`。

### Raylib Backend Notes

- `EndDrawing()` is never called — `present()` manually flushes + swaps
- `PollInputEvents()` is called only in `InputBackend::newFrame()`, once per frame
- `SetTargetFPS` is not used — frame rate limited by `WaitTime` in `RenderDevice::present()`
- Fonts loaded at `size * 96/72` and cached by `(data, size, cpHash)`
