# CheckBox 复选框设计文档

## 1. 概述

复选框（CheckBox）是一种常见的 UI 控件，用于表示布尔状态或从多个选项中选择多个选项。本设计文档描述了复选框的功能规格、接口定义和实现细节。

## 2. 功能规格

### 2.1 核心功能

- **状态切换**：支持两种模式：
  - 三态模式（默认）：未选中 → 选中 → 三态 → 未选中
  - 两态模式：仅在未选中 ↔ 选中之间切换
- **样式支持**：支持三种视觉样式（Classic、Cross、Circle）
- **布局支持**：支持文字在左侧或右侧两种布局
- **垂直对齐**：支持三种垂直对齐方式（居中、顶部、底部）
- **尺寸比例**：复选框尺寸基于字体大小的比例计算
- **颜色自定义**：支持自定义勾选符号、叉号、三态、边框颜色

### 2.2 状态定义

```cpp
enum class CheckState {
    Unchecked,     // 未选中
    Checked,       // 选中
    Indeterminate  // 三态（部分选中）
};
```

### 2.3 样式定义

```cpp
enum class CheckBoxStyle {
    Classic,   // 方框 + 勾选符号
    Cross,     // 方框 + X 符号
    Circle     // 圆形 + 勾选符号
};
```

### 2.4 布局定义

```cpp
enum class CheckBoxLayout {
    TextRight,  // 文字在右（默认）
    TextLeft    // 文字在左
};

enum class CheckBoxVerticalAlign {
    Center,      // 垂直居中（默认）
    Top,         // 与第一行顶部对齐
    Bottom       // 与最后一行底部对齐
};
```

## 3. 尺寸计算规则

### 3.1 复选框尺寸

复选框尺寸根据字体大小和缩放比例计算：

```
复选框尺寸 = 字体大小 × 缩放比例 × 尺寸倍率
```

- **默认倍率**：`0.8`（即复选框尺寸为字体大小的 80%）
- **用户可调整**：通过 `setSizeRatio(float ratio)` 设置倍率

### 3.2 布局计算

#### TextRight 布局
```
+------------------------------------------+
| [复选框] [文字区域]                      |
+------------------------------------------+
```

- 复选框区域：左侧，尺寸为 `m_sizeRatio * fontSize * scale`
- 间距：`CHECKBOX_BOX_MARGIN`（常量）
- 文字区域：剩余宽度

#### TextLeft 布局
```
+------------------------------------------+
| [文字区域] [复选框]                      |
+------------------------------------------+
```

### 3.3 多行文字垂直对齐

当文字有多行时，复选框的垂直位置根据 `m_verticalAlign` 确定：

| 对齐方式 | 说明 |
|---------|------|
| Center | 复选框垂直居中于整体文字区域 |
| Top | 复选框与第一行顶部对齐 |
| Bottom | 复选框与最后一行底部对齐 |

## 4. 接口设计

### 4.1 CheckBox 类

```cpp
class CheckBox : public ControlImpl {
public:
    using OnCheckChangedHandler = std::function<void (shared_ptr<CheckBox>, CheckState)>;

private:
    CheckState m_checkState;
    CheckBoxStyle m_style;
    CheckBoxLayout m_layout;
    CheckBoxVerticalAlign m_verticalAlign;

    shared_ptr<Label> m_caption;
    OnCheckChangedHandler m_onCheckChanged;

    float m_sizeRatio;         // 复选框尺寸与字体大小的比例（默认0.8）
    float m_captionSize;
    string m_captionText;

public:
    CheckBox(Control *parent, SRect rect, float xScale = 1.0f, float yScale = 1.0f);

    void update(void) override;
    void draw(void) override;
    bool handleEvent(shared_ptr<Event> event) override;
    void setRect(SRect rect) override;

    void onMouseEnter(float x, float y) override;
    void onMouseLeave(float x, float y) override;

    void setCheckState(CheckState state);
    CheckState getCheckState() const;

    void setStyle(CheckBoxStyle style);
    CheckBoxStyle getStyle() const;

    void setLayout(CheckBoxLayout layout);
    CheckBoxLayout getLayout() const;

    void setVerticalAlign(CheckBoxVerticalAlign align);
    CheckBoxVerticalAlign getVerticalAlign() const;

    void setSizeRatio(float ratio);
    float getSizeRatio() const;

    void setCaption(string caption);
    string getCaption() const;

    void setCaptionSize(float size);

    void setOnCheckChanged(OnCheckChangedHandler handler);

private:
    float calculateCheckBoxSize();
    SRect calculateCheckBoxRect();
    SRect calculateCaptionRect();
    void updateCaptionPosition();

    void drawCheckBoxFrame(SRect boxRect);
    void drawCheckMark(SRect boxRect);
    void drawCrossMark(SRect boxRect);
    void drawIndeterminateMark(SRect boxRect);
};
```

