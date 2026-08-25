// ============================================================================
// ContextMenu_Design.md -- 右键上下文菜单设计文档
// 上游决策：design/ContextMenu_Analysis.md（决策点 1-8 已拍板）
// 实现文件：include/ContextMenu.h / src/ContextMenu.cpp / src/LayoutParser.cpp(控件级 contextMenu)
//           src/UICornerstoneAPI.cpp(ContextMenu C ABI) / test/test_contextmenu.cpp
// ============================================================================

# ContextMenu 右键上下文菜单设计文档

> 状态：**设计定稿 · 已实施（对照源码自检）**
> 前身：[ContextMenu_Analysis.md](ContextMenu_Analysis.md)（决策点 1-8 拍板；本文档为设计化定稿 + 实施期决策回写）
> 关联：[Menu_Enhancement 相关文档]（MenuPanel 能力来源）、[ListView_Design.md](ListView_Design.md)（格式基准）、[StatusBar_Design.md](StatusBar_Design.md)（MenuPanel 弹窗同源）、[FocusSystem_Design.md](FocusSystem_Design.md)（作用域边界）
> 效果图：[ContextMenu_Preview.svg](ContextMenu_Preview.svg)（嵌入 §3）
> 修订注记：v1（2026-08-24）——按 Analysis 决策初版实施
> 修订注记：v2（2026-08-25）——**按 ListView_Design 格式重写**：回写实施期决策（chrome 抑制三点防御与 Popup::create 重置根因/MenuPanel scale 固定 1/双层框线根因/命中含子菜单）；补焦点系统/缩放方案/属性四层矩阵/测试策略；对照源码自检 2 遍

## 1. 需求概述

右键上下文菜单：任意位置弹出（纯 API / 控件绑定 / JSON 声明三路径），
Esc / 外部点击 / 点项后自动关闭；支持子菜单、分隔线、图标、快捷键列（MenuPanel 既有能力全复用）。

## 2. 现状调研（关键行号）

| 项目 | 现状 |
|---|---|
| 菜单能力 | MenuPanel：hover/子菜单/分隔线/逐项字体/`closeMenuChain`（`Menu.cpp:218`） |
| 浮层机制 | Popup（`Dialog.cpp`）：挂 BENCH 顶层 / watcher（Esc/外点）/ focusFirstContent / registerBoundary |
| 菜单构建 | `populateMenuPanel`（LayoutParser，MenuBar/StatusBar 同用） |
| 键盘体系 | FocusManager 作用域边界（`FocusSystem_Design.md §4.3`） |

## 3. 架构方案

```
ContextMenu : Popup                           （ControlType::Popup，浮层挂 BENCH 顶层）
├── m_menuPanel（MenuPanel，scale 固定 1——挂树继承父复合）
├── chrome 抑制（transparent / borderVisible=false / showFocusRing=false）
└── show(x,y)：recalc → Popup::open（挂树+watcher）→ 视口钳制 → 面板布局
```

![ContextMenu 效果图](ContextMenu_Preview.svg)

**关键设计决策**：

| 决策 | 选择 | 理由 / 排除项 |
|---|---|---|
| 浮层载体 | **Popup 基类**（挂 BENCH 顶层 + watcher 免费获得 Esc/外点关闭） | 子控件挂树会被父矩形裁剪且无 watcher；排除自管理浮层 |
| 菜单视觉 | **MenuPanel 全权负责**（chrome 抑制，§6.3） | Popup 默认底色/边框与面板圆角描边叠加形成双层框线（实测根因，§6.3） |
| 子面板缩放 | **MenuPanel scale 固定 1**（挂树经 setParent 继承父复合） | ctor 传父同款 scale 会双重叠加（2x 场景实测 4x 复合、双框错位） |
| 点项关闭 | **包装 onClick：用户回调后自动 close**（Analysis 决策 5） | 菜单语义；原始回调保留可空 |

## 4. 数据模型

```cpp
class ContextMenu : public Popup {
    shared_ptr<MenuPanel> m_menuPanel;   // 内容面板（唯一视觉载体）
};
```

- 菜单项数据全部存于 MenuPanel（`m_items`，MenuItem：caption/shortcut/leading/
  subMenu/checked/onClick）——ContextMenu 自身零菜单数据
- `ItemClickHandler = function<void(shared_ptr<MenuItem>)>`

## 5. 缩放方案

