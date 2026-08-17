# TreeView 增强需求分析（前置控件容器 + 逐 Item 字体）

> 状态：已拍板（2026-08-16，决策点 1/2/3 采纳用户意见）· 待实施
> 关联：[TreeView_Design.md](TreeView_Design.md)、[ControlBase_Design.md](ControlBase_Design.md)、[CheckBox_Design.md](CheckBox_Design.md)

> **修订注记（v1 → v4，源码勘察复核 + 决策拍板 + 实施修订）**：
> ① 行控件 rect 必须为**局部坐标**（直接 setRect 绝对坐标会经 getDrawRect 二次叠加父偏移，
> 与 2026-08 修复的 Material/Actor 双重偏移 bug 同款）；
> ② 行控件绘制归属改为**行循环内绘制 + 子控件循环排除**（即决策点 2 方案 A 的定稿实现：
> "子控件循环中单独 pushClipRect(cr) 再 draw"在 popClipRect 之后无法复用 cr 且会与
> 子控件循环同帧重复绘制）；
> ③ `cloneNode` 不得浅拷贝 leadingControl（项目无控件深拷贝机制，共享实例会冲突），置空；
> ④ 3.4 命中检测顺序为 hitTestRow 之后、selectNode 之前；
> ⑤ m_nodeFonts 增加树结构变更清理时机；⑥ 行控件默认不 focusable（Tab 链约束）；
> ⑦（v3）行控件高度**自适应行高**（决策点 3：高 = rowH，宽按原始宽高比等比缩放）；
> ⑧（v3）点击行控件**同时选中该行**（决策点 1）；
> ⑨（v4）**槽起点 = 行内容起点**（arrowX，即 LEFT_PADDING+缩进），有箭头行避开箭头区
> （+arrowGap），文字起点 = 槽右缘 + leadingGap——原"槽右缘贴 labelX"在
> arrowGap+LEFT_PADDING(20px) < slotW+gap(30px) 时图片左缘越出内容起点被裁剪
> （实测 r.left=-10）。Actor 宽高比取纹理自然尺寸（rect 初始 0 时仍可等比）。
> ⑩（v5）**槽高 = 文字高度**（垂直居中，行间留空隙），宽按槽高等比——原"高=行高"
> 图片上下贴满行，两行图片之间无空隙（用户实测反馈）。
> ⑪（v6）**槽起点 = 原文本起点 labelX**（图片缩进与无容器时文本缩进一致；v4 的
> "槽起点=内容起点"偏左、v1 的"槽右缘贴 labelX"在 slotW+gap > arrowGap+LEFT_PADDING
> 时越界被裁剪，均被用户实测否决）。
> ⑫（v7）**JSON 第二期交付**：item 级 `leadingControl`（复用 parseControl type 分发，
> 缺 rect 自动补占位）/`leadingGap`/`font`/`size`；**CABI item-id 定位模式**
> （item-leading-gap/font-size/font/leading-control，借用语义）。
> 用例：test_treeview 16 项 json enh 断言 + test_treeview_cabi 4 组 CABI 断言（三后端 + DLL 全过）。
> 证据行号：`getDrawRect = 父drawRect + rect×scale`（ControlBase.cpp:602）、`setRect` 纯赋值
> （ControlBase.cpp:442）、`m_frameDrawRect` 在 beforeDraw 刷新（ControlBase.cpp:209-222）、
> 无控件深拷贝（ControlBase.cpp:76-77 拷贝构造为 Todo）。

## 1. 需求概述

1. **Item 前置控件容器**：每个 TreeView Item 前面预留一个控件容器位置，可放置图片（Actor/Image）、CheckBox 等任意控件
2. **逐 Item 字体属性**：每个 Item 可独立设置字体（FontName）与字号，未设置时继承 TreeView 级字体

## 2. 现状调研（关键行号）

