# Image 图片控件设计文档

> 状态：**已审核**（2026-08-05 验收通过，实现见 image-control 分支 d27ccca）

## 1. 概述

Image（图片控件）是用于**纯图片显示**的 UI 控件（无交互语义），对标 QLabel+Pixmap / `<img>` 标签，用于背景图、装饰图、图标等场景。

当前 `Actor` 类（`Actor → Material → ControlImpl → Control`，Actor.h:14）已经是 Control 子类，具备渲染、事件、两阶段加载、多实例适配等全部基础能力，但定位是**内部贴图资源**（Button 四态图、WinFrame 关闭按钮、LuotiAni 帧），**未向 C ABI 暴露为独立控件**。

本设计决定**直接复用 Actor 暴露为 C ABI 图片控件**，零架构改动。

## 2. 现状分析

### 2.1 继承链与既有能力（已核实）

| 能力 | 现状 | 证据 |
|------|------|------|
| 渲染 | `Material::draw(void)` 按 `m_rect`+锚点绘制，走标准控件树遍历 | Material.cpp:37-39 |
| 事件透明 | 不重写 `handleEvent`，`ControlImpl` 默认只透传不消费 | ControlBase.cpp:276-314 |
| 两阶段加载 | `create()` 在 context 就绪后由 setContext 触发 | Actor.cpp:53-62 |
| 缩放模式 | STRETCH / FIT_CENTER / CENTER_CROP / NONE 四种 | Actor.cpp:183-224 |
| 锚点 | Material 提供 9 种锚点 | Material.h:10-23 |
| matchParentRect | 匹配父控件绘图区域（setParent 时生效） | Actor.cpp:141-156 |
| 图片来源 | 文件路径（相对路径经 `Platform::GetBasePath` 拼接）与资源 ID（经 `ResourceProvider`） | Actor.cpp:64-121 |

### 2.2 现状缺口

1. **C ABI 无纯图片控件**：20 个 `UICornerstone_Create*` 中唯一图片入口是 `CreateImageButton`（Button+状态 Actor，UICornerstoneAPI.cpp:854），含按钮交互语义，不适合背景/装饰/图标场景。
2. **属性系统未开放**：`m_matchParentRect` 私有（仅 ActorBuilder 可设，Actor.cpp:228-231）；无 alpha 成员（独立控件走 `draw(void)` 硬编码 alpha=255）；`setScaleType`/锚点仅 C++ 侧可达，C ABI 属性系统（`SetEnum`/`SetBool`/`SetInt`）无法设置。
3. **Panel 贴图声明未实现**：`Panel::m_actors`（Panel.h:18）在全部 src 中无任何使用处，Panel 贴图功能实际不可用——本次不处理（与本设计正交）。LuotiAni 为 header-only（无独立 .cpp，实现内联于 include/LuotiAni.h）。

### 2.3 决策依据

- "图片控件"是 UI 框架基础能力，当前 C ABI 完全缺失；
- Actor 已是 Control，暴露成本极低（仅 C ABI 包装 + 属性映射），**不需要新建类、不改绘制引擎、不改事件体系**；
- 与多实例设计（UIInstance 首参）天然兼容（两阶段加载已适配，Actor.cpp:53-62）。

## 3. 架构选择

| 方案 | 说明 | 结论 |
|------|------|------|
| **A（选定）**：直接暴露 Actor | C ABI 工厂创建 `shared_ptr<Actor>` 挂入 `instance->bench`，属性经现有 property 系统映射 | 零架构改动；与 CreateImageButton 内部实现同构（同是 Actor 挂 bench） |
| B：新建 ImageView 类包装 Actor | 多一层继承/组合，仅转发接口 | 排除：无收益，徒增复杂度 |
| C：仅扩展 Panel 背景图 | 只能解决背景场景，装饰/图标仍需通用控件 | 排除：与 A 不冲突，可后续独立做 |

选定方案 A，与同类控件（Slider/ProgressBar 等）保持一致的继承模式（直接继承 ControlImpl 体系）与属性分发惯例（strcmp 分发 + `_stricmp` 枚举解析，见 ProgressBar.cpp:316-342）。

## 4. 功能规格

### 4.1 核心功能

- **图片显示**：文件路径（相对路径经基路径拼接）或资源 ID（经 ResourceProvider）
- **缩放模式**：`stretch`（拉伸填满，默认）/ `fit-center`（等比居中）/ `center-crop`（等比裁剪）/ `none`（原始尺寸居中）
- **matchParentRect**：强制匹配父控件绘图区域尺寸（开启后 w/h 被父尺寸覆盖）
- **透明度**：alpha 0-255
- **锚点**：9 种（top-left/mid-left/bottom-left/top-right/mid-right/bottom-right/top-center/center/bottom-center）
- **无交互**：不消费任何事件、不可聚焦、无状态、无边框/背景绘制
- **rect 语义（行为修正，详见 §6.1）**：显式 rect 优先；w/h 传 0 表示按纹理自然尺寸