### 4.2 CheckBoxBuilder 类

```cpp
class CheckBoxBuilder {
private:
    shared_ptr<CheckBox> m_checkBox;

public:
    CheckBoxBuilder(Control *parent, SRect rect, float xScale = 1.0f, float yScale = 1.0f);

    CheckBoxBuilder& setStyle(CheckBoxStyle style);
    CheckBoxBuilder& setLayout(CheckBoxLayout layout);
    CheckBoxBuilder& setVerticalAlign(CheckBoxVerticalAlign align);
    CheckBoxBuilder& setCheckState(CheckState state);
    CheckBoxBuilder& setSizeRatio(float ratio);
    CheckBoxBuilder& setCaption(string caption);
    CheckBoxBuilder& setCaptionSize(float size);
    CheckBoxBuilder& setOnCheckChanged(CheckBox::OnCheckChangedHandler handler);
    CheckBoxBuilder& setBackgroundStateColor(StateColor stateColor);
    CheckBoxBuilder& setBorderStateColor(StateColor stateColor);
    CheckBoxBuilder& setTextStateColor(StateColor stateColor);
    CheckBoxBuilder& setId(int id);
    CheckBoxBuilder& setEnable(bool enable);

    shared_ptr<CheckBox> build(void);
};
```

## 5. 常量定义

在 `ConstDef.h` 中添加：

```cpp
// 复选框相关常量
static const float CHECKBOX_SIZE_RATIO;         // 默认尺寸倍率（0.8）
static const float CHECKBOX_BOX_MARGIN;          // 复选框与文字的间距
static const float CHECKBOX_DEFAULT_CAPTION_SIZE; // 文字默认大小
static const SDL_Color CHECKBOX_CHECK_COLOR;     // 勾选符号颜色
static const SDL_Color CHECKBOX_CROSS_COLOR;     // X 符号颜色
static const SDL_Color CHECKBOX_INDETERMINATE_COLOR; // 三态颜色
```

## 6. 事件处理

### 6.1 状态切换逻辑

点击复选框时，状态按以下顺序循环切换：

```
Unchecked → Checked → Indeterminate → Unchecked
```

### 6.2 事件处理流程

```cpp
bool CheckBox::handleEvent(shared_ptr<Event> event) {
    if (!getEnable() || !getVisible()) return false;

    if (EventQueue::isPositionEvent(event->m_eventName)) {
        if (!event->m_eventParam.has_value()) return false;

        try {
            auto pos = std::any_cast<shared_ptr<SPoint>>(event->m_eventParam);
            if (!pos) return false;

            if (getDrawRect().contains(pos->x, pos->y)) {
                switch (event->m_eventName) {
                    case EventName::MOUSE_LBUTTON_UP:
                        // 循环切换状态
                        switch (m_checkState) {
                            case CheckState::Unchecked:
                                setCheckState(CheckState::Checked);
                                break;
                            case CheckState::Checked:
                                setCheckState(CheckState::Indeterminate);
                                break;
                            case CheckState::Indeterminate:
                                setCheckState(CheckState::Unchecked);
                                break;
                        }
                        if (m_onCheckChanged) {
                            m_onCheckChanged(dynamic_pointer_cast<CheckBox>(getThis()), m_checkState);
                        }
                        return true;

                    case EventName::MOUSE_MOVING:
                        if (getState() != ControlState::Hover) {
                            setState(ControlState::Hover);
                        }
                        return true;
                }
            } else {
                if (getState() == ControlState::Hover) {
                    setState(ControlState::Normal);
                }
            }
        } catch (...) {
            return false;
        }
    }

    return ControlImpl::handleEvent(event);
}
```

