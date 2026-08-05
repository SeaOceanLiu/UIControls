# UICornerstone 用户开发教程

## 1. 概述

UICornerstone 是一个轻量级 C/C++ GUI 库，支持三后端（SDL3/SFML/Raylib），提供声明式（JSON 布局）和命令式（C ABI 工厂函数）两种 UI 定义方式，以及四种集成模式：

| 模式 | 示例 | 适用场景 |
|------|------|----------|
| 完全静态链接 | hello_uicornerstone | 独立 exe，零外部依赖 |
| 命令式 C ABI | sample_programmatic | 不想用 JSON，偏好代码创建 UI |
| 混合集成（核心 DLL + 后端源码） | sample_fromsource | 核心控件封装为 DLL，后端编译进 exe |
| 显式 LoadLibrary | sample_loadlibrary | 完全运行时加载，零导入库依赖 |

## 2. 环境准备

### 2.1 克隆仓库

```batch
git clone --recursive https://github.com/SeaOceanLiu/UICornerstone.git
cd UICornerstone
```

`--recursive` 是必须的，会拉取所有子模块（SDL3、SFML、raylib、json、assets、libs）。

### 2.2 构建库

#### SDL3 后端（默认，推荐）

```batch
build_scripts\build.bat sdl3
```

输出：`build/sdl3/Debug/UICornerstone.lib`（静态库）或 `build/sdl3_dll/Debug/UICornerstone.dll`（动态库）。

#### SFML / Raylib 后端

```batch
build_scripts\build.bat sfml
build_scripts\build.bat raylib
```

### 2.3 目录结构

```
include/              ← 公有头文件（UICornerstoneAPI.h 等）
src/                  ← 核心源码
  backend/            ← 后端实现（sdl3/ sfml/ raylib/）
build/
  sdl3/               ← SDL3 静态构建
  sdl3_dll/           ← SDL3 DLL 构建
  sfml/               ← SFML 静态构建
  sample/             ← 示例输出
subModules/           ← 子模块（SDL3、SFML、raylib 等）
assets/               ← 字体、图片资源
```

## 3. 第一个应用：声明式 UI（JSON 布局）

### 3.1 概念

通过 JSON 描述 UI 控件树，`UICornerstone_LoadLayout` 自动解析并创建控件。适合布局较复杂的场景。

### 3.2 代码

```c
#include "UICornerstoneAPI.h"
#include <stdio.h>

static int g_clickCount = 0;
static UIInstance g_inst;

static void onBtnClick(UIControlHandle ctl, void* user) {
    (void)ctl; (void)user;
    g_clickCount++;
    char buf[64];
    snprintf(buf, sizeof(buf), "Clicked: %d", g_clickCount);
    UIControlHandle status = UICornerstone_FindControl(g_inst, "status");
    if (status) UICornerstone_SetString(g_inst, status, "caption", buf);
}

static const char* LAYOUT =
"{"
"  \"controls\": [{"
"    \"type\": \"Panel\", \"id\": \"root\","
"    \"rect\": {\"x\":0,\"y\":0,\"w\":800,\"h\":480},"
"    \"children\": ["
"      {\"type\":\"Label\",\"id\":\"title\","
"       \"rect\":{\"x\":20,\"y\":10,\"w\":760,\"h\":30},"
"       \"font\":{\"size\":18},\"caption\":\"UICornerstone Sample\"},"
"      {\"type\":\"Button\",\"id\":\"btn\",\"caption\":\"Click Me\","
"       \"rect\":{\"x\":20,\"y\":60,\"w\":200,\"h\":80},"
"       \"colors\":{\"background\":{\"normal\":\"#4A90D9\",\"hover\":\"#5BA0E9\",\"pressed\":\"#3A80C9\"}},"
"       \"events\":{\"onClick\":\"onBtnClick\"}},"
"      {\"type\":\"Label\",\"id\":\"status\","
"       \"rect\":{\"x\":20,\"y\":160,\"w\":400,\"h\":24},"
"       \"caption\":\"Click the button above\"}"
"    ]}"
"  }]}";

int main(void) {
    g_inst = UICornerstone_CreateInstanceFromPlugin("sdl3", NULL);
    if (!g_inst) return 1;
    UICornerstone_SetViewport(g_inst, 0, 0, 800, 480);
    UICornerstone_RegisterAction(g_inst, "onBtnClick", onBtnClick, NULL);
    if (!UICornerstone_LoadLayout(g_inst, LAYOUT)) {
        UICornerstone_DestroyInstance(g_inst); return 1;
    }
    while (!UICornerstone_IsQuitRequested(g_inst)) {
        UICornerstone_ProcessEvents(g_inst);
        UICornerstone_Update(g_inst, 1.0 / 60.0);
        UICornerstone_Clear(g_inst);
        UICornerstone_Render(g_inst);
        UICornerstone_Present(g_inst);
    }
    UICornerstone_DestroyInstance(g_inst);
    return 0;
}
```