### 4.2 与按钮状态 Actor 的关系

同一 Actor 类两种挂载方式，互不影响：

| 挂载方式 | 语义 | 入口 |
|----------|------|------|
| 非 children 挂载（现有） | 按钮状态图/关闭按钮图，setParent 仅挂渲染链 | `setNormalStateActor` 等 |
| **children 挂载（新增）** | 独立图片控件，加入控件树（渲染顺序=添加顺序） | `UICornerstone_CreateImage` |

### 4.3 不做的事（范围界定）

- 不新增动图（LuotiAni 已存在）、不新增渲染后端、不改资源加载机制
- 不修改 Button/WinFrame/Panel/LuotiAni 现有 Actor 用法
- 不加点击/悬停等交互属性（如需交互用 CreateImageButton）
- **LayoutParser JSON 布局接入（本期不做）**：parse 分发链（LayoutParser.cpp:221-266）新增 "Image" 类型属正交扩展，留待后续独立排期

## 5. 接口设计

### 5.1 Actor 类新增（C++ 侧）

```cpp
// include/Actor.h 新增：
class Actor: public Material {
public:
    void setMatchParentRect(bool match) { m_matchParentRect = match; }
    void setAlpha(uint8_t alpha) { m_alpha = alpha; }
    uint8_t getAlpha() const { return m_alpha; }

    // 事件命中：纯显示控件不参与事件命中与遮挡检测
    // （ControlBase.cpp:309-316 的 covered 检测只看 visible+isContainsPoint、不看事件是否消费，
    //  不重写的话 Image 会屏蔽其下层兄弟控件的事件——详见 §6.2）
    bool isContainsPoint(float x, float y) override { return false; }

    // 属性系统重写（实现放 src/Actor.cpp，惯例同 Button.cpp:335-353）：
    int setStringProperty(const char* prop, const char* value) override;  // "image" / "image-resource"
    int setBoolProperty(const char* prop, int value) override;            // "match-parent-rect"
    int setIntProperty(const char* prop, int value) override;             // "alpha"
    int setEnumProperty(const char* prop, const char* value) override;    // "scale-type" / "anchor"
    // Getter（T3 往返测试需要）：
    int getEnumProperty(const char* prop, const char*& out) override;     // "scale-type" / "anchor"
    int getBoolProperty(const char* prop, int& out) override;             // "match-parent-rect"
    int getIntProperty(const char* prop, int& out) override;              // "alpha"
    // 注意：image/image-resource 不做 getter（m_filePath 是 fs::path，string() 返回临时
    // 对象，const char*& out 会悬垂；与 ProgressBar 的成员 string 可持久不同）——只写不读

    // draw(void) 重写：使用成员 alpha（原走 Material::draw 默认 255）
    void draw(void) override;

private:
    uint8_t m_alpha = 255;
};
```

要点：

- `ActorBuilder::setMatchParentRect` 保留并委托到新公开方法（Actor.cpp:228-231 现直接访问私有成员，改造后调用 `setMatchParentRect`）。
- `draw(void)` 实现：`draw(m_rect.left + m_anchorPoint.x, m_rect.top + m_anchorPoint.y, m_alpha);`（与 Material::draw 同构，Material.cpp:37-39）。
- 枚举解析惯例同 ProgressBar.cpp:330-338（`_stricmp` 大小写不敏感，逐值匹配）。

### 5.2 PropertyNames 常量（include/PropertyNames.h 新增）

```cpp
// -- Image --
PROP_CONSTEXPR const char* kImage            = "image";             // 文件路径
PROP_CONSTEXPR const char* kImageResource     = "image-resource";   // 资源 ID
PROP_CONSTEXPR const char* kScaleType         = "scale-type";       // 枚举
PROP_CONSTEXPR const char* kMatchParentRect   = "match-parent-rect";// 布尔
PROP_CONSTEXPR const char* kAlpha             = "alpha";            // 整数 0-255
PROP_CONSTEXPR const char* kAnchor            = "anchor";           // 枚举
```

### 5.3 C ABI 工厂（include/UICornerstoneAPI.h / src/UICornerstoneAPI.cpp）

```c
// image 为文件路径（可为 NULL，之后经 UICornerstone_SetString(inst, ctl, "image", path) 设置；
// 资源 ID 经 "image-resource" 设置）
UICORNERSTONE_API UIControlHandle UICornerstone_CreateImage(
    UIInstance instance,
    const char* image,
    float x, float y, float w, float h);
```

实现（仿 CreateImageButton，UICornerstoneAPI.cpp:854）：

