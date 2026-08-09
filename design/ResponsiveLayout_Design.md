# 响应式布局增强设计文档（ResponsiveLayout）

> 状态：**待审核**（未审核通过前不做源码变更）
> 日期：2026-08-09
> 适用范围：`design/LayoutSystem_Design.md` 已定义布局系统之上的**响应式增强**（窗口缩放自适应、断点布局切换、尺寸约束）。实施者为主设计开发 Session，本文档按「先验证 → 再修 → 后补测试」原则组织。

## 1. 概述

本设计为 UICornerstone 布局系统补齐三类响应式能力，目标场景：

1. **窗口缩放自适应（核心）**：窗口/视口缩放时，嵌套容器内的控件按比例伸缩、弹性分配，布局自动重排。
2. **断点布局切换**：容器宽度跨过阈值时切换布局引擎/参数（如 h-flow → v-flow），隐藏或重排次要控件。
3. **尺寸约束**：布局引擎分配尺寸后按 `min/max` 夹逼、按 `aspect-ratio` 校正，防止控件被压扁/撑爆。

设计原则（与 Image_Design.md 一致）：

- **零架构改动**：全部基于既有 `LayoutEngine` 抽象、`Panel` 侧表、`SRect` 百分比字段与 resize 链路，不引入新的布局框架。
- **C ABI 零改动**：布局属性是容器级声明式数据（JSON / C++ 侧表），不进入控件属性系统。
- **向后兼容**：现有 JSON 布局文件、现有引擎行为、现有控件用法全部不变（增量能力）。

## 2. 现状分析

### 2.1 已具备的响应式能力（核实于 2026-08-09）

| 能力 | 实现 | 证据 |
|------|------|------|
| 百分比尺寸 | `SRect` 内嵌 `leftIsPct/...Pct` 字段，`resolve(cw,ch)` 幂等换算 | Utility.h:193-194, 273-277 |
| 弹性分配 | HFlow/VFlow `flexWeight`，第二趟 `flexUnit = max(0, remaining)/totalFlex` 分配 | LayoutEngine.h:13-15; LayoutEngine.cpp:38, 52, 100, 114 |
| 网格弹性 | `GridSize{Fixed,Flex,Auto}`，Flex 按权重分配剩余，支持 span | LayoutEngine.h:71-75; LayoutEngine.cpp:132-261 |
| 锚点拉伸/填充 | 9 对齐 + 5 拉伸（top/bottom/left/right-stretch、fill） | LayoutEngine.cpp:268-336 |
| 容器重排入口 | `Panel::setRect/resized` override → 有引擎 reflow / 无引擎 resolve 百分比 | Panel.cpp:59-75 |
| 窗口 resize 链路 | 后端窗口事件 → `bench->resized`（Owned 循环记录 :81/:84、debounce 后 :134 派发 / C ABI 事件泵与注入队列） | MainWindow.cpp:81-135; UICornerstoneAPI.cpp:627,668; Bench.cpp:115-117 |
| 嵌套递归（间接） | 引擎 reflow 与百分比 resolve 均经 `child->setRect` → 子 Panel 的 `setRect` override 会递归触发其自身 reflow/resolve | LayoutEngine.cpp:59,121; Panel.cpp:59-66 |
| JSON 布局 | `layout{type,gap,padding,columns,rows}` + 子控件 `flowWeight/anchor/grid` + rect 百分比 | LayoutParser.cpp:748-869; 2303-2320; 2487-2508 |

### 2.2 缺口清单（本设计要解决的）