| 项目 | 现状 |
|---|---|
| TreeNode 结构 | `include/TreeView.h:17-23`：仅 `id/label/expanded/children/userData`，无控件/字体字段 |
| 行绘制 | `src/TreeView.cpp:162-189`：行高亮 → 箭头（`drawArrow`）→ 文字；`labelX = arrowX + arrowGap*scale`（:179）；行垂直居中 `textY = y + (rowH - fontH)/2`（:185） |
| 子控件绘制 | `src/TreeView.cpp:194-196`：**在 `popClipRect` 之后**逐子控件 draw（无 clip）——行内控件需 clip 到视口 |
| 子控件事件分发 | `src/ControlBase.cpp:296-334`：`ControlImpl::handleEvent` 逆向遍历子控件 + 兄弟遮挡检测；TreeView 自身 `handleEvent` 末尾 `return ControlImpl::handleEvent(event)`（`TreeView.cpp:353`） |
| TreeView 点击逻辑 | `src/TreeView.cpp:292-314`：MouseDown 左键 → 滚动条优先 → `hitTestRow` → 箭头展开 → **`selectNode` 并 return true**（行内控件点击会被选中行截获） |
| 字体机制 | `src/TreeView.cpp:78-108`：TreeView 级 `m_fontName/m_fontSize/m_font`，`ensureFont()` 按复合缩放重建；`refreshScaleWith`（:111-119）失效重建 |
| 内容宽度 | `calcContentWidth`（`TreeView.cpp:597-612`）按全局字体测量 |
| 树结构操作 | `setItems`（:395-412）、`addChild`（:414-427）、`removeNode`（:429-470）、`clearItems`（:486-497）——行控件需随节点生命周期挂/摘 |
| JSON 解析 | `LayoutParser::parseTreeView`（`LayoutParser.cpp:1756-1811`）：items 递归解析 `id/label/expanded/userData/children` |
| FontName 枚举 | `include/ConstDef.h:42-71`：**无 None/继承值**——"未设置"需用 `fontSize==0` 或独立标志表达 |
| 测试 | `test/test_treeview.cpp`：`makeNode` + `setItems` 模式，263 行 |

## 3. 需求 1：Item 前置控件容器（leadingControl）

### 3.1 数据模型

`TreeNode` 扩展：

```cpp
std::shared_ptr<Control> leadingControl;  // 前置控件（任意 Control 子类：CheckBox/Actor/Image 等）
float leadingGap = 6.0f;                  // 控件与文字之间的空隙（默认 6px，可调）
```

- `cloneNode`（`TreeView.h:38-52`）需复制新字段：**leadingControl 必须置空**（`fontName/fontSize` 照常复制）——项目无控件深拷贝机制（`ControlBase.cpp:76-77` 拷贝构造为 Todo），浅拷贝会让两棵树共享同一控件实例（getParent 冲突、双树绘制/事件串扰）
- `makeNode` 保持向后兼容（不增加参数，leadingControl 由调用方赋值）

### 3.2 挂树与生命周期

- **挂树**：`setItems`/`addChild`/`removeNode` 等结构变更统一走 `rebuildFlatRows()`——**在 rebuildFlatRows 内一次性完成行控件挂/摘同步**（遍历 flatRows 收集 leadingControl 挂树 + 登记 `m_rowControls`，对已摘节点 removeControl + 移出集合），避免在各操作入口重复实现遗漏
- **摘树**：由 rebuildFlatRows 的统一挂摘逻辑覆盖（`removeNode`/`clearItems`/`setItems` 替换时先 `removeControl` 对应行控件并移出 `m_rowControls`，再释放节点；`ControlBase.cpp:384` 已有 removeControl）
- 行控件在 TreeView `create()` 之前挂树可安全（参照滚动条在 create() 内挂树、`addControl` 的 renderer 传播在父挂树时补发；子控件 create 由 `setContext` 递归 `recreate` 驱动，`ControlBase.cpp:117-127`）

### 3.3 绘制与滚动跟随

- `draw()` 行循环内（:162-189），对可见行若有 `leadingControl`：
  - **rect 必须为局部坐标**（子控件 rect 语义为相对父，`getDrawRect = 父drawRect + rect×scale`，
    `ControlBase.cpp:602`；滚动条即用局部坐标 `{0, m_rect.height - W, ...}`）。**禁止直接
    setRect(绝对坐标)**——绝对坐标再经 getDrawRect 叠加父偏移 = 双重偏移，与 2026-08 修复的
    Material/Actor bug 同款（见 [LayoutSystem_Design.md](LayoutSystem_Design.md) 绘制坐标约定）。
  - **槽高 = 文字高度（决策点 3 修订，v5）**：控件高度 = 行内文字高度 `fontH`（无字体时回退
    行高），**行内垂直居中**，行与行之间保留空隙；宽度按原始宽高比等比缩放
    `slotW = slotH * (原始宽/原始高)`（CheckBox 1:1 → 正方形；Image 等比；宽高比优先取纹理
    自然尺寸，Actor rect 初始为 0 时仍可等比）。**槽起点 = 行内容起点**（arrowX，即
    LEFT_PADDING+缩进）；**有箭头行避开箭头区**（槽起点 +arrowGap，箭头宽约 10px 不重叠）；
    **文字起点 = 槽右缘 + leadingGap**（不再固定 labelX，leading 行文字被槽让位）。
    rect 换算公式（arrowX/y 为绝对坐标且已含滚动偏移；cr 为 m_frameDrawRect 减滚动条宽度的
    **内缩副本，left/top 不变**，在 beforeDraw 刷新；slotW 为等比缩放后的局部宽度，不乘 scale）：
    `localX = (slotStartX - cr.left) / scaleX`、`localY = (y + (scaledRowH - slotH*scaleY)/2 - cr.top) / scaleY`
    逆推 getDrawRect 可精确还原绝对位置。**禁止直接 setRect(绝对坐标)**——绝对坐标再经
    getDrawRect 叠加父偏移 = 双重偏移，与 2026-08 修复的 Material/Actor bug 同款
    （见 [LayoutSystem_Design.md](LayoutSystem_Design.md) 绘制坐标约定）。
    **槽起点 = 原文本起点 labelX**（= LEFT_PADDING+缩进+arrowGap，即无容器时文本的缩进位置，
    用户实测确认）：有箭头行箭头在图片左侧不重叠，局部坐标恒非负（槽向右延伸，永不越出
    内容起点）；**文字起点 = 槽右缘 + leadingGap**（文字被槽让位右移）。
  - 每次 draw 调用 `setRect`（纯赋值无副作用，`ControlBase.cpp:442`；滚动偏移已体现在
    labelX/y 中 → 控件自动跟随滚动）