```cpp
UIControlHandle UICornerstone_CreateImage(UIInstance instance,
    const char* image, float x, float y, float w, float h)
{
    if (!instance || !instance->initialized) return nullptr;
    // Actor 构造家族无 SRect 参数（Actor.h:23-26，仅 parent/xScale/yScale/filePath/resourceId），
    // 用构造 + setRect（构造时已 setParent(bench)，Actor.cpp:10）
    auto actor = std::make_shared<Actor>(instance->bench);
    actor->setRect(SRect(x, y, w, h));                 // 显式 rect 优先（见 §6.1）
    if (image) actor->loadFromFile(fs::path(image));   // 两阶段：挂树后 create() 加载
    instance->bench->addControl(actor);                // shared_ptr 生命周期安全
    actor->create();
    actor->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(actor.get()));
}
```

声明插入位置：UICornerstoneAPI.h:289（CreateImageButton 之后）。

### 5.4 属性表（C ABI 侧）

| 属性名 | 类型 | 取值 | 说明 |
|--------|------|------|------|
| `image` | String | 文件路径 | 相对路径经基路径拼接（同 CreateImageButton）；**只写不读** |
| `image-resource` | String | 资源 ID | 经 ResourceProvider 读取；**只写不读** |
| `scale-type` | Enum | `stretch`/`fit-center`/`center-crop`/`none` | 默认 `stretch`；可读 |
| `match-parent-rect` | Bool | 0/1 | 开启后 w/h 被父尺寸覆盖；可读 |
| `alpha` | Int | 0-255 | 默认 255；可读 |
| `anchor` | Enum | 9 种锚点 | 默认 `top-left`；可读 |

只写不读原因：`m_filePath` 为 `fs::path`，`getStringProperty` 的 `const char*&` 出参需要持久 char* 缓冲，`string()` 返回临时对象会悬垂（与 ProgressBar 的成员 `std::string m_customText` 可直接 `c_str()` 不同）；不引入缓存成员，保持最小改动。GetString 对 image/image-resource 返回 0（不支持）。

通用属性（rect/visible/enable 等）走 ControlImpl 既有实现，无需重写。

## 6. 行为语义

### 6.1 加载 rect 行为修正（现有代码缺陷，必须随本设计修正）

`Actor::loadTextureFromSurface`（Actor.cpp:122-132）与 `loadFromFile` 的 fallback 分支（Actor.cpp:82-90）在加载成功时**无条件重置 `m_rect`**（left/top 清零、w/h 取纹理自然尺寸或父尺寸）——若不修正，C ABI 传入的 x/y/w/h 在加载后全部丢失（现有按钮状态图用法因 `matchParentRect=true` 走父尺寸分支，未暴露此问题）。

**修正为统一规则**：

| m_matchParentRect | m_rect 显式尺寸（w/h>0） | 加载后 rect |
|---|---|---|
| true | — | 父控件尺寸（现状不变） |
| false | w<=0 或 h<=0 | 纹理自然尺寸（现状不变），left/top 保持 0 |
| false | w>0 且 h>0 | **保留显式 rect（新行为）** |

**向后兼容验证**（现有用法全部走前两行，行为不变）：Button 四态 Actor / WinFrame 关闭按钮 / LuotiAni 帧（均为 matchParentRect=true，或未设 rect 的 test_button ActorBuilder 用法）。

**新语义**：Image 控件创建时 w/h 传 0 表示按纹理自然尺寸（装饰图标常用）；挂树后 `SetString("image")` 换图时，显式 rect 保留、自然尺寸跟随新图。

### 6.2 事件语义（含遮挡修正）

- **不消费事件**：不重写 `handleEvent`（ControlImpl 默认只向子控件透传，ControlBase.cpp:276-314），自身点击无反应。
- **遮挡修正（必须）**：`ControlImpl::handleEvent` 的 covered 检测（ControlBase.cpp:309-316）只看 `getVisible() && isContainsPoint()`、**不看事件是否消费**——若 Image 不重写 `isContainsPoint`，处于上层的 Image 会屏蔽其下层兄弟控件（如：先加 Button 再叠 Image，点击重叠区 Button 收不到事件）。**Actor 重写 `isContainsPoint` → false**（纯显示控件不参与事件命中），已列入 §5.1。isContainsPoint 的其余使用处（ComboBox/ColorPicker/TextArea/TreeView/Menu 等控件自身事件逻辑、Dialog 的 anchor 检测）均针对各自控件，不受影响。
- **非容器**：Image 不应挂子控件（`addControl` 子控件后，因 Image 的 isContainsPoint=false，子控件在其区域内的事件会被上层 covered 检测跳过）。
- **渲染顺序**：按 `addControl` 添加顺序绘制（后加在上层，与 ControlImpl 惯例一致）。

### 6.3 其余语义

