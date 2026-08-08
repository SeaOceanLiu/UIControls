# C++ Binding 用户手册

> 对应 Phase 17 | 编制 2026-08-07 | 状态: **初版** | 配套设计文档：`doc/CppBinding_Design.md`

本手册面向使用 C++ Binding（`binding/` 目录，MIT 许可证）开发 UICornerstone 应用的用户，从零开始讲解配置、控件、事件、循环模式与调试。所有 API 与当前源码一致；与设计文档冲突时以本手册（实机验证）为准。

## 目录

1. [简介与快速上手](#1-简介与快速上手)
2. [构建与运行](#2-构建与运行)
3. [Config 配置详解](#3-config-配置详解)
4. [创建控件](#4-创建控件)
5. [属性系统](#5-属性系统)
6. [事件系统](#6-事件系统)
7. [双模式循环](#7-双模式循环)
8. [布局与动作](#8-布局与动作)
9. [资源路径](#9-资源路径)
10. [输入事件注入](#10-输入事件注入)
11. [高级控件：菜单族 / 滚动条 / 树 / 句柄](#11-高级控件菜单族--滚动条--树--句柄)
12. [多实例与子视口](#12-多实例与子视口)
13. [控件生命周期与安全](#13-控件生命周期与安全)
14. [错误处理](#14-错误处理)
15. [调试与测试](#15-调试与测试)
16. [常见问题 FAQ](#16-常见问题-faq)

---

## 1. 简介与快速上手

C++ Binding 是对 C ABI（`include/UICornerstoneAPI.h`）的类型安全封装：

- 句柄 `void*` → `Control` 类（属性访问、回调绑定）
- C 函数指针回调 → `std::function` lambda
- 字符串属性名 → `PropertyNames.h` 常量
- 事件数据 union → `Event` 具名访问器

**许可与依赖**：Binding 源码为 MIT；编译期仅依赖 `UICornerstoneAPI.h`；**链接期不链接核心库导入库**——核心 DLL 与后端 DLL 均在运行时经 `LoadLibrary` 纯动态加载（exe 中零核心库符号）。

**最小示例**（对应 `binding/samples/sample_cpp_hosted.cpp`）：

```cpp
#include "UICornerstone.h"
#include "Control.h"
#include "Event.h"
#include "PropertyNames.h"

int main() {
    auto ui = UICornerstone::UICornerstone::Create(
        UICornerstone::UICornerstone::Config{}
            .WithBackend("sdl3")
            .WithWindow("My App", 800, 600));
    if (!ui) return 1;   // 创建失败：DLL 缺失或路径配置错误

    auto button = ui->CreateButton("Click Me", 20, 20, 140, 36);
    auto label  = ui->CreateLabel("Hello", 20.0f, 20, 70, 220, 40);

    button.SetCallback(PropertyNames::kEventClick, [&](const Event&) {
        label.SetString(PropertyNames::kCaption, "Clicked!");
    });

    return ui->Run([](double dt) { (void)dt; });   // Hosted 模式
}
```

程序启动时依次 `LoadLibrary`：

1. `UICornerstone.dll`（核心库，导出全部 C ABI 函数）
2. `UIBackend_sdl3.dll`（后端插件，导出 `GetUIBackendCallbacks`）

两个 DLL 与后端运行库（SDL3.dll 等）必须在 exe 同目录或 `Config` 指定目录（见 §3）。

## 2. 构建与运行

### 2.1 依赖与前置

- 已构建核心库 DLL（`UICornerstone.dll`）与后端插件（`UIBackend_<backend>.dll`）
- CMake ≥ 3.16，C++17 编译器（MSVC / GCC / Clang）

### 2.2 构建命令

Binding 位于主工程内时，随主工程一起构建：

```bash
cmake -B build -DUICORNERSTONE_BUILD_DLL=ON -DUICORNERSTONE_BACKEND=SDL3
cmake --build build --config Debug --target sample_cpp_hosted sample_cpp_embed sample_cpp_multiview sample_cpp_multiinstance
```

仅当主工程存在 `UICornerstone` 核心 target 时，Binding samples 才会构建（`binding/CMakeLists.txt:53`）。

### 2.3 运行时文件部署

POST_BUILD 已自动把以下文件拷贝到每个样例 exe 目录（`build/binding/Debug/`）：

| 文件 | 来源 |
|------|------|
| `UICornerstone.dll` | 核心库 target `UICornerstone_dll` |
| `UIBackend_sdl3.dll` | 后端插件 target `UIBACKEND_TARGET` |
| `SDL3.dll` 等后端运行库 | `BACKEND_DLLS` 变量 |
| `assets/` 资源目录 | `subModules/assets` |

自己的项目可参考 `binding/CMakeLists.txt` 的 `add_custom_command(TARGET ... POST_BUILD ...)` 模式，或直接把 DLL 复制到 exe 目录。

### 2.4 独立使用 Binding（自己的项目）

```cmake
add_subdirectory(binding)
target_link_libraries(my_app PRIVATE UICornerstoneBinding)
```

`UICornerstoneBinding` 是静态包装库，内部不链接任何核心库导入库；你的 exe 只需在运行期提供 DLL。

## 3. Config 配置详解

`UICornerstone::UICornerstone::Config` 是唯一的创建参数（`binding/include/UICornerstone.h`）：

| 字段 | 默认值 | 含义 |
|------|--------|------|
| `backend` | `"sdl3"` | 后端名：`"sdl3"` / `"sfml"` / `"raylib"` |
| `backendSearchPath` | 空 | 后端 DLL 搜索目录（空 = exe 同目录 / 系统搜索） |
| `coreLibraryDir` | 空 | 核心 DLL 所在目录（空 = exe 同目录 / 系统搜索） |
| `resourceRoot` | 空（→ 核心默认 exe 目录/assets） | 资源根路径（§9） |
| `windowTitle` | `"UICornerstone"` | 窗口标题 |
| `windowWidth` / `windowHeight` | 1024 / 768 | 窗口尺寸（0 或负值 → 核心库默认） |
| `windowFlags` | 0 | 跨后端窗口标志，值对齐 SDL_WINDOW_*（§3.3） |

### 3.1 链式配置

```cpp
auto config = UICornerstone::UICornerstone::Config{}
    .WithBackend("sdl3")
    .WithBackendSearchPath("./plugins")   // UIBackend_sdl3.dll 所在目录
    .WithCoreLibraryDir("./runtime")      // UICornerstone.dll 所在目录
    .WithResourceRoot("./my_assets")
    .WithWindow("Game HUD", 1280, 720)
    .WithWindowFlags(0x00000020);         // SDL_WINDOW_RESIZABLE
```

### 3.2 目录语义（关键）

两个路径字段都是**目录**，内部拼出完整 DLL 文件名：

| 字段 | 空（默认） | 非空 |
|------|-----------|------|
| `coreLibraryDir` | `LoadLibrary("UICornerstone.dll")`（exe 同目录 / 系统搜索） | `LoadLibrary(dir + "/UICornerstone.dll")` |
| `backendSearchPath` | `LoadLibrary("UIBackend_sdl3.dll")` | `LoadLibrary(dir + "/UIBackend_sdl3.dll")` |

典型部署（exe 与 DLL 分离时）：

```
my_game/
├── my_game.exe
├── runtime/
│   ├── UICornerstone.dll
│   ├── UIBackend_sdl3.dll
│   └── SDL3.dll
└── assets/
```

```cpp
UICornerstone::UICornerstone::Config{}
    .WithCoreLibraryDir("./runtime")
    .WithBackendSearchPath("./runtime")
    .WithResourceRoot("./assets");
```

### 3.3 windowFlags 取值

| 标志（十六进制） | 含义 | 生效后端 |
|-----------------|------|---------|
| `0x00000020` | 窗口可调整大小 | SDL3 |
| `0x00002000` | 高 DPI 支持 | SDL3 |
| 其他 | 透传（SFML/raylib 通常忽略） | — |

## 4. 创建控件

所有工厂方法挂在 `UICornerstone` 对象上，返回 `Control`（值类型，拷贝共享同一底层状态）。坐标单位为像素，参数顺序均为 `(x, y, w, h)`（左上角 + 宽高）。

### 4.1 基础控件

```cpp
auto btn  = ui->CreateButton("OK", 20, 20, 140, 36);
auto lbl  = ui->CreateLabel("Text", 20.0f, 20, 70, 220, 40);      // (文本, 字号, x,y,w,h)
auto cb   = ui->CreateCheckBox("Agree", 20, 120, 160, 30);
auto edit = ui->CreateEditBox(20, 160, 240, 32);
auto bar  = ui->CreateProgressBar(20, 210, 260, 24);
auto sld  = ui->CreateSlider(20, 250, 260, 32, 0.f, 100.f, 50.f); // (min, max, value)
auto panel = ui->CreatePanel(0, 0, 800, 600);
auto ta   = ui->CreateTextArea(20, 300, 400, 120);
```

### 4.2 复合控件

```cpp
auto win   = ui->CreateWinFrame("Title", 50, 50, 300, 200);
auto combo = ui->CreateComboBox(50, 60, 180, 30);
auto pick  = ui->CreateColorPicker(50, 100, 180, 30, "#ff8800");  // 初始颜色字符串
auto nud   = ui->CreateNumericUpDown(50, 140, 120, 30);
auto split = ui->CreateSplitter(50, 180, 8, 200, 0);              // 0=水平 1=垂直
```

### 4.3 图片与动画

```cpp
auto imgBtn = ui->CreateImageButton("btn.png", "btn_hover.png", "btn_pressed.png",
                                    20, 20, 100, 40);
auto img    = ui->CreateImage("hero.png", 20, 80, 128, 128);
auto anim   = ui->CreateAnimation("idle.jsonc", 20, 220, 256, 256);  // LuotiAni 动画
```

图片/动画路径相对 `resourceRoot` 解析（§9）。

### 4.4 对话框

```cpp
auto dlg = ui->CreateDialog("Confirm", "Cancel", 200, 150, 400, 200);
dlg.SetCallback(PropertyNames::kEventConfirm, [&](const Event&) { /* 确认 */ });
dlg.SetCallback(PropertyNames::kEventCancel,  [&](const Event&) { /* 取消 */ });
```

## 5. 属性系统

所有属性以**字符串名**访问，与 JSON 布局字段同名。常量见 `PropertyNames.h`（构建时自动复制到构建目录 `include/`，直接 `#include "PropertyNames.h"`）。

### 5.1 设置 / 读取

```cpp
// 设置（一一对应 C ABI）
ctl.SetColor("background", {74, 144, 217, 255});   // UIColor{r,g,b,a}
ctl.SetStateColor("background", {normal, hover, pressed, disabled});  // UIStateColor 四态
ctl.SetBool("visible", true);
ctl.SetInt("value", 42);
ctl.SetFloat("value", 3.14f);
ctl.SetString("caption", "New Text");
ctl.SetEnum("orientation", "horizontal");
ctl.SetPtr("user-data", myPointer);

// 读取
UIColor c = ctl.GetColor("background");
bool    v = ctl.GetBool("visible");
int     i = ctl.GetInt("value");
float   f = ctl.GetFloat("value");
std::string s = ctl.GetString("caption");     // 控件不支持/属性不存在 → 空串
std::string e = ctl.GetEnum("orientation");
void*   p = ctl.GetPtr("user-data");
```

### 5.2 常用属性名（`PropertyNames.h` 常量）

| 常量 | 字符串值 | 适用 |
|------|---------|------|
| `kCaption` | `"caption"` | 文本（Label/Button/CheckBox 等） |
| `kBackground` | `"background"` | 背景色（StateColor 四态） |
| `kBorder` / `kText` | `"border"` / `"text"` | 边框色 / 文本色 |
| `kVisible` / `kEnabled` | `"visible"` / `"enabled"` | 通用布尔 |
| `kValue` | `"value"` | Slider/ProgressBar 数值（float） |
| `kJsonMin` / `kJsonMax` / `kStep` | `"min"` / `"max"` / `"step"` | 数值区间 |
| `kSelectedIndex` | `"selected-index"` | ComboBox 选中索引 |
| `kCheckState` | `"check-state"` | CheckBox 状态 |
| `kOrientation` | `"orientation"` | 滚动条/分割条方向 |

> 全部常量以 `include/PropertyNames.h` 为准（约 590 个）；控件不支持某个属性时，Set 静默失败、Get 返回空值/0——不会崩溃。

### 5.3 通用操作

```cpp
ctl.SetRect(x, y, w, h);          // 位置尺寸
UIRect r = ctl.GetRect();
ctl.AddChild(child);              // 挂到父控件（Panel 等容器）
ctl.Destroy();                    // 销毁（见 §13）
std::string id = ctl.GetId();     // 控件 id（空 = 未设置）
bool ok = ctl.IsValid();
UIControlHandle raw = ctl.Handle();  // 回退到裸 C ABI 时使用
```

## 6. 事件系统

回调签名：`std::function<void(const Event&)>`，经 `Control::SetCallback(eventName, callback)` 注册。**事件名用 `PropertyNames.h` 的 `kEvent*` 常量**。

```cpp
button.SetCallback(PropertyNames::kEventClick, [&](const Event& e) {
    // 事件数据经 Event 具名访问器读取
});
```

### 6.1 事件名一览

| 常量 | 事件名 | 触发控件 | 数据访问器 |
|------|--------|---------|-----------|
| `kEventClick` | `"click"` | Button/MenuItem | — |
| `kEventValueChanged` | `"value-changed"` | Slider/ProgressBar | `GetValueChanged()` (float) |
| `kEventValueChanged` | 同上 | NumericUpDown | `GetValueChangedDouble()` (double) |
| `kEventTextChanged` | `"text-changed"` | EditBox/TextArea | `GetTextChanged()` |
| `kEventSelectionChanged` | `"selection-changed"` | ComboBox | `GetSelectedIndex()` / `GetSelectedValue()` |
| `kEventCheckChanged` | `"check-changed"` | CheckBox | `GetCheckState()` |
| `kEventColorChanged` | `"color-changed"` | ColorPicker | 恒 false——用 `GetColor` 轮询 |
| `kEventPositionChanged` | `"position-changed"` | ScrollBar | `GetPositionChanged()` (float) |
| `kEventMoved` | `"moved"` | Splitter | `GetMovedPosition()` (float) |
| `kEventConfirm` / `kEventCancel` | `"confirm"` / `"cancel"` | Dialog | — |
| `kEventClose` | `"close"` | Dialog/Popup | `GetCloseResult()` |
| `kEventEnter` | `"enter"` | EditBox（回车） | — |
| `kEventSelect` | `"select"` | TreeView | `GetNodeId()` / `GetNodeUserData()` |
| `kEventExpand` / `kEventCollapse` | `"expand"` / `"collapse"` | TreeView | 同上 |

### 6.2 判断与读取

```cpp
slider.SetCallback(PropertyNames::kEventValueChanged, [](const Event& e) {
    if (e.IsValueChanged()) {
        float v = e.GetValueChanged();
    }
});
```

`Event` 提供三组接口（`binding/include/Event.h`）：
- **具名访问器**：`IsValueChanged()` + `GetValueChanged()` 等，编译期类型安全
- **通用原始访问**：`GetIntVal()` / `GetFloatVal()` / `GetDoubleVal()` / `GetStrVal()` / `GetPtrVal()`
- **原始数据**：`Raw()` / `GetNameRaw()`

## 7. 双模式循环

### 7.1 Hosted 模式（推荐，UI 托管循环）

`Run(update, onRender)` 内部完成「事件处理 → 更新 → 渲染 → 呈现」主循环，你的代码只提供每帧回调：

```cpp
int exitCode = ui->Run(
    [](double dt) { updateGame(dt); },   // 每帧逻辑更新，dt 单位秒
    []() { /* 每帧 UI 渲染后钩子（可选） */ }
);
// 窗口关闭后返回 0
```

**dt 约定**：首帧 dt ≈ 0；单帧 dt 上限 0.1 秒（调试断点恢复后不会暴增）。需要处理 dt=0 的首帧。

### 7.2 Embedded 模式（用户托管循环）

把 UI 嵌进你自己的游戏/应用循环：

```cpp
while (!ui->IsQuitRequested()) {
    ui->ProcessEvents();        // 1. 处理 UI 事件（不阻塞）
    updateGame(dt);             // 2. 游戏逻辑
    ui->Update(dt);             // 3. UI 更新（布局、动画、重复按键）
    renderGame();               // 4. 游戏渲染
    ui->Clear();                // 5. 清屏（顺序可选）
    ui->Render();               //    UI 渲染
    ui->Present();              //    交换缓冲
    limitFramerate(60);
}
ui->Shutdown();                 // 销毁实例
```

### 7.3 实例生命周期

- `Create()` 即完成初始化（无需独立 Init）；返回 `nullptr` 表示失败（§14）
- `~UICornerstone()` 自动销毁实例（`DestroyInstance`）并释放后端 DLL 句柄
- `Shutdown()` 可提前销毁；之后对象仍可安全析构（幂等）

## 8. 布局与动作

### 8.1 JSON 布局加载

```cpp
bool ok = ui->LoadLayout(jsonContent);       // 字符串形式
bool ok = ui->LoadLayoutFromFile("ui/main.json");   // 相对 resourceRoot（§9）
```

### 8.2 查找控件

```cpp
auto ctl = ui->FindControl("start_button");  // JSON 中 id 字段
if (ctl.IsValid()) { /* 操作 */ }
```

### 8.3 布局动作注册

JSON 布局中的动作（onClick 等）经 `RegisterAction` 绑定：

```cpp
ui->RegisterAction("start_game", [](Control ctl) {
    startGame();
});
```

## 9. 资源路径

所有资源相对 `resourceRoot` 解析，拼接规则 `resourceRoot + "/" + 相对路径`：

```
resourceRoot = "my_assets"               // 默认空串 → 核心库用 exe 目录/assets
"btn.png"            → my_assets/btn.png
"ui/icons/btn.png"   → my_assets/ui/icons/btn.png
"layouts/main.json"  → my_assets/layouts/main.json
```

> **默认值**：`Config::resourceRoot` 默认空串——核心库回退到 exe 目录下 `assets/`（任意工作目录均可运行，不依赖 cwd）。需自定义资源目录时在 `Create` 前设置（`WithResourceRoot(...)`）。

- 字体路径在核心库常量表中已含 `"fonts/"` 前缀，无需用户指定
- 运行期查询：`ui->GetResourceRoot()` / `ui->ResolveResource("btn.png")`
- `SetResourceRoot()` 只改 Binding 侧路径拼接（`LoadLayoutFromFile` 等），**不**影响已创建实例的核心库 Provider——若需整体重定向，应在 `Create` 前设好 `Config::resourceRoot`

## 10. 输入事件注入

外部输入系统（自定义引擎输入层）可把事件注入 UI：

```cpp
#include "UIEventFactory.h"

ui->PushEvent(UICornerstone::Input::MouseButton(1, 100.f, 200.f, true));  // 按下
ui->PushEvent(UICornerstone::Input::MouseMove(150.f, 220.f));
ui->PushEvent(UICornerstone::Input::MouseWheel(0.f, 1.f, 150.f, 220.f));
ui->PushEvent(UICornerstone::Input::Key(SDLK_SPACE, 0, true));
ui->PushEvent(UICornerstone::Input::TextInput("hello"));
```

便捷包装（同一效果）：

```cpp
ui->PushMouseButton(1, 100.f, 200.f, true);
ui->PushMouseMove(150.f, 220.f);
ui->PushMouseWheel(0.f, 1.f, 150.f, 220.f);
ui->PushKey(SDLK_SPACE, 0, true);
ui->PushTextInput("hello");    // ≤ UI_TEXT_MAX=32 字节
```

## 11. 高级控件：菜单族 / 滚动条 / 树 / 句柄

### 11.1 菜单族（组装顺序）

```cpp
auto bar   = ui->CreateMenuBar(0, 0, 400, 24);
auto file  = ui->CreateMenuPanel();
auto open  = ui->CreateMenuItem("Open", 0);       // 0=Normal 1=Separator 2=SubMenu
auto recent = ui->CreateMenuPanel();
ui->MenuPanelAddItem(file, open);
ui->MenuItemSetSubMenu(open, recent);             // 子菜单
ui->MenuBarAddMenu(bar, "File", file);
```

辅助操作：`MenuPanelAddItem(panel, item)`、`MenuPanelAddSeparator(panel)`。

### 11.2 滚动条 / 树 / 句柄

```cpp
auto sb  = ui->CreateScrollBar(20, 20, 16, 200, 0);   // 0=水平 1=垂直
auto tree = ui->CreateTreeView(20, 40, 300, 240);
auto hdl = ui->CreateHandleControl(target, 400, 200, 24, 24);  // 拖动 target 的句柄
```

## 12. 多实例与子视口

### 12.1 多实例（独立窗口）

每个 `Create()` 产生独立窗口/实例，可并存（游戏窗口 + 工具窗口）。Hosted 模式各自 `Run(...)` 独立事件循环；Embedded 模式需**手动驱动所有实例直到队列空**（每实例只消费自己窗口的事件，窗口间互不串扰）：

```cpp
auto uiA = UICornerstone::UICornerstone::Create(configA);
auto uiB = UICornerstone::UICornerstone::Create(configB);

while (!uiA->IsQuitRequested() && !uiB->IsQuitRequested()) {
    int processed;
    do {                                   // 内层：驱动所有实例直到全局队列空
        processed = 0;
        if (uiA->ProcessEvents()) processed = 1;
        if (uiB->ProcessEvents()) processed = 1;
    } while (processed > 0);

    uiA->Update(dt);  uiB->Update(dt);     // 各实例独立更新/渲染
    uiA->Render();    uiB->Render();
    uiA->Present();   uiB->Present();
}
```

> `ProcessEvents()` 返回 `bool`（是否处理了 ≥1 个事件）；单窗口场景忽略返回值即可，行为与旧版一致。跨窗口隔离（鼠标/hover/焦点/键盘）由核心库与 sdl3 后端按窗口 ID 过滤，Binding 无感知。参考样例：`binding/samples/sample_cpp_multiinstance.cpp`（双窗口双向通信，`UICORN_AUTO=1` 冒烟验证）。

**能力位查询（后端差异，必读）**：多窗口渲染前先查询后端能力 `uiA->GetBackendCapabilities()`，与 `UICORN_BACKEND_CAP_MULTI_WINDOW` 按位与后再决定是否渲染第二个实例：

```cpp
uint32_t caps = uiA->GetBackendCapabilities();
bool multiWindow = (caps & UICORN_BACKEND_CAP_MULTI_WINDOW) != 0;
...
if (multiWindow) { uiB->Clear(); uiB->Render(); uiB->Present(); }
```

原因：**raylib 后端是单窗口架构**（内部全局只跟踪最近创建的窗口，且预编译 DLL 无源码不可修补）。它声明的能力位**不含 MULTI_WINDOW**——多实例下只有第一个实例有真实窗口，其余实例为 headless（`Window::isHeadless()`）。此时若照常渲染第二实例，内容会串扰到主实例窗口（A/B 交替闪动）。sdl3/sfml 具备完整四能力（`MULTI_WINDOW|RENDER_TARGET|CLIP_RECT|READBACK`），双窗口渲染不受限。详见 `doc/BackendAbstraction_Design.md` §20。

### 12.2 子视口（同一窗口内的多个视图）

`CreateViewport(x, y, w, h)` 在 owner 窗口内划分一个子视图区域——**共享后端与渲染设备，但拥有独立的控件树、事件队列与焦点管理**：

```cpp
auto vp = ui->CreateViewport(0, 0, 400, 300);   // 左上象限
// vp 拥有独立控件树；析构时只销毁视口，不销毁 owner
```

关键约束：

- **只能从 owner 创建**：`ui->CreateViewport(...)`；视口对象上不能再创建视口
- **控件归属**：视口实例上的 `CreateXxx`/`FindControl` 操作的是视口自己的控件树；`CreateDialog` 弹窗也挂在视口的 bench 上
- **坐标是视口本地坐标**：视口内控件的 `(x, y)` 相对视口左上角（控件树坐标链
  `getDrawRect()` 最终加上视口偏移得到窗口绝对坐标）；窗口输入按全局坐标自动
  路由到对应视口，**事件坐标保持窗口绝对坐标分发**（控件命中测试与绘制均基于
  绝对坐标）
- **事件注入直达**：`vp->PushMouseButton(...)` 等用**窗口绝对坐标**（视口本地坐标
  + 视口偏移，如 vpB 在 (400,300) 时按钮位于 (412,382)），事件进视口自己的队列
  （不按坐标路由）；若注入到 owner，事件只进 owner 队列，不会转到视口
- **必须显式驱动**：`ui->Run()` 只驱动 owner 自己的帧——**子视口需要各自调用 `ProcessEvents()` / `Update(dt)` / `Render()`**（Render 内部按视口区域裁剪）
- **渲染顺序**：先 `ui->Clear()` 清整窗，再依次 `vpA->Render(); vpB->Render();`，最后 `ui->Present()`

### 12.3 例程：一个窗口内两个 Bench（sample_cpp_multiview）

`binding/samples/sample_cpp_multiview.cpp`——左上、右下各一个 Bench（子视口），每个 Bench 内是 Label（caption 标明 Bench A/B）、EditBox、Button；按下 Button 读取本 Bench EditBox 的内容，用弹窗显示：

```
┌────────────────────┐
│ Bench A (视口1)     │  ← CreateViewport(0, 0, 400, 300)
│ Label/EditBox/Button│
├────────────────────┤
│                    │
│    Bench B (视口2)  │  ← CreateViewport(400, 300, 400, 300)
└────────────────────┘
```

```cpp
struct Bench {
    ::UICornerstone::UICornerstone* vp = nullptr;   // 子视口实例
    std::string name;                               // "Bench A" / "Bench B"
    Control edit;

    void Build(const char* initialText) {
        auto label = vp->CreateLabel(name, 20.0f, 12, 10, 200, 24);   // caption 标明 Bench
        edit = vp->CreateEditBox(12, 42, 220, 28);
        edit.SetString(PropertyNames::kTextContent, initialText);

        auto btn = vp->CreateButton("Show", 12, 82, 100, 30);
        btn.SetCallback(PropertyNames::kEventClick, [this](const Event&) {
            // 读 EditBox 内容 → 弹窗显示
            std::string content = edit.GetString(PropertyNames::kTextContent);
            auto dialog = vp->CreateDialog("OK", "", 0, 0, 280, 120);   // 居中弹窗
            auto label  = vp->CreateLabel(content, 14.0f, 20, 30, 240, 60);  // 内容 Label
            dialog.AddChild(label);   // Label 坐标相对 Dialog 本地
        });
    }
};

int main() {
    auto ui = UICornerstone::UICornerstone::Create(
        UICornerstone::UICornerstone::Config{}
            .WithBackend("sdl3")
            .WithWindow("Multi-Viewport Sample", 800, 600));

    auto vpA = ui->CreateViewport(0, 0, 400, 300);        // 左上 Bench A
    auto vpB = ui->CreateViewport(400, 300, 400, 300);    // 右下 Bench B

    Bench benchA(vpA.get(), "Bench A");
    Bench benchB(vpB.get(), "Bench B");
    benchA.Build("Hello from Bench A");
    benchB.Build("Hello from Bench B");

    // ── 主循环：多视口必须显式驱动（Run() 只驱动 owner）──
    while (!ui->IsQuitRequested()) {
        ui->ProcessEvents();      // 窗口输入轮询（按坐标路由到子视口）
        vpA->ProcessEvents();     // 子视口注入队列
        vpB->ProcessEvents();

        vpA->Update(dt);
        vpB->Update(dt);
        ui->Clear();
        vpA->Render();            // 各自 clip 到自身视口区域
        vpB->Render();
        ui->Present();
    }
    ui->Shutdown();
}
```

要点：

- Label 文本用 `PropertyNames::kCaption`（`"caption"`）；EditBox 内容读写用 `PropertyNames::kTextContent`（`"text"`）
- 弹窗：`CreateDialog(confirmText, cancelText, x, y, w, h)`（`x=y=0` 自动居中）——确认/取消按钮自动生成，点击确认（`kEventConfirm`）或取消（`kEventCancel`）自动关闭；取消文本传空串则为纯信息弹窗（只保留确认按钮）；弹窗内容须自行创建 Label 并 `dialog.AddChild(label)`（Label 坐标相对 Dialog 本地）
- 自动冒烟：`UICORN_AUTO=1` 时给两个视口分别注入按钮点击（窗口绝对坐标：
  vpA 按钮在 (12,82) 起、vpB 按钮在 (412,382) 起），验证 `popup: Hello from
  Bench A/B` 输出后 240 帧干净退出（exit=0）

### 12.4 后端配置

```cpp
ui->SetBackendConfig("vsync", "true");
ui->SetBackendConfigBool("vsync", true);
bool vsync = false;
ui->GetBackendConfigBool("vsync", vsync);
```

## 13. 控件生命周期与安全

### 13.1 Control 语义

- `Control` 是值类型，内部 `shared_ptr<ControlState>`——拷贝共享同一控件状态
- `IsValid()`：控件未被销毁时为 true
- 核心库自动销毁控件后（如 Popup close），`Control` 变成悬挂态：所有 Set/Get/SetCallback **静默跳过**，不崩溃

### 13.2 Destroy

```cpp
ctl.Destroy();   // 销毁控件（幂等；不级联销毁子控件）
```

### 13.3 回调安全（重要）

回调中捕获的 lambda 由 Binding 内部持有，生命周期与 Impl 解耦——即使创建控件时用的临时 `Control` 已析构，回调仍然安全：

```cpp
ui->CreateButton("OK", 0, 0, 100, 30)
    .SetCallback(PropertyNames::kEventClick, [&](const Event&) { ... });  // 安全
```

同一控件重复 `SetCallback` 只保留最新回调（不堆积）。

### 13.4 线程约束

所有方法必须在**同一线程**调用（`ProcessEvents` / `Update` / `Render` / `Present` / `Shutdown` / 各 Create*）。`Create()` 与析构也必须在同一线程。

## 14. 错误处理

| 场景 | 表现 |
|------|------|
| 核心 DLL 找不到 | `Create()` 返回 `nullptr`；`GetLastError()` 可查描述 |
| 后端 DLL 找不到 | 同上 |
| `CreateInstance` 失败 | 同上（后端 DLL 已自动释放，不泄漏） |
| 控件不存在 / 已销毁 | `FindControl` 返回空 `Control`；`IsValid()==false` |
| 属性设置失败 | 静默（不崩溃） |
| `Run()` 异常退出 | 返回 1（正常关闭返回 0） |

```cpp
auto ui = UICornerstone::UICornerstone::Create(config);
if (!ui) {
    // 常见原因：UICornerstone.dll / UIBackend_sdl3.dll 不在搜索路径
    return 1;
}
```

## 15. 调试与测试

### 15.1 Debug 辅助（`_DEBUG` 构建下有效）

```cpp
int alive = UICornerstone::UICornerstone::DebugGetAliveCount();        // 活跃实例数
auto inst = UICornerstone::UICornerstone::DebugGetAliveInstance(0);    // 按索引取实例
bool focused = UICornerstone::UICornerstone::DebugIsControlFocused(inst, ctl.Handle());
```

Release 构建下 `DebugGetAliveCount()` 返回 0、`DebugGetAliveInstance()` 返回 NULL。

### 15.2 自动冒烟（样例自带）

四个样例支持 `UICORN_AUTO` 环境变量：设置后自动注入鼠标事件并运行 240 帧后干净退出（exit=0），供 CI 回归：

```bash
UICORN_AUTO=1 ./build/binding/Debug/sample_cpp_hosted.exe && echo OK
UICORN_AUTO=1 ./build/binding/Debug/sample_cpp_embed.exe && echo OK
UICORN_AUTO=1 ./build/binding/Debug/sample_cpp_multiview.exe && echo OK
UICORN_AUTO=1 ./build/binding/Debug/sample_cpp_multiinstance.exe && echo OK  # 双向通信注入
```

### 15.3 核心库测试

C ABI 回归测试（`test/test_layout` 等）运行时要加 `auto=<秒>` 参数才自退出：

```bash
./build/Debug/test_layout.exe auto=3
```

无参运行会挂起等待人工交互，是设计行为（`test/TestInstance.h` 的 scheduleAutoQuit）。

## 16. 常见问题 FAQ

**Q：exe 双击后闪退 / Create 返回 nullptr？**
A：`UICornerstone.dll`、`UIBackend_<backend>.dll`、后端运行库（SDL3.dll 等）未随 exe 部署。用 `coreLibraryDir` / `backendSearchPath` 指定目录，或把 DLL 放到 exe 同目录。

**Q：可以同时用 C ABI 和 C++ Binding 吗？**
A：可以。`Control::Handle()` 暴露原始 `UIControlHandle`，同一实例句柄可混合操作。Binding 只是封装，不绕过 C ABI。

**Q：切换后端要重新编译吗？**
A：不用。改 `Config::backend`（如 `"sfml"`）并在部署目录放对应的 `UIBackend_sfml.dll` 即可。

**Q：设置属性没反应？**
A：先确认属性名常量正确（`PropertyNames.h`），且该控件类型支持该属性。Get 返回空值（空串/0/NULL）表示属性不存在或不支持。

**Q：窗口关闭后如何退出？**
A：Hosted 模式 `Run()` 自动返回 0；Embedded 模式循环条件 `!ui->IsQuitRequested()` 退出后调 `ui->Shutdown()`。

**Q：lib 里还能看到 UICornerstone 符号吗？**
A：不能。`UICornerstoneBinding.lib` 无任何 `UICornerstone_*` 未定义符号（可用 `nm -u` 验证），全部经 `DynamicApi` 运行时解析。

---

## 附录：API 速查

```cpp
// 生命周期
UICornerstone::Create(config)                  // → unique_ptr<UICornerstone>（nullptr=失败）
ui->Run(update, onRender)                      // Hosted：返回退出码
ui->ProcessEvents() / Update(dt) / Clear() / Render() / Present()   // Embedded
ui->IsQuitRequested() / Shutdown()
ui->GetBackendCapabilities()                   // UICORN_BACKEND_CAP_* 位组合（§12.1）
ui->Handle()                                   // UIInstance 裸句柄

// 控件工厂（返回 Control）
CreateButton / CreateLabel / CreateCheckBox / CreateEditBox / CreateProgressBar
CreateSlider / CreatePanel / CreateTextArea / CreateWinFrame / CreateComboBox
CreateColorPicker / CreateNumericUpDown / CreateSplitter / CreateImageButton
CreateImage / CreateAnimation / CreateDialog
CreateMenuBar / CreateMenuPanel / CreateMenuItem / CreateScrollBar
CreateTreeView / CreateHandleControl(target, x, y, w, h)

// 布局与资源
LoadLayout(json) / LoadLayoutFromFile(path) / FindControl(id) / RegisterAction(name, cb)
SetResourceRoot(path) / GetResourceRoot() / ResolveResource(rel)

// 事件注入
PushEvent(UIEvent) / PushMouseButton / PushMouseMove / PushMouseWheel / PushKey / PushTextInput

// 控件操作（Control）
SetColor / SetStateColor / SetBool / SetInt / SetFloat / SetString / SetEnum / SetPtr
GetColor / GetStateColor / GetBool / GetInt / GetFloat / GetString / GetEnum / GetPtr
SetCallback(event, lambda) / SetRect / GetRect / AddChild / Destroy / GetId
IsValid() / Handle()

// 调试与错误
GetLastError() / DebugGetAliveCount() / DebugGetAliveInstance(i) / DebugIsControlFocused()
```
