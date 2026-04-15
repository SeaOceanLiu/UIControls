# UIControls

基于SDL3的C++17 UI控件库，用于游戏和图形应用。

## 功能特性

- 完整的UI控件系统（Label, Button, CheckBox, EditBox, Menu等）
- 灵活的布局管理
- 丰富的动画支持
- 现代化设计

## 环境要求

- Windows 10+
- Visual Studio 2022
- CMake 3.16+

## 快速开始

### 克隆仓库（包含所有依赖）

```bash
git clone --recursive https://github.com/SeaOceanLiu/UIControls.git
cd UIControls
```

### 编译

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Debug
```

### 运行测试

```bash
cd Debug
test_label.exe
test_menu.exe
test_editbox.exe
test_checkbox.exe
test_progressbar.exe
```

### 单独编译某个测试

```bash
cd UIControls
build_scripts\build_test.bat test_label
```

## 项目结构

```
UIControls/
├── src/              # 源代码
├── include/          # 头文件
├── test/             # 测试用例
├── doc/              # 设计文档
├── build_scripts/   # 编译脚本
└── subModules/      # 依赖（通过submodule管理）
    ├── libs/        # SDL3库
    ├── SDL3/        # SDL3头文件
    ├── SDL3_ttf/    # SDL3_ttf头文件
    ├── DebugTrace/ # 调试库
    └── assets/     # 资源文件（字体、图像等）
```

## 依赖项

- SDL3 (zlib license)
- SDL3_ttf (zlib license)
- DebugTrace (MIT license)
- 字体资源 (SIL OFL license)

## 许可证

- 源码: GNU General Public License v3.0
- SDL3/SDL3_ttf: zlib License
- DebugTrace: MIT License
- 字体: SIL Open Font License

## 作者

SeaOceanLiu
