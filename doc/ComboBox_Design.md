# ComboBox 下拉选择框控件设计文档

## 1. 概述

ComboBox（下拉选择框）是一种允许用户从预定义选项列表中选择值、或手动输入自定义文本的 UI 控件，广泛应用于属性编辑器中的枚举/选择类型属性（如 orientation 选择 Horizontal/Vertical、textAlign 选择 Left/Center/Right）。包含闭合状态（文本输入框 + 下拉箭头）和展开状态（下拉列表 Popup）两种视觉形态。

### 1.1 视觉结构

```
闭合状态:
┌──────────────────────┬─┐
│ Horizontal           │▼│
└──────────────────────┴─┘

展开状态（下方有空间）:
┌──────────────────────┬─┐
│ Horizontal           │▲│
├────────────────────────┤
│ ■ Horizontal         │  ← 选中项高亮
│   Vertical           │
│   Option 3 ...       │  ← 长文本截断
└────────────────────────┘

展开状态（下方空间不足 → 向上弹出）:
┌────────────────────────┐
│   Vertical             │
│   Option 3 ...         │
├────────────────────────┤
│ Horizontal           │▲│
└────────────────────────┘
```

> 注：上方为 ASCII 示意图，精确 SVG 将在实现阶段补充。

### 1.2 需求来源

- **CornerstoneDesigner F3** 属性编辑器中的枚举/选择类型属性
- 通用 UI 模式，几乎所有桌面应用都需要
- 需求文档：`requirements/ComboBox_需求文档.md`

---

## 2. 设计决策

### 2.1 决策记录

| 编号 | 决策 | 方案 | 理由 |
|------|------|------|------|
| D1 | **基类选择** | 继承 `EditBox` 而非 `ControlImpl` | ComboBox IS-A 可输入文本的编辑框；EditBox 提供了文本输入、光标、选区、剪贴板、IMM 等全部基础设施，避免 ~500 行重复实现 |
| D2 | **下拉箭头** | `RenderDevice::drawTriangle` 填充三角形，一个调用绘制箭头 | 零外部依赖、无字体耦合、颜色可控、缩放后视觉一致；三角形是 GPU 原生基元，性能优于 TextRenderer 和位图 |
| D3 | **弹出列表** | 使用 `Popup` 而非直接 Panel | Popup 内置外部点击关闭、ESC 关闭、z-order 提升（BENCH->addControl）、焦点边界管理 |
| D4 | **列表渲染** | 自定义绘制（`ComboBoxListPanel`）而非 Label 子控件 | 避免 items 变化时频繁创建/销毁子控件；滚动时仅调整绘制偏移；选中/悬停状态绘制层直接处理 |
| D5 | **条目类型** | Phase 1 纯文本 → Phase 2 Renderer 接口扩展 | 降低 Phase 1 复杂度，同时保证可扩展性 |
| D6 | **条目高度** | Phase 1 固定高度 → Phase 2 支持可变高度 | 同上，Phase 2 开放 `ComboBoxItem::customHeight` |
| D7 | **空间不足自适应** | 选空间更大侧 → 调整 popup 高度适配可用空间 → 不足一个条目时拒显 | 避免弹出被裁切的列表，提升体验 |
| D8 | **闭合态循环选择** | 聚焦时 ↑/↓/滚轮直接循环切换选中项（不弹出 Popup） | 无需展开即可快速切换，与 Windows 原生 ComboBox 一致 |
| D9 | **ScrollBar 区域适配** | 方案 A：文本绘制 clip rect 缩减宽度，背景/高亮铺满全宽 | 视觉连贯，实现简洁 |
| D10 | **双模式** | `m_editable` 布尔开关（默认 false=只读）：只读模式点击任意区域开/收下拉、禁键盘输入；可编辑模式点正文聚焦编辑、点箭头开/收下拉 | 属性编辑器需要只读枚举选择，也需要可输入自定义文本；与 Windows CBS_DROPDOWN/DROPDOWNLIST 对应 |
| D11 | **type-ahead 匹配** | 可编辑模式输入时 `findItemByText()` 先精确（忽略大小写）后前缀匹配，回车选中；无匹配保留输入且 `selectedIndex=-1` | 输入即匹配最常用，无匹配不清空用户输入 |
| D12 | **展开态点击自身区域收起** | before watcher 在 `create()` 注册（先于 Popup clickOutside watcher），展开时点击自身区域（编辑模式箭头区/只读任意区域）直接收起并消费事件 | 避免 Popup 先关后 `togglePopup()` 又重开（关闭→立即重开的体验 bug） |

### 2.2 架构关系图

```
┌─────────────────────────────────────────────────────┐
│ ComboBox : EditBox                                   │
│                                                      │
│  [自身绘制]                                          │
│  ├── EditBox::draw()  → 背景 + 文本 + 光标 + 选区    │
│  └── 箭头叠加绘制     → ▼ / ▲ (Unicode, m_margin.right 隔离)│
│                                                      │
│  [事件处理]                                          │
│  ├── 只读拦截        → TextInput 直接 return true    │
│  ├── 点击分流        → 只读任意区域/可编辑箭头区 toggle│
│  ├── ↓/↑ KeyDown     → openPopup()/闭合态循环切换    │
│  ├── 可编辑输入      → EditBox 写入 + type-ahead 高亮 │
│  └── 其余事件        → EditBox::handleEvent()        │
│                                                      │
│  [before watcher]  create() 注册 KeyDown+MouseDown   │
│  ├── 展开态键盘导航 → ↑/↓/Enter/Esc/PageUp/Home/End  │
│  └── 展开态点击自身 → closePopup() + return true     │
│                                                      │
│  m_popup ─────────────────────────────────────────┐  │
│  (shared_ptr<Popup>)                               │  │
│  ┌──────────────────────────────────────────────┐  │  │
│  │  m_listPanel (ComboBoxListPanel : ControlImpl)│  │  │
│  │  [自定义绘制列表 items]                      │  │  │
│  │  [MouseMove → 更新 m_hoveredIndex]           │  │  │
│  │  [MouseDown → selectItem → closePopup]       │  │  │
│  │  [MouseWheel → 调整滚动偏移]                 │  │  │
│  │  [↑/↓ KeyDown → 移动高亮]                    │  │  │
│  │  [Enter KeyDown → selectItem → closePopup]   │  │  │
│  │                                              │  │  │
│  │  Items (索引 0..N-1 固定高度, 滚动偏移渲染)   │  │  │
│  │  ┌───────┐ ┌───────┐ ┌───────┐              │  │  │
│  │  │Item 0 │ │Item 1 │ │Item 2 │ ...           │  │  │
│  │  │选中高亮│ │  正常 │ │ 悬停  │              │  │  │
│  │  └───────┘ └───────┘ └───────┘              │  │  │
│  └──────────────────────────────────────────────┘  │  │
│                                                     │  │
│  m_scrollBar ───────────────────────────────────────┘  │
│  (shared_ptr<ScrollBar>)                               │
│  [items.size() > m_maxVisibleItems 时显示]              │
│  [控制列表滚动偏移]                                     │
└─────────────────────────────────────────────────────────┘
```

---

## 3. 功能规格

### 3.1 功能列表

| 编号 | 功能 | 优先级 | 说明 |
|------|------|--------|------|
| F1 | 下拉列表展示 | P0 | 点击箭头展开列表，显示所有选项 |
| F2 | 选中高亮 | P0 | 当前选中的选项在列表中高亮显示 |
| F3 | 闭合显示选中值 | P0 | 关闭时输入框中显示当前选中项的 label |
| F4 | 手动输入文本 | P0 | 可编辑模式（`editable=true`）可键入任意文本，type-ahead 高亮匹配项；只读模式拦截输入 |
| F5 | 键盘导航 | P1 | 展开后 ↑/↓ 移动高亮，Enter 选中，Esc 关闭 |
| F6 | 搜索/过滤 | P2 | 输入文字自动过滤选项列表（后续版本） |
| F7 | 禁用项 | P2 | 部分选项可标记为 disabled，灰色不可选 |
| F8 | 可变条目高度 | P2 | 支持每项自定义高度（后续版本） |
| F9 | 多条目不 Popup 渲染 | P2 | 支持 CheckBox/图标+文本等多类型条目（后续版本，通过 Renderer 接口） |