### 3.3 构建

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyApp C)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /utf-8")

add_executable(myapp myapp.c)
target_include_directories(myapp PRIVATE "path/to/UICornerstone/include")
target_link_directories(myapp PRIVATE "path/to/UICornerstone/build/sdl3/lib/Debug")
target_link_libraries(myapp UICornerstone SDL3 SDL3_ttf SDL3_image)

# 复制 DLL + assets
add_custom_command(TARGET myapp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "path/to/UICornerstone/build/sdl3/test/Debug/" $<TARGET_FILE_DIR:myapp>)
```

### 3.4 运行

```batch
cd build/myapp/Debug/
myapp.exe
```

窗口显示一个带标题、按钮和状态标签的界面。点击按钮更新计数。

## 4. 进阶：命令式 UI（C ABI 工厂函数）

### 4.1 概念

不需要 JSON，直接通过 `UICornerstone_CreateButton`、`UICornerstone_CreateLabel` 等工厂函数创建控件。

### 4.2 代码

```c
#include "UICornerstoneAPI.h"
#include <stdio.h>

static int g_clickCount = 0;
static UIInstance g_inst;
static UIControlHandle g_statusLabel = NULL;

static void onBtnClick(UIControlHandle ctl, void* user) {
    (void)ctl; (void)user;
    g_clickCount++;
    char buf[64];
    snprintf(buf, sizeof(buf), "Clicked: %d", g_clickCount);
    if (g_statusLabel)
        UICornerstone_SetString(g_inst, g_statusLabel, "caption", buf);
}

int main(void) {
    g_inst = UICornerstone_CreateInstanceFromPlugin("sdl3", NULL);
    if (!g_inst) return 1;
    UICornerstone_SetViewport(g_inst, 0, 0, 800, 480);

    UIControlHandle root = UICornerstone_CreatePanel(g_inst, 0, 0, 800, 480);
    UIControlHandle title = UICornerstone_CreateLabel(
        g_inst, "My App (Programmatic)", 18, 20, 10, 760, 30);
    UICornerstone_AddChildControl(g_inst, root, title);

    UIControlHandle btn = UICornerstone_CreateButton(
        g_inst, "Click Me", 20, 60, 200, 80);
    UICornerstone_SetColor(g_inst, btn, "background", (UIColor){74, 144, 217, 255});
    UICornerstone_SetCallback(g_inst, btn, "click", onBtnClick, NULL);
    UICornerstone_AddChildControl(g_inst, root, btn);

    g_statusLabel = UICornerstone_CreateLabel(
        g_inst, "Click the button above", 14, 20, 160, 400, 24);
    UICornerstone_AddChildControl(g_inst, root, g_statusLabel);

    while (!UICornerstone_IsQuitRequested(g_inst)) {
        UICornerstone_ProcessEvents(g_inst);
        UICornerstone_Update(g_inst, 1.0 / 60.0);
        UICornerstone_Clear(g_inst);
        UICornerstone_Render(g_inst);
        UICornerstone_Present(g_inst);
    }
    UICornerstone_DestroyInstance(g_inst);
    return 0;
}
```

### 4.3 关键 API

| 函数 | 作用 |
|------|------|
| `UICornerstone_CreateInstanceFromPlugin("sdl3", NULL)` | 加载后端插件并创建实例 |
| `UICornerstone_CreatePanel/CreateButton/CreateLabel` | 创建控件（首参为实例句柄） |
| `UICornerstone_AddChildControl(inst, parent, child)` | 挂接父子关系 |
| `UICornerstone_SetString(inst, handle, "caption", text)` | 设置文本（属性系统，见下） |
| `UICornerstone_SetColor(inst, handle, "background", color)` | 设置颜色 |
| `UICornerstone_SetCallback(inst, handle, "click", cb, user)` | 绑定点击事件 |
| `UICornerstone_DestroyInstance(inst)` | 销毁实例并释放全部资源 |

> 旧版 `SetText`/`SetBGColor`/`SetOnClick` 等专用导出已移除，统一由属性系统 API（`SetString`/`SetColor`/`SetBool`/`SetFloat`/`SetCallback`…）替代，属性名与 JSON 字段一致（如 Label 文本用 `"caption"`）。

## 5. 进阶：混合集成（核心 DLL + 后端源码）

### 5.1 概念

将核心控件封装为 `UICornerstone.dll`，后端（Window、RenderDevice 等）编译进 exe。exe 体积小（~272KB），DLL 体积大（~3.2MB）。

### 5.2 关键区别

- 使用 `GetUIBackendCallbacks()` 获取后端回调查表
- 通过 `UICornerstone_CreateInstance(callbacks, NULL)` 创建实例（而非 `CreateInstanceFromPlugin`）
- 需构建时启用 `UICORNERSTONE_BUILD_DLL=ON`

### 5.3 代码

```c
#include "UICornerstoneAPI.h"
#include <stdio.h>

