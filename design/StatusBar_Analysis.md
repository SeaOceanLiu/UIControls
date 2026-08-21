# StatusBar（状态栏）需求分析

> 状态：**已拍板（2026-08-17）· 转主设计开发 Session 实施**
> 关联：[Menu_Design.md](Menu_Design.md)、[Menu_Enhancement_Analysis.md](Menu_Enhancement_Analysis.md)（MenuPanel 增强直接惠及状态栏弹窗）、[ContextMenu_Analysis.md](ContextMenu_Analysis.md)（决策先例：CABI/Binding 一期、钳制、Esc）、[WinFrame_Design.md](WinFrame_Design.md)
> 效果图：[StatusBar_Preview.svg](StatusBar_Preview.svg)（嵌入 §3）
> 修订注记：v1（2026-08-17 拍板）——决策点 1-4 采纳建议；**决策点 5/6 由"icon 二期"改为"全部一期完成"**（leadingControl/icon 的 JSON 与 CABI/Binding 均一期）
> 修订注记：v2（2026-08-17）——**对齐 ListView 一致性补充**：① 新增 §5.1 属性一致性矩阵（fontSize/itemHeight 四层同步，键名 `"font-size"`/`"item-height"` 循 CABI kebab 惯例）；② **API 精简**：CABI 数据操作全部直接传 item id（删除"TreeView v7 先例"错误引用——`item-id` 两步定位为 CABI_Property_Design.md 二期通用机制，一期不引入）；③ **源码行号核对修正**：MenuBar::handleEvent=:1105-1160、openMenu=:950-966、向下弹出=:961-964、外部关闭=:1123-1141、hover 切换=:1143-1149、layoutEntries=:919-935、hitTest=:937-948、parseMenuBar=:1157（原 923-980/768-784/782/949-957 等引用已失准）；④ C++ 命名对齐 `addStatusItem`/`setStatusItemMenu`（原 `setStatusItem`）
> ⚠️ **依赖说明**：Menu 增强的 icon JSON/CABI 原为二期立项（Menu_Enhancement_Analysis 决策点 4）；StatusBar 一期需 icon 机制 → **实施时 icon 解析/CABI 机制随 StatusBar 一期同步落地**（与 Menu 增强共用同一实现，Menu 增强直接受益，无需二期）

## 1. 需求概述

提供 StatusBar 控件（VSCode 风格底部状态栏）：

1. 可添加不同状态信息项（文本、分区、可更新）
2. 点击项后**向上弹出菜单**（类似菜单栏点击向下弹窗，状态栏在底部故向上弹）

## 2. 现状调研（关键行号）

| 项目 | 现状 |
|---|---|
| 现有实现 | **无 StatusBar**（全库 grep 无命中） |
| 控件类型 | `ControlType` 枚举（`ControlBase.h:139-144`）含 MenuItem/MenuPanel/MenuBar/WinFrame——需新增 `StatusBar`；字符串注册模式 `"menu-bar"`（`PropertyNames.h:572`） |
| 弹窗能力（复用） | MenuPanel：布局/绘制/hover/子菜单/icon 区容器/逐项字体/变行高（MenuPanel 实现区，`Menu.cpp:370-800` + Menu_Enhancement_Analysis 增强） |
| 弹窗交互模式（复用） | **MenuBar::handleEvent**（`Menu.cpp:1105-1160`）：点击 entry → `openMenu`（:950-966）→ 面板向下弹出 `y = hitRect.top + hitRect.height`（:961-964）；hover 切换 `switchMenu`（:1143-1149）；点击面板外 → `exitMenuMode` 关闭（:1123-1141） |
| 坐标语义 | MenuPanel 是 MenuBar 子控件，`setPosition` 需相对父控件坐标（扣除父链偏移，`Menu.cpp:954-960` openMenu 换算注释）；MenuBar::layoutEntries 更新自身宽度（:919-935） |
| 键盘/Esc | MenuBar 无键盘处理（现状限制）；Esc 机制仅在 Popup watcher（`Dialog.cpp:228-235`） |
| 底部布局 | WinFrame 仅标题栏 + 客户区（`WinFrame.h:26-29`），无底部槽位；LayoutEngine 无 dock——状态栏由应用/父容器放置（普通控件 rect） |
| JSON 解析 | MenuBar 模式：`parseMenuBar`（`LayoutParser.cpp:1157-1215`）+ `populateMenuPanel`（:1217-1270，含递归子菜单与事件解析）——可直接复用 |
| 测试 | 可视化测试模式（test_menu/test_winframe 先例） |