### 3.2 交互行为

| 操作 | 只读模式（`editable=false`，默认） | 可编辑模式（`editable=true`） |
|------|------|------|
| **点击任意区域** | `togglePopup()` — 闭合时展开，展开时**收起** | 仅点击箭头区 `togglePopup()`；点正文 → 聚焦进入文本编辑 |
| **展开态点击自身区域** | 收起 Popup（不重新打开） | 点箭头区收起；点正文 → 聚焦编辑 |
| **文本输入（TextInput）** | **拦截丢弃**（EditBox 仅检查 m_focused，不检查可编辑性，必须在此拦截） | 写入编辑框 + type-ahead：`findItemByText()` 先精确（忽略大小写）后前缀匹配，命中则高亮该项 |
| **Enter 键（聚焦+闭合）** | — | 回车确认：精确/前缀匹配到项 → `selectItem`；无匹配 → `selectedIndex=-1`、输入内容保留 |
| **↓/↑ 键（聚焦+闭合）** | 直接按列表项顺序切换选中项（不展开 Popup），循环回卷 | 同上 |
| **MouseWheel（聚焦+闭合，且鼠标在控件内）** | 等同于 ↑/↓ 键 | 同上 |
| **Enter 键（展开时）** | 选中当前高亮项 → 更新文本 → 关闭（Confirmed）→ 触发回调；高亮无效/禁用 → 仅关闭 | 同上 |
| **ESC 键（展开时）** | 关闭 Popup，文本**恢复**为展开前的值 | 关闭 Popup，**保留当前输入**（含未匹配文本与选中状态） |
| **点击列表项** | 选中该项 → 文本更新为选中 label → 关闭 → 触发回调 | 同上 |
| **点击 Popup 外部** | 关闭 Popup，文本恢复为展开前的值 | 关闭 Popup，保留当前输入 |
| **MouseWheel（展开时）** | 滚动列表内容（超出 maxVisibleItems 时） | 同上 |
| **Tab 键（展开时）** | 关闭 Popup，焦点移到下一个控件 | 同上 |

> **注意**：展开时文本不随悬停变化。只有 Enter 或点击确认时文本才更新。ESC/外部点击按模式区分：只读恢复原值，可编辑保留输入。与 Windows 原生 ComboBox 的 DROPDOWNLIST / DROPDOWN 风格一致。

### 3.3 闭合态方向键/MouseWheel 循环选择

聚焦且闭合时，用户可通过 ↑/↓ 键或鼠标滚轮**直接切换选中项**，无需展开 Popup：

```
闭合态, 聚焦, m_items = [A, B, C, D], m_selectedIndex = 1(B)

  按 ↓ → m_selectedIndex = 2(C),  setText("C"),  触发 onChange
  按 ↓ → m_selectedIndex = 3(D),  setText("D"),  触发 onChange
  按 ↓ → m_selectedIndex = 0(A),  setText("A"),  触发 onChange  (回卷)
  按 ↑ → m_selectedIndex = 3(D),  setText("D"),  触发 onChange

  滚轮向下 → 等同 ↓
  滚轮向上 → 等同 ↑
```

实现要点：
- 跳过禁用项（`disabled = true` 的条目不参与循环）
- 只有一条目时，↑/↓/滚轮无效果
- `m_items` 为空时忽略
- 循环选择不展开 Popup，不修改 `m_savedSelectedIndex`
- 每切换一次触发一次 `onSelectionChanged`
- 可通过 `setCycleEnabled(false)` 关闭（属性 `cycle-enabled`），关闭后 ↑/↓/滚轮不循环切换

### 3.3 双模式（可编辑开关）

```cpp
bool m_editable = false;   // 默认只读（CBS_DROPDOWNLIST 风格）
void setEditable(bool editable);
bool isEditable() const;
```

| 模式 | 对应 Windows 风格 | 输入 | 点击 |
|------|------------------|------|------|
| 只读（默认） | CBS_DROPDOWNLIST | 禁止（TextInput 拦截） | 任意区域开/收下拉 |
| 可编辑 | CBS_DROPDOWN | 允许 + type-ahead 匹配 | 正文编辑，箭头开/收下拉 |

> 原设计中的 `ComboBoxStyle` 枚举未实现：双模式通过 `m_editable` 布尔开关（默认 false）区分，经 Builder/JSON/C ABI/属性系统均可配置。

---

## 4. 类设计

### 4.1 ComboBoxItem 数据结构

```cpp
struct ComboBoxItem {
    std::string label;       // 显示文本
    std::string value;       // 实际值
    bool disabled = false;

    // ── Phase 2 扩展字段 ──
    // float customHeight = 0.0f;           // 0 = 使用默认 m_itemHeight
    // shared_ptr<ComboBoxItemRenderer> renderer;  // 自定义渲染器
};
```

### 4.2 ComboBox 类

```cpp
class ComboBox : public EditBox {
    friend class ComboBoxBuilder;
public:
    using OnSelectionChangedHandler =
        std::function<void(shared_ptr<ComboBox>, int index, const string& value)>;

private:
    // ── 选项数据 ──
    vector<ComboBoxItem> m_items;
    int m_selectedIndex = -1;       // -1 = 无选中
    int m_hoveredIndex = -1;        // Popup 中悬停高亮
    int m_savedSelectedIndex = -1;  // 展开前保存（只读模式取消时恢复）
    string m_placeholder;
    bool m_editable = false;        // 双模式开关（默认只读）
    bool m_cycleEnabled = true;     // 闭合态 ↑/↓/滚轮循环选择

    // ── 视觉属性 ──
    float m_arrowWidth = 20.0f;         // 箭头区域宽度
    float m_itemHeight = 24.0f;         // 列表单项高度
    int   m_maxVisibleItems = 6;        // 最多显示项数
    SColor m_arrowColor{180, 180, 180, 255};
    SColor m_arrowHoverColor{255, 255, 255, 255};
    SColor m_itemSelectedColor{0, 120, 215, 255};     // 选中高亮背景
    SColor m_itemHoverColor{50, 50, 50, 255};         // 悬停高亮背景
    SColor m_itemDisabledColor{80, 80, 80, 255};      // 禁用文本色
    SColor m_listBgColor{37, 37, 38, 255};            // 列表背景
    SColor m_listBorderColor{60, 60, 60, 255};        // 列表边框

    // ── Popup ──
    shared_ptr<Popup> m_popup;
    shared_ptr<ComboBoxListPanel> m_listPanel;
    shared_ptr<ScrollBar> m_scrollBar;

    // ── 回调 ──
    OnSelectionChangedHandler m_onSelectionChanged;

    // ── 状态 ──
    float m_dropdownOffset = 2.0f;   // Popup 与 ComboBox 的间距
    bool m_arrowHovered = false;     // 鼠标是否在箭头区域

private:
    // ── Popup 控制 ──
    SRect computePopupRect() const;     // 计算 Popup 位置（上下翻转）
    void  openPopup();
    void  closePopup();
    bool  isPopupOpen() const;
    void  togglePopup();

    // ── Popup 内容构建 ──
    void rebuildPopupContent();             // 重建列表和滚动条
    void updateScrollBar();                 // 更新滚动条范围
    void syncListFromScroll();              // 滚动偏移 → 列表起始索引

    // ── 选择 ──
    void selectItem(int index);             // 选中 + 更新文本 + 关闭 + 回调
    void restorePreviousSelection();        // 恢复选中（只读模式取消时）

    // ── 文本辅助 ──
    int   findItemByText(const string& text) const;  // 精确→前缀匹配，忽略大小写
    string getTruncatedText(const string& text, float maxWidth) const;

    // ── 事件 ──
    bool isInArrowArea(float x) const;

public:
    ComboBox(Control* parent, SRect rect,
             float xScale = 1.0f, float yScale = 1.0f);
    ~ComboBox();
    void create(void) override;
    void draw(void) override;
    bool handleEvent(shared_ptr<Event> event) override;
    void setRect(SRect rect) override;
    void update(void) override;

    // ── 选项管理 ──
    void setItems(const vector<ComboBoxItem>& items);
    void addItem(const string& label, const string& value, bool disabled = false);
    void clearItems();
    void removeItem(int index);
    const vector<ComboBoxItem>& getItems() const;
    int getItemCount() const;

    // ── 选中 ──
    void setSelectedIndex(int index);
    int  getSelectedIndex() const;
    void setSelectedValue(const string& value);
    string getSelectedValue() const;
    string getSelectedLabel() const;

    // ── 双模式 ──
    void setEditable(bool editable);
    bool isEditable() const;

    // ── 占位文本 ──
    void setPlaceholder(const string& text);
    string getPlaceholder() const;

    // ── 视觉配置 ──
    void setArrowWidth(float width);
    void setItemHeight(float height);
    void setMaxVisibleItems(int count);
    int  getMaxVisibleItems() const;

    // ── 颜色配置 ──
    void setArrowColor(SColor color);
    void setItemSelectedColor(SColor color);
    void setItemHoverColor(SColor color);
    void setListBgColor(SColor color);

    // ── 事件 ──
    void setOnSelectionChanged(OnSelectionChangedHandler handler);

    // ── 测试辅助 ──
    shared_ptr<ComboBoxListPanel> getListPanel();
    shared_ptr<ScrollBar> getListScrollBar();
    void openPopupForTest();
};
```

