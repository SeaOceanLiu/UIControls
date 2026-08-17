# Menu 增强需求分析（前置控件容器 + 逐 Item 字体）

> 状态：**已拍板（2026-08-17，决策点 1/2/3/4/5/6 全部定稿）· 转主设计开发 Session 实施**
> 关联：[TreeView_Enhancement_Analysis.md](TreeView_Enhancement_Analysis.md)（同型增强，已实施，本文多处复用其结论）、[Menu_Design.md](Menu_Design.md)
> 效果图：[Menu_Enhancement_Preview.svg](Menu_Enhancement_Preview.svg)

> **修订注记（v1 → v2，决策拍板 + SVG 效果图）**：
> ① **icon 区即控件容器**（决策点 1）：原 `ICON_AREA_WIDTH=20` 固定图标区直接改造为
> 控件容器位——容器为**正方形、边长 = 字体高度**（决策点 6），水平居中于 icon 区；
> **icon 区宽改为实例状态** = max(各行字体高度, 20)，随逐项字体变化；文字起点 =
> icon 区右缘 + leadingGap（不再固定 28）；无容器时 icon 区中心画勾选标记（保留现状）。
> ② 行高 = **方案 A 变行高**（决策点 2）：每行高 = 自身字号×1.6（覆盖项）或面板字号×1.6
> （继承项）；layoutItems/hitTest 逐行累加模式已支持。
> ③ 点击前置控件：**不触发 item onClick、不关闭菜单链**，控件自身交互生效（决策点 3）。
> ④ **JSON/CABI 第二期立项**，同 TreeView（决策点 4）；MenuBar 顶级 entry 本期不支持
> （决策点 5）。

## 1. 需求概述

1. **Item 前置控件容器**：每个 MenuItem 前面预留一个控件容器位置，可放置图片（Actor/Image）、CheckBox 等任意控件
2. **逐 Item 字体属性**：每个 MenuItem 可独立设置字体（FontName）与字号，未设置时继承面板级字体

## 2. 现状调研（关键行号）

| 项目 | 现状 |
|---|---|
| MenuItem 结构 | `include/Menu.h:29-85`：**本身就是 ControlImpl**（有 `draw`/`handleEvent`/`getDrawRect`），成员仅 caption/shortcut/checked/onClick/subMenu/m_font/m_fontSize |
| Item 挂树方式 | **MenuItem 不在 MenuPanel 的 m_children 中**（`Menu.cpp:255-256` 注释明确），由 `addItem` 挂入 m_items；context 需手动传播（`Menu.cpp:255-261`） |
| 事件分发 | **MenuPanel::handleEvent 手动分发**（`Menu.cpp:597-633`）：MouseDown 左键 → `hitTest` → 命中 Normal 项直接 `m_onClick` + 关菜单链（:617-624），**不经过 ControlBase 子控件分发**；MenuItem::handleEvent（:170-195）实际是备用路径（item 不在 m_children，分发到不了它） |
| 绘制 | MenuPanel::draw 行循环内手动调 `item->draw()`（`Menu.cpp:568-588`）；item 自身绘制勾选标记/标题/快捷键/箭头（:120-168） |
| 图标区 | `ICON_AREA_WIDTH=20`（`Menu.cpp:39`）**仅勾选标记使用**：勾选中心 = `drawRect.left + 10`（:127-133）；标题起点 = `ITEM_LEFT_PADDING=28`（:143）——图标区与文字之间有 8px 空隙 |
| 布局测量 | `recalculateSize`（:370-419）**已按 item->m_font 逐项测量** maxCaptionWidth/maxShortcutWidth（:387-395）→ 逐项字体测量天然支持；面板宽度 = 28 + caption + shortcut(≥60) + arrow(20) + 20 |
| 行高 | 统一 `m_itemHeight = m_fontSize × m_heightRatio`（:298）；但 `layoutItems`（:421-433）与 `hitTest`（:435-454）**已是逐行累加模式**（分隔行高不同），支持变行高 |
| 字体注入 | `MenuItem::setMenuFont`（:94-97）由面板注入；`updateItemsFont`（:289-293）**全量覆盖**所有 item；触发点：addItem（:330）、setFontSize（:295-305）、setFontName（:316-324）、refreshScaleWith（:271-287 缩放重建） |
| 字体加载 | `loadMenuFont(ctl, fontName, fontSize)`（:53-67）：FontName→文件路径→readFile→`loadFontFromMemoryWithText`（字号×复合缩放）——MenuItem 可复用（getTextRenderer/getResourceProvider 走父链） |
| 子菜单 | `setSubMenu` 时 `panel->setParent(this)`（:112-117）——子菜单面板是 MenuItem 的子节点，**不在 m_children** |
| 滚动 | **菜单无滚动条**（面板定高一次性绘制）→ 无 TreeView 的滚动跟随问题（与 TreeView 关键差异） |
| JSON | `populateMenuPanel`（`LayoutParser.cpp:1217-1270`）：item 键 = type/caption/shortcut/checked/enabled/items(子菜单递归)/events；menu 键 = caption/items |
| CABI | `UICornerstoneAPI.h:326-337`：Menu 三件套已存在（CreateMenuBar/Panel/Item + MenuPanelAddItem + MenuItemSetSubMenu）；caption/checked/shortcut/click 走统一属性系统；**无 item-id 定位模式**（TreeView v7 已建立先例） |
| 测试 | `test/test_menu.cpp`（184 行）：Builder 组装 + 手动事件模拟点击断言 |
| FontName 枚举 | `include/ConstDef.h:42-71`：无 None/继承值 → 与 TreeView 同款方案：`fontSize==0` 作"未设置"标志 |

