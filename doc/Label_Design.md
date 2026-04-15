# Label 标签设计文档

## 1. 概述

Label（标签）是一种用于显示文本的 UI 控件，支持单行和多行文本渲染、阴影效果、对齐方式等功能。

## 2. 功能规格

### 2.1 核心功能

- **文本渲染**：支持单行和多行文本显示
- **对齐方式**：支持 9 种对齐模式
- **阴影效果**：支持文本阴影及偏移设置
- **自动扩展**：支持根据文本内容自动扩展尺寸
- **点击事件**：支持点击回调

### 2.2 对齐模式定义

```cpp
enum class AlignmentMode: int{
    AM_TOP_LEFT,      // 顶部左对齐
    AM_MID_LEFT,      // 中部左对齐
    AM_BOTTOM_LEFT,   // 底部左对齐
    
    AM_TOP_RIGHT,     // 顶部右对齐
    AM_MID_RIGHT,     // 中部右对齐
    AM_BOTTOM_RIGHT,  // 底部右对齐
    
    AM_TOP_CENTER,    // 顶部居中
    AM_CENTER,        // 完全居中
    AM_BOTTOM_CENTER  // 底部居中
};
```

## 3. 接口设计

### 3.1 Label 类

```cpp
class Label: public ControlImpl {
    friend class LabelBuilder;
    using OnClickHandler = std::function<void (shared_ptr<Label>)>;
    
private:
    TTF_Text *m_ttfText;
    TTF_Font *m_font;
    TTF_TextEngine *m_textEngin;
    
    SPoint m_shadowOffset;
    AlignmentMode m_AlignmentMode;
    Margin m_margin;
    int m_fontSize;
    string m_caption;
    bool m_shadowEnabled;
    FontName m_fontName;
    
    OnClickHandler m_onClick;
    std::vector<std::string> m_lines;
    std::vector<TTF_Text*> m_lineTexts;
    int m_lineHeight;
    bool m_enableExpand;
    
public:
    Label(Control *parent, SRect rect, float xScale=1.0f, float yScale=1.0f);
    void update(void) override;
    void draw(void) override;
    bool handleEvent(shared_ptr<Event> event) override;
    void setRect(SRect rect) override;
    
    void setCaption(string caption);
    string getCaption(void) const;
    void setFont(FontName fontName);
    void setAlignmentMode(AlignmentMode Alignment);
    AlignmentMode getAlignmentMode(void) const;
    void setMargin(Margin margin);
    Margin getMargin(void) const;
    void setFontSize(int fontSize);
    void setShadow(bool enabled);
    void setShadowOffset(SPoint offset);
    void setOnClick(OnClickHandler handler);
    
    void setLineHeight(int height);
    int getLineHeight() const;
    
    void setEnableExpand(bool enable);
    bool getEnableExpand() const;
};
```

### 3.2 LabelBuilder 类

```cpp
class LabelBuilder {
private:
    shared_ptr<Label> m_label;
public:
    LabelBuilder(Control *parent, SRect rect, float xScale=1.0f, float yScale=1.0f);
    
    LabelBuilder& setTextStateColor(StateColor stateColor);
    LabelBuilder& setTextShadowStateColor(StateColor stateColor);
    LabelBuilder& setCaption(string caption);
    LabelBuilder& setFont(FontName fontName);
    LabelBuilder& setAlignmentMode(AlignmentMode Alignment);
    LabelBuilder& setFontSize(int fontSize);
    LabelBuilder& setMargin(Margin margin);
    LabelBuilder& setShadow(bool enabled);
    LabelBuilder& setShadowOffset(SPoint offset);
    LabelBuilder& setOnClick(Label::OnClickHandler handler);
    LabelBuilder& setLineHeight(int height);
    LabelBuilder& setEnableExpand(bool enable);
    LabelBuilder& setBorderStateColor(StateColor stateColor);
    LabelBuilder& SetFontStyle(TTF_FontStyleFlags fontStyle);
    LabelBuilder& setId(int id);
    
    shared_ptr<Label> build(void);
};
```

## 4. 实现细节

### 4.1 文本解析

Label 支持多行文本，通过 `\n` 分割：
```cpp
void Label::parseMultilineText(const string& caption) {
    m_lines.clear();
    size_t start = 0;
    while (true) {
        size_t pos = caption.find('\n', start);
        if (pos == string::npos) {
            m_lines.push_back(caption.substr(start));
            break;
        } else {
            m_lines.push_back(caption.substr(start, pos - start));
            start = pos + 1;
        }
    }
}
```

### 4.2 对齐计算

根据不同的对齐模式计算文本渲染位置：
- 水平对齐：左对齐、居中、右对齐
- 垂直对齐：顶部、中部、底部

### 4.3 阴影渲染

在主文本之前渲染阴影文本，支持偏移量设置。

## 5. 常量定义

在 `ConstDef.h` 中定义 Label 相关常量。

## 6. 文件结构

```
UIControls/
├── include/
│   └── Label.h
├── src/
│   └── Label.cpp
└── CMakeLists.txt
```