### 4.3 ComboBoxListPanel 内部类

```cpp
class ComboBoxListPanel : public ControlImpl {
    friend class ComboBox;
private:
    ComboBox* m_owner;                          // 反向引用父 ComboBox
    int m_scrollOffset = 0;                     // 当前滚动到的 item 起始索引
    int m_hoveredIndex = -1;                    // 悬停项索引
    bool m_keyboardMode = false;                // 键盘导航中（↑↓）

    void setOwner(ComboBox* owner) { m_owner = owner; }

    // 计算可见 items 范围
    int getVisibleStart() const { return m_scrollOffset; }
    int getVisibleEnd() const;                  // exclusive
    int getItemY(int index) const;              // 第 index 项的 y 坐标
    int hitTest(float y) const;                 // y → item index

public:
    ComboBoxListPanel(Control* parent, SRect rect,
                      float xScale = 1.0f, float yScale = 1.0f);
    void create(void) override;
    void draw(void) override;
    bool handleEvent(shared_ptr<Event> event) override;

    void setScrollOffset(int offset);
    int getScrollOffset() const { return m_scrollOffset; }
    int getTotalHeight() const;                 // 全部 items 总高度
};
```

### 4.4 ComboBoxBuilder 类

```cpp
class ComboBoxBuilder {
private:
    shared_ptr<ComboBox> m_comboBox;
public:
    ComboBoxBuilder(Control* parent, SRect rect,
                    float xScale = 1.0f, float yScale = 1.0f);

    ComboBoxBuilder& setItems(const vector<ComboBoxItem>& items);
    ComboBoxBuilder& setSelectedIndex(int index);
    ComboBoxBuilder& setEditable(bool editable);
    ComboBoxBuilder& setPlaceholder(const string& text);
    ComboBoxBuilder& setArrowWidth(float width);
    ComboBoxBuilder& setItemHeight(float height);
    ComboBoxBuilder& setMaxVisibleItems(int count);
    ComboBoxBuilder& setOnSelectionChanged(ComboBox::OnSelectionChangedHandler handler);
    ComboBoxBuilder& setBackgroundStateColor(StateColor color);
    ComboBoxBuilder& setBorderStateColor(StateColor color);
    ComboBoxBuilder& setArrowColor(SColor color);
    ComboBoxBuilder& setItemSelectedColor(SColor color);
    ComboBoxBuilder& setItemHoverColor(SColor color);
    ComboBoxBuilder& setListBgColor(SColor color);
    ComboBoxBuilder& setText(const string& text);       // 初始文本（EditBox）
    ComboBoxBuilder& setFont(FontName fontName);
    ComboBoxBuilder& setFontSize(int size);
    ComboBoxBuilder& setAlignmentMode(AlignmentMode mode);

    shared_ptr<ComboBox> build(void);
};
```

---

## 5. 交互逻辑

### 5.1 弹窗生命周期

```
点击箭头 / ↓键（聚焦时）
  → openPopup()
    ├── m_savedSelectedIndex = m_selectedIndex    保存当前选中
    ├── rebuildPopupContent()                      创建/重建列表内容
    │     ├── 计算 popup 尺寸 (width = ComboBox.width)
    │     │   height = min(items.size, maxVisibleItems) * itemHeight
    │     ├── 设置 m_listPanel 尺寸
    │     ├── 设置 m_scrollBar (if needed)
    │     └── updateScrollBar()
    ├── SRect popupRect = computePopupRect()
    │     ├── 尝试下方
    │     ├── 空间不足 → 尝试上方
    │     └── 均不足 → 选空间更大一侧
    ├── 绝对坐标（基于 getDrawRect）转父（bench）相对本地坐标
    │     （减 BENCH->getDrawRect() 偏移，2026-08-08 修订：Popup m_rect 与普通
    │      控件一致为父相对，不再使用窗口绝对坐标）
    ├── m_popup->setAbsolute(popupRect)
    ├── m_popup->open()                             注册 watcher + add to BENCH
    └── 焦点保持在 EditBox（用户可继续输入）

点击列表项
  → ComboBoxListPanel::handleEvent (MouseDown)
    ├── hitTest(y) → index
    ├── if (items[index].disabled) return
    ├── m_owner->selectItem(index)
    │     ├── m_selectedIndex = index
    │     ├── setText(items[index].label)           更新 EditBox 文本
    │     ├── closePopup()
    │     └── m_onSelectionChanged(this, index, value)
    └── return true

Enter 键（展开时）
  → 由 beforeEventHandlingWatcher 拦截 KeyDown/Enter
    ├── if (m_hoveredIndex >= 0 且 未禁用)
    │     └── selectItem(m_hoveredIndex)
    │     └── closePopup(DialogResult::Confirmed)
    └── 高亮无效/禁用 → 仅 closePopup()（不选中）

↑/↓ 键（展开时）
  → 由 beforeEventHandlingWatcher 拦截 KeyDown
    ├── 更新 m_hoveredIndex（跳过禁用项）
    ├── 如果超出可见范围 → scrollToItem()
    └── return true

ESC 键（展开时）
  → beforeEventHandlingWatcher 拦截 → closePopup()

关闭回调（setOnClose，Popup 关闭时统一处理恢复逻辑）
  → Popup::setOnClose(result)
    ├── result == Cancelled（ESC / 外部点击 / 点自身区域收起）
    │     ├── 只读模式 → restorePreviousSelection()
    │     │     ├── m_selectedIndex = m_savedSelectedIndex
    │     │     └── setText(原选中项的 label 或 清空)
    │     └── 可编辑模式 → 不恢复，保留当前输入（含未匹配文本与选中状态）
    └── result == Confirmed（Enter/点击列表项选中）→ 不做恢复
```

> 恢复逻辑集中在 `setOnClose` 回调而非事件分发处，保证 ESC、外部点击、点击自身区域收起三条路径行为一致。

### 5.2 弹窗定位（computePopupRect）