## 3. 架构方案（决策点 1/2）

```
StatusBar : ControlImpl
├── m_items（vector<StatusItem>：left/right 两组）
├── m_popupPanel（共享 MenuPanel 实例：点击有菜单的 item 时向上弹出）
└── 交互：仿 MenuBar::handleEvent 模式（点击弹窗 / hover 高亮 / 点击外部关闭）
```

![StatusBar 效果图](StatusBar_Preview.svg)

### 3.1 数据模型

```cpp
struct StatusItem {
    string id;                                  // 唯一标识（更新定位用）
    string text;                                // 显示文本
    bool   rightAlign = false;                  // false → 左侧组；true → 右侧组
    shared_ptr<Control> leadingControl;         // 可选：文字前缀控件（任意 Control：Image 静态图标 / LuotiAni 动画 / 用户自建），空则不留白
    function<void(shared_ptr<StatusItem>)> onClick;      // 可选：点击直接执行（无菜单时）
    shared_ptr<MenuPanel> menuPanel;            // 可选：有 → 点击向上弹窗；无 → 仅文字/onClick
};
```

- `addStatusItem(item)` / `updateStatusItemText(id, text)` / `removeStatusItem(id)` / `setStatusItemLeadingControl(id, control)` / `setStatusItemMenu(id, panel)`（**命名规范：C++ `Verb+Object`，与 CABI `StatusBar+Verb+Object` 对齐**）
- 右侧组从右缘向左排（VSCode 惯例：语言/行号/编码等居右）
- **弹窗惰性创建**：`m_popupPanel` 在首个带 menuPanel 的 item 添加时才构造——**纯文字状态栏（全 item 无菜单）不产生弹窗实例**，StatusBar 退化为纯文本绘制条

### 3.1.1 StatusItem.leadingControl（状态栏文字前缀控件）

> **作用位置：StatusBar 条上 item 的文字前**（如分支图标、加载动画），**与弹窗 MenuPanel 的菜单项无关**——弹窗菜单项的前缀由 MenuPanel 增强（leadingControl 容器）自带，两者是不同位置的两个能力。

- 布局语义参照 Menu_Enhancement_Analysis 决策②（icon 容器）：**有 → 文字前留容器区，边长 = 字号×1.4**（与 TabControl/ListView 同款公式，全库统一）；无 → 不留白
- 内容为任意 Control（Image / LuotiAni 粒子动画 / 用户自建控件如自绘 spinner），由应用创建并挂入，`relayout` 时 `setPosition` 到容器区；控件自身事件自理
- **JSON/CABI 一期**（拍板：全部一期）：JSON items 支持 `icon` 字段 + CABI 接口，与 Menu 增强 icon 机制共用解析/接口实现（ContextMenu 先例延伸）
- **实施注意点**：动画控件 tick 依赖现有动画驱动链路（LuotiAni 走 update/draw 循环），嵌入 StatusBar 后需验证透传——与 MenuPanel leadingControl 增强同一问题，一并验证

### 3.2 布局（仿 MenuBar::layoutEntries，`Menu.cpp:919-935`）

- 字号/行高可配（**属性，§5.1 矩阵**：`setFontSize`/`setItemHeight`，默认对齐 Menu 字号体系）；item 间距 + 左右 padding 常量
- left 组从左缘累加；right 组从右缘向左累加
- `setRect`/`setParent` 时 relayout（同 MenuBar 模式）
- item 命中检测：`hitTest(x, y)` 遍历两组 hitRect（同 MenuBar `:937-948`）

### 3.3 弹窗（决策点 2：向上）

**采纳 MenuBar 模式**（StatusBar 内嵌共享 MenuPanel，自身管理位置与关闭）——与 MenuBar 弹窗交互代码路径同源，坐标精确可控：

- 点击 item（有 menuPanel）→ `menuPanel->recalculateSize()` → **向上定位**：
  `panelLocalY = itemLocalTop - panelHeight`（面板**底缘贴 item 顶缘**；MenuBar 向下为 `hitRect.top + hitRect.height` 贴面板顶（`Menu.cpp:961-964`），方向相反）
- 左右位置：与 item 左缘对齐（可加小偏移）
- 打开后进入菜单模式：面板内 hover/子菜单由 MenuPanel 自理；点击面板外 → 关闭（仿 `Menu.cpp:1123-1141`）
- 顶部越界（窗口矮）：钳制（决策点 4）
- 子菜单仍向右展开（`Menu.cpp:490` 现成），不随弹窗方向变化