| # | 缺口 | 证据 | 影响 |
|---|------|------|------|
| G1 | `ControlImpl::resized` 只更新宽高**不递归**；`resolveChildPercentages`/`reflowChildren` 只做一层，嵌套 Panel 断链**待实证**（见 §3 阶段 0） | ControlBase.cpp:287-294; Panel.cpp:50-57, 68-75；LayoutSystem_Design.md:958 自述限制 | 多层嵌套时 resize 可能不传播到最内层 |
| G2 | `moved()` 为死代码，位置变化无刷新（**本设计判定：非缺口**——子控件坐标为父相对，渲染经 mapToDrawRect 自动跟随，见 §5.4） | ControlBase.cpp:291-294；仅定义无调用 | 无 |
| G3 | 无 min/max/aspect-ratio 约束：flex 分配后不夹逼，窄屏下控件被压扁 | FlowItemProps 仅 flexWeight（LayoutEngine.h:13-15） | 弹性控件无法保底/限幅 |
| G4 | 无断点机制：布局引擎/参数静态，无法随宽度切换 | Panel.h:19 单一 `m_layoutEngine` | 窄窗口无法换布局 |
| G5 | 布局引擎数学无单元测试，级联行为无断言 | test_layout/test_layout_advanced 仅视觉验证 | 回归无保障 |
| G6 | `setRect` → `recreate()` 级联（Label/CheckBox），响应式放大后性能隐患 | Label.cpp:457-462; CheckBox.cpp:288-293 | 高频 resize 时开销 |

### 2.3 与主设计开发 Session 现行工作的关系

- 当前 main 工作区含未提交改动：`binding/*`（C++ Binding 实施）、`include/UICornerstoneAPI.h`、`src/UICornerstoneAPI.cpp`。本设计的文件集（`include/Panel.h`、`include/LayoutEngine.h`、`src/Panel.cpp`、`src/LayoutEngine.cpp`、`src/LayoutParser.cpp`、`test/*`、`design/*`）**与其无交集**，可独立实施。
- 布局系统与 LuotiAni（include/LuotiAni.h）无文件交集。

## 3. 方案

分四个阶段，每阶段独立可验收：

### 阶段 0：缺口实证（必须先做）

`resize 递归断链（G1）` 的结论来自设计文档自述（LayoutSystem_Design.md:958），而实现的 `setRect` 递归链可能已覆盖大部分场景。**先写实证测试再决定修法**，避免修不存在的 bug：

- 新增 `test/test_layout_responsive.cpp`（定义见 §6）中的 **E1 用例**：真实实例 → `bench->resized(0,0,W,H)` 模拟窗口缩放 → 断言两层嵌套百分比 Panel 的最内层尺寸。
- **E1 通过** → G1 关闭跟踪，仅保留测试作回归；**E1 失败** → 按 §4.1 修复。

### 阶段 1：传播链修复（G1，仅在 E1 失败时执行）

### 阶段 2：尺寸约束（G3）

在布局引擎分配尺寸后统一夹逼/校正：

- `LayoutEngine.h` 新增通用约束结构（见 §4.2），扩展 `FlowItemProps` 与 `GridItemProps`。
- HFlow/VFlow/Grid 的 flex 分配结果在 `child->setRect` 前应用 clamp + aspect 校正（插入点：HFlow :51-53 / VFlow :113-115 / Grid 列 :179-183、行 :219-223）。
- `LayoutParser` 解析子控件 `min-width/max-width/min-height/max-height/aspect-ratio` 写入侧表。

### 阶段 3：断点布局切换（G4）

- `Panel` 增加断点表（`minWidth/maxWidth → LayoutEngine` 映射），`setRect/resized` 时按**自身宽度**评估命中项并切换引擎（见 §4.3）。
- `LayoutParser` 解析 `layout.breakpoints` 数组（JSON 结构见 §4.4）。

### 阶段 4：测试与文档同步（G5、G6）

- 新增布局引擎单元测试（§6）；回归清单见 §6.3。
- G6（recreate 级联）本设计**不修**（避免放大改动面），仅列入风险观察项。

## 4. 接口设计

### 4.1 传播链修复（阶段 1，E1 失败时启用）

| 改动点 | 内容 |
|--------|------|
| `Panel::resized`（Panel.cpp:68-75） | 追加：遍历直接子控件，对**子 Panel** 调 `child->resized(SRect(0,0,父宽,父高))`（若子 Panel 有百分比或引擎则其自身 setRect 链已覆盖，此步仅覆盖「子 Panel 自身 rect 无百分比但含百分比子孙」的断链） |
| `Panel::resolveChildPercentages`（Panel.cpp:50-57） | 追加：对子 Panel 递归调用 `resolveChildPercentages`（同步修正深层百分比） |
| 防循环 | 重排/递归仅发生在 `resized`/`setRect` 事件路径；`resolve` 幂等（Utility.h:273-277）天然可重入；不引入深度标记（控件树为有向无环，父链不变） |
| `moved()` | **不接线**（见 §2.2 G2 判定） |