```cpp
//
// 定位策略（优先级从高到低）：
//   1. 下方完全容纳所有可见条目 → 放在下方
//   2. 上方完全容纳所有可见条目 → 放在上方
//   3. 上下均无法完全容纳 → 选空间更大的一侧
//   4. 根据所选侧的实际空间调整 popup 高度
//   5. 调整后高度 < 一个条目高度 → 不显示 popup（返回空 SRect）
//
SRect ComboBox::computePopupRect() const {
    SRect dr = getDrawRect();          // 已缩放的绝对坐标
    float sx = getScaleXX();
    float sy = getScaleYY();

    int visibleCount = min((int)m_items.size(), m_maxVisibleItems);
    float pw = dr.width;                             // 与 ComboBox 等宽
    float fullPh = visibleCount * m_itemHeight * sy; // 完整高度

    SSize ws = MAINWIN->getWindowSize();
    float screenW = (float)ws.width;
    float screenH = (float)ws.height;

    // 默认：下方左对齐
    float x = dr.left;
    float bestY = dr.bottom() + m_dropdownOffset * sy;
    float bestPh = fullPh;
    bool found = false;

    // ── 尝试下方 ──
    if (bestY + fullPh <= screenH) {
        found = true;
    }
    // ── 尝试上方 ──
    else {
        float yAbove = dr.top - fullPh - m_dropdownOffset * sy;
        if (yAbove >= 0) {
            bestY = yAbove;
            found = true;
        }
        // ── 上下均不足 → 选空间更大的一侧 ──
        else {
            float spaceBelow = screenH - dr.bottom() - m_dropdownOffset * sy;
            float spaceAbove = dr.top - m_dropdownOffset * sy;

            if (spaceBelow >= spaceAbove) {
                // 放在下方，调整高度
                bestPh = max(0.0f, spaceBelow);
                bestY  = dr.bottom() + m_dropdownOffset * sy;
            } else {
                // 放在上方，调整高度
                bestPh = max(0.0f, spaceAbove);
                bestY  = dr.top - bestPh - m_dropdownOffset * sy;
            }

            // 调整后的高度 < 一个条目 → 拒显
            float oneItemH = m_itemHeight * sy;
            if (bestPh < oneItemH) {
                return SRect();  // 返回空 SRect 表示不显示
            }

            // 调整可见条目数量（确保整数个条目）
            int adjustedCount = (int)(bestPh / oneItemH);
            bestPh = adjustedCount * oneItemH;
            found = (adjustedCount > 0);
        }
    }

    if (!found) return SRect();

    // 水平 clamp
    if (x + pw > screenW) x = screenW - pw;
    if (x < 0) x = 0;

    return SRect(x, bestY, pw, bestPh);
}
```

### 5.3 文本与选中的关系

```
闭合状态:
  ┌──────────────────────┬─┐
  │ 用户输入/选中文本     │▼│
  └──────────────────────┴─┘

文本与 m_selectedIndex 的关系:
  - 用户从列表选中 → setText(label) + m_selectedIndex = index
  - 用户手动输入（可编辑模式）→ type-ahead：
    findItemByText(text) 先精确匹配（_stricmp，忽略大小写），
    再前缀匹配（_strnicmp），命中则 m_hoveredIndex 高亮该项（不自动选中）
  - 回车确认 → 命中项 selectItem(index)；无匹配 → m_selectedIndex = -1，输入内容保留
  - setSelectedIndex(n) → setText(items[n].label) + m_selectedIndex = n
```

---

## 6. 绘制规范

### 6.1 ComboBox::draw() 中的箭头绘制

箭头使用 `RenderDevice::drawTriangle` 绘制**填充三角形**，不依赖任何字体或位图。一个三角形调用即可完成，GPU 原生基元，性能最优。

```
闭合 ▼ (等腰三角形, 顶点在下, 似「▼」):
███████████
 █       █
  █     █
   █   █
    ███

展开 ▲ (等腰三角形, 顶点在上, 似「▲」):
    ███
   █   █
  █     █
 █       █
███████████
```

```cpp
void ComboBox::draw() {
    // 1. 先绘制 EditBox 背景 + 文本 + 选区 + 光标
    //    此时文本区域通过 m_margin.right 预留箭头空间
    EditBox::draw();

    // 2. 使用 drawTriangle 绘制填充箭头
    SRect dr = getDrawRect();
    float sx = getScaleXX();
    float arrowRight  = dr.right();
    float arrowLeft   = arrowRight - m_arrowWidth * sx;
    float arrowCenterX = (arrowLeft + arrowRight) / 2.0f;
    float arrowCenterY = dr.top + dr.height / 2.0f;

    // 箭头尺寸 = min(箭头区域宽, 控件高) * 比例，确保缩放后视觉协调
    float arrowMax = min(m_arrowWidth * sx, dr.height);
    float halfW = arrowMax * COMBOBOX_ARROW_WIDTH_RATIO;  // 半宽比例
    float halfH = arrowMax * COMBOBOX_ARROW_HEIGHT_RATIO; // 半高比例

    SColor arrowColor = m_arrowHovered ? m_arrowHoverColor : m_arrowColor;
    auto* device = GET_RENDERDEVICE;
    device->setDrawColor(arrowColor);

    if (isPopupOpen()) {
        // ▲：顶点朝上
        // v0=左下, v1=右下, v2=顶
        device->drawTriangle(
            { {arrowCenterX - halfW, arrowCenterY + halfH}, arrowColor },
            { {arrowCenterX + halfW, arrowCenterY + halfH}, arrowColor },
            { {arrowCenterX,         arrowCenterY - halfH}, arrowColor }
        );
    } else {
        // ▼：顶点朝下
        // v0=左上, v1=右上, v2=底
        device->drawTriangle(
            { {arrowCenterX - halfW, arrowCenterY - halfH}, arrowColor },
            { {arrowCenterX + halfW, arrowCenterY - halfH}, arrowColor },
            { {arrowCenterX,         arrowCenterY + halfH}, arrowColor }
        );
    }
}
```

> **缩放说明**：`dr` 已为缩放后的绝对坐标，`m_arrowWidth` 为未缩放值（乘 `sx` 后参与计算），`dr.height` 本身已缩放。三个顶点坐标均在此缩放空间中，因此 `drawTriangle` 绘制的三角形在各缩放倍率下视觉比例一致。`COMBOBOX_ARROW_WIDTH_RATIO` 和 `COMBOBOX_ARROW_HEIGHT_RATIO` 定义在 `ConstDef.h` 中。

### 6.2 ComboBoxListPanel::draw()

```cpp
void ComboBoxListPanel::draw() {
    if (!m_visible || !m_owner) return;

    ControlImpl::beforeDraw();
    SRect dr = getDrawRect();
    auto* device = GET_RENDERDEVICE;

    // 1. 绘制列表背景
    device->setDrawColor(m_owner->m_listBgColor);
    device->fillRect(dr);

    // 2. 对每个可见 item 进行绘制
    int start = getVisibleStart();
    int end = min(getVisibleEnd(), (int)m_owner->m_items.size());
    auto& items = m_owner->m_items;

    getTextRenderer();   // assume cached
    float sx = getScaleXX();
    float sy = getScaleYY();

    for (int i = start; i < end; ++i) {
        float itemY = (i - start) * m_owner->m_itemHeight * sy;
        SRect itemRect(dr.left, dr.top + itemY, dr.width, m_owner->m_itemHeight * sy);

        // 选中高亮背景（优先级高于悬停）
        if (i == m_owner->m_selectedIndex) {
            device->setDrawColor(m_owner->m_itemSelectedColor);
            device->fillRect(itemRect);
        }
        // 悬停高亮背景
        else if (i == m_hoveredIndex) {
            device->setDrawColor(m_owner->m_itemHoverColor);
            device->fillRect(itemRect);
        }

        // 绘制文本
        SColor textColor = items[i].disabled
            ? m_owner->m_itemDisabledColor
            : m_owner->m_textColor.getNormal();

        // 截断长文本
        float textMaxWidth = dr.width - 8 * sx;
        string displayText = getTruncatedText(items[i].label, textMaxWidth);

        getTextRenderer()->drawText(m_owner->getFont(), displayText,
            dr.left + 4 * sx, dr.top + itemY + 2 * sy, textColor);
    }

    // 3. 绘制边框
    device->setDrawColor(m_owner->m_listBorderColor);
    device->drawRect(dr);

    ControlImpl::afterDraw();
}
```

### 6.3 弹出列表与 ComboBox 的视觉关联

- 列表宽度 = ComboBox 宽度（含箭头区域），边界严格对齐
- 列表边框颜色与 ComboBox 边框颜色协调
- 选中项高亮颜色采用系统主题蓝色（类似 Windows 默认）
- 悬停项高亮颜色比背景亮一级（深色主题下用 `#323232`）

---

## 7. 滚动机制

### 7.1 触发条件

`m_items.size() > m_maxVisibleItems` 时，Popup 中显示右侧 ScrollBar。

### 7.2 同步逻辑

