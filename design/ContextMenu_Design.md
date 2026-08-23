// ============================================================================
// ContextMenu_Design.md -- 右键上下文菜单控件设计文档
// 关键决策已拍板：design/ContextMenu_Analysis.md（决策点 1-8 全定稿）
// 实现文件：include/ContextMenu.h / src/ContextMenu.cpp
// 接入层：ControlBase 右键 / LayoutParser(contextMenu) / C ABI / 复用 MenuPanel
// ============================================================================

# ContextMenu 右键上下文菜单

## 1. 架构（决策点 1）
`ContextMenu : public Popup`，content = 一个 `MenuPanel`。**零新增布局/绘制/事件核心逻辑**——
PopMenu 浮层机制（挂 BENCH 顶层 / watcher / Esc / 外部点击 / 焦点边界）与 MenuPanel 的
菜单能力（hover / 子菜单 / 点击 / 逐项字体）全部复用。

## 2. 打开流程（show）
`show(x, y)`（x,y 为 bench 本地坐标，通常取右键事件鼠标坐标）：
1. 借最近实例确保 `GET_CONTEXT` 就绪；
2. `menuPanel->recalculateSize()` 得面板尺寸；
3. `Popup::open()` 挂树 + 创建 + 布局 + 显示 + 注册 watcher；
4. 第二遍按真实字体重算尺寸，并按视口钳制（不出屏）；
5. `setRect` 重定浮层尺寸 + `menuPanel->setRect(0,0,w,h)` + `layoutItems` + `show()`。

## 3. 关闭语义
- Esc：`Popup::beforeEventHandlingWatcher` → `onEscPressed` → `close`（现成）。
- 外部点击：watcher → `onOutsideClicked` → `close`（现成）。
- 点击菜单项：`ContextMenu::addItem` 包装用户回调，**先执行用户回调再 `close()`**。
- `isContainsPoint` 重写：自身 rect **或** MenuPanel 的 `isContainsPoint`（递归子菜单）→
  点击子菜单区不误判为外部点击。

## 4. 右键打开机制（决策点 2：A+B+C 全支持）
- **A 纯 API**：`menu->show(x, y)`。
- **B ControlBase 扩展**：`ControlImpl::setContextMenu(menu)` + `handleEvent` 在
  `MouseDown + Right` 命中本控件且菜单未显示时自动 `show(mx, my)`。
- **C JSON 键**：控件 JSON `"contextMenu": {"items":[...]}` → LayoutParser 复用
  `populateMenuPanel` 构造菜单并 `setContextMenu` 绑定。

## 5. CABI（决策点 5/6：一期）
| 函数 | 作用 |
|------|------|
| UICornerstone_CreateContextMenu | 创建 |
| UICornerstone_ContextMenuAddItem | 添加菜单项（caption/shortcut），点击自动关闭 |
| UICornerstone_ContextMenuAddSeparator | 添加分隔线 |
| UICornerstone_ContextMenuShow | 在 (x,y) 弹出 |
| UICornerstone_ContextMenuClose | 关闭 |
| SetPtr(inst, ctl, "context-menu", menu) | 控件绑定（决策点 B 的 CABI 形态） |

## 6. 验收（对照 AGENTS.md）
- [x] 声明式 UI（contextMenu JSON 键）+ CABI + C++ 类 API 接入
- [x] 三后端（test_contextmenu 断言 + SDL3 截图）经 18 项检查全过
- [x] 交互：点项回调+关闭 / 外部点击关闭 / Esc 关闭 / 边缘钳制 / 右键控件弹出
- [x] 属性键入 PropertyNames.h（kPropContextMenu="context-menu"）
- [x] 控件类型复用 ControlType::Popup（无需新增枚举）
- [x] CABI 5 函数 + SetPtr 绑定可经 C 创建/操作
- [x] 设计文档（ContextMenu_Analysis.md 决策拍板 + 本文档）
- [ ] README / 用户手册 / API_Mapping_Table / JSON Schema：统一刷新阶段处理（暂缓 §13）

## 7. 已知约束
- 键盘导航（上下键 + 回车）决策点 4 定稿：**后续与 Menu 控件一起实现**（同一 MenuPanel 键盘逻辑）。
- 边缘自适应为「钳制」（决策点 3），未实现翻转。
- 子菜单 / 分隔线 / 禁用项 / 快捷键 / 逐项字体 / icon 区：全部继承 MenuPanel 既有能力。
