# Shape（形状控件）需求分析

> 状态：**分析中 · 待拍板**
> 关联：[ListView_Analysis.md](ListView_Analysis.md)（属性四层一致性矩阵规则 §5.6）、[CABI_Property_Design.md](CABI_Property_Design.md)（通用属性系统）
> 效果图：[Shape_Preview.svg](Shape_Preview.svg)（嵌入 §3）

## 1. 需求概述

提供 Shape 控件：绘制基础几何图形（矩形/圆角矩形/圆/椭圆/折线/多边形），支持填充色、描边色、线宽——方便用户在界面上直接画出所需形状（装饰分割线、色块、示意图、图标底座等）。**含空心（仅描边）与圆环/椭圆环（环带）能力**。

**与现有能力的关系**：Panel 可填背景色 + 边框（矩形色块已可用）；Shape 补充**非矩形几何体**（圆/椭圆/圆角/折线/多边形）与**细粒度描边控制**。

## 2. 现状能力盘点（渲染原语层——先确认已支持什么）

**核心结论：渲染原语层已覆盖全部形状需求（实心/描边矩形、直线、三角形、任意凸多边形、圆/椭圆/圆角/环均可由现有原语组合表达），Shape 控件是纯属性层封装（fill/stroke/lineWidth/radius/ringWidth/points + 三角化算法 + JSON/CABI/Binding 四层），无需新增任何 RenderDevice 原语。**

| 渲染能力 | 现状 | 依据（`RenderDevice.h`） |
|---|---|---|
| 实心矩形 | ✅ 直接支持 | `fillRect`（:31） |
| 描边矩形 | ✅ 直接支持（固定 1px） | `drawRect`（:32） |
| 直线 | ✅ 直接支持（固定 1px） | `drawLine`（:33） |
| 点 | ✅ 直接支持 | `drawPoint`（:34） |
| 单色三角形/四边形 | ✅ 直接支持 | `drawTriangle`/`drawQuad`（:46-47） |
| 任意多边形（顶点带色） | ✅ 直接支持（凸语义） | `drawTriangles`/`Strip`/`Fan`，每顶点 `SColor`（:37-43） |
| 粗线 / 宽描边 | 🔧 组合：填充四边形（`drawQuad`/`fillRect` 边带） | 无 lineWidth 参数，1px 固定 |
| 圆 / 椭圆 | 🔧 组合：`drawTriangleFan` 扇形逼近（32 段，顶点色=填充色） | 无曲线/弧原语 |
| 圆角矩形 | 🔧 组合：四角扇形 + 中部 `fillRect` | 同上 |
| 环带 | 🔧 组合：外轮廓扇形 − 内轮廓反向扇形 | 同上 |
| 半透明 / 混合 | ✅ 顶点色 + `setBlendMode(Blend/Add/Mod/Mul)`（:23-24） | 透明 fill 天然支持 |
| 裁剪 | ✅ `setClipRect`/`pushClipRect`/`popClipRect`（:25-28） | 控件 rect 裁剪 |

**Shape 控件的新增面**（盘点后确认的增量，均非渲染原语）：
- 三角化算法：圆/椭圆/圆角/环的扇形顶点生成 + 粗线/描边边带生成（纯 CPU 几何计算）
- 属性层：`shape`/`fill`/`stroke`/`lineWidth`/`radius`/`ringWidth`/`points` 的 C++/JSON/CABI/Binding 四层封装
- 绘制排序：fill 先于 stroke、环带内孔透明

**现状限制**（与 Shape 需求相关）：`drawRect`/`drawLine` 线宽固定 1px（粗线需边带组合）；无曲线/弧原语（需三角化）；扇形逼近在段数足够时视觉无差（32 段）。

**属性系统现状**：`setColorProperty` 支持 `"background"`/`"border"`/`"text"`/`"selected"`/`"hover"` 等单字键（`ControlBase.cpp:876-903`）；`setBoolProperty` 支持 `"visible"`/`"enabled"`/`"transparent"`/`"border-visible"`（:916-926）；**`setFloatProperty`/`setEnumProperty`/`setIntProperty` 基类返回 0**——新属性需 Shape override（:908-914）。控件注册：`ControlType` 枚举（`ControlBase.h:139-144`）需新增 `Shape`；字符串键 kebab-case 惯例（`PropertyNames.h:565-582`）。