## 3. 需求 1：Item 前置控件容器（leadingControl）

### 3.1 数据模型

`MenuItem` 扩展：

```cpp
std::shared_ptr<Control> leadingControl;  // 前置控件（任意 Control 子类：CheckBox/Actor/Image 等）
float leadingGap = 8.0f;                  // 控件与文字之间的空隙（默认 8px 可调）
```

- 与 TreeView 差异：TreeView 的行控件存于 TreeNode 数据并在 rebuildFlatRows 统一挂/摘；**Menu 的 item 本身就是控件节点**，leadingControl 直接作为 **MenuItem 的 m_children 子控件**挂树（`addControl`）——MenuItem 的 `create`/`setContext` 走 ControlBase 默认实现，自动传播到 children（`ControlBase.cpp:117-127`）
- 挂树时机：运行时 `setLeadingControl` 亦可（`addControl` 幂等可安全重入，TreeView v7 CABI 已验证同机制）
- 生命周期：item 析构自动回收 children；`removeItem` 摘除 item 时 children 随 item 释放
- **无深拷贝问题**：Menu 无 cloneNode 机制（与 TreeView 不同），不涉及

### 3.2 槽位置与勾选共存（已拍板：icon 区即控件容器）

**决策点 1/6 定稿**：原 icon 区（`ICON_AREA_WIDTH=20`，目前仅勾选标记使用）**直接改造为控件容器**，不另设槽位：

- **icon 区宽改为实例状态**：`m_iconAreaWidth = max(各行字体高度 fontHeight, 20)`（随逐项字体变化；recalculateSize 时求 max）——默认字号 20 时仍为 20，与现状一致
- **容器尺寸**：正方形，**边长 = 该行字体高度**（决策点 6；逐项字体行按自身字体）；在 icon 区内**水平居中**、行内垂直居中
- **文字起点** = icon 区右缘 + `leadingGap`（默认 8，可调）——不再固定 `ITEM_LEFT_PADDING=28`（现 28 = 20 icon 区 + 8 空隙，改造后语义显式化）；slotW 不设上限（icon 区宽已随最大容器自适应）
- **勾选标记**：无容器时在 icon 区中心画勾选（现状不变）；**有容器时不绘制**（勾选视觉由容器内 CheckBox 等自身承担；勾选状态可独立设置）
- 面板宽度公式（recalculateSize）改为：`m_iconAreaWidth + leadingGap + maxCaptionW + shortcut(≥60) + arrow(20 若有) + ITEM_RIGHT_PADDING(20)`
- rect 为**局部坐标**（子控件 rect 语义相对父，`ControlBase.cpp:602` 双重偏移陷阱——TreeView 修订注记① 同款约束）；槽 y = `(m_itemHeight - slotH×scale) / 2`（垂直居中）
- **绘制归属**：在 `MenuItem::draw` 内先 `setRect` 再 `child->draw()`（item 自身绘制路径唯一，MenuPanel 无重复绘制问题——与 TreeView 的行循环+子控件循环排除不同，Menu 结构天然无此问题）