- **matchParentRect 时序**：在 `setParent` 时匹配父控件当前尺寸（Actor.cpp:141-156）；父控件后续 resize 不自动跟随（现有行为，保持）。挂入 Panel 布局系统时以布局后尺寸为准。
- **锚点**：仅影响绘制偏移（Material::draw 在 mapToDrawRect 前减去锚点偏移，Actor.cpp:170-174），不参与布局系统。
- **焦点**：不可聚焦（默认 `isFocusable()==false`），不参与 Tab 循环与焦点环。
- **多实例**：两阶段加载已适配（setContext → create()），无新增工作。
- **销毁**：`UICornerstone_DestroyControl` 通用路径，无特殊处理。

## 7. 测试计划

新增 `test/test_image.cpp`（纯 DLL 动态加载模式，结构仿 test_multi_instance_cabi.cpp），注册到 `UI_TEST_EXECUTABLES`（CMake 按 `${test_name}.cpp` 定位，test/CMakeLists.txt:117，命名 test_image）：

| # | 用例 | 步骤 | 预期 |
|---|------|------|------|
| T1 | 工厂创建 | CreateImage（文件路径，显式 w/h） | 句柄非空；GetRect 与创建参数一致（验证 §6.1 显式 rect 保留） |
| T2 | 自然尺寸 | CreateImage（w=0, h=0） | GetRect 返回纹理自然尺寸（验证 §6.1） |
| T3 | 属性往返 | SetEnum("scale-type")/SetEnum("anchor")/SetBool("match-parent-rect")/SetInt("alpha") → 对应 GetEnum/GetBool/GetInt | 往返一致；GetString("image") 返回 0（只写不读） |
| T4 | matchParentRect | 挂 Panel 下开启 | 渲染尺寸跟随父矩形 |
| T5 | 事件遮挡修正 | Button 与 Image 重叠，两种叠加顺序各点一次 | 两种顺序下 Button 回调均触发（验证 §6.2 isContainsPoint=false） |
| T6 | 锚点 | SetEnum("anchor") 各值 | 位置偏移正确（渲染冒烟） |
| T7 | 渲染冒烟 | 60 帧 Update/Render 循环 | 不崩溃、无异常纹理状态 |
| T8 | 挂树后换图 | SetString("image") 换图 | 显式 rect 保留；自然尺寸跟随新图 |

回归（Actor 加载 rect 修正的向后兼容验证，§6.1）：test_button（四态图 + animation 属性路径）、test_winframe（关闭按钮）——现有用法行为不变。LuotiAni 为 header-only（include/LuotiAni.h 内联实现），随 test_button 的 animation 用例一并覆盖。

## 8. 文档同步

| 文档 | 改动 |
|------|------|
| `doc/UICornerstone_DLL_Design.md` | API 清单加 `UICornerstone_CreateImage`（:424 CreateImageButton 附近） |
| `doc/CABI_MultiInstance_Design.md` | API 迁移表加 CreateImage 行（:404 附近）；§6 实施清单加项 |
| `doc/CABI_Property_Design.md` | Image 属性表（§5.4 内容，含 match-parent-rect 覆盖 w/h 语义） |
| `doc/guidelines/history.md` | 记录实施与验证结果 |

## 9. 实施顺序与验收标准

| 步骤 | 内容 | 验收标准 |
|------|------|----------|
| 1 | Actor 控件化：新增 setMatchParentRect/setAlpha/getAlpha/isContainsPoint/draw(void)、**加载 rect 修正（§6.1，两处）**、属性重写（setter + getter）+ PropertyNames 常量 | 编译通过；test_button/test_winframe/LuotiAni 无回归（按钮状态图、关闭按钮行为不变） |
| 2 | C ABI 工厂 + 头文件声明 | 声明导出；编译链接通过 |
| 3 | 测试 test_image.cpp + CMake 注册 | T1-T8 全绿 |
| 4 | 文档同步 4 处 | 审核通过后合并 |

## 10. 风险与注意事项

1. **加载 rect 修正的回归面（§6.1）**：这是对现有 Actor 行为的两处代码修改（loadTextureFromSurface、loadFromFile fallback），虽向后兼容，但回归必须覆盖 test_button/test_winframe（LuotiAni 为 header-only，经 test_button 的 animation 属性用例覆盖）。
2. **alpha 语义**：仅影响独立控件 `draw(void)` 路径；按钮状态 Actor 的 alpha 仍由 Button 绘制时传入，不受影响。
3. **`image-resource` 依赖 ResourceProvider**：未配置 provider 时设置无效（现有行为，Actor.cpp:101-105），文档属性表注明。
4. **isContainsPoint=false 的副作用面**：Image 不可作为容器（子控件事件被屏蔽）；若未来需要"可点击图片"，应新增交互控件而非放宽此限制。