## 3. 架构方案

```
Shape : ControlImpl
├── m_shape（ShapeType：rect/filled-rect/round-rect/circle/ellipse/polyline/polygon）
├── m_fillColor（填充色）/ m_strokeColor（描边色）/ m_lineWidth（描边宽，px）
├── m_radius（圆角半径，round-rect 生效）
├── m_points（折线/多边形顶点，相对控件局部坐标）
└── draw()：按形状三角化/描边绘制（无布局、无事件、无子控件）
```

- **零布局/零交互**：Shape 无子控件、无事件、无焦点——只有 `draw()`（最简控件形态）
- **坐标体系**（决策点 3 拍板）：`circle`/`ellipse` 内切**整个控件 rect**（随 `setRect` 缩放，各向异性拉伸）；`round-rect` 同 rect 边；`polyline`/`polygon` 点为**控件本地坐标（像素 float，与其他控件一致）**，`setPoints` 记录基准 rect、resize 时按 `(newW/baseW, newH/baseH)` 等比缩放，并提供 `mapToDrawPoint(lx, ly)`（本地→全局，主 API）与 `getDrawPoint(index)`（顶点便捷查询，等价 mapToDrawPoint(points[i])）
- **空心（仅描边）**：`fill` 设透明（**缺省值即为透明**）→ 空心圆/空心椭圆/空心矩形——无需新形状，空心是 fill+stroke 的普通组合
- **环（圆环/椭圆环）**：`circle`/`ellipse` + `ringWidth > 0` 绘制环带（fill 色填充环带、内圆区域透明、stroke 可选外缘描边）；`ringWidth = 0`（缺省）为实心
- **缓存重算（决策点 7 补充）**：`setShape`/`setRect`/`resized`（布局引擎驱动）/`setPoints`/`setRadius`/`setRingWidth` 均强制重算——**resize 时缩放系数变化必须重算**（归一化点 × 新尺寸、内切椭圆随新 rect 变化）
- **三角化实现**（RenderDevice 无曲线图元）：
  - 圆/椭圆：`drawTriangleFan` 扇形逼近（圆心 + 32 段圆周顶点，色随顶点）
  - 环：外轮廓扇形（32 段）+ 内轮廓反向扇形（不填充，留出内孔）——形成环带三角形带
  - 圆角矩形：圆角段 = 扇形（每角 8 段）+ 中间矩形区域（fillRect/三角化）
  - 填充多边形：扇形/三角剖分（简单凸多边形顶点扇；凹多边形一期钳制为凸语义，见决策点 5）
- **描边实现**：`lineWidth = 1` 用现成 `drawRect`/`drawLine`（像素对齐）；`lineWidth > 1` 用**填充边带**（矩形描边 = 4 条 fillRect 边带；圆描边 = 外圆-内圆环三角化；线段 = 端点四边形）——见决策点 4
- **缓存**：形状几何（顶点）在 `setShape`/`setRect`/`setPoints`/`setRadius` 时重算缓存（绘制零计算）；三角化顶点仅缓存坐标（颜色每帧取当前属性）

![Shape 效果图](Shape_Preview.svg)

### 3.1 形状语义（7 种）