### 3.3 事件路由（已拍板）

现状：MenuPanel::handleEvent 命中 item 后**直接 onClick + 关菜单链**（:617-624），容器控件收不到事件（item 不在 m_children，ControlBase 分发到不了）。

改动：**MenuPanel::handleEvent 命中 item 后改为调用 `item->handleEvent(event)`**（复用 MenuItem::handleEvent 现有实现 :170-195，其 onClick/closeMenuChain/fireCCallback 逻辑完整），MenuItem::handleEvent 增加前置控件分支：

- 命中前置控件（`leadingControl->isContainsPoint(mx,my)`，用控件 getDrawRect 绝对 rect）→ **return false 不消费**，并调用 `ControlImpl::handleEvent(event)` 走子控件分发路径（CheckBox 接收完成自身交互如勾选翻转；遮挡检测由 `ControlBase.cpp:317-326` 保证）——事件由此被 item 的 children 正常消费
- 未命中 → 现有逻辑不变

**决策点 3 定稿**：点击 CheckBox 等前置控件**不触发 item 的 onClick、不关闭菜单链**（与 TreeView"点击行控件同时选中行"相反——菜单场景点击勾选框不应关闭菜单；如后续需要"点击容器也触发 item"可加开关）。

> 约束：前置控件**默认不 focusable**（TreeView 同款约束：FocusManager Tab 链与菜单交互模式打架）；禁用项（`setEnable(false)`）的前置控件交互不响应（item->handleEvent 首行 getEnable 检查已覆盖）。

### 3.4 子菜单与 MenuBar 顶级

- **子菜单**：子菜单面板 parent = MenuItem（:112-117），与 leadingControl（m_children）并存无冲突
- **MenuBar 顶级 entry 本期不支持**（顶级 caption 是 MenuEntry 数据而非 MenuItem，无 draw/handleEvent 节点；VSCode 顶级菜单亦无图标惯例）——列为决策点

## 4. 需求 2：逐 Item 字体

### 4.1 数据模型

`MenuItem` 扩展：

```cpp
FontName fontName = FontName::MapleMono_NF_CN_Regular;  // 与面板默认一致；仅 fontSize>0 时生效
int fontSize = 0;                                       // 0 = 继承面板级字号
```

（FontName 枚举无 None 值 → `fontSize==0` 作"未设置"标志，TreeView 4.1 同款）

### 4.2 字体获取与失效

- `MenuItem::ensureOwnFont()`：`fontSize > 0` → 按 `fontName + fontSize` 经 `loadMenuFont(this, ...)`（:53-67 复用）创建自身字体；否则用面板注入的 `m_font`
- **updateItemsFont 改造**（:289-293）：未覆盖 item 注入面板字体（现逻辑）；**已覆盖 item 重建自身字体**（fontSize>0 时按自身 fontName/fontSize）
- 失效点：面板 `setFontSize`/`setFontName`/`refreshScaleWith`（缩放变化）→ updateItemsFont 全量刷新（覆盖项重建、未覆盖项重注入）；`addItem` 注入
- recalculateSize 测量已按 item->m_font（:387-395）→ 面板宽度随覆盖项自动更新

### 4.3 行高（已拍板：方案 A 变行高）

**决策点 2 定稿**：每行高 = `item->fontSize>0 ? item->fontSize×ratio : m_fontSize×ratio`（方案 A）。layoutItems/hitTest 已是逐行累加模式（:421-454），recalculateSize 面板高度循环（:408-415）改逐行累加即可——实现量小、无溢出，菜单紧凑（无滚动条，变高无成本）。

### 4.4 绘制