## 7. 实现细节

### 7.1 复选框尺寸计算

复选框尺寸根据字体大小和缩放比例计算：

```
复选框尺寸 = 字体大小 × 缩放比例 × 尺寸倍率
```

- **默认倍率**：`0.8`
- 用户可通过 `setSizeRatio(float ratio)` 调整

### 7.2 布局实现

#### 复选框位置计算

```cpp
float boxX = (m_layout == CheckBoxLayout::TextRight) 
    ? drawRect.left                                    // 左侧布局：复选框在左
    : drawRect.left + drawRect.width - boxSize;       // 右侧布局：复选框在右
```

#### 文字区域计算

```cpp
if (m_layout == CheckBoxLayout::TextRight) {
    // 文字区域从复选框右侧开始
    return {drawRect.left + boxSize + scaledMargin, ...};
} else {
    // 文字区域从复选框左侧结束位置开始向前计算
    float boxX = drawRect.left + drawRect.width - boxSize;
    return {boxX - scaledMargin - captionWidth, ...};
}
```

#### 对齐模式选择

根据布局选择不同的对齐模式：
- **TextRight** 使用 `AM_MID_LEFT`：文字从 Label 左侧开始渲染
- **TextLeft** 使用 `AM_MID_RIGHT`：文字从 Label 右侧开始渲染，配合 captionRect 从复选框右侧开始计算，实现文字紧贴复选框的效果

### 7.3 特殊实现说明

1. **Label 父控件设置**：内部 Label 的父控件设置为 `nullptr`，避免继承 CheckBox 的缩放导致位置偏移

2. **Margin 设置**：Label 的 margin 设置为 `{0, 0, 0, 0}`，避免额外间距

## 8. 绘制逻辑

### 7.1 绘制流程

1. 计算复选框区域和文字区域
2. 绘制复选框外框（根据样式）
3. 绘制内部标记（根据状态）
4. 绘制文字

### 7.2 样式绘制

| 样式 | 外框 | 选中状态 | 三态 |
|------|------|----------|------|
| Classic | 方框边框 | 勾选符号（√） | 横线 |
| Cross | 方框边框 | X 符号（×） | 横线 |
| Circle | 圆形边框 | 勾选符号（√） | 横线 |

## 8. 测试用例

### 8.1 基本功能测试

```cpp
void testBasicCheckBox() {
    auto checkbox = CheckBoxBuilder(nullptr, SRect(50, 50, 200, 30))
        .setCaption("Accept Terms")
        .build();
    BENCH->addControl(checkbox);
}
```

### 8.2 状态切换测试

```cpp
void testStateChange() {
    auto checkbox = CheckBoxBuilder(nullptr, SRect(50, 100, 200, 30))
        .setCaption("Enable Feature")
        .setCheckState(CheckState::Checked)
        .setOnCheckChanged([](shared_ptr<CheckBox> cb, CheckState state) {
            cout << "State changed to: " << (int)state << endl;
        })
        .build();
}
```

### 8.3 样式测试

```cpp
void testStyles() {
    auto classic = CheckBoxBuilder(nullptr, SRect(50, 150, 200, 30))
        .setStyle(CheckBoxStyle::Classic)
        .setCaption("Classic Style")
        .build();

    auto cross = CheckBoxBuilder(nullptr, SRect(50, 200, 200, 30))
        .setStyle(CheckBoxStyle::Cross)
        .setCaption("Cross Style")
        .build();

    auto circle = CheckBoxBuilder(nullptr, SRect(50, 250, 200, 30))
        .setStyle(CheckBoxStyle::Circle)
        .setCaption("Circle Style")
        .build();
}
```