| 形状     | 枚举值（JSON/CABI） | 绘制                          | 关键属性                     |
| -------- | ------------------- | ----------------------------- | ---------------------------- |
| 描边矩形 | `rect`              | drawRect（线宽 1）或边带      | stroke/lineWidth             |
| 填充矩形 | `filled-rect`       | fillRect                      | fill                         |
| 圆角矩形 | `round-rect`        | 圆角扇形 + 中部矩形           | fill/stroke/lineWidth/radius |
| 圆 | `circle` | 扇形逼近（内切整个 rect，rect 方时为正圆）；`ringWidth>0` 画环带 | fill/stroke/lineWidth/**ringWidth** |
| 椭圆 | `ellipse` | 扇形逼近（内切整个 rect，与 circle 同语义）；`ringWidth>0` 画环带 | fill/stroke/lineWidth/**ringWidth** |
| 折线 | `polyline` | 逐段 drawLine/边带（开放）；本地坐标点，resize 时基准等比缩放 | stroke/lineWidth/points |
| 多边形 | `polygon` | 顶点扇填充 + 边带描边（闭合）；本地坐标点，resize 时基准等比缩放 | fill/stroke/lineWidth/points |

> **空心/环四种形态**：空心圆 = `circle` + fill 透明（缺省）；圆环 = `circle` + `ringWidth>0`；空心椭圆 = `ellipse` + fill 透明（缺省）；椭圆环 = `ellipse` + `ringWidth>0`——均无需新增形状枚举，由 fill 透明与 ringWidth 组合表达（见决策点 8）。

### 3.2 组合维度（粗细边 × 是否填充）

每个形状的视觉 = **fill（是否填充 + 填充色）× stroke（描边色）× lineWidth（粗细）** 三轴组合：

| 形状 | 是否填充 | 粗细边 | 组合覆盖 |
|---|---|---|---|
| `polyline` | 无填充概念（开放路径，仅描边） | ✅ `lineWidth` 任意 px + `stroke` 色 | 细线/粗线均可（折线 = 逐段粗线段） |
| `polygon` | ✅ `fill`（透明 = 不填充，仅轮廓） | ✅ `lineWidth` + `stroke` | 4 种组合：实心+描边 / 实心无描边 / 空心+描边 / 空心无描边 |
| `rect` | ✅ `fill`（透明 = 仅描边矩形） | ✅ `lineWidth` + `stroke` | 同上 4 种 |
| `circle`/`ellipse` | ✅ `fill`（透明 = 空心） | ✅ `lineWidth` + `stroke` | 同上 4 种 + `ringWidth` 环带轴 |

> 例：`polygon` 空心无描边 = 只 `fill` 透明 + `stroke` 透明（或 `lineWidth 0`）→ 不绘制；实心无描边 = `fill` 设色 + `lineWidth 0`。四层属性（C++/JSON/CABI/Binding）一致覆盖。

### 3.3 默认值

- `fill`：透明（**缺省空心**，只描边）；`stroke`：黑色（`SColor::Black`）；`lineWidth`：1
- `radius`：0（直角）；`ringWidth`：0.0f（实心圆/椭圆）；`shape`：`rect`；`points`：空（polyline/polygon 未设点 → 不绘制）
- 绘制顺序：fill 先于 stroke（填充在描边之下）

## 4. 决策点

> **拍板记录（2026-08-19）**：决策点 1-10 已全部拍板（2/3/7/8 按用户意见修订，3 的查询 API 以 mapToDrawPoint 为准，10 同意 Shape : ControlImpl）。

### 决策点 1：形状集范围（一期）（已拍板：同意）

**拍板**：7 种（rect/filled-rect/round-rect/circle/ellipse/polyline/polygon）——`triangle` 由 `polygon` 3 点覆盖（不单独提供）；`arc`（圆弧/饼图）与渐变填充列后续增强（§10 二期图元）。

### 决策点 2：Circle 语义（已拍板：拉伸）

**拍板**：`circle` 与 `ellipse` **同语义**——均内切**整个控件 rect**（随 rect 拉伸，x/y 各向异性；控件 rect 为正方形时 `circle` 即正圆）。两枚举并存，`circle` 为语义别名（正方形 rect 下表达"正圆"意图）。
- 原建议（内切 min(w,h) 保持正圆）已废弃——用户意见：Circle 也拉伸为椭圆。

### 决策点 3：点坐标体系（polyline/polygon）（已拍板：本地坐标 + 基准 rect 等比缩放）

**拍板（v2，用户意见）**：点 = **控件本地坐标（像素 float，0,0 = 控件左上，与其他控件绘制坐标系一致）**，**随 rect 缩放**——`setPoints` 时记录当前 rect 为**基准尺寸**（baseW/baseH），rect 变化（resize）时按 `(newW/baseW, newH/baseH)` **各向异性等比缩放**（x/y 独立比例，用户通过基准 rect 自行控制缩放比例）。
- v1 归一化（0..1）已废弃——归一化本质是 baseW=baseH=1 的特例，但与其他控件的本地像素坐标系不一致（用户意见）；本方案同时满足"随 rect 缩放"（拍板语义不变）与全库坐标一致性
- 坐标映射 API（用户意见：**mapToDrawPoint**，与 getDrawPoint 类似）：
  - **`mapToDrawPoint(float lx, float ly)`**（主 API）：输入**本地坐标**（与 points 同语义，像素），返回映射后的**全局绘制坐标**——`(rect.left + lx, rect.top + ly)`；用户可据此叠加其他 UI 元素/自行计算
  - **`getDrawPoint(int index)`**（便捷查询）：返回第 index 个顶点的全局坐标，等价于 `mapToDrawPoint(points[index])`——两者语义一致
- JSON 点集为本地像素：`{"x": 8, "y": 56}`；CABI `ShapeSetPoints` 接收浮点数组 + `ShapeMapToDrawPoint` 映射查询
- **points 语义**：`setPoints` 始终以**当前 rect** 为基准写入；读取返回**当前缩放后**的本地坐标（resize 后为等比缩放结果）
- 原建议（局部像素、不随 rect 缩放）已废弃——用户意见：点随 rect 缩放。

### 决策点 4：描边线宽（已拍板：同意）

**拍板**：一期支持任意 `lineWidth`（float ≥ 0；0 = 无描边）——`1` 用现成图元，`>1` 用填充边带/圆环三角化（实现见 §3）。

### 决策点 5：多边形凹性（已拍板：凸语义一期 + 凹方案分阶段落地）

**拍板**：一期按**凸多边形语义**绘制（顶点扇；凹多边形绘制结果不确定，不保证）；**凹多边形方案（耳切三角剖分）已输出为后续增强计划**（§10.5），分阶段落地。

### 决策点 6：JSON / CABI / C++ Binding（已拍板：同意）

**拍板**：一期全支持（ListView/StatusBar 惯例）——JSON `"type": "shape"` + 形状/颜色/线宽/半径/点集；CABI `UICornerstone_CreateShape` + 属性走通用 setter（`SetEnum("shape")`/`SetColor("fill"/"stroke")`/`SetFloat("line-width"/"radius"/"ring-width")`）+ `ShapeSetPoints` 专用（浮点数组参数）；Binding 统一属性接口 `setProperty` + `setPoints`。

### 决策点 7：缓存策略（已拍板：同意，resize 必须重算）

**拍板**：几何顶点缓存（`setShape`/`setRect`/`setPoints`/`setRadius`/`setRingWidth` 时重算），绘制零计算；三角化顶点数固定（圆 32 段、圆角 8 段/角）——性能恒定。
- **补充（用户意见）**：发生 **resize（`setRect`/`resized`）时缩放系数变化，必须重算**——归一化点集 × 新 rect 尺寸、内切椭圆随新 rect 变化，均需在重算链路中强制触发（含布局引擎驱动的 resized 回调，非仅 setRect）。

### 决策点 8：空心与环的实现方式（已拍板：同意，ringWidth 浮点可调）

**拍板**：**不新增形状枚举**——
- 空心（空心圆/空心椭圆）：`fill` 缺省透明即空心，仅 stroke 描边（fill+stroke 普通组合）
- 环（圆环/椭圆环）：`circle`/`ellipse` + 新属性 `ringWidth`（**float 浮点环带宽度 px**，`0.0f` 缺省 = 实心，`>0` 画环带：fill 填充环带、内孔透明、stroke 可选外缘描边；**用户可随时调整**——C++ `setRingWidth`/JSON `"ringWidth"`/CABI `SetFloat("ring-width")`/Binding `setProperty("ringWidth")` 四层可调）
- 三角化：外轮廓扇形 - 内轮廓反向扇形（环带三角形带）；`ringWidth ≥ 内切半径` 时钳制为实心（无孔）
- 备注：环带宽度**恒定 px**（内外轮廓等距），可变宽环带列后续增强。

### 决策点 10：Shape 继承关系（已拍板：Shape : ControlImpl）

**拍板**：`Shape : ControlImpl`（不继承 Panel）——用户同意建议，理由：
- Panel 的增量能力 = **容器语义**（`addControl`/`removeAllControls` + LayoutEngine 流式/锚点/网格布局 + 子控件事件分派，`Panel.h:14-41`）——Shape 为纯绘制控件，无子控件/布局/事件需求，继承引入不必要的状态（`m_actors`/`m_layoutEngine`/`m_flowItemProps`/`m_anchorItemProps`/`m_gridItemProps`）与每帧容器遍历开销
- 背景色/边框/透明：`ControlImpl` 公共属性已提供（`setBGColor` 等为 PanelBuilder 便捷方法，底层即公共属性）——继承 Panel 不增加能力
- 语义混淆：Panel 背景 = 矩形底色，与 Shape 的 `fill`（形状填充）叠层语义重叠（Shape 矩形 + 背景矩形双层），易误用
- 编码规范（简洁优先）：Shape 定位"最简绘制控件"，继承关系最小化
- 备选：`Shape : Panel`——获得容器能力（形状内嵌子控件场景），但引入上述开销与语义混淆，收益（当前无场景）远低于成本；后续若出现"形状容器"需求可再重构（单一继承点，成本可控）。

### 决策点 11（后续增强，暂不实施）：mapToDrawPoint 的其他输入语义
- 一期仅**本地坐标**输入（与 points 同语义）；后续若需归一化（0..1）/绝对坐标映射，扩展重载（成本低，需求出现再做）。

## 5. 属性一致性矩阵（C++ API / JSON / CABI / C++ Binding 四层同步，ListView §5.6 同规则）

> **分层命名惯例**：C++ `set+UpperCamel` / JSON camelCase（颜色单字键 `"fill"`/`"stroke"` 循 `"background"`/`"border"` 惯例）/ CABI 通用属性 kebab-case（`"line-width"` 循 `"row-height"` 惯例）/ Binding `setProperty(key, value)`（键名 = JSON 键）。
> **API 精简原则**：属性走通用接口；**专用 `ShapeXxx` 仅保留数组参数类**（`ShapeSetPoints`）。


| 属性             | C++（规范实现）             | JSON               | CABI（通用属性）                                              | C++ Binding（统一属性接口）    |
| ---------------- | --------------------------- | ------------------ | ------------------------------------------------------------- | ------------------------------ |
| 形状`shape`      | `setShape(ShapeType)`       | `"shape"`          | `SetEnum(inst, sh, "shape", "circle"/...)`                    | `setProperty("shape", ...)`    |
| 填充色`fill`     | `setFillColor`              | `"fill"`           | `SetColor(inst, sh, "fill", color)`                           | `setProperty("fill", color)`   |
| 描边色`stroke`   | `setStrokeColor`            | `"stroke"`         | `SetColor(inst, sh, "stroke", color)`                         | `setProperty("stroke", color)` |
| 线宽`lineWidth`  | `setLineWidth`              | `"lineWidth"`      | `SetFloat(inst, sh, "line-width", v)`                         | `setProperty("lineWidth", v)`  |
| 圆角半径`radius` | `setRadius`                 | `"radius"`         | `SetFloat(inst, sh, "radius", v)`                             | `setProperty("radius", v)`     |
| 环带宽度`ringWidth` | `setRingWidth`           | `"ringWidth"`      | `SetFloat(inst, sh, "ring-width", v)`                         | `setProperty("ringWidth", v)`  |
| 顶点集`points`   | `setPoints(vector<SPointF>)` + `mapToDrawPoint(float lx, float ly)`（主映射）+ `getDrawPoint(int index)`（顶点便捷） | `"points"`（本地像素 float，一期） | `ShapeSetPoints(inst, sh, count, float x[], float y[])` + `ShapeMapToDrawPoint(inst, sh, lx, ly, float* x, float* y)`（专用） | `setPoints(...)` + `mapToDrawPoint(lx,ly)` + `getDrawPoint(i)` |

**事件**：无（纯展示控件，无交互事件——四层均无）。

> 说明：`setFillColor`/`setStrokeColor` 名称带 Color 后缀（`setNormalStateBGColor` 等既有命名风格参照）；`fill`/`stroke` 为 JSON 与 CABI 同串单字键。

## 6. JSON（决策点 6，一期）

```json
{
  "type": "shape",
  "rect": {"x": 10, "y": 10, "w": 64, "h": 64},
  "shape": "circle",             // rect(缺省)/filled-rect/round-rect/circle/ellipse/polyline/polygon
  "fill": "#3B82F6",             // 填充色（缺省透明 = 空心）
  "stroke": "#1E293B",           // 描边色（缺省黑）
  "lineWidth": 2,                // 描边宽 px（缺省 1；0 = 无描边）
  "radius": 8,                   // 圆角半径（round-rect 生效，缺省 0）
  "ringWidth": 12,               // 环带宽度（float，circle/ellipse 生效；缺省 0 = 实心，>0 画环带）
  "points": [{"x": 8, "y": 56}, {"x": 56, "y": 56}, {"x": 32, "y": 8}]  // polyline/polygon 顶点（本地坐标像素，resize 时基准等比缩放）
}
```

- `parseShape` 仿 `parsePanel` 结构（rect + 公共属性）——颜色解析复用现有 `parseColor` 语法（`"#RRGGBB"` 等）
- 公共属性（visible/enabled/transparent 等）经 `parseCommonProperties`（既有链路）
- 事件：无（Shape 无事件）

## 7. CABI / C++ Binding（决策点 6，一期）

- **CABI**：`UICornerstone_CreateShape` + 属性通用 setter（`SetEnum(inst, sh, "shape", ...)`/`SetColor(inst, sh, "fill"/"stroke", ...)`/`SetFloat(inst, sh, "line-width"/"radius"/"ring-width", ...)`）+ 专用 `ShapeSetPoints(inst, sh, count, float x[], float y[])`（浮点数组参数，通用 setter 无法表达）+ `ShapeMapToDrawPoint(inst, sh, lx, ly, float* x, float* y)`（本地→全局坐标映射，主 API）
- **C++ Binding**：Shape 类 + Builder（属性统一 `setProperty(key, value)` 接口 + `setPoints`；`mapToDrawPoint(lx, ly)` 映射查询 + `getDrawPoint(i)` 顶点便捷；无事件）

## 8. 涉及文件清单


| 文件                                                      | 改动                                                                                                                                                                         |
| --------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `include/Shape.h`（新）                                   | Shape + ShapeType 枚举 + Builder；ControlType 枚举加 Shape（`ControlBase.h:139-144`）                                                                                        |
| `src/Shape.cpp`（新）                                     | draw（一期：直接调用一期图元 `drawEllipse`/`drawRoundRect`/`drawPolyline`/`drawPolygon`/线宽参数，见 §10；图元落地前暂用内部三角化）、setShape/setFillColor/setStrokeColor/setLineWidth/setRadius/setRingWidth/setPoints/mapToDrawPoint/getDrawPoint（重算缓存，resize 强制重算）、属性 override（setFloatProperty/setEnumProperty/setIntProperty） |
| `src/LayoutParser.cpp`                                    | `parseShape` + 控件类型注册（`"shape"`，PropertyNames）                                                                                                                      |
| `include/RenderDevice.h` + 三后端（一期图元，§10）        | 新增 `drawRoundRect`/`drawEllipse`/`drawLine` 宽度参数/`drawPolyline`/`drawPolygon` 基类默认实现（决策点 9：默认实现 → 后端零改动）                                            |
| `include/UICornerstoneAPI.h` + `src/UICornerstoneAPI.cpp` | 一期 CABI（见 §7）                                                                                                                                                          |
| Binding（`binding/`）                                     | 一期 Binding 暴露                                                                                                                                                            |
| `test/test_shape.cpp`（新）+ `test/CMakeLists.txt`        | 可视化 + 断言（7 形状绘制、空心/环带、颜色/线宽/半径/环宽属性、点集、透明填充、公共属性）                                                                                             |

## 9. 现状限制（注明，后续增强）

- 无 arc 圆弧/饼图（§10 二期图元）
- 环带宽度恒定 px（内外轮廓等距），可变宽环带后续
- 无渐变填充（§10 二期图元）
- 凹多边形一期凸语义；凹/自交/带洞多边形方案见 §10.5（分阶段落地）
- 线宽虚线/点线样式不做（后续）
- 无旋转（TextRenderer 无旋转；形状旋转需三角化旋转，后续）

## 10. 图元新增建议（RenderDevice 层，分期实现）

### 10.1 依据：现有代码的组合痛点（建议图元高频被手工组合）

| 现有手工组合 | 位置 | 对应建议图元 |
|---|---|---|
| 矩形边框 = 4 条 `drawLine` 手拼 | `ControlBase.cpp:839-854`（绘制边框）、`CheckBox.cpp:477-480`（勾选框）、`HandleControl.cpp:458-461`（手柄框） | `drawRoundRect`（含直角/描边） |
| 对勾/箭头 = 多段 1px `drawLine` 拼 | `CheckBox.cpp:517-518/537-538`、`Menu.cpp:148-149`（子菜单箭头）、`ComboBox.cpp:134-140`、`NumericUpDown.cpp:86-93`、`TreeView.cpp:308-314` | 保持现状（小元素适合直接三角）；不新增 |
| 折线/多边形 = 每帧循环 `drawLine` | `GraphTool.cpp:280/542/602/664/737`（DrawingContext 已自封装 `drawLine(rectPoints)` 循环 :66/:75/:161） | `drawPolyline`/`drawPolygon` |
| 分割线/刻度 = 1px `drawLine` | `Menu.cpp:1096`（分隔线）、`Slider.cpp:496/550`（刻度） | `drawLine` 宽度参数 |
| 图表线宽 = DrawingContext 自行处理 | `GraphTool.cpp:39-161` | `drawLine` 宽度参数 |
| 圆/椭圆/圆角（无现成） | Shape 控件需求 | `drawEllipse`/`drawRoundRect` |
| 虚线/渐变/弧 | Shape 后续需求 | 二期图元 |

### 10.2 分期清单

**一期（随 Shape 控件落地，Shape 直接消费）**——均为 CPU 顶点生成 + 现有 `drawTriangles` 提交，三后端零风险：

| 图元 | 签名（建议） | 受益方 | 成本 |
|---|---|---|---|
| 圆角矩形（含直角） | `drawRoundRect(rect, radius, fill, stroke, lineWidth)` | Shape/Button 圆角（`Enhancements_Analysis.md §3.3` 待办）/进度条/面板/CheckBox | 低（4 角扇形+中部矩形） |
| 椭圆（rx=ry 即圆） | `drawEllipse(cx, cy, rx, ry, fill, stroke, lineWidth)` | Shape/GraphTool/状态点 | 低（32 段扇形+环） |
| 线宽参数 | `drawLine(x1,y1,x2,y2, width = 1)`（默认参数，既有 54 处调用不变；C ABI 新增 `bridge_drawLineEx`） | 全库（分割线/边框/图表） | 低（端帽四边形边带） |
| 折线 | `drawPolyline(points, count, color, width)` | Shape/GraphTool（消除逐段循环） | 低（逐段复用 drawLine） |
| 多边形（填充+描边） | `drawPolygon(points, count, fill, stroke, lineWidth)` | Shape/GraphTool | 低（顶点扇+边带） |

**二期（独立价值，Shape 后续扩展消费）**：

| 图元 | 签名（建议） | 受益方 | 成本 |
|---|---|---|---|
| 圆弧 | `drawArc(cx, cy, r, startDeg, endDeg, color, width)` | 进度环/仪表盘/Shape arc | 中（弧段顶点+边带） |
| 扇形填充 | `drawPie(cx, cy, r, startDeg, endDeg, fill)` | 饼图/仪表 | 低（扇形顶点） |
| 虚线 | `drawDashedLine(x1,y1,x2,y2,width,dash,gap)` | 分割线样式/图表网格/选区 | 低（循环 drawLine） |
| 线性渐变矩形 | `fillGradientRect(rect, colorA, colorB, dir{Vertical,Horizontal})` | 按钮/面板/背景（顶点色插值，三后端天然支持） | 低（对角双三角，顶点色插值） |

**三期（高成本/需评估，暂不建议原生）**：抗锯齿图元（三后端能力差异大：SDL3 几何无 AA、raylib 部分原生 AA、sfml 需自实现；建议维持现状或全局超采样）；贝塞尔曲线（GraphTool 平滑需求待确认）；阴影/发光（需 blur 后处理，成本高）。

### 10.3 实现架构：基类默认实现（决策点 9，已拍板：同意）

**拍板**：新增图元在 `RenderDevice` 基类提供**默认实现**（CPU 顶点生成，内部调用既有纯虚 `drawTriangles`/`drawLine` 等）——声明为非纯虚（带默认实现）后：
- **三后端（sdl3/raylib/sfml）与 CallbackRenderDevice 零改动自动获得新图元**（回调链路自动转发到 `drawTriangles`）
- 后端可选 override 用原生能力加速（如 raylib `DrawCircleSector`、`DrawRing`）——纯优化，非必需
- 无新增 C ABI 回调函数（旧 `drawLine` 等回调不变；仅新增 `bridge_drawLineEx` 转发）

- 备选：三后端各自实现——代码重复 3 份，顶点生成算法完全一致，无收益；反选：仅 Shape 内部组合不新增图元——GraphTool/ControlBase 等痛点仍存在，图元价值未释放。

### 10.4 分期映射到 Shape

| 阶段 | Shape 能力 | 依赖 |
|---|---|---|
| 一期 | 全部 7 形状 + 空心/环（绘制逻辑直接调用一期图元，Shape.cpp 三角化退化为参数组装） | 一期 5 图元 |
| 二期 | arc/饼图、虚线样式、渐变填充（§9 限制解锁 3 项） | 二期 4 图元 |
| 三期 | （视需求）平滑曲线等 | 三期评估项 |

> 说明：一期图元落地前 Shape 的三角化算法暂存 Shape.cpp 内部（§3 方案），图元落地后下沉至 RenderDevice 默认实现（决策点 9 拍板）。

### 10.5 凹多边形增强方案（决策点 5 拍板：方案已输出，后续分阶段落地）

**背景**：一期 polygon 按凸语义绘制（顶点扇）；凹多边形（星形/箭头槽口/L 形等）绘制结果不确定。

**方案：耳切三角剖分（Ear Clipping）**：
1. 输入：顶点序列（归一化 0..1 → 局部像素）；先做**凸性检测**（O(n) 叉积符号一致）——凸多边形直接走一期顶点扇路径（零额外成本）
2. 凹多边形：耳切算法（O(n²)，逐顶点判断"耳"：左右邻叉积 + 其他顶点不在三角形内）——n 为顶点数，UI 场景 n ≤ 64，成本可忽略
3. 输出：三角形列表 → 一次 `drawTriangles` 提交（渲染层零改动）
4. 位置：算法作为 Shape.cpp 内部实现（或随一期图元下沉至 `drawPolygon` 默认实现——见决策点 9）
5. **阶段划分**：Phase A（随 Shape 一期交付）：凸性检测 + 简单凹多边形（无自交、无洞）耳切；Phase B（后续）：自交多边形（扫描线/夹板法，需求出现再做）；Phase C（后续）：多边形带洞（多边形化处理，需求出现再做）
6. 边界语义：凹多边形的 `points` 仍为本地坐标、随 rect 基准等比缩放（决策点 3）；描边沿剖分后轮廓边（耳切保持原始边界顶点，描边边带不受剖分影响）
- 点坐标不随 rect 缩放（决策点 3 语义）