extern UIBackendCallbacks* GetUIBackendCallbacks(void);

int main(void) {
    UIBackendCallbacks* callbacks = GetUIBackendCallbacks();
    UIInstance inst = UICornerstone_CreateInstance(callbacks, NULL);
    if (!inst) return 1;

    UICornerstone_SetViewport(inst, 0, 0, 800, 480);
    // ... 创建控件，帧循环同前
    UICornerstone_DestroyInstance(inst);
    return 0;
}
```

### 5.4 CMake 构建

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyApp C)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /utf-8")

# 后端源码编译进 exe
set(BACKEND_SOURCES
    src/backend/sdl3/Window.cpp
    src/backend/sdl3/RenderDevice.cpp
    src/backend/sdl3/TextRenderer.cpp
    src/backend/sdl3/InputBackend.cpp
    src/backend/sdl3/Cursor.cpp
    src/backend/sdl3/BackendPlugin.cpp)

add_executable(myapp myapp.c ${BACKEND_SOURCES})
target_include_directories(myapp PRIVATE "include" "src/backend")
target_link_libraries(myapp UICornerstone_dll SDL3 SDL3_ttf SDL3_image)
target_compile_definitions(myapp PRIVATE UICORNERSTONE_BUILD_SHARED)
```

## 6. 进阶：显式 LoadLibrary

### 6.1 概念

不链接 `UICornerstone_dll.lib`，完全通过 `LoadLibrary` + `GetProcAddress` 运行时加载。后端源码通过 `#include .cpp` 编译入同一 TU。

### 6.2 关键区别

- C ABI 调用全部通过函数指针（`GetProcAddress`）
- 不定义 `UICORNERSTONE_BUILD_SHARED`，无 `dllimport`
- 内联实现 3 个 Core 符号：`Surface::registerFactories`、`Cursor::registerFactories`（空实现）、`ResourceProvider::createFilesystem`（完整实现）

### 6.3 代码骨架

```cpp
#include <windows.h>
#include "UICornerstoneAPI.h"

// #include 后端 6 个 .cpp（同一 TU）
#include "src/backend/sdl3/Window.cpp"
#include "src/backend/sdl3/RenderDevice.cpp"
#include "src/backend/sdl3/TextRenderer.cpp"
#include "src/backend/sdl3/InputBackend.cpp"
#include "src/backend/sdl3/Cursor.cpp"
#include "src/backend/sdl3/BackendPlugin.cpp"

// 内联 Core 符号（替代导入库）
namespace Surface { void CORE_API registerFactories(...) {} }
namespace Cursor { void CORE_API registerFactories(...) {} }

#include "src/ResourceProvider.cpp"  // FilesystemResourceProvider

ResourceProvider* ResourceProvider::createFilesystem(const string& base) {
    return new FilesystemResourceProvider(base);
}

int main(void) {
    HMODULE dll = LoadLibraryA("UICornerstone.dll");
    // GetProcAddress 解析 C ABI 函数指针：
    //   UICornerstone_CreateInstanceFromPlugin / CreateInstance /
    //   DestroyInstance / SetViewport / LoadLayout / 帧循环五件套 ...
    UIBackendCallbacks* cbs = GetUIBackendCallbacks();
    UIInstance inst = pfnCreateInstanceFromPlugin("sdl3", NULL);  // 或 pfnCreateInstance(cbs, NULL)
    // ... 帧循环 ...
    pfnDestroyInstance(inst);
    FreeLibrary(dll);
    return 0;
}
```

## 7. 后端选择

| 后端 | 优点 | 缺点 |
|------|------|------|
| **SDL3** | 功能最全，主开发后端 | 需 3 个 DLL（SDL3, SDL3_ttf, SDL3_image） |
| **SFML** | C++ 风格 API，跨平台好 | 事件响应略慢于 SDL3 |
| **Raylib** | 单个 DLL，部署简单 | 中文需码点扩展，纹理桥接有已知 issue |

切换后端只需改 CMake 变量，代码无需修改：

```batch
cmake -B build -DUICORNERSTONE_BACKEND=SDL3
cmake -B build -DUICORNERSTONE_BACKEND=SFML
cmake -B build -DUICORNERSTONE_BACKEND=RAYLIB
```