| 层 | 规则 |
|---|---|
| 面板尺寸 | `MenuPanel::recalculateSize()` 按菜单项内容计算（本地空间） |
| 复合 | MenuPanel scale 固定 1 → 挂 Popup 子树后 `m_xxScale = 1 × Popup scale`，**单次复合** |
| Popup 定位 | `show(x,y)`：视口钳制（`px/py` 收进 `GET_CONTEXT->viewport`）后 `setRect(px,py,w,h)`（绝对）|
| 渲染 | MenuPanel::draw 全走 `getDrawRect()`（阴影/底色/圆角描边/条目均随 scale）——2.0x 截图验证 |

## 6. 交互

### 6.1 打开与关闭（决策点 2：A/B/C 三路径）
- **A 纯 API**：`show(x, y)`（bench 坐标）——确保 context（借 `UIContext::getLastInstance`）→
  面板 create/recalc → `Popup::open()`（挂树 + watcher + focusFirstContent）→
  二遍 recalc + 视口钳制 + 面板布局
- **B 控件绑定**：`ControlImpl` 右键 hook——`MouseDown + Right` 命中本控件且菜单未显示 →
  `m_contextMenu->show(mx, my)`（`ControlBase.cpp:299`）；绑定经 `SetPtr(inst,ctl,"context-menu",menu)`
  或 C++ `setContextMenu`
- **C JSON**：控件级 `"contextMenu": {"items":[...]}` → `populateMenuPanel` 构建 + 绑定
- 关闭：点面板项（包装 onClick：**用户回调先执行，随后 close**）/ Esc / 外部点击
  （`Popup::beforeEventHandlingWatcher`，MouseDown 外点判定）

### 6.2 命中
- `isContainsPoint` 重写：自身 rect **或** MenuPanel 的 `isContainsPoint`（递归子菜单）——
  点击子菜单不误判为外部点击

### 6.3 焦点系统与 chrome 抑制（双层框线根因记录）
- `Popup::create()` 设定浮层焦点语义：`setFocusBoundary(true)`（作用域边界）+
  `setFocusable(false)` + `setShowFocusRing(false)`
- `focusFirstContent()` 兜底聚焦 Popup 自身——环已全局关闭，无焦点环方框
- **chrome 抑制（双层框线根因修复）**：菜单视觉由 MenuPanel 全权负责。`Popup::create()`
  会无条件重置为 Dialog 默认视觉（`transparent=false` / `borderVisible=true`，默认边框色
  83,83,90）——与 MenuPanel 圆角描边 (69,69,69) 叠加；GraphTool 1px 描边在**底/右边
  光栅化偏移 +1px** → 底边/右竖边双框线（顶/左两线重合，用户观察吻合）
- `applyChromeSuppression()`（transparent / borderVisible=false / showFocusRing=false）
  在 **ctor / create() 重写 / show()（Popup::open 之后）三点防御调用**——覆盖任何重建路径

## 7. 属性一致性（C++ / JSON / CABI / C++ Binding 四层）

| 项 | C++（规范实现） | JSON | CABI | C++ Binding |
|---|---|---|---|---|
| 菜单项 | `addItem(item)` / `addItem(caption, onClick)` / `addSeparator` | `"contextMenu".items[]`（caption/shortcut/separator/icon…，复用 populateMenuPanel） | `ContextMenuAddItem(inst,menu,caption,shortcut)` / `ContextMenuAddSeparator` | `Builder.addItem` / `Builder.addSeparator` |
| 显示/关闭 | `show(x,y)` / `close(result)` | —（运行时行为） | `ContextMenuShow(inst,menu,x,y)` / `ContextMenuClose` | `Builder.build` 后同 C++ |
| 控件绑定 | `setContextMenu` / `getContextMenu` | `"contextMenu"` 键（解析即绑定） | `SetPtr(inst, ctl, "context-menu", menu)`（`kPropContextMenu`） | `setProperty("context-menu", ...)` |
| 位置 | `show(x,y)` 参数 | —（运行时） | `ContextMenuShow` 参数 | 同 C++ |

> ContextMenu 为行为型浮层，无独立可配属性集——菜单项内容/样式由 MenuPanel/MenuItem
> 体系承载（逐项字体/图标/子菜单等，见 Menu 相关文档）。

## 8. JSON（控件级 contextMenu 键）