```cpp
// ScrollBar 的 value 变化时：
void ComboBox::syncListFromScroll() {
    int newOffset = (int)(m_scrollBar->getValue());
    m_listPanel->setScrollOffset(newOffset);
}

// ScrollBar 范围设置：
void ComboBox::updateScrollBar() {
    if (!m_scrollBar) return;
    int totalItems = (int)m_items.size();
    if (totalItems <= m_maxVisibleItems) {
        m_scrollBar->setVisible(false);
        return;
    }
    m_scrollBar->setVisible(true);
    // 保存预期 offset —— setRange/setPageSize 的 notifyPositionChanged
    // 回调会触发 syncListFromScroll 用 ScrollBar 当前值(0)重置 scrollOffset
    int intendedOffset = m_listPanel->getScrollOffset();
    m_scrollBar->setRange(0, totalItems - m_maxVisibleItems);
    m_scrollBar->setPageSize(m_maxVisibleItems);
    m_scrollBar->setStepSize(1.0f);
    m_scrollBar->setValue((float)intendedOffset);
}
```

### 7.3 MouseWheel 路由

MouseWheel 事件经过 3 层路由才能到达列表滚动处理：

```
BENCH → Popup::handleEvent
          ├── isContainsPoint(mx, my)?  ← Popup 区域检查
          │    是 → 转发给子控件 (ScrollBar, ListPanel)
          │    否 → return false（不落到 Panel::handleEvent）
          ├── ScrollBar::handleEvent → 不处理 MouseWheel → false
          └── ComboBoxListPanel::handleEvent
                ├── 无位置检查（Popup 已保证正确性）
                ├── m_scrollOffset += delta
                ├── setScrollOffset → clamp [0, maxOffset]
                └── updateScrollBar → 同步 ScrollBar 位置
```

关键实现细节：
- `Popup::handleEvent` 加 `isContainsPoint` 检查，不在区域内直接 `return false` 阻止落到 `Panel::handleEvent`（否则子控件被重新遍历时不带区域检查，导致鼠标在 Popup 外也能滚动）
- `ComboBoxListPanel::handleEvent` 不检查 `getDrawRect().contains()`（Popup 已保证区域正确性，且 ScrollBar 区域也应可滚动）

闭合态（聚焦、Popup 关闭时）MouseWheel 只响应鼠标在 ComboBox 范围内的滚动：

```cpp
if (event->m_type == EventType::MouseWheel) {
    // 只有鼠标在 ComboBox 范围内才响应
    if (isContainsPoint(event->mouseWheel.x, event->mouseWheel.y)) {
        int delta = (event->mouseWheel.scrollY > 0) ? -1 : 1;
        cycleSelection(delta);
        return true;
    }
}
```

### 7.4 ScrollBar 显示时列表区域的适配

ScrollBar 显示时列表面板的可绘制区域会缩小。有三种方案：

| 方案 | 描述 | 优点 | 缺点 |
|------|------|------|------|
| **A. 缩减文本宽度** | ScrollBar 与列表项**重叠**，文本绘制时使用 clip rect 缩减宽度，不影响条目背景/选中高亮的绘制区域 | 条目背景视觉完整，文本自动避开滚动条 | 需要额外 clip rect |
| **B. 缩减列表面板宽度** | 列表面板右边界左移 SCROLLBAR_WIDTH，ScrollBar 放在右侧空白区域 | 视觉隔离清晰 | 需要调整 m_listPanel 尺寸，布局略复杂 |
| **C. 半透明覆盖** | ScrollBar 半透明覆盖在列表项之上，列表项绘制完全不变 | 实现最简单 | 滚动条覆盖部分文本，视觉冲突 |

**选定方案 A**（缩减文本宽度）：

```
无 ScrollBar 时:
┌──────────────────────┐
│ item text            │
│ item text            │
└──────────────────────┘

有 ScrollBar 时:
┌──────────────────────┬─┐
│ item text        ░░░│░│   ← 背景/高亮铺满全宽
│ item text        ░░░│░│   ← 文本 clip rect = (left, top, width - sbWidth, height)
└──────────────────────┴─┘
```

实现方式：
- `ComboBoxListPanel::draw()` 中，不修改 `itemRect`（保证选中/悬停背景铺满整行）
- 文本绘制前设置 `setClipRect()` 缩减右侧 `SCROLLBAR_WIDTH`，确保文本不穿入滚动条区域
- ScrollBar 作为独立的子控件（或同位控件）放置在 Popup 右侧边缘，z-order 在列表之上
- ScrollBar 的宽度使用 `ConstDef::SCROLLBAR_WIDTH`

```cpp
// ComboBoxListPanel::draw() 中文本绘制部分
void ComboBoxListPanel::draw() {
    // ... 绘制背景和每项的高亮背景（不缩窄）...

    // 为文本绘制设置 clip rect（避开右侧滚动条）
    SRect dr = getDrawRect();
    float textMaxWidth = dr.width - 8 * sx;
    if (m_owner->m_scrollBar && m_owner->m_scrollBar->getVisible()) {
        float sbWidth = ConstDef::SCROLLBAR_WIDTH * sx;
        SRect textClip(dr.left, dr.top,
                       dr.width - sbWidth - 4 * sx, dr.height);
        GET_RENDERDEVICE->setClipRect(textClip);
        textMaxWidth = textClip.width;
    }

    // ... 绘制文本 ...
}
```

## 8. 键盘导航

### 8.1 键位映射

| 键位 | 展开时 | 闭合时（聚焦） |
|------|--------|---------------|
| **↓** | 高亮下移一项（跳过禁用项） | **直接循环选中下一项**（不展开） |
| **↑** | 高亮上移一项（跳过禁用项） | **直接循环选中上一项**（不展开） |
| **Enter** | 选中高亮项 → 更新文本 → 关闭(Confirmed) | 可编辑模式：匹配/前缀项选中，无匹配置 -1；只读模式无效果 |
| **Esc** | 关闭（只读恢复原值 / 可编辑保留输入） | — |
| **PageUp** | 上翻一页（maxVisibleItems 项） | — |
| **PageDown** | 下翻一页 | — |
| **Home** | 高亮跳转到第一项 | 文本光标到行首 |
| **End** | 高亮跳转到最后一项 | 文本光标到行末 |

### 8.2 展开态 beforeEventHandlingWatcher（键盘导航 + 点击收起）

展开状态下的 KeyDown/MouseDown 由 `beforeEventHandlingWatcher` 拦截（**在 `create()` 末尾注册** KeyDown + MouseDown 两个 watcher，早于任何 `popup->open()` 时注册的 clickOutside watcher）。此注册时机是点击自身区域收起的关键：watcher 按注册顺序执行，ComboBox 的先处理保证点击自身区域时 popup 先被收起并消费事件，不会落到 Popup 的 clickOutside 关闭后再由 handleEvent 重新打开：

```cpp
bool ComboBox::beforeEventHandlingWatcher(shared_ptr<Event> event)
{
    if (!isPopupOpen()) return false;

    // 展开态点击控件自身区域 → 直接收起并消费事件：
    // 编辑模式仅箭头区（点正文用于聚焦编辑），只读模式任意区域
    if (event->m_type == EventType::MouseDown &&
        event->mouseButton.button == MouseButton::Left &&
        isContainsPoint(event->mouseButton.x, event->mouseButton.y) &&
        (!m_editable || isInArrowArea(event->mouseButton.x))) {
        closePopup();
        return true;
    }

    if (event->m_type == EventType::KeyDown) {
        switch (event->keyEvent.keycode) {
        case KeyCode::Down:
            // 下移并跳过禁用项
            return true;
        case KeyCode::Up:
            // 上移并跳过禁用项
            return true;
        case KeyCode::Return:
        case KeyCode::KPEnter:
            // 高亮有效且未禁用 → selectItem + closePopup(Confirmed)；
            // 否则仅 closePopup()
            return true;
        case KeyCode::Escape:
            closePopup();   // 恢复逻辑集中在 setOnClose 回调
            return true;
        case KeyCode::PageUp:
        case KeyCode::PageDown:
        case KeyCode::Home:
        case KeyCode::End:
            // 移动高亮 + scrollToItem()
            return true;
        default:
            return false;  // 其他按键传递给 EditBox
        }
    }
    return false;
}
```

---

## 9. 与 EditBox 的继承关系细节

### 9.1 构造时的调整