**不做**：修改 `ControlImpl::setRect` 全局加通知（会放大 recreate 级联与重排频率，G6）。

### 4.2 尺寸约束（阶段 2）

```cpp
// LayoutEngine.h —— 新增
struct LayoutConstraints {
    float minWidth  = 0.0f;      // 0 = 不限
    float maxWidth  = 0.0f;      // 0 = 不限
    float minHeight = 0.0f;
    float maxHeight = 0.0f;
    float aspectRatio = 0.0f;    // w/h；0 = 不限。仅当 min/max 未同时固定宽高时生效
};

// FlowItemProps 扩展
struct FlowItemProps {
    float flexWeight = 1.0f;
    LayoutConstraints constraints;
};

// GridItemProps 扩展
struct GridItemProps {
    int row = 0, col = 0, rowSpan = 1, colSpan = 1;
    LayoutConstraints constraints;
};
```

**应用规则**（实现于 `LayoutEngine.cpp` 共享辅助函数 `applyConstraints(float& w, float& h, const LayoutConstraints&)`）：

1. flex 分配得到 `(w, h)` 后依次 clamp：`w = clamp(w, minW>0?minW:0, maxW>0?maxW:INF)`，h 同理。
2. `aspectRatio > 0` 时校正：以 clamp 后的 `w` 为准 `h = w / aspectRatio`，再对 `h` 做一次 clamp。
3. **边界规则**：`min > max` 时以 `min` 为准；aspect 校正后仍越界 → 回退到纯 clamp 结果（保底不收缩）。
4. **适用边界**：约束只作用于**引擎弹性分配**的尺寸（flex 分配、Grid flex 列/行）；**Anchor stretch/fill 不夹逼**（offset 是作者显式意图，LayoutEngine.cpp:313-332）；无引擎绝对定位下显式 rect 不受夹逼（绝对定位是作者自定尺寸）。

**JSON Schema**（子控件级，LayoutParser 解析，仅数值 px）：

```json
{
  "type": "panel",
  "rect": { "x": 0, "y": 0, "w": "100%", "h": "100%" },
  "layout": { "type": "h-flow", "gap": 8 },
  "children": [
    { "type": "button", "rect": { "w": 120, "h": 40 },
      "flowWeight": 1,
      "min-width": 80, "max-width": 240,
      "min-height": 32, "aspect-ratio": 2.5 },
    { "type": "button", "rect": { "w": 120, "h": 40 }, "flowWeight": 2 }
  ]
}
```

### 4.3 断点布局切换（阶段 3）

```cpp
// Panel.h —— 新增
struct Breakpoint {
    float minWidth = 0.0f;           // 0 = 不设下限
    float maxWidth = 0.0f;           // 0 = 不设上限
    shared_ptr<LayoutEngine> engine; // 命中后切换的引擎（含 gap/padding/columns/rows 完整配置）
};
```

- `Panel` 新增 `vector<Breakpoint> m_breakpoints;`、`int m_activeBreakpoint = -1;`（-1 = 用 base 引擎）。
- 新增 `void addBreakpoint(Breakpoint bp);`
- **评估时机**：`Panel::setRect/resized` 中、reflow 之前，调用新增私有方法 `evaluateBreakpoints()`；基准 = **Panel 自身宽度** `m_rect.width`（支持嵌套，每个容器独立评估）。
- **命中规则**：按插入顺序取**第一个**满足 `width >= minWidth && (maxWidth == 0 || width <= maxWidth)` 的断点；命中则 `setLayoutEngine(bp.engine)`；未命中则恢复 base 引擎（base 可空——`setLayoutEngine(nullptr)` 后回归百分比 resolve 模式，Panel.cpp:59-75 现状逻辑自动适配）。
- **缓存**：`m_activeBreakpoint` 缓存上次命中项——**仅命中项变化时切换引擎**（避免重建）；reflow 由既有 setRect/resized 路径照常执行（同一断点内的宽度变化仍重排 flex）。
- **C++ API 用法**：

