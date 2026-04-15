#ifndef LabelH
#define LabelH

#include <SDL3_ttf/SDL_ttf.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include "ConstDef.h"
#include "ControlBase.h"
#include "ResourceLoader.h"

// #include "FontSuite.h"
enum class LabelState {
    Normal,
    Hover,
    Pressed
};

enum class AlignmentMode: int{
    AM_TOP_LEFT,
    AM_MID_LEFT,
    AM_BOTTOM_LEFT,

    AM_TOP_RIGHT,
    AM_MID_RIGHT,
    AM_BOTTOM_RIGHT,

    AM_TOP_CENTER,
    AM_CENTER,
    AM_BOTTOM_CENTER
};
class Label: public ControlImpl {
    friend class LabelBuilder;
    using OnClickHandler = std::function<void (shared_ptr<Label>)>;
private:
    TTF_Text *m_ttfText;
    TTF_Font *m_font;
    TTF_TextEngine *m_textEngin;
    SPoint m_translatedPos;
    SSize m_textSize;
    SDL_Cursor *m_hoverCursor;
    SDL_Cursor *m_defaultCursor;

    SPoint m_shadowOffset;
    AlignmentMode m_AlignmentMode;
    Margin m_margin;
    int m_fontSize;
    string m_caption;
    bool m_shadowEnabled;
    FontName m_fontName;
    fs::path m_fontFile;
    TTF_FontStyleFlags m_fontStyle;
    SRect m_hotRect;

    OnClickHandler m_onClick;

    std::vector<std::string> m_lines;
    std::vector<TTF_Text*> m_lineTexts;
    int m_lineHeight;
    bool m_enableExpand;
    SRect m_originalRect;

private:
    void loadFromFile(void);
    void loadFromResource(string resourceId);
    void createTextEngine(void);
    void initializeFontAndTextEngine(void);
    void parseMultilineText(const string& caption);
    void truncateLine(string& line, float maxWidth);
    void createLineTexts(void);
    void calculateTextSize(void);
    float getTextWidth(const string& text);

public:
    Label(Control *parent, SRect rect, float xScale=1.0f, float yScale=1.0f);
    ~Label(void);
    void update(void) override;
    void draw(void) override;
    bool handleEvent(shared_ptr<Event> event) override;
    void setRect(SRect rect) override;
    SRect getHotRect(void);
    SRect getMarginedRect(void);

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

    void SetFontStyle(TTF_FontStyleFlags fontStyle);
};

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
    LabelBuilder& setId(int id);

    LabelBuilder& setLineHeight(int height);

    LabelBuilder& setEnableExpand(bool enable);

    LabelBuilder& setBorderStateColor(StateColor stateColor);

    LabelBuilder& SetFontStyle(TTF_FontStyleFlags fontStyle);

    shared_ptr<Label> build(void);
};
#endif  // LabelH