```cpp
ComboBox::ComboBox(Control *parent, SRect rect, float xScale, float yScale)
    : EditBox(parent, rect, xScale, yScale)
{
    // ComboBox 特定的初始值
    setPasswordMode(false);                // 禁用密码模式
    m_focusable = true;

    // 调整右边距为箭头腾出空间
    m_margin.right += m_arrowWidth;

    // 默认字体大小
    m_fontSize = 14;
}
```

### 9.2 箭头区域判定

```cpp
bool ComboBox::isInArrowArea(float x) const {
    SRect dr = getDrawRect();
    float arrowStartX = dr.right() - m_arrowWidth * getScaleXX();
    return x >= arrowStartX && x <= dr.right();
}
```

### 9.3 事件分流

```cpp
bool ComboBox::handleEvent(shared_ptr<Event> event) {
    if (!m_enable || !m_visible) return false;

    // ═══ 只读模式：拦截文本输入 ═══
    // EditBox 的 TextInput 处理只检查 m_focused 不检查可编辑性，
    // 必须在最前拦截，防止文字写入编辑框
    if (!m_editable && event->m_type == EventType::TextInput) {
        return true;
    }

    // ═══ 箭头/正文区域点击 ═══
    if (event->m_type == EventType::MouseDown &&
        event->mouseButton.button == MouseButton::Left) {
        // 只读：点击任意区域开/收下拉；可编辑：仅箭头区 togglePopup
        if (isContainsPoint(event->mouseButton.x, event->mouseButton.y) &&
            (!m_editable || isInArrowArea(event->mouseButton.x))) {
            togglePopup();
            return true;
        }
        // 可编辑模式点正文：不拦截，落到 EditBox 聚焦编辑
    }

    if (event->m_type == EventType::MouseMove) {
        bool inArrow = isInArrowArea(event->mousePos.x);
        if (inArrow != m_arrowHovered) {
            m_arrowHovered = inArrow;
        }
    }

    // ═══ 闭合态（聚焦）: ↑/↓ 键循环切换选中项 + 回车匹配确认 ═══
    if (!isPopupOpen() && getFocused()) {
        if (event->m_type == EventType::KeyDown) {
            int delta = 0;
            if (event->keyEvent.keycode == KeyCode::Down) delta = 1;
            else if (event->keyEvent.keycode == KeyCode::Up) delta = -1;

            if (delta != 0 && !m_items.empty()) {
                cycleSelection(delta);   // 循环切换，跳过 disabled
                return true;
            }

            // 可编辑模式：回车 → type-ahead 匹配确认
            if (m_editable &&
                (event->keyEvent.keycode == KeyCode::Return ||
                 event->keyEvent.keycode == KeyCode::KPEnter)) {
                int idx = findItemByText(m_text);
                if (idx >= 0) {
                    selectItem(idx);      // 匹配 → 选中
                } else {
                    m_selectedIndex = -1; // 无匹配 → 保留输入，视为无选中
                }
                return true;
            }
        }

        // 可编辑模式：文字输入 → 写入 + type-ahead 高亮
        if (m_editable && event->m_type == EventType::TextInput) {
            EditBox::handleEvent(event);
            if (!m_items.empty()) {
                int idx = findItemByText(m_text);
                if (idx >= 0) m_hoveredIndex = idx;
            }
            return true;
        }

        // MouseWheel 也切换选中项（须在控件范围内）
        if (event->m_type == EventType::MouseWheel &&
            isContainsPoint(event->mouseWheel.x, event->mouseWheel.y)) {
            int delta = (event->mouseWheel.deltaY > 0) ? -1 : 1;
            cycleSelection(delta);
            return true;
        }
    }

    // ═══ 其余事件委托给 EditBox ═══
    return EditBox::handleEvent(event);
}

// 输入文字匹配：先精确匹配（忽略大小写），再前缀匹配；跳过禁用项
int ComboBox::findItemByText(const string& text) const {
    if (text.empty()) return -1;
    for (size_t i = 0; i < m_items.size(); ++i)
        if (!m_items[i].disabled &&
            _stricmp(m_items[i].label.c_str(), text.c_str()) == 0)
            return (int)i;   // 精确匹配
    for (size_t i = 0; i < m_items.size(); ++i)
        if (!m_items[i].disabled &&
            _strnicmp(m_items[i].label.c_str(), text.c_str(), text.length()) == 0)
            return (int)i;   // 前缀匹配
    return -1;
}
```

---

## 10. JSON 布局格式

```json
{
  "type": "combo-box",
  "id": "orientationPicker",
  "rect": { "x": 10, "y": 10, "w": 150, "h": 24 },
  "scale": { "x": 1.0, "y": 1.0 },
  "selectedIndex": 0,
  "editable": false,
  "maxVisibleItems": 8,
  "itemHeight": 24,
  "arrowWidth": 20,
  "placeholder": "请选择",
  "items": [
    { "label": "Horizontal", "value": "horizontal" },
    { "label": "Vertical",   "value": "vertical" },
    { "label": "Disabled Item", "value": "disabled", "disabled": true }
  ],
  "colors": {
    "background": { "normal": "#FFFFFF", "hover": "#F0F0F0", "pressed": "#E0E0E0" },
    "border": { "normal": "#CCCCCC" },
    "text": { "normal": "#333333" }
  },
  "events": {
    "onSelectionChanged": "onOrientChanged"
  }
}
```

### LayoutParser 解析流程

```cpp
// parseControl() 新增分支
else if (type == "ComboBox") {
    result = parseComboBox(j, parent);
}

// parseComboBox()
shared_ptr<ComboBox> LayoutParser::parseComboBox(
    const json& j, Control* parent)
{
    SRect rect = parseRect(j["rect"]);
    auto cb = make_shared<ComboBox>(parent, rect);

    m_theme.applyCommonColors(cb, "combobox");
    parseCommonProperties(cb, j);

    // ComboBox 特有属性
    if (j.contains("maxVisibleItems"))
        cb->setMaxVisibleItems(j["maxVisibleItems"].get<int>());
    if (j.contains("itemHeight"))
        cb->setItemHeight(j["itemHeight"].get<float>());
    if (j.contains("arrowWidth"))
        cb->setArrowWidth(j["arrowWidth"].get<float>());
    if (j.contains("placeholder"))
        cb->setPlaceholder(j["placeholder"].get<string>());
    if (j.contains("selectedIndex"))
        cb->setSelectedIndex(j["selectedIndex"].get<int>());
    if (j.contains("editable"))
        cb->setEditable(j["editable"].get<bool>());

    // 解析 items
    if (j.contains("items") && j["items"].is_array()) {
        vector<ComboBoxItem> items;
        for (auto& item : j["items"]) {
            ComboBoxItem ci;
            ci.label    = item.value("label", "");
            ci.value    = item.value("value", "");
            ci.disabled = item.value("disabled", false);
            items.push_back(ci);
        }
        cb->setItems(items);
    }

    // 事件绑定
    parseEvents(cb, j);

    // 注册 ID
    if (j.contains("id") && j["id"].is_string())
        m_controlsById[j["id"].get<string>()] = cb;

    cb->create();
    return cb;
}

// parseEvents() 新增分支
if (auto cb = dynamic_pointer_cast<ComboBox>(ctrl)) {
    if (events.contains("onSelectionChanged") && events["onSelectionChanged"].is_string()) {
        string handlerName = events["onSelectionChanged"].get<string>();
        auto it = m_handlers.find(handlerName);
        if (it != m_handlers.end()) {
            auto handler = it->second;
            cb->setOnSelectionChanged([handler](shared_ptr<ComboBox> sender, int, const string&) {
                handler(sender);
            });
        }
    }
}
```

---

## 11. C ABI

> **修订注记（2026-08-05）**：本小节开头为早期设计快照，**仅作历史参考**。`UICornerstone_CreateComboBox`（已带 `UIInstance` 首参）与 11.1 通用属性系统 API 有效；`ComboBoxSetItems`/`ClearItems`/`SetSelectedIndex`/`GetSelectedIndex`/`SetMaxVisibleItems`/`SetItemHeight`/`SetOnSelectionChanged` 等专用导出已由属性系统 API 替代（`SetString(inst, ctl, "items", json)`、`SetInt(inst, ctl, "selected-index"/"max-visible-items", v)`、`SetFloat(inst, ctl, "item-height", v)`、`SetCallback(inst, ctl, "selection-changed", cb, user)`）。**当前 API 以 `include/UICornerstoneAPI.h` 为准**；现成可运行示例见 `test/test_combobox_cabi.cpp`。