- **绘制归属（防重复绘制）**：行控件在**行循环内**（`pushClipRect(cr)` 区内，天然被裁剪）先
  `setRect` 再 `child->draw()`；TreeView::draw 末尾的子控件循环（:194-196，popClipRect 之后）
  必须**跳过 `m_rowControls` 中的行控件**——否则同帧重复绘制（且该循环无 clip，行控件会溢出视口）。
  滚动条不在 m_rowControls 内，照旧在子控件循环绘制（z 序在最上层）
- 行控件绘制顺序：行高亮 → 箭头 → 行控件 → 行文字（行控件与文字无重叠，先画控件保证
  与 CheckBox 视觉一致性）

### 3.4 事件路由（关键决策）

现状：MouseDown 命中行即 `selectNode` 并 return true（:305-313），会截获行内控件点击。

改动：MouseDown 分支中，**先 `hitTestRow` 得到 row（命中才可定位节点），再对该行节点的 `leadingControl` 做 `isContainsPoint`**（用控件当前 getDrawRect 绝对 rect）——检测插在**箭头检测之后、`selectNode` 之前**：
- 命中 → **先 `selectNode`（点击行控件同时选中该行，决策点 1），再 return false 不消费**，事件继续走到末尾 `return ControlImpl::handleEvent(event)` 的子控件分发路径（`TreeView.cpp:353`），由 CheckBox 等接收并完成自身交互（如勾选翻转；遮挡检测由 `ControlBase.cpp:317-326` 保证）
- 未命中 → 现有逻辑不变

默认语义：点击 CheckBox 等行控件**同时选中该行**（selectNode + 控件自身交互都生效）；如后续需要"点击控件不选中行"，可加开关。

> 约束：行控件**默认不 focusable**——`setFocusable(true)` 会经 `setContext` 注册进全局
> FocusManager Tab 链（`ControlBase.cpp:117-120`），与 TreeView 自身键盘导航
> （`TreeView.cpp:226-262` Up/Down/Left/Right）打架；第一期交互仅鼠标，保持非焦点控件。
> 行 hover 高亮（MouseMove 分支设 m_hoveredRow）与 CheckBox 自身 hover 互不干扰，无需处理。

### 3.5 JSON 支持（已实施 v7）

- 第一期（C++ API）：`makeNode` 后直接赋值 `node->leadingControl = CheckBoxBuilder(...).build()`，`setItems` 生效；JSON 不扩展
- 第二期（JSON，**已实施**）：items 项支持 `"leadingControl": {"type": "check-box", "checkState": "checked"}` 或
  `"leadingControl": {"type": "image", "image": "assets/images/xx.png"}`——`parseItems` 内递归调用
  `parseControl` 复用 type 分发（勘察结论：可行）；item 键：`leadingGap`（float）、`font`（枚举，
  如 `harmonyos-sans-sc-bold` 粗体）、`size`（int，0=继承 TreeView 级）。**注意：item 内控件 JSON
  缺省 rect 时解析器自动补 `{0,0,0,0}` 占位**（parseRect 强取 rect 键，否则 nlohmann 抛异常）；
  控件 parent 传 nullptr，由 `syncRowControls` 挂树（create 在挂树后经 setContext→recreate 重放）。