## 8. 帧循环生命周期

所有模式共享相同的帧循环结构（首参均为实例句柄）：

```c
while (!UICornerstone_IsQuitRequested(inst)) {
    UICornerstone_ProcessEvents(inst);   // 处理输入事件
    UICornerstone_Update(inst, dt);      // 更新控件状态
    UICornerstone_Clear(inst);           // 清空渲染目标
    UICornerstone_Render(inst);          // 绘制控件树
    UICornerstone_Present(inst);         // 交换缓冲区
}
```

## 8.1 多实例（C ABI 全面实例化）

UICornerstone 为多实例架构：`UIInstance` 是根句柄（含窗口、控件树、事件队列、焦点与注册表），**可同时创建多个实例**，彼此完全隔离：

- `UICornerstone_CreateInstance(callbacks, config)` — 以回调查表创建（混合集成模式）
- `UICornerstone_CreateInstanceFromPlugin(backendName, config)` — 以后端名创建（自动加载插件，静态/fromsource/LoadLibrary 模式）
- `UICornerstone_DestroyInstance(inst)` — 销毁实例，级联释放其全部控件与资源

实例化后的规则：

- **所有函数首参均为实例句柄**（`CreateButton(inst, ...)`、`LoadLayout(inst, json)`、`FindControl(inst, id)`、`SetString(inst, ctl, prop, val)`…），无全局隐式实例
- 回调（`UIActionCallback`/`UIEventCallback`）不携带实例参数——如需在回调中操作控件，用全局变量保存 `UIInstance` 或经 `userData` 传递
- 两个实例的事件、Action 注册表（`RegisterAction`）、控件 id 互不干扰；`ProcessEvents`/`Update` 分别驱动各自实例
- Debug 构建下，跨实例句柄误用（把 A 实例的控件句柄传给 B 实例）会被归属校验断言捕获

```c
// 双实例示例（sdl3 后端，两个独立窗口）
UIInstance win1 = UICornerstone_CreateInstanceFromPlugin("sdl3", NULL);
UIInstance win2 = UICornerstone_CreateInstanceFromPlugin("sdl3", NULL);
// ... 各自 SetViewport / LoadLayout / 创建控件 ...
while (!UICornerstone_IsQuitRequested(win1) &&
       !UICornerstone_IsQuitRequested(win2)) {
    UICornerstone_ProcessEvents(win1);
    UICornerstone_Update(win1, 1.0 / 60.0);
    UICornerstone_Clear(win1);
    UICornerstone_Render(win1);
    UICornerstone_Present(win1);
    // win2 同上……
}
UICornerstone_DestroyInstance(win1);
UICornerstone_DestroyInstance(win2);
```

> 多实例隔离的完整设计见 `doc/CABI_MultiInstance_Design.md`，验证用例见 `test/test_multi_instance_cabi.cpp`（事件隔离/生命周期/销毁再创建 x100）与 `test/test_multiviewport_cabi.cpp`（多视口 + 键盘导航）。

## 9. 常见问题

### 9.1 中文显示为 ??

- **SDL3 后端**：字体文件需包含 CJK 码点（HarmonyOS Sans SC 等）
- **Raylib 后端**：首次加载只包含 ASCII 码点，但文本变化时会自动扩展（`ensureFontCodepoints`）
- **EditBox**：输入中文时自动调用 `loadFontInternal()` 重新加载字体

### 9.2 纹理/图片不可见

- **SFML**：`Actor::setParent` 会覆盖已有纹理——已在 `src/Actor.cpp` 修复（`&& !m_texture` 保护）
- **Raylib**：`DrawTexturePro` 在 DLL 桥接模式下不可见——改用 `rlPushMatrix + DrawTextureEx`
- **通用**：检查纹理路径是否正确，DLL 运行时路径是否包含 assets

### 9.3 链接错误

- `CORE_API` 在不同编译单元不一致：要么统一定义 `UICORNERSTONE_BUILD_SHARED`（走 `dllimport`），要么全部不定义
- 零导入库模式：必须内联实现 3 个 Core 符号

### 9.4 帧率不佳

- SFML 默认开启 vsync：需在 `Window.cpp` 中调用 `setVerticalSyncEnabled(false)`
- Raylib 帧率由 `WaitTime` 控制：默认 60Hz
- 检查 `present()` 是否阻塞（如 `EndDrawing()` 内部 `PollInputEvents`）

## 10. 下一步

- 阅读 `doc/Sample_Design.md` 了解四种集成模式的架构设计
- 阅读 `samples/` 中的完整示例代码
- 阅读各控件设计文档（`doc/*_Design.md`）了解控件 API
- 阅读 `doc/Build_Guide.md` 获取完整构建参考