```cpp
// 工厂函数
UIControlHandle UICornerstone_CreateComboBox(
    UIControlHandle parent, float x, float y, float w, float h);

// 选项管理
void UICornerstone_ComboBoxSetItems(
    UIControlHandle handle, const char** labels, const char** values, int count);
void UICornerstone_ComboBoxClearItems(UIControlHandle handle);

// 选中
void UICornerstone_ComboBoxSetSelectedIndex(
    UIControlHandle handle, int index);
int  UICornerstone_ComboBoxGetSelectedIndex(UIControlHandle handle);

// 配置
void UICornerstone_ComboBoxSetMaxVisibleItems(
    UIControlHandle handle, int count);
void UICornerstone_ComboBoxSetItemHeight(
    UIControlHandle handle, float height);

// 事件
void UICornerstone_ComboBoxSetOnSelectionChanged(
    UIControlHandle handle, void (*callback)(UIControlHandle, int, const char*));
```

### 11.1 属性系统访问（通用 C ABI）

除上述专用函数外，ComboBox 通过通用属性 CAPI 暴露双模式与文本/选中状态：

| 属性 | 类型 | 语义 |
|------|------|------|
| `editable` | bool | 双模式开关（可 get/set） |
| `text` | string | 编辑框当前内容：输入中的文字 / 无匹配时保留的输入 / 选中后的项 label（可 get/set，set 经 `setText`） |
| `selected-value` | string | 选中项的 `value`；`selectedIndex == -1`（未匹配）时返回编辑框保留的输入内容 |
| `selected-index` | int | 选中项索引；`-1` 表示无匹配（用户输入了未匹配文本） |
| `max-visible-items` / `item-height` / `arrow-width` | int/float | 视觉配置 |
| `cycle-enabled` | bool | 闭合态 ↑/↓/滚轮循环选择开关 |
| `items` | string(JSON) | 以 JSON 数组设置 items（label/value/disabled） |
| `on-selection-changed` | callback | 选中变化回调 |

> **selected-value 语义**：严格返回**项的 value 字段**（而非 label/文本），供属性编辑器回写；未匹配时返回输入内容，避免编辑器读取到空值。判定是否匹配统一用 `selected-index == -1`。

---

## 12. 常量定义

```cpp
// include/ConstDef.h 新增
static const int     COMBOBOX_DEFAULT_MAX_VISIBLE_ITEMS;
static const float   COMBOBOX_DEFAULT_ITEM_HEIGHT;
static const float   COMBOBOX_DEFAULT_ARROW_WIDTH;
static const SColor  COMBOBOX_DEFAULT_ARROW_COLOR;
static const SColor  COMBOBOX_DEFAULT_ARROW_HOVER_COLOR;
static const SColor  COMBOBOX_DEFAULT_ITEM_SELECTED_COLOR;
static const SColor  COMBOBOX_DEFAULT_ITEM_HOVER_COLOR;
static const SColor  COMBOBOX_DEFAULT_ITEM_DISABLED_COLOR;
static const SColor  COMBOBOX_DEFAULT_LIST_BG_COLOR;
static const SColor  COMBOBOX_DEFAULT_LIST_BORDER_COLOR;
static const float   COMBOBOX_DROPDOWN_OFFSET;
```

---

## 13. 边界与约束

### 13.1 空数据处理

- `m_items` 为空时，点击箭头不展开 Popup（无可用选项）
- `m_selectedIndex == -1` 时，EditBox 显示 `m_placeholder`（灰色文本）
- `m_items` 非空但 `m_selectedIndex == -1` 时，EditBox 显示 `m_placeholder`，列表无默认高亮

### 13.2 禁用项

- 禁用项灰色文本（`m_itemDisabledColor`）
- 悬停时禁用项不高亮
- 点击禁用项无响应（不选中、不关闭）
- 键盘导航跳过禁用项（`findFirstEnabled()`/`findLastEnabled()`）

### 13.3 长文本截断

- 闭合状态：EditBox 自带文本截断（通过 clip rect 和 m_margin 控制可见区域）
- 展开状态：`ComboBoxListPanel::draw()` 中用 `getTruncatedText()` 截断，方法同 `Label::truncateLine()`（二分查找 + "..."）

### 13.4 缩放支持

- Popup 尺寸和位置全部使用缩放后的坐标
- `m_itemHeight` 为未缩放值，实际绘制时乘以 `getScaleYY()`
- 箭头区域宽度乘以 `getScaleXX()`
- 文本大小使用 `m_fontSize * getScaleXX()`（继承 EditBox 行为）

### 13.5 焦点行为

- ComboBox 获得焦点时，EditBox 显示光标（可编辑模式下可输入）
- Popup 展开时焦点保持在 ComboBox/EditBox（可编辑模式可继续输入，type-ahead 更新高亮）
- Popup 的 `FocusBoundary` 确保 Tab 不会陷入列表项
- 外部点击关闭 Popup 时焦点回到 ComboBox

### 13.6 Phase 2 扩展预留

| 扩展 | 设计预留 |
|------|---------|
| Filter（输入过滤） | EditBox 文本变化 → `onTextChanged` → 过滤 `m_items` → 重建 Popup |
| ComboBoxItem::customHeight | `ComboBoxListPanel::getItemY()` / `hitTest()` 改为累加高度 |
| ComboBoxItemRenderer | items 保存 `shared_ptr<Renderer>`，`draw()` 调用 `renderer->draw()` |

> **DropDownList 只选模式已通过 `m_editable=false` 实现**（见 §3.3 双模式），不再作为 Phase 2 预留。

---

## 14. 实现规范

### 14.1 常量定义（禁止魔鬼数字）

所有数值常量必须定义在 `include/ConstDef.h` 中，并添加 `COMBOBOX_` 前缀：

```cpp
// ── ComboBox 相关常量 ──
static const int     COMBOBOX_DEFAULT_MAX_VISIBLE_ITEMS = 6;
static const float   COMBOBOX_DEFAULT_ITEM_HEIGHT       = 24.0f;
static const float   COMBOBOX_DEFAULT_ARROW_WIDTH       = 20.0f;
static const float   COMBOBOX_DROPDOWN_OFFSET           = 2.0f;
static const float   COMBOBOX_ARROW_MARGIN              = 4.0f;   // 箭头右侧与控件右缘的间距
static const float   COMBOBOX_LIST_PADDING              = 4.0f;   // 列表文本左侧间距
static const float   COMBOBOX_ARROW_WIDTH_RATIO          = 0.35f;  // 箭头半宽 = max(arrowWidth, height) * 此比例
static const float   COMBOBOX_ARROW_HEIGHT_RATIO         = 0.45f;  // 箭头半高 = max(arrowWidth, height) * 此比例
static const int     COMBOBOX_DEFAULT_FONT_SIZE          = 14;
static const SColor  COMBOBOX_DEFAULT_ARROW_COLOR         {180, 180, 180, 255};
static const SColor  COMBOBOX_DEFAULT_ARROW_HOVER_COLOR   {255, 255, 255, 255};
static const SColor  COMBOBOX_DEFAULT_ITEM_SELECTED_COLOR {0, 120, 215, 255};
static const SColor  COMBOBOX_DEFAULT_ITEM_HOVER_COLOR    {50, 50, 50, 255};
static const SColor  COMBOBOX_DEFAULT_ITEM_DISABLED_COLOR {80, 80, 80, 255};
static const SColor  COMBOBOX_DEFAULT_LIST_BG_COLOR       {37, 37, 38, 255};
static const SColor  COMBOBOX_DEFAULT_LIST_BORDER_COLOR   {60, 60, 60, 255};
```

实现代码中不得出现 `6`、`24`、`20`、`4` 等字面量用于「列表可见数」「条目高度」「箭头宽度」「间距」等含义，必须引用上述常量。

