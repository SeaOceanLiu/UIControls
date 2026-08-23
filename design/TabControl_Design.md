// ============================================================================
// TabControl_Design.md -- 选项卡控件设计文档
// 决策与架构：design/TabControl_Analysis.md（决策点 1-8 已拍板，含 v1/v2 修订）
// 实现文件：include/TabControl.h / src/TabControl.cpp
// ============================================================================

# TabControl 选项卡控件

## 1. 架构（决策点 1：一体自绘）
`TabControl : ControlImpl`，**不拆 TabBar 子控件**——tab 条自绘（矩形 + 标题 + hover/选中高亮 +
4px 指示条），内容区承载页面控件（任意 Control，直接挂 TabControl 子树）。

## 2. 数据模型
- `enum class TabPosition { Top, Bottom, Left, Right }`（缺省 Top）
- `struct TabPage { title; page(Control); leadingControl(可选图标); tabRect }`
- 页面控件 `setParent(this)` + `addControl`，切换 = 旧 `hide()`/新 `show()` + `setRect(内容区)` + `onTabChange`

## 3. 四方向布局（决策点 5）
`relayout()` 按 position 计算 tab 条区与内容区（均相对控件原点，绘制时加 `m_rect` 绝对偏移）：
- Top/Bottom：条厚 = `fontSize*1.4 + 2*padding`，页签横排堆叠
- Left/Right：条宽 = 最长标题宽 + 2*padding，页签竖排堆叠（**文字不旋转**，决策点 2）
- 选中指示条贴页签条内侧（Top→下缘 / Bottom→上缘 / Left→右缘 / Right→左缘）

## 4. 交互
- MouseMove：hover 高亮（`hitTestTab` 绝对坐标命中）
- MouseDown 左键命中页签：`setCurrentIndex` → 页面切换 + `onTabChange`
- **键盘导航一期**（决策点 4）：TabControl 自身收到 KeyDown 时
  - Top/Bottom：`←/→` 上/下一页（循环），`Home`/`End` 首/尾
  - Left/Right：`↑/↓` 上/下一页（循环），`Home`/`End` 首/尾
  - 页面控件内方向键不截获（留给内部控件）

## 5. 属性（决策点 8 / §8.1 四层同步）
| 属性 | C++ | JSON | CABI | Binding |
|---|---|---|---|---|
| position | setPosition | `"position"` | SetEnum "position" | setProperty |
| fontSize | setFontSize | `"fontSize"` | SetFloat "font-size" | setProperty |
| currentIndex | setCurrentIndex/getCurrentIndex | `"currentIndex"` | SetInt/GetInt "current-index" | setProperty |

数据类专用 API：`addTab`/`insertTab`/`removeTab`/`setTabText`/`setTabPage`/`setTabLeadingControl`。
CABI 一期：`CreateTabControl` / `TabAddPage` / `TabSetTitle` / `TabSetPage` / `TabSetTabLeadingControl`。

## 6. JSON（决策点 7）
`tab-control` + `tabs[]`（title / icon / page 递归 parseControl 铺满内容区）+ `position` /
`fontSize` / `currentIndex` + `events.onTabChange`。见 test 内 JSON 用例。

## 7. 验收（对照 AGENTS.md）
- [x] 声明式 UI（tab-control JSON）+ CABI + C++ 类 API 接入
- [x] 三后端经 31 项检查全过（CPU 断言 20 + CABI 10 + 可视化切换 + JSON 4）
- [x] 交互：点页签切换 / 键盘导航(←→HomeEnd 循环) / 页面显隐 / removeTab 钳制 / 四方向
- [x] 属性键入 PropertyNames.h（kJsonTabs/kJsonPage/kJsonPosition/kJsonCurrentIndex/kControlTypeTabControl）
- [x] 控件类型枚举 ControlType::TabControl
- [x] CABI 5 函数 + 通用属性（position/current-index/font-size）可经 C 创建/操作
- [x] 设计文档（TabControl_Analysis.md 决策拍板 + 本文档）
- [ ] README / 用户手册 / API_Mapping_Table / JSON Schema：统一刷新阶段处理（暂缓 §13）

## 8. 已知约束（决策点 3 / §11）
- 关闭钮一期不做（需求未提）
- 竖向页签文字不旋转（水平排布）
- 页面内 Popup 浮层不随页面 hide 自动关闭（现状 WinFrame 同行为）
- tab 条无滚动（页签溢出截断，列后续增强）
