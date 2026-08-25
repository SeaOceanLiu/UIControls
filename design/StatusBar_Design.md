// ============================================================================
// StatusBar_Design.md -- 状态栏控件设计文档
// 决策点 1-6 已拍板（2026-08-22 与用户确认）
// 实现文件：include/StatusBar.h / src/StatusBar.cpp
// 接入层：LayoutParser（status-bar）+ C ABI（StatusBar* 6 函数）+ C++ Binding（Statusable）
// ============================================================================

# StatusBar 状态栏控件设计

## 1. 定位与风格
VSCode 风格底部状态栏：一条细长的水平条，横向排列若干「段」（segment），
左侧段左对齐、右侧段右对齐。每个段可显示文本、图标（任意控件）、或可点击
弹出菜单（如 VSCode 的「分支」段点击弹出分支列表）。

## 2. 数据模型
- `vector<StatusItem> m_items`：有序段列表。
- `StatusItem`：`{id,text,rightAlign,leadingControl,onClick,menuPanel,hitRect}`。
- 段宽度 = padding + (图标?图标宽+间距) + 文本宽 + padding。
- 布局：从左到右累加 left 段；rightAlign 段从右到左累加。

## 3. 弹窗（决策点 5）
- 内嵌一个**共享** `MenuPanel` 子控件（`m_popupPanel`，惰性创建）。
- 点击带 `menuPanel` 的段：向上弹出（与 MenuBar 下拉相反，`popDirection=Up`）。
- 复制：每次打开时清空共享面板，从目标 item.menuPanel 逐个 `addItem` 复制菜单项。
- 关闭：点击面板项 / 外部点击（Event::ClickOutside）/ Esc。
- 隐藏：弹出时 `m_popupPanel->setVisible(true)`，否则 `setVisible(false)`。

## 4. 交互
- 命中测试：`hitTestIndex(screenX, screenY)`——屏幕坐标经 drawRect 原点与 scale
  **二维逆变换**到本地布局空间后按 `hitRect` 判定（X/Y 缺一不可）。
- MouseMove：命中则更新 `m_hoveredItem` 并重绘（hover 高亮）；未命中（含 Y 出界）清除。
- LMBDown：若命中带菜单段 → openPopup；否则触发 item.onClick。
- 外部点击：ClosePopup。

## 5. 属性系统（§5.1 矩阵）
| JSON 属性       | CABI / C++ 方法            | 类型   | 触发 relayout |
|-----------------|----------------------------|--------|---------------|
| font-size       | setFontSize / getFontSize  | float  | 是            |
| item-height     | setItemHeight/getItemHeight| float  | 是            |
（尺寸 / 位置走 Control 通用 rect 属性，由 Bench 统一处理）

## 6. 渲染
- 背景：`m_bgColor`（默认 #007ACC VSCode 蓝，可通过 fill-color 覆盖）。
- 文本：`m_fontSize`，左对齐，垂直居中。
- hover 段：背景略微提亮（alpha 叠加）。
- 图标：`leadingControl` 作为子控件，draw 时按 hitRect 左对齐绘制。

## 7. 声明式 UI（status-bar）
```json
{
  "type": "status-bar",
  "id": "status",
  "rect": [0, 876, 1400, 24],
  "font-size": 13,
  "items": [
    {"id":"branch","text":"main","icon":"icons/git.png","menu":"#branchMenu"},
    {"id":"encoding","text":"UTF-8","right":true}
  ]
}
```
- `items`：数组，每项 `id`(必填) / `text` / `icon`(控件 id) / `menu`(MenuPanel id) / `right`(bool)。
- `icon` / `menu` 解析为控件句柄挂到对应段。

## 8. CABI（6 函数）
| 函数 | 作用 |
|------|------|
| UICornerstone_CreateStatusBar | 创建 |
| UICornerstone_StatusBarAddItem | 添加段（text, rightAlign）|
| UICornerstone_StatusBarSetItemText | 改文本 |
| UICornerstone_StatusBarRemoveItem | 移除段 |
| UICornerstone_StatusBarSetItemMenu | 绑定弹窗 MenuPanel |
| UICornerstone_StatusBarSetItemIcon | 绑定图标控件 |

## 9. 验收清单（对照 AGENTS.md）
- [x] 声明式 UI（JSON）+ CABI + C++ Binding 接入
- [x] 三后端渲染（SDL3/Skia/Headless）经 test_statusbar 断言 + 截图
- [x] 缩放：setRect 触发 relayout，段按新宽重排
- [x] 属性键入 PropertyNames.h（font-size/item-height/status-bar/right）
- [x] 字面量常量化
- [x] 控件类型枚举 + 字符映射（kControlTypeStatusBar）
- [x] CABI 6 函数可经 C 创建/操作
- [x] C++ Binding 封装（Statusable）
- [x] JSON Schema 同步（暂缓，待统一刷新）
- [x] 设计文档 / README / 用户手册 / API_Mapping_Table 刷新（统一阶段）

## 10. 已知约束
- 弹窗 MenuPanel 复用为「共享单例」，同时只能弹出一个段菜单。
- 段不支持自定义宽度（自动按内容），后续如需固定宽可加 `width` 属性。