### 14.2 实现计划

| 阶段 | 内容 | 文件 | 预估行数 |
|------|------|------|---------|
| 1 | 基础结构：ComboBox 类骨架 + Builder + create()/draw()/handleEvent() | `ComboBox.h`, `ComboBox.cpp` | ~250 |
| 2 | Popup 集成 + 列表渲染 + 事件 | `ComboBox.cpp` (ComboBoxListPanel) | ~250 |
| 3 | 键盘导航 + 滚动 | `ComboBox.cpp` | ~100 |
| 4 | ConstDef 常量 | `ConstDef.h`, `ConstDef.cpp` | ~30 |
| 5 | LayoutParser 解析 | `LayoutParser.cpp` | ~50 |
| 6 | C ABI | `UICornerstoneAPI.h`, `UICornerstoneAPI.cpp` | ~60 |
| 7 | 测试 | `test/test_combobox.cpp` + `test/test_combobox_cabi.cpp` | ~250 |

**总计预估：~990 行**

---

## 15. 测试内容

### 15.1 标准测试（test_combobox）

`test_combobox.cpp` — 使用 Builder/JSON 布局创建多个 ComboBox，覆盖各种配置和交互场景。

| 测试项 | 方法 | 通过标准 |
|--------|------|---------|
| 闭合渲染 | 设 items + selectedIndex | 显示选中文本 + ▼ 箭头 |
| 展开渲染 | 点击箭头 | 弹出列表，选中项高亮 |
| 选择项 | 点击列表项 | 文本更新，popup 关闭，回调触发 |
| 手动输入 | 键盘输入文本 | 文本显示在 EditBox 中 |
| 键盘导航（展开态） | ↑↓ Enter Esc | 高亮移动、选中、关闭正常 |
| 闭合态循环选择 | 聚焦时按 ↑/↓ | 选中项逐一切换，不展开 Popup，回调触发 |
| 闭合态滚轮选择 | 聚焦时滚轮滚动 | 等同 ↑/↓，选中项变化 |
| 回卷循环 | 从头/尾按 ↑/↓ | 到底后回卷到首/尾 |
| 箭头点击 | 点击箭头区域 | togglePopup 行为正常 |
| 外部点击关闭 | 展开后点击外部 | Popup 关闭，只读恢复原值/可编辑保留输入 |
| 空数据 | 空 items | 不崩溃，箭头点击不弹列表 |
| 长文本 | 超长 label | 列表项显示 ... |
| 禁用项 | disabled=true | 灰色不可选，↑/↓/鼠标点击跳过 |
| 全部禁用 | 所有 items disabled | 循环选择不改变选中项 |
| 滚动 | items > maxVisibleItems | 显示滚动条，滚动正常 |
| 上方弹出 | ComboBox 靠近屏幕底部 | Popup 向上弹出 |
| 空间不足拒显 | 屏幕边缘无足够空间 | 不弹出 Popup |
| 缩放 | scale=2 | 所有元素双倍大小，定位正确 |
| 事件 | onSelectionChanged | 选中变化时触发回调 |
| JSON 解析 | 全参数/缺省 | 解析正确 |
| 双模式 | editable=true/false 对比 | 可编辑可输入+type-ahead；只读拦截输入 |
| type-ahead 匹配 | 可编辑输入 "be" + Enter | 精确/前缀匹配选中；无匹配保留输入，index=-1 |
| 只读输入拦截 | 只读模式展开后输入字符 | 文字不写入编辑框 |
| 可编辑取消保留 | 可编辑输入未匹配文本后 ESC | 输入内容不被清除 |
| 展开态点击自身区域 | 展开后点箭头/任意区域 | 收起且不重新打开 |
| 属性读取 | 经属性 CAPI 读 text/selected-value/index | 未匹配时 selected-index=-1，text 保留输入 |

### 15.2 `test_combobox_cabi` 独立测试

新增 `test_combobox_cabi` 单源文件测试（三后端共享，通过编译定义区分），内嵌 JSON 布局包含 ComboBox 控件，验证 `UICornerstone_CreateComboBox` / `ComboBoxSetItems` / `ComboBoxSetOnSelectionChanged` 基本可用。

| 测试项 | 方法 | 通过标准 |
|--------|------|---------|
| C ABI 创建 | `UICornerstone_CreateComboBox` | 控件可见，箭头/文本正常 |
| C ABI 设 items | `UICornerstone_ComboBoxSetItems` | 下拉列表显示正确选项 |
| C ABI 事件 | `UICornerstone_ComboBoxSetOnSelectionChanged` | 选中变化时回调触发 |

### 15.3 纯 DLL 集成测试（test_combobox_cabi）

> 命名与 `test_dialog_cabi` 一致，后缀 `_cabi` 表示「C ABI + LoadLibrary」模式。

`test_combobox_cabi.cpp`（新增）— 使用 `LoadLibrary("UICornerstone.dll")` + `GetProcAddress` 解析全部 C ABI 函数，**不链接导入库**，与 `test_dialog_cabi` 模式相同。

| 测试项 | 方法 | 通过标准 |
|--------|------|---------|
| DLL 加载 | `LoadLibrary` | 返回有效 HMODULE |
| 符号解析 | `GetProcAddress` 解析全部 ComboBox C ABI | 无 NULL |
| 工厂+设 items | C ABI 链式调用 | 控件正常渲染 |
| 选择+回调 | 点击选项触发回调 | onSelectionChanged 输出日志 |
| 完整帧循环 | init→processEvents→update→render→shutdown | 运行 3 秒无崩溃 |
| 双模式注入验证 | `--sim-inject` 进程内事件注入 | 见 15.4 |

### 15.4 `--sim-inject` 进程内事件注入（双模式端到端验证）

> 鼠标/键盘模拟工具不可靠：`keybd_event`/`SendKeys` 依赖窗口焦点（无前台窗口时 `GetForegroundWindow()` 返回 0，事件静默丢失）。`--sim-inject` 通过 `UICornerstone_PushUIEvent` → queuedEvents → `uiEventToEvent` → dispatchToBench 走**真实事件链路**，不依赖窗口焦点，可稳定驱动 15 阶段注入序列。

| 阶段 | 注入内容 | 验证点 |
|------|---------|--------|
| 0 | 点击 comboEditable 正文 | 可编辑框获得焦点 |
| 1-2 | 输入 "b"、"e" | type-ahead 高亮匹配项 |
| 3 | Enter | 精确匹配 → `Selected: #0 = Beijing` |
| 4 | 点击 comboMain（只读） | 展开 Popup |
| 5 | 输入 "x" | 只读拦截，文字不写入 |
| 6 | Escape | 只读恢复原值，Popup 关闭 |
| 7-9 | 清空文本 + 输入 "zzz" + Enter | 无匹配 → text='zzz'，selected-index=-1 |
| 10 | 点击 Dump 按钮 | 属性 CAPI 读回 text/selected-value/index 并打印 |
| 11-12 | 点击箭头两次 | 先展开后收起（自身区域收起不重开） |
| 13 | 点击列表项位置 | Popup 未重新打开 → 无选中事件 |
| 14 | 再次 Dump | 关闭后输入内容保留（text 仍 'zzz'） |
| 15 | Dump + injectClose | 最终状态校验，自动退出 |

**15.2 与 15.3 的分工差异：**

| 维度 | 15.2 fromsource | 15.3 test_combobox_cabi |
|------|----------------|------------------------|
| **DLL 加载方式** | ILT 隐式加载（链接 `UICornerstone_dll.lib`） | `LoadLibrary` 显式加载 |
| **后端来源** | 源码编译进 exe | `UIBackend_sdl3.dll` 插件 |
| **C ABI 调用方式** | 直接符号链接 | `GetProcAddress` 函数指针 |
| **测试密度** | 1 个 ComboBox，验证基本可用 | 2-3 个 ComboBox，完整覆盖各类场景 |
| **与其他控件的组合测试** | 是，已有 Label/Button/ColorPicker 共存 | 纯 ComboBox 专项测试 |

两者互补：15.2 保证 ComboBox 在已有集成框架中不破坏其他控件；15.3 保证纯 DLL 部署场景下 ComboBox C ABI 的完整功能。