**弹窗菜单项自动获得 MenuPanel 增强能力**（Menu_Enhancement_Analysis 已拍板）：leadingControl 容器（icon 区）/ 逐项字体 / 变行高——MenuPanel 级特性，StatusBar 弹窗直接继承，**实施顺序依赖 Menu 增强先行落地**。

> 备选：Popup + MenuPanel content（ContextMenu 架构）——watcher 关闭现成，但锚定 item 需换算坐标，且需处理子菜单 isContainsPoint（ContextMenu §3.3 问题）；MenuBar 模式更近，采纳。

### 3.4 外部点击关闭与 Esc（决策点 3）

- 外部点击：仿 MenuBar（`:1123-1141`）✓
- **Esc：本期不做**——MenuBar 现状亦无 Esc；与菜单键盘导航（上下键/回车/Esc）**后续随 Menu 控件键盘增强一起实现**（ContextMenu 决策点 4 同款：同一 MenuPanel 键盘逻辑一次做）

## 4. 顶部越界处理（决策点 4）

- 状态栏贴底，弹窗向上空间大；窗口矮时（如弹窗高 > item 顶到窗口顶距离）：
- **钳制**（建议，与 ContextMenu 钳制决策一致）：`panelLocalY = max(panelLocalY, 窗口顶 - 面板顶)`——面板不出窗口顶部；参考 `Dialog.cpp:96-103` Anchored 钳制公式模式
- 翻转（向下弹）列为可选项，本期不做

## 5. JSON（决策点 5）

```json
{
  "type": "status-bar",
  "rect": {"x": 0, "y": 740, "w": 1024, "h": 24},
  "fontSize": 13,                // 文本字号（缺省对齐 Menu 字号体系）
  "itemHeight": 24,              // 条高/行高（缺省随 rect.h 或字号体系）
  "items": [
    {"id": "branch", "text": "main", "align": "left", "icon": "provider:icons/git-branch",
     "menu": {"items": [{"caption": "切换分支", "events": {...}}, ...]}},
    {"id": "lang", "text": "C++", "align": "right",
     "menu": {"items": [...]}},
    {"id": "ln", "text": "行 12, 列 34", "align": "right"}
  ]
}
```

- items 数组：`id`/`text`/`align`(left|right)/`icon`（**一期**，资源引用语义与 Menu 增强 icon 共用）/`menu`（内嵌 items，**复用 `populateMenuPanel`**：`LayoutParser.cpp:1217-1270`，含递归子菜单 + 事件解析）
- 无 menu 的 item 点击事件：`events.onClick`（复用 parseEvents）
- `parseStatusBar` 仿 `parseMenuBar` 结构（`LayoutParser.cpp:1157`）；`icon` 字段解析与 Menu 增强 icon 机制共用（拍板：全部一期）

### 5.1 属性一致性（C++ API / JSON / CABI / C++ Binding 四层同步，ListView §5.6 同规则）

> **规则**：属性四层同步、无静默缺失；分层命名惯例：C++ `set+UpperCamel` / JSON camelCase / CABI 通用属性 kebab-case（既有惯例 "font-size"/"row-height"，`UICornerstoneAPI.h:449-470`）/ Binding `setProperty(key, value)`（键名 = JSON 键）。
> **API 精简原则**：控件级属性 → 通用属性接口；**专用 `StatusBarXxx` 仅保留需 item id 直接传参或对象参数的数据操作**。

| 属性 | C++（规范实现） | JSON | CABI（通用属性） | C++ Binding（统一属性接口） |
|---|---|---|---|---|
| 字号 `fontSize` | `setFontSize` | `"fontSize"` | `SetFloat(inst, bar, "font-size", v)`（"font-size" 为既有 CABI 属性先例） | `setProperty("fontSize", v)` |
| 条高/行高 `itemHeight` | `setItemHeight` | `"itemHeight"` | `SetFloat(inst, bar, "item-height", v)` | `setProperty("itemHeight", v)` |

**数据类（专用 API，item id 直接传参）**：