```json
{
  "controls": [{
    "type": "label", "id": "lbl", "rect": [10, 10, 120, 24], "caption": "t",
    "contextMenu": {
      "items": [
        {"caption": "Open", "shortcut": "Ctrl+O"},
        {"type": "separator"},
        {"caption": "Delete"}
      ]
    }
  }]
}
```

- `parseControl` 控件级 `"contextMenu"` 键（`LayoutParser.cpp:334`）：`populateMenuPanel`
  构建 MenuPanel → `setContextMenu` 绑定（决策点 2-C）
- items 字段复用 MenuPanel 既有解析（caption/shortcut/type:separator/leading 等）

## 9. C ABI / C++ Binding

**C ABI（5 专用函数 + SetPtr 绑定）**：

| 函数 | 作用 |
|---|---|
| `UICornerstone_CreateContextMenu(inst, x,y,w,h, xs,ys)` | 创建（挂 bench、隐藏） |
| `UICornerstone_ContextMenuAddItem(inst, menu, caption, shortcut)` | 添加项（包装自动关闭） |
| `UICornerstone_ContextMenuAddSeparator(inst, menu)` | 分隔线 |
| `UICornerstone_ContextMenuShow(inst, menu, x, y)` | 弹出 |
| `UICornerstone_ContextMenuClose(inst, menu)` | 关闭 |
| `UICornerstone_SetPtr(inst, ctl, "context-menu", menu)` | 控件右键绑定（决策点 2-B） |

**C++ Binding**：ContextMenu 类即规范实现 + `ContextMenuBuilder`（无 rect，仅 scale）：
`addItem(caption, onClick)` / `addItem(item)` / `addSeparator` → `build()`（内部 create）。

## 10. 涉及文件清单

| 文件 | 改动 |
|---|---|
| `include/ContextMenu.h` / `src/ContextMenu.cpp` | ContextMenu + ContextMenuBuilder + chrome 抑制 |
| `include/ControlBase.h` / `src/ControlBase.cpp` | m_contextMenu + 右键 hook + SetPtr 绑定 |
| `src/LayoutParser.cpp` | 控件级 `"contextMenu"` 键（populateMenuPanel 复用） |
| `include/UICornerstoneAPI.h` + `src/UICornerstoneAPI.cpp` | 一期 CABI（§9） |
| `include/PropertyNames.h` | kPropContextMenu = "context-menu" |
| `test/test_contextmenu.cpp` + `test/CMakeLists.txt` | 断言 + CABI + JSON + 交互链路 + 截图 |

## 11. 测试策略（test_contextmenu.cpp，19 项全过）

1. **断言**（独立 probe）：面板存在 / addItem·addSeparator / 点项回调触发 + 菜单关闭
2. **C ABI**：Create / AddItem×2 / AddSeparator / SetPtr 绑定 / Show / Close
3. **JSON**：`contextMenu` 键解析 → 菜单绑定 / 面板填充 / 条目布局
4. **交互链路**（onRender 帧驱动）：点项关闭 / 外部点击关闭 / Esc 关闭 /
   右键控件弹出（路径 B）/ 2.0x 缩放实例（ContextMenuBuilder）截图
5. **视觉核对**：单层圆角边框 + 阴影（双层框线回归检查）+ leadingControl 图标

## 12. 决策点（ContextMenu_Analysis.md，已拍板）

1. 架构：Popup + MenuPanel 复用（零新增布局/绘制/事件核心）✅
2. 打开路径：A 纯 API + B 控件绑定 + C JSON 三路径全支持 ✅
3. 边缘策略：视口钳制（不翻转）✅
4. 键盘导航：随 Menu 键盘统一（后续）✅
5. 点项自动关闭（包装 onClick）✅
6. CABI 一期（5 函数 + SetPtr）✅
7. JSON 控件级 contextMenu 键 ✅
8. 子菜单/分隔线/禁用项/快捷键/逐项字体：继承 MenuPanel ✅

## 13. 实施边界与现状限制

- 键盘导航（上下键 + 回车）随 Menu 键盘统一实现（同一 MenuPanel 键盘逻辑，后续）
- 边缘自适应为视口钳制，未实现翻转
- 弹出后菜单项不支持运行时动态增删后的自动重排（需再次 show 触发 recalc）
- chrome 抑制依赖 `applyChromeSuppression` 三点调用——新增重建路径时须补调用点
  （Popup::create 重置行为为框架既有语义）