```cpp
auto panel = make_shared<Panel>(parent, rect);
panel->setLayoutEngine(make_shared<HFlowLayout>(8));
panel->addBreakpoint({0, 480, make_shared<VFlowLayout>(4)});   // 窄于 480 → 垂直流
panel->addBreakpoint({480, 0, make_shared<HFlowLayout>(8)});   // 宽于 480 → 水平流
```

### 4.4 断点 JSON Schema（阶段 3）

```json
{
  "type": "panel",
  "rect": { "x": 0, "y": 0, "w": "100%", "h": "100%" },
  "layout": {
    "type": "h-flow", "gap": 8,
    "breakpoints": [
      { "max-width": 480, "layout": { "type": "v-flow", "gap": 4 } },
      { "min-width": 800, "layout": { "type": "grid", "gap": 8,
          "columns": ["1fr", "1fr", "1fr"] } }
    ]
  },
  "children": [ /* 同 §4.2 */ ]
}
```

- `breakpoints` 数组内 `layout` 对象与顶层 `layout` **同构**（type/gap/padding/columns/rows 全部可用）。
- 解析实现：`LayoutParser::parsePanel`（LayoutParser.cpp:791-827）追加断点解析，复用现有引擎构造分支（:803-826）。

### 4.5 C++ API 汇总（新增）

| 位置 | 新增 |
|------|------|
| `include/LayoutEngine.h` | `struct LayoutConstraints`；`FlowItemProps`/`GridItemProps` 扩展 |
| `include/Panel.h` | `struct Breakpoint`；`vector<Breakpoint> m_breakpoints`；`int m_activeBreakpoint`；`void addBreakpoint(Breakpoint)` |
| `src/Panel.cpp` | `setRect/resized` 内断点评估（`evaluateBreakpoints`）；`resizeChildPanels`（阶段 1 用） |
| `src/LayoutEngine.cpp` | `applyConstraints` 辅助 + 三引擎接入 |
| `src/LayoutParser.cpp` | 约束字段解析（§4.2 JSON）、`breakpoints` 解析（§4.4） |
| `test/CMakeLists.txt` | 注册 `test_layout_engine`（标准测试）、`test_layout_responsive`（标准测试） |

### 4.6 C ABI 边界

**零改动**。理由：布局属性是容器级声明式数据（JSON/侧表），不进入控件属性系统；窗口缩放经既有 `SetViewport` → `bench->setRect`（UICornerstoneAPI.cpp:519-524）链路自动覆盖。断点/约束不导出新函数。

## 5. 行为语义

### 5.1 触发时机与职责划分

| 事件 | 触发链 | 本设计新增动作 |
|------|--------|----------------|
| 窗口 resize | 后端 → `bench->resized`（MainWindow.cpp:81-135 / UICornerstoneAPI.cpp:627,668） | Panel::resized 内：断点评估 → reflow/resolve → 阶段 1 子 Panel 递归 |
| 容器 setRect | 引擎 reflow / 显式调用 | Panel::setRect 内：断点评估 → reflow/resolve（现状保留） |
| 子控件属性变化 | setChildXxxProps（Panel.h:34-36） | 不自动 reflow（现状语义保持；作者需手动 reflow） |

### 5.2 幂等与性能

- `resolve()` 幂等（Utility.h:273-277）；`reflowChildren` 每次以容器当前 rect 全量重算（LayoutEngine.cpp:8-63）——**全量语义保持**，不做脏标记（改动面控制）。
- 断点切换仅在命中项变化时发生（`m_activeBreakpoint` 缓存），resize 期间引擎不重建。
- G6（recreate 级联）为已知风险：响应式放大 setRect 频率后，Label/CheckBox 的 recreate（Label.cpp:457-462）开销上升。**本设计不修**，列入风险清单（§9），后续单独评估脏标记。

### 5.3 兼容性

