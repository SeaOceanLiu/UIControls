# CAPI C++ Binding 设计

> 对应 Phase 17 | 编制 2026-07-30 | 状态: **草案**

## 目录

1. [动机](#1-动机)
2. [设计目标](#2-设计目标)
3. [方案分析](#3-方案分析)
   - [3.1 后端选择](#31-后端选择)
   - [3.2 资源路径](#32-资源路径)
   - [3.3 循环嵌入](#33-循环嵌入)
   - [3.4 目录放置与许可证](#34-目录放置与许可证)
4. [整体架构](#4-整体架构)
5. [详细设计](#5-详细设计)
   - [5.1 UICornerstone 主类](#51-uicornerstone-主类)
   - [5.2 Control 代理类](#52-control-代理类)
   - [5.3 事件系统](#53-事件系统)
    - [5.4 资源路径管理](#54-资源路径管理)
    - [5.5 后端管理](#55-后端管理)
    - [5.6 架构约束分析](#56-架构约束分析)
    - [5.7 Control 生命周期管理](#57-control-生命周期管理)
    - [5.8 RegisterAction 实例化重构](#58-registeraction-实例化重构)
    - [5.9 Impl 结构总览](#59-impl-结构总览)
    - [5.10 Create(Config) → Init 全链路（含错误处理）](#510-createconfig--init-全链路含错误处理)
    - [5.11 Hosted Run 内部实现](#511-hosted-run-内部实现)
    - [5.12 Config 校验规则](#512-config-校验规则)
    - [5.13 CMake 构建集成](#513-cmake-构建集成)
6. [文件布局与许可证](#6-文件布局与许可证)
7. [示例程序设计](#7-示例程序设计)
   - [7.1 sample_cpp_hosted](#71-sample_cpp_hosted--在-uicornerstone-循环中嵌入游戏逻辑)
   - [7.2 sample_cpp_embed](#72-sample_cpp_embed--在用户游戏循环中嵌入-uicornerstone)
8. [实施计划](#8-实施计划)
9. [与现有 C ABI 的关系](#9-与现有-c-abi-的关系)

---

## 1. 动机

现有 C API（`UICornerstoneAPI.h`）提供了一组纯 C 接口，可供 C、C#、Python 等语言调用。但对于 C++ 开发者，直接使用 C API 存在以下不足：

| 问题 | 表现 |
|------|------|
| **句柄模式** | `UIControlHandle` 是 `void*`，无类型安全，无 IDE 补全 |
| **字符串属性名** | `SetColor(ctl, "selected", ...)` — 拼错只在运行时暴露 |
| **回调原始** | C 函数指针 + `void* userData`，无 lambda/`std::function` 支持 |
| **生命周期手动** | 手动管理句柄生命周期，无 RAII |
| **零散全局函数** | 所有函数在全局命名空间，无封装，无上下文隔离 |

C++ binding 旨在消除这些摩擦，同时**不破坏**现有 C ABI 的兼容性，并允许用户自由修改 binding 源码。

## 2. 设计目标

1. **零开销抽象** — Binding 层为 inline 转发，不引入额外虚拟化或分配
2. **类型安全** — 结构体属性、枚举、回调均用 C++ 类型表达
3. **可配置后端** — 运行时选择后端，不依赖编译宏
4. **可配置资源** — 用户设定字体、图片、布局、DLL 的搜索路径
5. **双模式循环** — 支持"嵌入用户循环"和"托管用户逻辑"两种集成方式
6. **向后兼容** — Binding 完全基于现有 C ABI `extern "C"` 函数，不改变 C 接口
7. **独立许可证** — Binding 源码放置于独立目录，采用宽松许可证（如 MIT）

## 3. 方案分析

### 3.1 后端选择

**问题**：当前后端在编译时通过 `-DUICORNERSTONE_BACKEND=SDL3|SFML|RAYLIB` 选择。每个后端依赖不同的第三方 SDK，无法同时链接到同一二进制中。能否在运行时选择后端？

**选项对比**：

| 方案 | 运行时切换 | 额外依赖 | 复杂度 | 可行性 |
|------|-----------|---------|--------|--------|
| A. 静态编译多个后端 | ❌ | SDK 符号冲突 | 高 | 不可行 |
| B. 插件 DLL 加载 | ✅ | 需编译后端 DLL | 中 | ✅ 可行 |
| C. 调用方提供回调查表 | ✅ | 无 | 低 | ✅ 可行 |
| D. 条件编译 + 全链接 | ❌ | 链接器冲突 | 高 | 不可行 |

**决策**：C++ Binding 同时支持方案 B 和方案 C。

- **方案 B（自动管理）**：Binding 内置 DLL 加载器，根据 `Config::backend` 自动查找并加载 `UIBackend_{name}.dll`。用户无需关心后端加载细节。
- **方案 C（回调查表）**：用户自行构造 `UIBackendCallbacks`，通过 `Create(callbacks)` 传入 Binding。Binding 不参与后端生命周期管理。

两种方案可共存于同一进程。方案 B 适用于快速集成，方案 C 适用于自定义后端或特殊加载需求。

### 3.2 资源路径

**问题**：`ConstDef::pathPrefix` 硬编码为 `Platform::GetBasePath() + "assets"`，`fontFiles` 映射表中的路径（如 `"fonts/Asul-Bold.ttf"`）相对此根目录。用户无法自定义资源存放位置。

**选项对比**：

| 方案 | 配置粒度 | 运行时修改 | 与现有兼容 | 易理解 |
|------|---------|-----------|-----------|-------|
| A. 单一根路径（推荐） | 粗 | ✅ | ✅ | ✅ |
| B. 多分类子路径 | 中等 | ✅ | ✅ | ❌ 路径重复陷阱 |
| C. 完全自定义 ResourceProvider | 细 | ❌（编译时） | ✅ | ❌ |
| D. 环境变量 | 粗 | ✅ | ❌ 不可控 | ✅ |

**决策**：采用方案 A（单一根路径）。用户只需设置一个 `resourceRoot`，所有资源路径以此为基础解析。`fontFiles` 表中的相对路径保持不变。

核心库内部已有完整的 `ResourceProvider` 抽象，Binding 只需在初始化时传入用户指定的 `resourceRoot`。用户代码：

```cpp
auto config = UICornerstone::Config{}
    .WithResourceRoot("C:/my_game_data");
// fontFiles 中的 "fonts/A.ttf" 解析为 "C:/my_game_data/fonts/A.ttf"
// 布局文件 "layouts/main.json" 解析为 "C:/my_game_data/layouts/main.json"
```

### 3.3 循环嵌入

**问题**：用户既可能希望将 UICornerstone 嵌入自己现有的游戏循环（Embedded），也可能希望让 UICornerstone 托管循环、自己只提供逻辑回调（Hosted）。

**选项对比**：

| 方案 | 循环所属 | 用户代码量 | 控制粒度 | 与 C ABI 关系 |
|------|---------|-----------|---------|--------------|
| A. 仅 Hosted（Run） | UICornerstone | ~5 行 | 低 | 完全封装 |
| B. 仅 Embedded（Tick） | 用户 | ~15 行 | 高 | 近 C ABI |
| C. 双模式（推荐） | 均可 | ~5-15 行 | 可调 | 同 C ABI |

**决策**：采用方案 C，双模式共存。Hosted 模式在内部调用 Embedded 模式的 tick API，两者共享同一实现核心。

### 3.4 目录放置与许可证

**问题**：C++ Binding 的 License 应与核心库不同。核心库是 GPL v3.0，Binding 源码需要更宽松的许可证以允许用户自由修改和集成。

**选项对比**：

| 方案 | 目录 | 许可证 | 与核心关系 |
|------|------|--------|-----------|
| A. 放在 `include/` 和 `src/` 内 | `include/cpp/` + `src/cpp/` | 与核心相同（GPL） | 紧密耦合 |
| B. 放在 `binding/` 下（推荐） | `binding/` | MIT | 独立，仅依赖 C ABI |
| C. 放在独立仓库 | 外部仓库 | MIT | 完全解耦 |

**决策**：采用方案 B。在项目根目录创建 `binding/` 目录，包含 C++ Binding 的全部源码、CMakeLists 和单独的 LICENSE 文件。Binding 仅依赖 `include/UICornerstoneAPI.h` 中定义的 C ABI 接口，不依赖核心库的内部头文件。

## 4. 整体架构

```mermaid
flowchart TB
    subgraph User["用户代码"]
        App["main.cpp<br/>游戏循环 / 应用逻辑"]
    end

    subgraph Binding["binding/ — C++ Binding (MIT)"]
        UIC["UICornerstone<br/>Config + 工厂 + 控件创建 + 双模式循环"]
        Ctrl["Control<br/>类型安全属性访问 + 便捷方法"]
        Evt["Event<br/>具名事件数据访问"]
        RM["ResourceManager<br/>resourceRoot 管理"]
        BR["BackendResolver<br/>DLL 加载 / 回调查表适配"]
    end

    subgraph CAPI["include/UICornerstoneAPI.h — C ABI"]
        CF["UICornerstone_Init<br/>UICornerstone_ProcessEvents<br/>UICornerstone_SetColor<br/>..."]
    end

    subgraph Core["src/ + include/ — UICornerstone 核心 (GPL)"]
        BM["BackendManager"]
        Controls["控件 (Button, Label, ...)"]
        Layout["LayoutParser"]
    end

    subgraph Backend["后端实现"]
        SDL3["UIBackend_sdl3.dll"]
        SFML["UIBackend_sfml.dll"]
        RL["UIBackend_raylib.dll"]
        CB["调用方自定义后端"]
    end

    App -->|"Create(Config)"| UIC
    App -->|"Create(callbacks)"| UIC

    UIC --> Ctrl
    UIC --> Evt
    UIC --> RM
    UIC --> BR

    UIC -->|"调用 C ABI"| CF
    Ctrl -->|"转发"| CF
    Evt -->|"通过 C 回调"| CF

    CF --> Core
    CF -->|"InitFromPlugin"| BR
    BR -->|"LoadLibrary"| SDL3
    BR -->|"LoadLibrary"| SFML
    BR -->|"LoadLibrary"| RL
    BR -->|"直接使用"| CB

    SDL3 --> Core
    SFML --> Core
    RL --> Core
    CB --> Core
```

**层间依赖**：

```
用户代码 → binding/ (MIT) → include/UICornerstoneAPI.h (C ABI) → UICornerstone 核心 (GPL)
```

Binding 层仅依赖 C ABI 头文件中声明的 `extern "C"` 函数和 POD 结构体。不 `#include` 任何核心库的内部头文件（`ControlBase.h`、`Bench.h` 等）。

## 5. 详细设计

### 5.1 UICornerstone 主类

```cpp
// binding/include/UICornerstone.h

#include <string>
#include <memory>
#include <functional>
#include "UICornerstoneAPI.h"

class Control;

class UICornerstone {
public:
    struct Config {
        // 后端选择
        std::string backend = "sdl3";            // "sdl3" | "sfml" | "raylib"
        std::string backendSearchPath;            // DLL 搜索目录（空=默认搜索顺序）

        // 资源根路径
        // 所有资源相对此路径解析：字体 → {root}/fonts/A.ttf、布局 → {root}/layouts/demo.json
        std::string resourceRoot   = "./assets";

        // 窗口参数
        std::string windowTitle    = "UICornerstone";
        int windowWidth  = 1024;
        int windowHeight = 768;
        int windowFlags  = 0;    // SDL3: 0x20=可调大小, 0x2000=高DPI; 后端无关

        Config& WithBackend(const std::string& name)
            { backend = name; return *this; }
        Config& WithBackendSearchPath(const std::string& path)
            { backendSearchPath = path; return *this; }
        Config& WithResourceRoot(const std::string& root)
            { resourceRoot = root; return *this; }
        Config& WithWindow(const std::string& title, int w, int h)
            { windowTitle = title; windowWidth = w; windowHeight = h; return *this; }
    };

    // 通过 Config 创建（自动管理后端加载）
    static std::unique_ptr<UICornerstone> Create(const Config& config);

    // 通过回调查表创建（不管理后端生命周期）
    static std::unique_ptr<UICornerstone> Create(const UIBackendCallbacks* callbacks);

    ~UICornerstone();

    // ── Hosted 模式 ──
    using FrameCallback = std::function<void(double deltaTime)>;
    using RenderCallback = std::function<void()>;

    int Run(FrameCallback update, RenderCallback onRender = nullptr);

    // ── Embedded 模式 ──
    bool Init();
    bool ProcessEvents();
    void Update(double deltaTime);
    void Render();
    void Present();
    void Shutdown();

    // ── 资源路径 ──
    void SetResourceRoot(const std::string& path);
    std::string GetResourceRoot() const;

    // ── 布局 ──
    bool LoadLayout(const std::string& jsonContent);
    bool LoadLayoutFromFile(const std::string& filePath);
    Control FindControl(const std::string& id);

    // ── 控件工厂（编程式创建，不全依赖 JSON） ──
    Control CreateButton(const std::string& text,
                         float x, float y, float w, float h);
    Control CreateLabel(const std::string& text, float fontSize,
                        float x, float y, float w, float h);
    Control CreateCheckBox(const std::string& text,
                           float x, float y, float w, float h);
    Control CreateEditBox(float x, float y, float w, float h);
    Control CreateProgressBar(float x, float y, float w, float h);
    Control CreateSlider(float x, float y, float w, float h,
                         float min, float max, float value);
    Control CreatePanel(float x, float y, float w, float h);
    Control CreateTextArea(float x, float y, float w, float h);
    Control CreateWinFrame(const std::string& title,
                           float x, float y, float w, float h);
    Control CreateComboBox(float x, float y, float w, float h);
    Control CreateColorPicker(float x, float y, float w, float h,
                              const std::string& color);
    Control CreateNumericUpDown(float x, float y, float w, float h);
    Control CreateSplitter(float x, float y, float w, float h, int orientation);
    Control CreateImageButton(const std::string& normal,
                              const std::string& hover,
                              const std::string& pressed,
                              float x, float y, float w, float h);
    Control CreateDialog(const std::string& confirmText,
                         const std::string& cancelText,
                         float x, float y, float w, float h);

    // ── 视口 ──
    void SetViewport(float x, float y, float w, float h);
    UIRect GetViewport() const;

    bool IsQuitRequested() const;

    // ── 事件注入（外部输入系统 → UICornerstone） ──
    // 当用户不使用 binding 的 ProcessEvents() 而自行管理输入时，
    // 通过此方法将构造好的 UIEvent 送入事件队列。
    void PushEvent(const UIEvent& event);

    // ── JSON 布局动作注册 ──
    // 为 JSON 布局中的 "events": { "onClick": "myAction" } 注册回调。
    using ActionCallback = std::function<void(Control)>;
    void RegisterAction(const std::string& name, ActionCallback callback);

    // ── 错误查询 ──
    const std::string& GetLastError() const;

    UICornerstone(const UICornerstone&) = delete;
    UICornerstone& operator=(const UICornerstone&) = delete;

private:
    UICornerstone();
    bool InitFromConfig(const Config& config);
    bool InitFromCallbacks(const UIBackendCallbacks* callbacks);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
```

### 5.2 Control 代理类

```cpp
// binding/include/Control.h

#include <string>
#include <functional>
#include "UICornerstoneAPI.h"

class Control {
public:
    Control() = default;
    explicit Control(UIControlHandle handle);

    // ── 属性设置 ──
    void SetColor(const char* prop, UIColor value);
    void SetStateColor(const char* prop, UIStateColor value);
    void SetBool(const char* prop, bool value);
    void SetInt(const char* prop, int value);
    void SetFloat(const char* prop, float value);
    void SetString(const char* prop, const std::string& value);
    void SetEnum(const char* prop, const std::string& value);
    void SetPtr(const char* prop, void* value);

    // ── 属性读取 ──
    UIColor     GetColor(const char* prop) const;
    UIStateColor GetStateColor(const char* prop) const;
    bool        GetBool(const char* prop) const;
    int         GetInt(const char* prop) const;
    float       GetFloat(const char* prop) const;
    std::string GetString(const char* prop) const;
    void*       GetPtr(const char* prop) const;

    // ── 枚举值读取（GetEnum 输出到栈 buffer，返回字符串）──
    std::string GetEnum(const char* prop) const;

    // ── 回调 ──
    template<typename Fn>
    void SetCallback(const char* event, Fn&& callback);

    // ── 便捷方法 ──
    void SetText(const std::string& text);
    std::string GetText() const;
    void SetVisible(bool visible);
    bool IsVisible() const;
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    void SetRect(float x, float y, float w, float h);
    UIRect GetRect() const;
    void AddChild(Control child);
    void Destroy();
    std::string GetId() const;
    bool IsValid() const { return m_handle != nullptr; }
    UIControlHandle Handle() const { return m_handle; }

private:
    UIControlHandle m_handle = nullptr;
};
```

### 5.3 事件系统

`UIEventData` 是**基于事件名辨别的联合体**：`eventName` 字段标记了 `data` union 中哪个成员有效。裸 C 调用时需要手动判断：

```c
void myHandler(UIControlHandle ctl, const UIEventData* ev, void* user) {
    if (strcmp(ev->eventName, "value-changed") == 0) {
        float val = ev->data.floatVal;  // 手动知道该读 floatVal
    }
}
```

C++ Binding 通过 `Event` 类将这种"手工辨别"封装为**具名推导访问器**，按事件名提供对应的类型安全取值路径：

```cpp
// binding/include/Event.h

#include "UICornerstoneAPI.h"

class Event {
public:
    explicit Event(const UIEventData* raw);

    // 事件名鉴别（所有事件的共同信息）
    std::string GetName() const;

    // ── 按事件类型划分的具名访问器 ──
    // 编译期类型安全：方法名自述含义，调用方不再面对 union

    // "click"：无数据负载，仅事件本身
    bool IsClick() const;

    // "value-changed"：floatVal
    bool IsValueChanged() const;
    float GetValueChanged() const;

    // "text-changed"：strVal
    bool IsTextChanged() const;
    std::string GetTextChanged() const;

    // "selection-changed"：selection { idx, val }
    bool IsSelectionChanged() const;
    int  GetSelectedIndex() const;
    std::string GetSelectedValue() const;

    // "check-changed"：intVal
    bool IsCheckChanged() const;
    int  GetCheckState() const;

    // "color-changed"：color { r,g,b,a }
    bool IsColorChanged() const;
    UIColor GetChangedColor() const;

    // "confirm" / "cancel" / "close"：无数据负载
    bool IsConfirm() const;
    bool IsCancel() const;
    bool IsClose() const;

    // ── 原始数据（必要时回退） ──
    const UIEventData* Raw() const { return m_raw; }

private:
    const UIEventData* m_raw;
};
```

```mermaid
flowchart LR
    subgraph 用户回调
        EC["Event ev"]
    end

    subgraph Event 内部
        GN["GetName() → 'value-changed'"]
        IVC["IsValueChanged() → true"]
        GVC["GetValueChanged() → 0.5f"]
    end

    subgraph UIEventData
        EN["eventName: 'value-changed'"]
        UD["data.union { floatVal: 0.5f }"]
    end

    EC --> GN --> EN
    EC --> IVC
    EC --> GVC --> UD
```

**典型用法**（在 `Control::SetCallback` 的 lambda 中）：

```cpp
btn.SetCallback("value-changed", [](const Event& ev) {
    if (ev.IsValueChanged()) {
        float val = ev.GetValueChanged();
        // 直接读，不接触 union
    }
});
```

每个具名访问器内部实现类似：

```cpp
bool Event::IsValueChanged() const {
    return strcmp(m_raw->eventName, "value-changed") == 0;
}

float Event::GetValueChanged() const {
    // 可在 Debug 断言事件类型，快速暴露误用
    assert(IsValueChanged());
    return m_raw->data.floatVal;
}
```

### 5.4 资源路径管理

**解析规则**：Binding 对路径做简单拼接 `resourceRoot + "/" + userPath`，**不自动插入任何子目录前缀**。目录结构完全由用户在参数中控制。

```
resourceRoot = "./assets"

传参                      → 实际路径
─────────────────────────────────────────────
"btn.png"                 → ./assets/btn.png
"images/btn.png"          → ./assets/images/btn.png
"ui/icons/btn.png"        → ./assets/ui/icons/btn.png
"layouts/demo.json"       → ./assets/layouts/demo.json
```

```mermaid
flowchart LR
    R["Config.resourceRoot<br/>'./assets'"]
    U["用户传参<br/>(自由决定相对路径)"]
    RES["实际路径<br/>resourceRoot + '/' + userPath"]

    R --> RES
    U --> RES
```

**三种资源类型的路径出处**：

| 资源类型 | 路径来源 | 示例传参 |
|---------|---------|---------|
| 字体 | `ConstDef::fontFiles` 表已有 `"fonts/"` 前缀 | `"fonts/A.ttf"`（表自带） |
| 图片 | `CreateImageButton("normal", "hover", ...)` 参数 | `"btn.png"` 或 `"ui/btn.png"` |
| 布局 | `LoadLayoutFromFile(path)` 参数 | `"demo.json"` 或 `"layouts/demo.json"` |

用户统一改 `resourceRoot` 即可重定向所有资源。如需把图片移到别处而字体不动，直接在调用 `CreateImageButton` 时传不同的相对路径即可，无需额外配置项。

```cpp
// binding/include/ResourceManager.h

class ResourceManager {
public:
    explicit ResourceManager(const std::string& root);

    void SetRoot(const std::string& root);
    std::string GetRoot() const;

    // 统一路径解析
    std::string Resolve(const std::string& relativePath) const;

    // 创建 ResourceProvider（核心库用）
    ResourceProvider* CreateProvider() const;

private:
    std::string m_root;
};
```

#### resourceRoot 生效链路

Binding 通过 C ABI 扩展函数 `UICornerstone_SetResourceRoot` 将路径传入核心库，核心库在创建 `ResourceProvider` 时使用该路径：

```
Config.resourceRoot = "./my_assets"
        │
        ▼
UICornerstone::Init()
        │
        ├── BackendResolver::LoadPlugin()     ← 加载后端 DLL
        ├── UICornerstone_SetResourceRoot     ← 注入 resourceRoot
        └── UICornerstone_Init(callbacks)     ← 初始化核心

核心库内部:
    MainWindow 构造函数:
        m_resourceProvider = ResourceProvider::createFilesystem(pathPrefix)
                                                    ▲
                                                    └── 由 SetResourceRoot 写入
```

```cpp
// UICornerstone::Init 中的关键步骤（伪代码）
bool UICornerstone::Init() {
    // 1. 后端初始化（通过 BackendResolver 或传入的回调查表）
    // 2. 设置资源根路径
    UICornerstone_SetResourceRoot(m_impl->resourceRoot.c_str());
    // 3. 初始化核心
    UICornerstone_Init(m_impl->callbacks);
    return true;
}
```

`UICornerstone_SetResourceRoot` 是核心库 C ABI 中新增的函数，单行实现：将字符串写入 `ConstDef::pathPrefix`，供后续 `MainWindow` 创建 `ResourceProvider` 时使用。

### 5.5 后端管理

```mermaid
flowchart TD
    Start["UICornerstone::Create(Config)"] --> Check{"backendSearchPath 非空?"}
    Check -->|是| Search1["搜索 backendSearchPath"]
    Check -->|否| Search2["搜索 exe 目录 + plugins/"]
    Search1 --> Found{"找到 UIBackend_{name}.dll?"}
    Search2 --> Found
    Found -->|是| Load["LoadLibrary + GetProcAddress<br/>获取 GetUIBackendCallbacks"]
    Found -->|否| Fallback["尝试 extern 'C' 符号<br/>（静态链接回退）"]
    Load --> Success{"成功?"}
    Fallback --> Success
    Success -->|是| Init["UICornerstone_Init(callbacks)"]
    Success -->|否| Return["返回 nullptr + 错误日志"]
```

**DLL 搜索顺序**（`FindPluginDLL`）：

1. 用户指定的 `backendSearchPath` + `UIBackend_{name}.dll`
2. exe 目录 + `plugins/UIBackend_{name}.dll`
3. exe 目录 + `UIBackend_{name}.dll`
4. 系统 `PATH` 环境变量中的目录

### 5.6 架构约束分析

#### 5.6.1 多实例限制

**现状**：当前 C ABI 内部使用全局变量管理状态：

```cpp
// src/UICornerstoneAPI.cpp (匿名 namespace)
namespace {
    const UIBackendCallbacks* g_callbacks = nullptr;
    Window*                   g_window = nullptr;
    RenderDevice*             g_renderDevice = nullptr;
    InputBackend*             g_inputBackend = nullptr;
    TextRenderer*             g_textRenderer = nullptr;
    bool g_initialized = false;
    bool g_quit = false;
    // ... 更多全局变量
}
```

`UICornerstone_Init()` 检查 `g_initialized`，已初始化时直接返回 1（成功）。这意味着**核心库在进程生命周期内只能初始化一次**。

**影响分析**：

| 场景 | 是否可行 | 说明 |
|------|---------|------|
| 进程内单实例 | ✅ | 正常使用 |
| 进程内多实例销毁后重创建 | ❌ | `Shutdown` 未重置 `g_initialized` |
| 两个独立 UICornerstone 实例共存 | ❌ | 第二次 `Init` 静默返回成功，但不生效 |
| 子线程创建第二个实例 | ❌ | 同上，线程不安全 |

**Binding 层面的处理策略**：

```cpp
// binding/src/UICornerstone.cpp — Impl 结构

struct UICornerstone::Impl {
    Config config;
    bool initialized = false;
    uint64_t lastTicks = 0;

    // Action 注册表（实例私有，非全局）
    std::unordered_map<std::string,
        std::shared_ptr<ActionCallback>> actions;

    // Backend 资源
    BackendResolver backendResolver;
    UIBackendCallbacks callbacks;

    // Control 生命周期追踪
    std::unordered_map<UIControlHandle, std::weak_ptr<ControlState>> liveControls;
};
```

```cpp
// Create() 实现

std::unique_ptr<UICornerstone> UICornerstone::Create(const Config& config) {
    // 当前限制：全局只能有一个实例
    static bool s_instanceCreated = false;
    if (s_instanceCreated) {
        printf("UICornerstone: only one instance allowed per process\n");
        return nullptr;
    }

    auto ui = std::unique_ptr<UICornerstone>(new UICornerstone());
    ui->m_impl->config = config;
    s_instanceCreated = true;
    return ui;
}
```

**未来扩展方向**：若核心库 C ABI 升级为支持多实例（例如将全局状态改为句柄模式，如 `UIInstanceHandle UICornerstone_CreateInstance(config)`），Binding 可以自然适配——`Impl` 已经以实例为单位封装状态。

#### 5.6.2 线程安全

**C ABI 假设**：所有函数必须在同一线程调用，不接受跨线程并发。核心库内部无锁。

**Binding 约束**：

```cpp
// UICornerstone 类注释
//
// 线程安全：
//   本类的所有方法（ProcessEvents / Update / Render / Present / Shutdown）
//   必须在同一线程调用。不得跨线程并发访问。
//   Create() 和 ~UICornerstone() 必须在同一线程调用。
```

**例外**：`PushEvent` 可以（在有限场景下）从其他线程调用，但需外部同步。当前版本不保证，记录为未来优化。

#### 5.6.3 错误处理策略

C ABI 用 `int` 返回值（1=成功，0=失败）报告错误。Binding 不抛出异常，延续同一风格，并通过 `lastError` 提供可查的错误描述：

```cpp
struct UICornerstone::Impl {
    // ...
    std::string lastError;
};

const std::string& UICornerstone::GetLastError() const {
    return m_impl->lastError;
}

// 内部辅助宏
#define UI_CHECK(expr, msg) \
    do { \
        if (!(expr)) { \
            m_impl->lastError = msg; \
            return false; \
        } \
    } while(0)

// 使用示例
bool UICornerstone::InitFromConfig(const Config& config) {
    UI_CHECK(!config.backend.empty(), "backend name is empty");

    if (m_impl->backendResolver.LoadPlugin(...)) {
        // 使用插件 DLL
    } else {
        m_impl->lastError = "Failed to load backend plugin: " + config.backend;
        return false;
    }

    UICornerstone_SetResourceRoot(config.resourceRoot.c_str());

    UI_CHECK(UICornerstone_Init(&m_impl->callbacks),
             "UICornerstone_Init failed");
    return true;
}
```

| 方法 | 错误指示 | 详情查询 |
|------|---------|---------|
| `Create(Config)` | 返回 `nullptr` | 无（无实例时无法存 error） |
| `Init()` | 返回 `false` | `GetLastError()` |
| `ProcessEvents()` | 返回 `false`（窗口关闭） | 非错误，不设 lastError |
| `Run()` | 返回 1（初始化失败） | 内部调用 Init，已设 lastError |
| `CreateXxx / FindControl` | 返回 `Control()`（空句柄） | `ctl.IsValid()` 判断 |
| `Control::SetXxx` | 静默失败 | 无（C ABI 返回 0 时忽略） |
| `PushEvent / RegisterAction` | 无返回值 | 无 |

### 5.7 Control 生命周期管理

#### 5.7.1 句柄有效性

C ABI 返回的 `UIControlHandle` 是一个裸指针 `void*`（实际是 `Control*`）。如果核心库销毁了控件（例如 Dialog `close()` 自动销毁），Binding 的 `Control` 对象变成悬挂句柄。

**防护设计**：引入 `ControlState` 共享状态对象，通过 `shared_ptr/weak_ptr` 追踪句柄有效性。

```cpp
// binding/src/ControlState.h (内部类)

struct ControlState {
    UIControlHandle handle;
    bool alive = true;

    // SetCallback 分配的 std::function 列表，Destroy 时统一清理
    std::vector<std::shared_ptr<void>> callbackUserData;
};

// binding/include/Control.h 更新
class Control {
public:
    Control() = default;
    explicit Control(UIControlHandle handle);

    bool IsValid() const;

    // ... 原有方法

private:
    std::shared_ptr<ControlState> m_state;

    UIControlHandle RawHandle() const {
        return m_state ? m_state->handle : nullptr;
    }
};
```

**工厂方法更新**——创建 Control 时注册到 `Impl::liveControls`：

```cpp
// binding/src/UICornerstone.cpp
Control UICornerstone::CreateButton(const std::string& text,
                                    float x, float y, float w, float h) {
    UIControlHandle h = UICornerstone_CreateButton(text.c_str(), x, y, w, h);
    return MakeControl(h);
}

Control UICornerstone::MakeControl(UIControlHandle h) {
    auto state = std::make_shared<ControlState>();
    state->handle = h;
    m_impl->liveControls[h] = state;
    return Control(std::move(state));
}
```

**`Destroy()` 方法更新**——清理回调 userData 并从注册表移除：

```cpp
void Control::Destroy() {
    if (!m_state || !m_state->alive) return;

    // 清理所有 callback userData（堆上分配的 std::function）
    m_state->callbackUserData.clear();

    // 通知核心库销毁控件
    UICornerstone_DestroyControl(m_state->handle);

    // 标记失效
    m_state->alive = false;
}
```

**自动失效检测**：核心库在某些场景（如 Popup close）会自动销毁控件。Binding 在调用 C ABI 函数返回后检查句柄是否仍然有效。当前版本不做自动全量同步，而是通过 `UICornerstone_FindControl` 返回空来间接感知。

#### 5.7.2 Destroy 后的行为

| 方法 | 已销毁的 Control 上调用 |
|------|----------------------|
| `SetColor / SetText / SetCallback` 等 | 静默跳过（检查 `m_state->alive`） |
| `Destroy()` | 幂等，第二次调用无效果 |
| `GetId()` | 返回空字符串 |
| `IsValid()` | 返回 `false` |
| `Handle()` | 返回原始 `UIControlHandle`（可能已失效） |

### 5.8 RegisterAction 实例化重构

`RegisterAction` 的注册表从全局 `static` 移到 `Impl` 中，保证多实例安全（即使当前不支持多实例，设计上不引入全局变量）：

```cpp
// binding/src/UICornerstone.cpp

void UICornerstone::RegisterAction(const std::string& name,
                                   ActionCallback callback) {
    auto cb = std::make_shared<ActionCallback>(std::move(callback));
    m_impl->actions[name] = cb;

    // 注册 C 回调
    UICornerstone_RegisterAction(name.c_str(),
        [](UIControlHandle ctl, void* userData) {
            auto& fn = *static_cast<ActionCallback*>(userData);
            fn(Control(ctl));
        },
        cb.get());
}
```

### 5.9 Impl 结构总览

```cpp
// binding/src/Impl.h (内部)

#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>

struct ControlState;

struct UICornerstone::Impl {
    Config config;
    bool initialized = false;
    uint64_t lastTicks = 0;
    std::string lastError;

    // Backend
    BackendResolver backendResolver;
    UIBackendCallbacks callbacks;
    bool ownsCallbacks = false;  // true 表示 binding 加载了 DLL

    // Resource
    std::string resourceRoot;

    // Action 注册表（实例私有）
    std::unordered_map<
        std::string,
        std::shared_ptr<std::function<void(Control)>>
    > actions;

    // Control 生命周期追踪
    std::unordered_map<
        UIControlHandle,
        std::weak_ptr<ControlState>
    > liveControls;
};
```

### 5.10 Create(Config) → Init 全链路（含错误处理）

```mermaid
sequenceDiagram
    participant User as 用户代码
    participant B as UICornerstone
    participant BR as BackendResolver
    participant C as C ABI
    participant Core as 核心库

    User->>B: Create(Config)
    B->>B: 检查单例限制（static flag）
    B->>B: 校验 Config 字段合法性
    B-->>User: unique_ptr<UICornerstone> / nullptr

    User->>B: Init()

    alt 自动管理后端
        B->>BR: LoadPlugin(backend, searchPath)
        alt DLL 加载失败
            BR-->>B: false
            B->>B: lastError = "找不到 UIBackend_sdl3.dll"
            B-->>User: false
        else DLL 加载成功
            BR-->>B: 回调查表指针
        end
    else 回调查表模式
        B->>B: 校验 callbacks 非空、version==1
    end

    B->>C: UICornerstone_SetResourceRoot(resourceRoot)
    alt 核心库 SetResourceRoot 失败
        C-->>B: 0
        B->>B: lastError = "SetResourceRoot 失败"
        B-->>User: false
    end

    B->>C: UICornerstone_Init(callbacks)
    C->>Core: 检查 g_initialized → 跳过 / 初始化
    alt 初始化失败
        C-->>B: 0
        B->>B: lastError = "UICornerstone_Init 失败"
        B-->>User: false
    else 初始化成功
        C-->>B: 1
        B->>B: initialized = true
        B-->>User: true
    end
```

### 5.11 Hosted Run 内部实现

```cpp
int UICornerstone::Run(FrameCallback update, RenderCallback onRender) {
    if (!Init()) return 1;

    m_impl->lastTicks = Platform::GetTicks();  // 首次 dt≈0，游戏逻辑自行处理首帧
    bool running = true;

    while (running) {
        running = ProcessEvents();
        if (!running) break;

        uint64_t now = Platform::GetTicks();
        double dt = (now - m_impl->lastTicks) / 1000.0;
        m_impl->lastTicks = now;

        dt = std::min(dt, 0.1);  // 防止长时间挂起后的 dt 暴增（如调试断点）

        Update(dt);
        if (update) update(dt);

        UICornerstone_Clear();
        UICornerstone_Render();
        if (onRender) onRender();
        UICornerstone_Present();
    }

    Shutdown();
    return 0;
}
```

**首帧处理**：`lastTicks` 在 Init 后首次进入 Run 时赋值为当前时间，第一帧 dt ≈ 0。游戏逻辑的 update 回调需要处理 dt=0 的情况（跳过或正常处理）。

### 5.12 Config 校验规则

| 字段 | 校验 | 不通过时 |
|------|------|---------|
| `backend` | 非空 | `Create()` 返回 nullptr |
| `backendSearchPath` | 可选，空则使用默认顺序 | — |
| `resourceRoot` | 非空 | `Create()` 返回 nullptr |
| `windowTitle` | 非空（"UICornerstone" 默认） | 使用默认值 |
| `windowWidth / windowHeight` | > 0 | 使用默认值 |
| `windowFlags` | 无校验 | 传递 0 |

`windowFlags` 含义（与后端具体实现相关）：

| 后端 | 常见 flags |
|------|-----------|
| SDL3 | `0x00000020` = 可调整大小，`0x00002000` = 高 DPI 支持 |
| SFML | 通常忽略 |
| Raylib | 通常忽略 |

Binding 不封装 flags 的符号常量，保持与核心库一致的裸值。

### 5.13 CMake 构建集成

```cmake
# binding/CMakeLists.txt

cmake_minimum_required(VERSION 3.16)
project(UICornerstoneBinding)

# 引入导入目标：core 项目提供 UICornerstone_dll 导入库
# 用户需先构建核心库的 DLL 模式
find_package(UICornerstone REQUIRED
    PATHS "${CMAKE_SOURCE_DIR}/../build/sdl3_dll"
    NO_DEFAULT_PATH
)

add_library(uic_binding INTERFACE)
target_include_directories(uic_binding INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(uic_binding INTERFACE
    UICornerstone_dll
)

# 编译 binding 实现为静态库（内部转发逻辑）
add_library(uic_binding_impl STATIC
    src/UICornerstone.cpp
    src/Control.cpp
    src/Event.cpp
    src/ResourceManager.cpp
    src/BackendResolver.cpp
)
target_include_directories(uic_binding_impl PUBLIC include)
target_link_libraries(uic_binding_impl PUBLIC UICornerstone_dll)

# 示例可执行文件
add_subdirectory(samples)
```

## 6. 文件布局与许可证

```
UIControls/
├── binding/                      ← C++ Binding 独立目录 (MIT)
│   ├── LICENSE                   ← MIT License（与核心 GPL 分开）
│   ├── CMakeLists.txt            ← Binding 构建配置
│   │
│   ├── include/                  ← Binding 公共头文件
│   │   ├── UICornerstone.h       ← 主类（应用入口）
│   │   ├── Control.h             ← 控件代理（属性访问）
│   │   └── Event.h               ← 事件包装（类型安全回调数据）
│   │
│   ├── src/                      ← Binding 实现
│   │   ├── UICornerstone.cpp     ← 主类（工厂 + 双模式循环）
│   │   ├── Control.cpp           ← Control 属性转发
│   │   ├── Event.cpp             ← Event 事件数据解析
│   │   ├── ResourceManager.h/.cpp← 资源路径管理（内部类）
│   │   └── BackendResolver.h/.cpp← 后端 DLL 加载器（内部类）
│   │
│   ├── samples/                  ← Binding 示例
│   │   ├── CMakeLists.txt
│   │   ├── sample_cpp_hosted/    ← Hosted 模式
│   │   └── sample_cpp_embed/     ← Embedded 模式
│
├── include/                      ← 核心库公共头文件 (GPL)
│   └── UICornerstoneAPI.h        ← C ABI（Binding 的唯一 #include 依赖）
│
├── src/                          ← 核心库实现 (GPL)
├── samples/                      ← 核心库示例 (GPL)
├── doc/                          ← 文档
└── LICENSE                       ← GPL v3.0
```

**依赖关系**：

```
binding/ 只依赖 include/UICornerstoneAPI.h（通过 #include "UICornerstoneAPI.h"）
binding/ 不依赖 src/ 或 include/ 中的任何 C++ 内部头文件
binding/ 不链接 UICornerstone 核心库，只链接 UICornerstone.dll（导入库）
```

## 7. 示例程序设计

### 7.1 sample_cpp_hosted — 在 UICornerstone 循环中嵌入游戏逻辑

**文件**：`binding/samples/sample_cpp_hosted/main.cpp`

**结构**：

```mermaid
flowchart TB
    subgraph main
        C1["配置 Config"]
        C2["Create(config)"]
        C3["LoadLayout + 绑定事件"]
        C4["Run(update, render)"]
    end

    subgraph UICornerstone::Run 内部
        L1{"ProcessEvents()"}
        L2["Update(dt)"]
        L3["Clear()"]
        L4["Render() — UICornerstone 渲染"]
        L5["用户 onRender()"]
        L6["Present()"]
    end

    C1 --> C2 --> C3 --> C4

    C4 --> L1
    L1 -->|"running"| L2
    L2 --> L3 --> L4 --> L5 --> L6
    L6 --> L1
    L1 -->|"false"| E["Run 返回"]
```

**代码**（~50 行，展示编程式 + JSON 布局两种控件创建方式）：

```cpp
#include "UICornerstone.h"
#include <cstdio>

static int g_clickCount = 0;

static void setupUI(UICornerstone& ui) {
    // 方式一：编程式创建控件
    auto root = ui.CreatePanel(0, 0, 800, 600);
    auto title = ui.CreateLabel("C++ Hosted Sample", 18, 20, 10, 760, 30);

    auto btn = ui.CreateButton("Click Me", 20, 60, 200, 80);
    btn.SetColor("background", {74, 144, 217, 255});
    btn.SetCallback("click", [&](const Event&) {
        g_clickCount++;
        auto status = ui.FindControl("status_label");
        status.SetText("Clicked: " + std::to_string(g_clickCount));
    });

    auto status = ui.CreateLabel("Click the button above", 14,
                                 20, 160, 400, 24);
    status.SetString("id", "status_label");

    root.AddChild(title);
    root.AddChild(btn);
    root.AddChild(status);

    // 方式二：JSON 布局（可选，适合复杂界面）
    // ui.LoadLayoutFromFile("demo_layout.json");
}

int main() {
    auto config = UICornerstone::Config{}
        .WithBackend("sdl3")
        .WithResourceRoot("./my_assets")
        .WithWindow("Hosted Sample", 800, 600);

    auto ui = UICornerstone::Create(config);
    if (!ui) return 1;

    setupUI(*ui);

    return ui->Run(
        [](double dt) { updateGameLogic(dt); },
        []()          { renderGame();         }
    );
}
```

### 7.2 sample_cpp_embed — 在用户游戏循环中嵌入 UICornerstone

**文件**：`binding/samples/sample_cpp_embed/main.cpp`

**结构**：

```mermaid
flowchart TB
    subgraph main
        C1["配置 Config"]
        C2["Create(config)"]
        C3["Init()"]
        C4["LoadLayout + 绑定事件"]
    end

    subgraph 用户游戏循环
        L1["计算 dt"]
        L2{"ProcessEvents()"}
        L3["游戏 Update(dt)"]
        L4["UI Update(dt)"]
        L5["Present() — 渲染"]
        L6["帧率控制"]
    end

    C1 --> C2 --> C3 --> C4

    C4 --> L1 --> L2
    L2 -->|"true"| L3 --> L4 --> L5 --> L6
    L6 --> L1
    L2 -->|"false"| E["Shutdown() 返回"]
```

**代码**（~60 行，展示在用户循环中精确控制 UI 更新时机）：

```cpp
#include "UICornerstone.h"
#include <cstdio>

int main() {
    auto config = UICornerstone::Config{}
        .WithBackend("sdl3")
        .WithResourceRoot("./my_assets")
        .WithWindow("Embed Sample", 800, 600);

    auto ui = UICornerstone::Create(config);
    if (!ui) return 1;

    // Init 阶段：绑定会在内部调用 UICornerstone_SetResourceRoot + Init
    if (!ui->Init()) return 1;

    // 混合使用工厂 + JSON 布局
    auto root = ui->CreatePanel(0, 0, 800, 600);

    auto label = ui->CreateLabel("Game UI Overlay", 16, 10, 10, 300, 24);
    label.SetColor("text", {255, 255, 255, 255});
    root.AddChild(label);

    // 也可以通过 JSON 加载复杂布局
    // ui->LoadLayoutFromFile("hud_layout.json");

    // 用户自己的游戏循环
    uint64_t lastTime = getTicks();
    bool running = true;
    while (running) {
        uint64_t now = getTicks();
        double dt = (now - lastTime) / 1000.0;
        lastTime = now;

        // 1. 处理 UI 事件（不阻塞，立即返回）
        if (!ui->ProcessEvents()) break;

        // 2. 游戏逻辑更新
        updateGameLogic(dt);
        updateParticles(dt);

        // 3. UI 更新（布局、动画等）
        ui->Update(dt);

        // 4. 渲染：先游戏 → 后 UI（或先 UI → 后游戏，由用户决定）
        renderGameScene();        // 用户自己的渲染
        ui->Present();            // 内部 Clear → UI Render → Present

        limitFramerate(60);
    }

    ui->Shutdown();
    return 0;
}
```

## 8. 实施计划

| 阶段 | 内容 | 验收标准 |
|------|------|---------|
| **P1** | `binding/` 目录结构 + CMakeLists + MIT LICENSE | `cmake -B build/binding` 识别 binding 子项目 |
| **P2** | 核心库 C ABI 新增 `UICornerstone_SetResourceRoot` | 调用后 `ConstDef::pathPrefix` 改变 |
| **P3** | `UICornerstone` 主类骨架：Config + 单例检查 + Create (两种) + ~dtor | 第二次 Create 返回 nullptr；两种 Create 均工作 |
| **P4** | `Control` + `ControlState` 共享状态 + 悬挂句柄检测 | Destroy 后 IsValid=false，SetXxx 静默跳过 |
| **P5** | 全部属性转发（含 SetPtr/GetPtr/GetEnum） | 控件 Set/Get 17 种属性操作正常 |
| **P6** | `Event` 包装 + SetCallback `std::function` 桥接 + `ControlState` 归集 userData | lambda 绑定后事件触发、数据读取正确 |
| **P7** | 双模式循环：`Run()` + tick API（Init/ProcessEvents/Update/Present/Shutdown） | dt 上限 0.1s，两种模式均 60fps 正常运行 |
| **P8** | `BackendResolver`：DLL 搜索 + 加载 + 错误分支 | 切换 backend="sfml" 换后端；找不到 DLL 时 lastError 有内容 |
| **P9** | `Impl` 封装：actions 注册表从全局 static 迁入 + `GetLastError` | 无全局 static 容器；error 可查询 |
| **P10** | `sample_cpp_hosted`：编程式创建 + `Run()` | 编译运行，按钮点击计数更新 Label |
| **P11** | `sample_cpp_embed`：工厂创建 + 用户循环 + 分步渲染 | 编译运行，用户循环内 UI 交互正常 |

## 9. 与现有 C ABI 的关系

```
C ABI 函数                        C++ Binding 封装
─────────────────────             ─────────────────────────────
UICornerstone_Init                Create(Config) / Create(callbacks)
UICornerstone_Shutdown            ~UICornerstone() / Shutdown()
UICornerstone_ProcessEvents       ProcessEvents()
UICornerstone_Update(dt)          Update(dt)
UICornerstone_Render              Render()
UICornerstone_Clear / Present     Present()
UICornerstone_SetViewport         SetViewport(x, y, w, h)
UICornerstone_GetViewport         GetViewport()
UICornerstone_IsQuitRequested     IsQuitRequested()

── 控件工厂 ─────────────────────────────────────────────────
UICornerstone_CreateButton(...)   ui.CreateButton(text, x, y, w, h)
UICornerstone_CreateLabel(...)    ui.CreateLabel(text, fontSize, x, y, w, h)
UICornerstone_CreateCheckBox(...) ui.CreateCheckBox(text, x, y, w, h)
UICornerstone_CreateEditBox(...)  ui.CreateEditBox(x, y, w, h)
UICornerstone_CreatePanel(...)    ui.CreatePanel(x, y, w, h)
UICornerstone_CreateSlider(...)   ui.CreateSlider(x, y, w, h, min, max, val)
UICornerstone_CreateComboBox(...) ui.CreateComboBox(x, y, w, h)
UICornerstone_CreateProgressBar   ui.CreateProgressBar(x, y, w, h)
UICornerstone_CreateTextArea      ui.CreateTextArea(x, y, w, h)
UICornerstone_CreateWinFrame      ui.CreateWinFrame(title, x, y, w, h)
UICornerstone_CreateColorPicker   ui.CreateColorPicker(x, y, w, h, color)
UICornerstone_CreateNumericUpDown ui.CreateNumericUpDown(x, y, w, h)
UICornerstone_CreateSplitter      ui.CreateSplitter(x, y, w, h, orientation)
UICornerstone_CreateImageButton   ui.CreateImageButton(n, h, p, x, y, w, h)
UICornerstone_CreateDialog        ui.CreateDialog(confirm, cancel, x, y, w, h)

── 布局 ─────────────────────────────────────────────────────
UICornerstone_LoadLayout(json)    ui.LoadLayout(json)
UICornerstone_LoadLayoutFromFile  ui.LoadLayoutFromFile(path)
UICornerstone_FindControl(id)     ui.FindControl(id)

── 属性系统 ─────────────────────────────────────────────────
UICornerstone_SetColor(prop)      ctl.SetColor("prop", color)
UICornerstone_SetStateColor(prop) ctl.SetStateColor("prop", stateColor)
UICornerstone_SetBool(prop, v)    ctl.SetBool("prop", v) / SetVisible(bool)
UICornerstone_SetInt(prop, v)     ctl.SetInt("prop", v)
UICornerstone_SetFloat(prop, v)   ctl.SetFloat("prop", v)
UICornerstone_SetString(prop, s)  ctl.SetString("prop", s) / SetText(s)
UICornerstone_SetEnum(prop, s)    ctl.SetEnum("prop", s)
UICornerstone_SetPtr(prop, p)     ctl.SetPtr("prop", p)
UICornerstone_GetColor(prop)      ctl.GetColor("prop")
UICornerstone_GetBool(prop)       ctl.GetBool("prop")
UICornerstone_GetInt(prop)        ctl.GetInt("prop")
UICornerstone_GetFloat(prop)      ctl.GetFloat("prop")
UICornerstone_GetString(prop)     ctl.GetString("prop")
UICornerstone_GetEnum(prop)       ctl.GetEnum("prop")
UICornerstone_GetPtr(prop)        ctl.GetPtr("prop")
UICornerstone_SetCallback         ctl.SetCallback("event", lambda)

── 通用操作 ─────────────────────────────────────────────────
UICornerstone_SetRect             ctl.SetRect(x, y, w, h)
UICornerstone_GetRect             ctl.GetRect()
UICornerstone_AddChildControl     ctl.AddChild(child)
UICornerstone_DestroyControl      ctl.Destroy()
UICornerstone_GetControlId        ctl.GetId()

── 资源与配置（新增 C ABI 函数） ─────────────────────────
UICornerstone_SetResourceRoot     Config::resourceRoot 自动调用

── 事件注入与动作注册 ─────────────────────────────────────
UICornerstone_PushUIEvent         ui.PushEvent(event)
UICornerstone_RegisterAction      ui.RegisterAction("name", lambda)
```

**兼容性保证**：

- C++ Binding 仅封装 C ABI，不替换、不绕过
- 同一进程中可混合使用 C ABI 和 C++ Binding
- `Control::Handle()` 暴露原始 `UIControlHandle`，需要时回退到 C API
- Binding 状态封装在 `UICornerstone::Impl`（单例限制见 §5.6.1）
- `include/UICornerstoneAPI.h` 是 Binding 的唯一头文件依赖