- **CABI（v7）**：item 级属性采用 **item-id 定位模式**——先
  `UICornerstone_SetString(tv, "item-id", id)` 定位目标节点，随后
  `SetFloat("item-leading-gap")` / `SetInt("item-font-size")` / `SetEnum("item-font")` /
  `SetPtr("item-leading-control")` 作用于该节点（getter 对称）。leading-control 为**借用语义**
  （shared_ptr 无删除器包装，生命周期由调用方保证，与 selected-user-data 同约定）。
  **运行时 SetPtr 立即触发 `syncRowControls()` 挂树**（create() 之后、结构变更之外亦可挂载，
  addControl 同父幂等可安全重入）；**传 NULL 解除容器并摘树**。
  容器类型经 CABI `UICornerstone_GetControlType` / Binding `Control::GetType()` 查询
  （基类 `m_ctlType` 枚举成员，构造时设置，O(1)；字符串经 switch 转 PropertyNames 常量）。
  "相对上一级的缩进" = `"indent-width"`（TreeView 级，既有 CABI，JSON 键 `indentWidth`）。

## 4. 需求 2：逐 Item 字体

### 4.1 数据模型

`TreeNode` 扩展：

```cpp
FontName fontName = FontName::HarmonyOS_Sans_SC_Regular;  // 与 TreeView 默认一致；仅 fontSize>0 时生效
int fontSize = 0;                                          // 0 = 继承 TreeView 级字号
```

（FontName 枚举无 None 值 → 用 `fontSize==0` 作为"未设置"标志，fontName 仅在 fontSize>0 时被读取）

### 4.2 字体获取与缓存

- `TreeView` 新增缓存：`unordered_map<const TreeNode*, SharedFont> m_nodeFonts`
- `getNodeFont(node)`：`node->fontSize > 0` → 按 `node->fontName + node->fontSize` 创建（走 `ensureFont` 同路径，`TreeView.cpp:78-94`）；否则返回 `m_font`
- 失效：`setFont`/`setFontSize`/`refreshScaleWith`（缩放变化）时清空 `m_nodeFonts`（与 `m_font.reset()` 同点，:96-108/:111-119）；**另需在 `clearItems`/`setItems`/`removeNode` 的节点清理处同点清空**——节点销毁后缓存 key（裸指针）悬空，虽因 flatRows 同步重建不会误命中，但防残留与指针复用
- 节点指针稳定（TreeNode 生命周期与节点相同），缓存无需随 rebuildFlatRows 失效

### 4.3 绘制与测量改造

- `draw()`（:159-188）：行字体 `fontH = renderer->getFontHeight(nodeFont)`、`drawText(nodeFont, ...)`（按行取）
- `calcContentWidth()`（:597-612）：按行节点字体测量文字宽度
- `cloneNode` 复制 `fontName/fontSize` 新字段（**leadingControl 置空**，见 3.1）

## 5. 涉及文件清单

| 文件 | 改动 |
|---|---|
| `include/TreeView.h` | TreeNode 扩展两字段；TreeView 新增缓存/辅助方法（getNodeFont、挂摘树辅助）、`set<Control*> m_rowControls` |
| `src/TreeView.cpp` | draw（行控件 rect 局部坐标 + 行循环内绘制 + 子控件循环排除）、handleEvent（hitTestRow 后控件命中检测）、rebuildFlatRows（行控件挂摘统一同步 + m_rowControls/m_nodeFonts 清理）、calcContentWidth（行字体） |
| `src/LayoutParser.cpp` | （第二期）parseItems 支持 `leading`/`fontSize`/`font` |
| `test/test_treeview.cpp` | 新增用例：前置 CheckBox/图片节点 + 逐节点字体；交互断言（点控件**同时选中行**、勾选生效） |

## 6. 待拍板决策点（含结论）

1. 行控件点击是否**不选中行**？还是"点击控件同时选中行"？ → **采纳"点击控件同时选中行"**（v3 已按此定稿 3.4：先 selectNode，再 return false 交控件完成交互）
2. clip 方案 A（行控件单独 clip）还是 B（整段子控件移入 clip）？ → **采纳方案 A**，定稿实现为"行循环内绘制 + 子控件循环排除"（单独 pushClipRect 的原始写法在 popClipRect 之后无法复用 cr 且会与子控件循环同帧重复绘制，见 3.3）
3. 行控件尺寸：固定尺寸还是按行高自适应缩放？ → **按行高自适应缩放**（高 = rowH，宽按原始宽高比等比缩放，CheckBox 1:1；v3 已按此定稿 3.3）
4. JSON 第二期是否立项？控件类型范围？ → **立项**，check-box/image 两类先行；实施前勘察 parseItems 内能否复用 LayoutParser 的 type 分发
5. 需求 2 的继承语义是否可接受（`fontSize==0` 继承）？ → **可接受**，fontSize==0 即继承 TreeView 级字号；字重/字族独立覆盖低频，不做
6. 行控件焦点（v2 新增）：**默认不 focusable**（Tab 链与 TreeView 键盘导航约束，见 3.4 约束）；如后续需要键盘勾选再评估