| 现有用法 | 影响 |
|----------|------|
| 现有 JSON 布局文件（test_layout.json / test_layout_advanced.json） | 无新字段 → 解析路径不变，零影响 |
| 无引擎 Panel 百分比行为 | 不变（setRect/resized → resolveChildPercentages 现状保留） |
| 引擎 reflow 数学（flex/grid/anchor） | 无约束字段时 `LayoutConstraints` 全 0 → clamp/aspect 均跳过，输出与现状逐位一致 |
| 断点未配置 | `m_breakpoints` 空 → 评估短路，零开销 |
| 多实例/多视口 | 断点基准为 Panel 自身宽度，与实例/视口无关；缩放经既有链路 |

### 5.4 G2 澄清（moved 不接线）

子控件坐标是**父相对**坐标；渲染经 `mapToDrawRect`（ControlBase.h:216）做父链变换。容器移动时子控件无需任何刷新即自动跟随。因此 `moved()` 死代码**不构成响应式缺口**，本设计不触碰（避免无谓改动与回归面）。

### 5.5 边界情况

| 场景 | 规则 |
|------|------|
| `min > max` | 以 `min` 为准 |
| aspect 与 clamp 冲突 | 先 clamp 后校正，校正越界则回退 clamp 结果（保底不收缩） |
| flex 剩余空间为负（`remaining < 0`） | 现状：`flexUnit = 0`（LayoutEngine.cpp:38,100），flex 子件取 `w = 0`；约束的 minWidth 此时**强制生效**（保底宽度）——这是约束模型对窄屏的核心收益 |
| 断点区间重叠 | 取插入顺序第一个命中项 |
| 断点与 base 无交集 | 永不命中 → base 引擎 |
| 约束仅一侧指定（如仅 min-height） | 另一侧自由，不约束 |

## 6. 测试计划

### 6.1 `test/test_layout_engine.cpp`（新增，纯逻辑单元测试，无需窗口）

直接构造 `Panel` + 子控件 + 引擎，断言 `setRect` 结果。注册标准测试（链接静态库，test/CMakeLists.txt 现有循环）。

| # | 用例 | 断言 |
|---|------|------|
| L1 | HFlow：flex 1:2 + 固定 120px，容器 400px | 固定子件宽 120；flex 子件按 `(400-gap-120)/3`、`×2` 分配 |
| L2 | VFlow 对称用例 | 高度分配正确 |
| L3 | HFlow + min-width 夹逼 | 窄容器（剩余为负）时 flex 子件取 minWidth，不塌陷为 0 |
| L4 | max-width 夹逼 | 宽容器时 flex 子件封顶 |
| L5 | aspect-ratio 校正 | 分配后 `h = w/ratio` 精确成立 |
| L6 | min>max 边界 | 以 min 为准 |
| L7 | Grid：`["1fr","1fr","1fr"]` + span | 列宽均分；colSpan 占两列宽 |
| L8 | Anchor：fill 与 top-stretch | 填充/拉伸尺寸 = 容器内缩 offset 后尺寸 |
| L9 | 断点切换 | addBreakpoint 后 setRect 跨阈值 → `getLayoutEngine()->getType()` 切换；回退 → 恢复 base |
| L10 | 无约束回归 | 全 0 约束输出与现状一致（快照式断言） |

### 6.2 `test/test_layout_responsive.cpp`（新增，实例级集成测试）

真实实例 + `bench->resized(0,0,W,H)` 模拟窗口缩放（可加 `SetViewport` 变体）。

| # | 用例 | 断言 |
|---|------|------|
| E1 | 嵌套断链实证（§3 阶段 0） | 两层嵌套百分比 Panel，resize 后最内层宽高 = 父×百分比（若实测已通则此用例即回归；失败则阶段 1 修复后通过） |
| R1 | 顶层 100% Panel + 子 h-flow | 窗口 resize → 顶层尺寸跟随、flex 子件按新宽度重排 |
| R2 | 断点切换（实例级） | 窗口跨 480 → 布局引擎切换（子件排列方向变化可断言 x/y 顺序） |
| R3 | 约束生效 | 窄窗口下 flex 子件宽 = min-width（防塌陷） |
| R4 | 多视口隔离 | 两个视口不同尺寸 → 各自断点独立评估 |

### 6.3 回归清单