### 8.4 布局测试

```cpp
void testLayouts() {
    auto textRight = CheckBoxBuilder(nullptr, SRect(50, 300, 200, 30))
        .setLayout(CheckBoxLayout::TextRight)
        .setCaption("Text on Right")
        .build();

    auto textLeft = CheckBoxBuilder(nullptr, SRect(300, 300, 200, 30))
        .setLayout(CheckBoxLayout::TextLeft)
        .setCaption("Text on Left")
        .build();
}
```

### 8.5 多行文字垂直对齐测试

```cpp
void testMultiLineVerticalAlign() {
    string multiline = "Line 1\nLine 2\nLine 3";

    auto center = CheckBoxBuilder(nullptr, SRect(50, 450, 250, 80))
        .setCaption(multiline)
        .setVerticalAlign(CheckBoxVerticalAlign::Center)
        .build();

    auto top = CheckBoxBuilder(nullptr, SRect(350, 450, 250, 80))
        .setCaption(multiline)
        .setVerticalAlign(CheckBoxVerticalAlign::Top)
        .build();

    auto bottom = CheckBoxBuilder(nullptr, SRect(650, 450, 250, 80))
        .setCaption(multiline)
        .setVerticalAlign(CheckBoxVerticalAlign::Bottom)
        .build();
}
```

### 8.6 缩放测试

```cpp
void testScaling() {
    auto scaled2x = CheckBoxBuilder(nullptr, SRect(50, 550, 200, 30), 2.0f, 2.0f)
        .setCaption("2x Scaled")
        .setCheckState(CheckState::Checked)
        .build();

    auto scaled0_5x = CheckBoxBuilder(nullptr, SRect(300, 550, 200, 30), 0.5f, 0.5f)
        .setCaption("0.5x Scaled")
        .setCheckState(CheckState::Indeterminate)
        .build();
}
```

### 8.7 尺寸倍率测试

```cpp
void testSizeRatios() {
    auto defaultRatio = CheckBoxBuilder(nullptr, SRect(50, 50, 200, 30))
        .setCaption("Default Ratio (0.8)")
        .build();

    auto largeRatio = CheckBoxBuilder(nullptr, SRect(50, 100, 200, 30))
        .setCaption("Large Ratio (1.0)")
        .setSizeRatio(1.0f)
        .build();

    auto smallRatio = CheckBoxBuilder(nullptr, SRect(50, 150, 200, 30))
        .setCaption("Small Ratio (0.6)")
        .setSizeRatio(0.6f)
        .build();
}
```

### 8.8 禁用状态测试

```cpp
void testDisabled() {
    auto disabled = CheckBoxBuilder(nullptr, SRect(50, 400, 200, 30))
        .setCaption("Disabled Checkbox")
        .setEnable(false)
        .setCheckState(CheckState::Checked)
        .build();
}
```

### 8.9 回调测试

```cpp
void testCallbacks() {
    auto checkbox = CheckBoxBuilder(nullptr, SRect(50, 650, 200, 30))
        .setCaption("Callback Test")
        .setOnCheckChanged([](shared_ptr<CheckBox> cb, CheckState state) {
            static int count = 0;
            count++;
            cout << "Callback #" << count << ": State = " << (int)state << endl;
        })
        .build();
}
```

## 9. 文件结构

```
UIControls/
├── include/
│   └── CheckBox.h        # 头文件
├── src/
│   └── CheckBox.cpp      # 实现文件
└── CMakeLists.txt        # 更新的构建配置

test/
└── test_checkbox.cpp     # 测试文件
```

## 10. 实现顺序

1. 在 `ConstDef.h` 添加常量声明
2. 在 `ConstDef.cpp` 添加常量定义
3. 创建 `UIControls/include/CheckBox.h`
4. 创建 `UIControls/src/CheckBox.cpp`
5. 更新 `UIControls/CMakeLists.txt`
6. 创建 `test/test_checkbox.cpp`
7. 编译测试