`MenuItem::draw`（:120-168）标题/快捷键改用自身字体绘制（m_font 已含覆盖）；勾选/箭头（图形绘制）不变。逐项字体覆盖 fontSize>0 时按自身字号测量对齐（:137 fontHeight）。

## 5. JSON 与 CABI（第二期，**已立项**，同 TreeView 模式）

**决策点 4 定稿**：立项，容器类型 check-box/image 先行（同 TreeView）；**决策点 5 定稿**：MenuBar 顶级 entry 本期不支持（顶级 caption 是 MenuEntry 数据而非 MenuItem，无 draw/handleEvent 节点；VSCode 顶级菜单亦无图标惯例）。

- **JSON**：`populateMenuPanel` item 键扩展：`leadingControl`（`{"type": "check-box", ...}` / `{"type": "image", ...}`，递归调用 parseControl 复用 type 分发——TreeView v7 已验证；缺 rect 自动补占位）、`leadingGap`、`font`、`size`（0=继承）。子菜单递归（:1255-1259）同享
- **CABI**：**item-id 定位模式**（TreeView v7 先例）：MenuPanel 内先 `SetString(panel, "item-id", id)` 定位，随后 `SetFloat("item-leading-gap")` / `SetInt("item-font-size")` / `SetEnum("item-font")` / `SetPtr("item-leading-control")` 作用于该 item；leading-control 借用语义（生命周期调用方保证）；传 NULL 解除容器并摘树
  - 前提：MenuItem 需新增 `id` 字段 + MenuPanel 的 item 定位能力（目前 MenuItem 无 id）
  - 容器类型经 `UICornerstone_GetControlType` 查询（TreeView v7 同机制）

## 6. 涉及文件清单

| 文件 | 改动 |
|---|---|
| `include/Menu.h` | MenuItem 扩展：leadingControl/leadingGap/fontName/fontSize/id + setter/getter + ensureOwnFont；Builder 扩展 setLeadingControl/setFontName/setFontSize |
| `src/Menu.cpp` | MenuItem::draw（icon 区容器绘制 + 自身字体 + 有容器跳过勾选）、handleEvent（前置控件分支）、updateItemsFont（覆盖感知）；MenuPanel::handleEvent 命中后调 item->handleEvent；**m_iconAreaWidth 改实例状态**（随各行字体高度自适应）；recalculateSize/layoutItems/hitTest（变行高 + 文字起点公式） |
| `src/LayoutParser.cpp` | （第二期）populateMenuPanel 支持 leadingControl/leadingGap/font/size |
| `src/UICornerstoneAPI.cpp` | （第二期）item-id 定位 + item-leading-* 属性 |
| `test/test_menu.cpp` | 新增：前置 CheckBox/图片项 + 逐项字体；断言：点容器**不关菜单不触发 onClick**、勾选翻转生效、面板宽度随覆盖字号变化 |

## 7. 决策点（全部已拍板）

1. **icon 区即控件容器**（决策点 1）→ **采纳"改造"方案**：原 icon 区直接改造为容器位（非新增槽位）；icon 区宽改实例状态 = max(各行字体高度, 20)；文字起点 = icon 区右缘 + leadingGap；有容器不画勾选（备选"icon 区右缘与文字之间 + 勾选保留"未采纳）
2. **行高** → **方案 A 变行高**（每行按自身字号×1.6；layoutItems/hitTest 逐行累加已支持）
3. **点击前置控件** → **不触发 item onClick、不关闭菜单链**（控件自身交互生效；与 TreeView 语义相反）
4. **JSON/CABI 第二期** → **立项**，同 TreeView（check-box/image 先行；item-id 定位需新增 MenuItem::id）
5. **MenuBar 顶级 entry** → **本期不支持**
6. **容器尺寸** → **边长 = 字体高度，正方形**（水平居中于 icon 区；icon 区宽随其自适应）

效果图 [Menu_Enhancement_Preview.svg](Menu_Enhancement_Preview.svg) 已随文档确认布局：icon 区（容器）、leadingGap、文字起点、变行高（32 vs 38.4）、勾选标记（无容器行）五要素。