- `test_layout` / `test_layout_advanced`（JSON 布局文件位于 `layouts/`，可缩放窗口已启用：test_layout_advanced.cpp:124,147）：JSON 布局解析 + 视觉验证（三后端）
- 全量现有测试：6 棵构建树（sdl3/sfml/raylib × 标准/_dll）
- 与主 Session 协调：构建期间独占 build/ 树

## 7. 实施顺序与工作量（供主 Session 排期）

| 步骤 | 内容 | 依赖 | 工作量 |
|------|------|------|--------|
| 0 | `test_layout_responsive.cpp` 之 E1 + 基础设施（实例 + bench->resized 模拟） | 无 | 中 |
| 1 | E1 结果分流：通过 → 跳过 4.1；失败 → 按 4.1 修复 | 步骤 0 | 小~中 |
| 2 | 约束模型：LayoutEngine.h + applyConstraints + 三引擎接入 + L1-L6/L10 | 无 | 中 |
| 3 | 约束 JSON 解析 + L7/L8 | 步骤 2 | 小 |
| 4 | 断点：Panel 断点表 + 评估 + L9 | 无 | 中 |
| 5 | 断点 JSON 解析 + R2/R3/R4 | 步骤 4 | 小 |
| 6 | 回归 + 文档同步（§8）+ history.md 追加 | 全部 | 小 |

建议单次提交覆盖步骤 0-6，提交信息建议：`feat: 响应式布局增强（断点切换 + 尺寸约束 + resize 传播验证）`。

## 8. 文档同步清单

| 文档 | 动作 |
|------|------|
| `design/LayoutSystem_Design.md` | §4.9 布局引擎章节追加「响应式增强」小节（约束/断点/传播），更新 :958 处自述限制为实证结论 |
| `design/ResponsiveLayout_Design.md`（本文档） | 状态 → 已审核 |
| `README.md` | 布局特性段补充响应式能力描述 |
| `design/guidelines/history.md` | 追加实施记录 |

## 9. 风险与注意事项

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| 1 | E1 实测已通（断链不存在）→ 阶段 1 改动白做 | 低 | 阶段 0 先实证，文档 4.1 按结果启用 |
| 2 | setRect 频率放大 → Label/CheckBox recreate 开销（G6） | 中 | 本设计不触碰 ControlImpl::setRect；若回归测试暴露性能问题，后续单独立项脏标记 |
| 3 | 断点切换引擎重建的瞬时开销 | 低 | `m_activeBreakpoint` 缓存，仅命中项变化时切换 |
| 4 | 约束字段与既有 JSON 控件键冲突 | 低 | 使用 `min-width` 等新键名，与现有 `flowWeight/anchor/grid` 无重叠（LayoutParser.cpp:830-858 核对） |
| 5 | 与主 Session binding 改动并行时的构建协调 | 低 | 文件集无交集；构建独占协调既有机制 |

## 10. 核审记录

| 轮次 | 日期 | 核审内容 | 结果 |
|------|------|----------|------|
| R1 事实核审 | 2026-08-09 | Panel/LayoutEngine/ControlBase/LayoutParser/Bench/MainWindow 逐行核对（行号、行为、JSON 字段、resize 链路） | 修正：MainWindow 引用 78-139→81-135；Grid flex 插入点 209-225→179-183/219-223；UICornerstoneAPI 626-629→627,668；G2 判定修正（moved 非缺口）；断链结论改为「待实证」 |
| R2 完整性核审 | 2026-08-09 | 用户三场景覆盖、边界表、兼容性表、C ABI 边界、遗漏排查 | 修正：Anchor stretch/fill 不夹逼（offset 为作者意图）；断点 base 可空（回归 resolve 模式）；回归清单补 layouts/ 目录与可缩放窗口证据；断点评估方法名补 `evaluateBreakpoints` |
| R3 可实施性核审 | 2026-08-09 | 主 Session 视角：依赖顺序、工作量、验收标准、文档同步、实施细节完备性 | 修正：缓存语义澄清（仅命中项变化切换引擎，reflow 照常）；L9 断言改经 `getType()` 可测；提交信息与单次提交建议已备。核审通过，文档可交付审核 |