| 操作 | C++ | JSON | CABI | C++ Binding |
|---|---|---|---|---|
| 添加 item | `addStatusItem` | `"items"`（一期） | `StatusBarAddItem(inst, bar, id, text, align)` | `Builder.addItem` |
| 更新文本 | `updateStatusItemText` | `items[].text` | `StatusBarSetItemText(inst, bar, id, text)` | `Builder.updateItemText` |
| 删除 item | `removeStatusItem` | —（JSON 静态） | `StatusBarRemoveItem(inst, bar, id)` | `Builder.removeItem` |
| 设置菜单 | `setStatusItemMenu` | `items[].menu`（一期） | `StatusBarSetItemMenu(inst, bar, id, handle)` | `Builder.itemMenu` |
| 设置前缀控件 | `setStatusItemLeadingControl` | `items[].icon`（一期） | `StatusBarSetItemIcon(inst, bar, id, handle)` | `Builder.itemIcon` |

**事件**：item 点击 `onClick`——C++ `StatusItem.onClick` 回调 + JSON `events.onClick` + CABI 事件注册 + Binding 回调，四层一期。

> **item 定位模式**：CABI item 级操作统一**直接传 `id` 参数**（一期简洁路径）；CABI 设计文档的通用 `item-id` 定位属性（先 `SetString("item-id", id)` 再属性读写，`CABI_Property_Design.md`）为**后续通用机制**，TreeView item 级属性（"item-leading-control" 等）与 StatusBar 二期统一接入——一期不引入。

## 6. CABI / C++ Binding（决策点 6，一期，ContextMenu 先例）

- **CABI**（全部一期）：`UICornerstone_CreateStatusBar` + 数据类专用（`StatusBarAddItem(inst, bar, id, text, align)` + `StatusBarSetItemText(inst, bar, id, text)` + `StatusBarRemoveItem(inst, bar, id)` + `StatusBarSetItemMenu(inst, bar, id, handle)` + `StatusBarSetItemIcon(inst, bar, id, handle)`，**全部 item id 直接传参**；icon/leadingControl 借用语义——生命周期调用方保证，同 CABI_Property_Design.md TreeView 约定）+ 控件级属性走通用 setter（`SetFloat("font-size")`/`SetFloat("item-height")`，§5.1 矩阵）
- **C++ Binding**：StatusBar 类 + Builder 暴露（addItem/updateItemText/removeItem/itemMenu/itemIcon + 事件 onClick + 属性统一 `setProperty` 接口，键名 = JSON camelCase）
- **无独立的 item 定位两步式 CABI**（"先 item-id 再属性"的通用模式为二期统一机制，见 §5.1）

## 7. 涉及文件清单

| 文件 | 改动 |
|---|---|
| `include/StatusBar.h`（新） | StatusBar + StatusItem + Builder；ControlType 枚举加 StatusBar（`ControlBase.h:139-144`） |
| `src/StatusBar.cpp`（新） | 布局（left/right 两组）、绘制（文本 + hover 高亮 + 分隔）、handleEvent（点击弹窗/外部关闭）、向上定位 + 钳制 |
| `src/LayoutParser.cpp` | `parseStatusBar` + 控件类型注册（`"status-bar"`，PropertyNames） |
| `include/UICornerstoneAPI.h` + `src/UICornerstoneAPI.cpp` | 一期 CABI（见 §6） |
| Binding（`binding/`） | 一期 Binding 暴露 |
| `test/test_statusbar.cpp`（新）+ `test/CMakeLists.txt` | 可视化 + 断言（items 布局、点击弹窗向上 `getRect` 位置、外部点击关闭、钳制） |

## 8. 决策点（2026-08-17 已拍板）

1. **分组布局**：left/right 两组（VSCode 惯例）✅ 同意
2. **弹窗实现**：StatusBar 内嵌共享 MenuPanel + 自身管理位置/关闭（MenuBar 模式）✅ 同意（备选 Popup 架构见 §3.3）
3. **Esc 关闭**：本期不做（与 MenuBar 一致），后续随 Menu 键盘增强一起 ✅ 同意
4. **顶部越界**：钳制 ✅ 同意（翻转可选项不做）
5. **JSON**：一期 `status-bar` 控件 + items 数组（复用 populateMenuPanel）；**item icon/leadingControl JSON 一期**（拍板修订：原建议二期，改为一期，与 Menu 增强 icon 机制共用）✅ 同意
6. **CABI / C++ Binding**：**全部一期**（拍板修订：原建议 icon 二期，改为一期）✅ 同意

## 9. 现状限制（注明，不处理）

- 子菜单向右展开可能超出窗口右缘（MenuBar 现状同限，`Menu.cpp:490` 无钳制）
- item 文本测量按面板字体（逐项字体增强后续惠及）
- **库内无帧动画/spinner 类控件**：Animation = LuotiAni 粒子系统；加载指示类动画需用户自建控件挂入 leadingControl（或后续新增）