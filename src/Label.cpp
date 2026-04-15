#include "Label.h"

Label::Label(Control *parent, SRect rect, float xScale, float yScale):
    ControlImpl(parent, xScale, yScale)
    , m_ttfText(nullptr)
    , m_font(nullptr)
    , m_textEngin(nullptr)
    , m_shadowOffset({2, 2})
    , m_AlignmentMode(AlignmentMode::AM_CENTER)
    , m_margin(Margin(4.0f, 2.0f, 4.0f, 2.0f))
    , m_fontSize(16)
    , m_caption("")
    , m_shadowEnabled(false)
    , m_fontName(FontName::HarmonyOS_Sans_SC_Regular)
    , m_fontStyle(0)
    , m_hotRect({0, 0, 0, 0})
    , m_onClick(nullptr)
    , m_lineHeight(20)
    , m_enableExpand(true)
    , m_originalRect(rect)
{
    m_rect = rect;
    setTransparent(true);
    setBorderVisible(false);
    m_lineHeight = static_cast<int>(m_fontSize * 1.2f);

    m_hoverCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    if (m_hoverCursor == nullptr) {
        SDL_Log("Failed to create cursor: %s", SDL_GetError());
    }
    m_defaultCursor = SDL_GetCursor();
    if(m_defaultCursor == nullptr) {
        SDL_Log("Failed to get default cursor: %s", SDL_GetError());
    }
}

void Label::initializeFontAndTextEngine() {
    if (getRenderer() == nullptr) {
        return;
    }

    m_fontFile = ResourceLoader::m_fontFiles[m_fontName];
    fs::path fontPath = ConstDef::pathPrefix / m_fontFile;
    int scaledFontSize = (int)(m_fontSize * getScaleXX());
    m_font = TTF_OpenFont(fontPath.string().c_str(), scaledFontSize);
    if (!m_font) {
        return;
    }
    m_textEngin = TTF_CreateRendererTextEngine(getRenderer());
}

Label::~Label(void){
    if (m_ttfText != nullptr) {
        TTF_DestroyText(m_ttfText);
        m_ttfText = nullptr;
    }
    if (m_textEngin != nullptr) {
        TTF_DestroyRendererTextEngine(m_textEngin);
        m_textEngin = nullptr;
    }
    if (m_font != nullptr) {
        TTF_CloseFont(m_font);   // 注意必须保证在TTF_Quit()之前执行关闭字体的操作，否则会报错
        m_font = nullptr;
    }
    if (m_hoverCursor != nullptr) {
        SDL_DestroyCursor(m_hoverCursor);
        m_hoverCursor = nullptr;
    }
}
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

void Label::truncateLine(string& line, float maxWidth) {
    if (maxWidth <= 0) {
        line = "";
        return;
    }

    const string ellipsis3 = "...";
    const string ellipsis2 = "..";
    const string ellipsis1 = ".";

    float w3 = getTextWidth(ellipsis3);
    float w2 = getTextWidth(ellipsis2);
    float w1 = getTextWidth(ellipsis1);

    if (maxWidth >= w3) {
        int low = 0;
        int high = static_cast<int>(line.length());
        while (low < high) {
            int mid = (low + high + 1) / 2;
            string test = line.substr(0, mid) + ellipsis3;
            if (getTextWidth(test) <= maxWidth) {
                low = mid;
            } else {
                high = mid - 1;
            }
        }
        line = line.substr(0, low) + ellipsis3;
    } else if (maxWidth >= w2) {
        line = ellipsis2;
    } else if (maxWidth >= w1) {
        line = ellipsis1;
    } else {
        line = "";
    }
}

void Label::createLineTexts() {
    for (auto text : m_lineTexts) {
        if (text != nullptr) {
            TTF_DestroyText(text);
        }
    }
    m_lineTexts.clear();

    SRect drawRect = getDrawRect();
    Margin margin = getMargin();
    float scaledMarginLeft = margin.left * getScaleXX();
    float scaledMarginRight = margin.right * getScaleXX();
    float availableWidth = drawRect.width - scaledMarginLeft - scaledMarginRight;
    if (availableWidth < 0) availableWidth = 0;

    for (const auto& line : m_lines) {
        string processedLine = line;
        if (!m_enableExpand && availableWidth > 0 && getTextWidth(processedLine) > availableWidth) {
            truncateLine(processedLine, availableWidth);
        }

        TTF_Text* textObj = TTF_CreateText(m_textEngin, m_font, processedLine.c_str(), processedLine.length());
        m_lineTexts.push_back(textObj);
    }
}

void Label::calculateTextSize() {
    float maxWidth = 0;
    float totalHeight = 0;

    for (size_t i = 0; i < m_lineTexts.size(); ++i) {
        if (m_lineTexts[i] == nullptr) continue;

        int w, h;
        TTF_GetTextSize(m_lineTexts[i], &w, &h);
        float scaledWidth = static_cast<float>(w) / getScaleXX();
        if (scaledWidth > maxWidth) {
            maxWidth = scaledWidth;
        }
        totalHeight += m_lineHeight * getScaleYY();
    }

    m_textSize = {maxWidth, totalHeight};
}

float Label::getTextWidth(const string& text) {
    if (m_font == nullptr || text.empty()) return 0;

    TTF_Text* tempText = TTF_CreateText(m_textEngin, m_font, text.c_str(), text.length());
    if (tempText == nullptr) return 0;

    int w, h;
    TTF_GetTextSize(tempText, &w, &h);
    TTF_DestroyText(tempText);

    return static_cast<float>(w / getScaleXX());
}
void Label::createTextEngine(void){
    fs::path fullPath = ConstDef::pathPrefix / m_fontFile;
    m_font = TTF_OpenFont(fullPath.string().c_str(), m_fontSize * getScaleXX());
    if (m_font == nullptr) {
        SDL_Log("Failed to load font: %s", SDL_GetError());
        return;
    }

    m_textEngin = TTF_CreateRendererTextEngine(getRenderer());
    if (m_textEngin == nullptr) {
        SDL_Log("Failed to create text engine: %s", SDL_GetError());
        return;
    }
}

void Label::loadFromResource(string resourceId){
    shared_ptr<Resource> resource = ResourceLoader::getInstance()->getResource(resourceId);
    if (resource == nullptr || resource->resourceType != ResourceLoader::RT_FONTS
        || resource->pMem == nullptr) {

        SDL_Log("LoadFromResource Error: '%s' is not a font\n", resourceId.c_str());
        return;
    }
    SDL_IOStream *resourceStream = SDL_IOFromConstMem(resource->pMem.get(), resource->resourceSize);
    if (resourceStream == nullptr) {
        SDL_Log("Failed to create IO stream for: %s", resourceId.c_str());
        return;
    }
    m_font = TTF_OpenFontIO(resourceStream, true, m_fontSize * getScaleXX());
    if (m_font == nullptr) {
        SDL_Log("Failed to load font: %s", SDL_GetError());
        return;
    }

    createTextEngine();
}

void Label::update(void){
    if(!getEnable()) return;

    ControlImpl::update();
}

void Label::draw(void){
    if(!getVisible()) return;

    if (m_font == nullptr || m_textEngin == nullptr) {
        initializeFontAndTextEngine();
        if (!m_caption.empty()) {
            setCaption(m_caption);
        }
    } else if (m_lineTexts.empty() && !m_caption.empty()) {
        setCaption(m_caption);
    }

    if (!m_lineTexts.empty() && (m_textSize.width == 0 || m_textSize.height == 0)) {
        setAlignmentMode(m_AlignmentMode);
    }

    if (m_lineTexts.empty()) {
        return;
    }

    SRect rect = getRect();
    SRect drawRect = getDrawRect();

    drawBackground(&drawRect);
    drawBorder(&drawRect);

    Margin margin = getMargin();
    float scaledMarginLeft = margin.left * getScaleXX();
    float scaledMarginTop = margin.top * getScaleYY();
    float scaledMarginRight = margin.right * getScaleXX();
    float scaledMarginBottom = margin.bottom * getScaleYY();

    SRect textRect;
    textRect.left = drawRect.left + scaledMarginLeft;
    textRect.top = drawRect.top + scaledMarginTop;
    textRect.width = drawRect.width - scaledMarginLeft - scaledMarginRight;
    textRect.height = drawRect.height - scaledMarginTop - scaledMarginBottom;

    float textX = textRect.left + m_hotRect.left;
    float textY = textRect.top + m_hotRect.top;

    SDL_Color shadowColor;
    switch(getState()) {
        case ControlState::Disabled:
            shadowColor = m_textShadowColor.getDisabled();
            break;
        case ControlState::Hover:
            shadowColor = m_textShadowColor.getHover();
            break;
        case ControlState::Pressed:
            shadowColor = m_textShadowColor.getPressed();
            break;
        default:
            shadowColor = m_textShadowColor.getNormal();
            break;
    }

    SDL_Color textColor;
    switch(getState()) {
        case ControlState::Disabled:
            textColor = m_textColor.getDisabled();
            break;
        case ControlState::Hover:
            textColor = m_textColor.getHover();
            break;
        case ControlState::Pressed:
            textColor = m_textColor.getPressed();
            break;
        default:
            textColor = m_textColor.getNormal();
            break;
    }

    for (size_t i = 0; i < m_lineTexts.size(); ++i) {
        if (m_lineTexts[i] == nullptr) continue;

        int lineWidth, lineHeight;
        TTF_GetTextSize(m_lineTexts[i], &lineWidth, &lineHeight);
        float scaledLineWidth = static_cast<float>(lineWidth);

        float lineX = textX;
        float lineY = textY + i * m_lineHeight * getScaleYY();

        if (m_shadowEnabled) {
            if(!TTF_SetTextColor(m_lineTexts[i], shadowColor.r, shadowColor.g, shadowColor.b, shadowColor.a)) {
                SDL_Log("Failed to set shadow text color: %s", SDL_GetError());
            }
            if (!TTF_DrawRendererText(m_lineTexts[i],
                                        lineX + m_shadowOffset.x * m_xxScale,
                                        lineY + m_shadowOffset.y * m_yyScale)) {
                SDL_Log("Failed to render shadow text: %s", SDL_GetError());
            }
        }

        if(!TTF_SetTextColor(m_lineTexts[i], textColor.r, textColor.g, textColor.b, textColor.a)) {
            SDL_Log("Failed to set text color: %s", SDL_GetError());
        }

        if (!TTF_DrawRendererText(m_lineTexts[i], lineX, lineY)) {
            SDL_Log("Failed to render text: %s", SDL_GetError());
        }
    }

    ControlImpl::draw();
}

bool Label::handleEvent(shared_ptr<Event> event){
    SDL_Log("Label::handleEvent CALLED: caption=%s, event=%d", m_caption.c_str(), (int)event->m_eventName);
    if(!getEnable() || !getVisible()) return false;

    if (ControlImpl::handleEvent(event)) return true;

    if (EventQueue::isPositionEvent(event->m_eventName)){
        if (!event->m_eventParam.has_value()) return false;
        try {
            shared_ptr<SPoint> pos = std::any_cast<shared_ptr<SPoint>>(event->m_eventParam);
            if (!pos) return false;
            SRect detectRect = getHotRect();
            SDL_Log("Label handleEvent: label=[%s] rect=(%.0f,%.0f,%.0f,%.0f) mouse=(%.0f,%.0f)",
                m_caption.c_str(), detectRect.left, detectRect.top, detectRect.width, detectRect.height, pos->x, pos->y);
            if (detectRect.contains(pos->x, pos->y)){
                switch(event->m_eventName){
                    case EventName::FINGER_DOWN:
                    case EventName::FINGER_MOTION:
                        if (m_onClick != nullptr){
                            m_onClick(dynamic_pointer_cast<Label>(this->getThis()));
                        }
                        setState(ControlState::Pressed);
                        return true;
                    case EventName::MOUSE_LBUTTON_DOWN:
                        setState(ControlState::Pressed);
                        if(m_hoverCursor != nullptr){
                            SDL_SetCursor(m_hoverCursor);
                        }
                        return true;
                    case EventName::MOUSE_LBUTTON_UP:
                        if (m_onClick != nullptr && m_state == ControlState::Pressed){
                            m_onClick(dynamic_pointer_cast<Label>(this->getThis()));
                        }
                        setState(ControlState::Hover);
                        if(m_hoverCursor != nullptr){
                            SDL_SetCursor(m_hoverCursor);
                        }
                        return true;
                    case EventName::MOUSE_MOVING:
                        setState(ControlState::Hover);
                        if(m_hoverCursor != nullptr){
                            SDL_SetCursor(m_hoverCursor);
                        }
                        return true;
                    case EventName::MOUSE_WHEEL:
                        return false;
                    default:
                        break;
                }
                return true;
            } else {
                setState(ControlState::Normal);
                if(m_defaultCursor){
                    SDL_SetCursor(m_defaultCursor);
                }
            }
        } catch (...) {
            return false;
        }
    }
    return false;
}

void Label::setRect(SRect rect){
    ControlImpl::setRect(rect);

    // 重新计算对齐位置，如果宽高不够，会自动扩展
    setAlignmentMode(m_AlignmentMode);
}
SRect Label::getHotRect(void){
    SRect drawRect = getDrawRect();
    float scaleX = getScaleXX();
    float scaleY = getScaleYY();
    SDL_Log("Label::getHotRect - label: %s, m_hotRect: (%.1f, %.1f, %.1f, %.1f), scale: (%.1f, %.1f), drawRect: (%.1f, %.1f, %.1f, %.1f), result: (%.1f, %.1f, %.1f, %.1f)",
        m_caption.c_str(), m_hotRect.left, m_hotRect.top, m_hotRect.width, m_hotRect.height, scaleX, scaleY,
        drawRect.left, drawRect.top, drawRect.width, drawRect.height,
        drawRect.left + m_hotRect.left,
        drawRect.top + m_hotRect.top,
        m_hotRect.width * scaleX,
        m_hotRect.height * scaleY);
    return {
        drawRect.left + m_hotRect.left,
        drawRect.top + m_hotRect.top,
        m_hotRect.width * scaleX,
        m_hotRect.height * scaleY
    };
}
SRect Label::getMarginedRect(void) {
    return SRect{
        getRect().left + getMargin().left,
        getRect().top + getMargin().top,
        getRect().width - getMargin().left - getMargin().right,
        getRect().height - getMargin().top - getMargin().bottom
    };
}

/*********************************************************for Builder mode**********************************************************/
void Label::setCaption(string caption){
    m_caption = caption;

    if (m_font == nullptr || m_textEngin == nullptr) {
        initializeFontAndTextEngine();
    }

    if (m_font == nullptr || m_textEngin == nullptr) {
        return;
    }

    parseMultilineText(caption);
    createLineTexts();
    calculateTextSize();
    setAlignmentMode(m_AlignmentMode);
}
string Label::getCaption(void) const{
    return m_caption;
}
void Label::setFont(FontName fontName){
    m_fontName = fontName;
    m_fontFile = fs::path(ResourceLoader::m_fontFiles[fontName]);
}
void Label::setAlignmentMode(AlignmentMode Alignment){
    float scaledTextWidth = m_textSize.width;
    float scaledTextHeight = m_textSize.height;
    float scaledMarginWidth = (m_margin.left + m_margin.right) * getScaleXX();
    float scaledMarginHeight = (m_margin.top + m_margin.bottom) * getScaleYY();

    float requiredWidth = scaledTextWidth + scaledMarginWidth;
    float requiredHeight = scaledTextHeight + scaledMarginHeight;

    if (m_enableExpand) {
        if (requiredWidth > getRect().width * getScaleXX()){
            setWidth(requiredWidth / getScaleXX());
        }
        if (requiredHeight > getRect().height * getScaleYY()){
            setHeight(requiredHeight / getScaleYY());
        }
    }

    SRect rect = getRect();
    float availableWidth = rect.width * getScaleXX() - scaledMarginWidth;
    float availableHeight = rect.height * getScaleYY() - scaledMarginHeight;

    float scaledMarginLeft = m_margin.left * getScaleXX();
    float scaledMarginTop = m_margin.top * getScaleYY();

    float offsetX = scaledMarginLeft;
    float offsetY = scaledMarginTop;

    switch (Alignment) {
        case AlignmentMode::AM_TOP_CENTER:
        case AlignmentMode::AM_CENTER:
        case AlignmentMode::AM_BOTTOM_CENTER:
            offsetX += (availableWidth - scaledTextWidth) / 2;
            break;
        case AlignmentMode::AM_TOP_RIGHT:
        case AlignmentMode::AM_MID_RIGHT:
        case AlignmentMode::AM_BOTTOM_RIGHT:
            offsetX += availableWidth - scaledTextWidth;
            break;
        default:
            break;
    }

    switch (Alignment) {
        case AlignmentMode::AM_CENTER:
        case AlignmentMode::AM_MID_LEFT:
        case AlignmentMode::AM_MID_RIGHT:
            offsetY += (availableHeight - scaledTextHeight) / 2;
            break;
        case AlignmentMode::AM_BOTTOM_LEFT:
        case AlignmentMode::AM_BOTTOM_CENTER:
        case AlignmentMode::AM_BOTTOM_RIGHT:
            offsetY += availableHeight - scaledTextHeight;
            break;
        default:
            break;
    }

    m_AlignmentMode = Alignment;
    m_hotRect = {offsetX, offsetY, m_textSize.width, m_textSize.height};
}
AlignmentMode Label::getAlignmentMode(void) const{
    return m_AlignmentMode;
}
void Label::setMargin(Margin margin){
    m_margin = margin;
    setAlignmentMode(m_AlignmentMode);
}
Margin Label::getMargin(void) const{
    return m_margin;
}
void Label::setFontSize(int fontSize){
    if (fontSize == m_fontSize) return;
    m_fontSize = fontSize;

    if (m_font == nullptr) return;
    if(!TTF_SetFontSize(m_font, fontSize * getScaleXX())) {
        SDL_Log("Failed to set font size: %s", SDL_GetError());
        // throw "Failed to set font size: %s", SDL_GetError();
        return;
    }

    // Todo: 下面需要对已经生成的参数重新计算一遍
    setCaption(m_caption);
}
void Label::setShadow(bool enabled){
    m_shadowEnabled = enabled;
}
void Label::setShadowOffset(SPoint offset){
    m_shadowOffset = offset;
}
void Label::setOnClick(OnClickHandler handler){
    m_onClick = handler;
}
// void Label::setId(int id){
//     m_id = id;
//     return;
// }
void Label::SetFontStyle(TTF_FontStyleFlags fontStyle){
    m_fontStyle = fontStyle;

    if (m_font == nullptr) return;

    TTF_SetFontStyle(m_font, fontStyle); // Todo: 下面需要对已经生成的参数重新计算一遍
    setCaption(m_caption);
}

void Label::setLineHeight(int height){
    if (height <= 0) return;
    m_lineHeight = height;
    if (!m_caption.empty()) {
        setCaption(m_caption);
    }
}

int Label::getLineHeight() const{
    return m_lineHeight;
}

void Label::setEnableExpand(bool enable){
    m_enableExpand = enable;
    if (!m_caption.empty() && m_font != nullptr && m_textEngin != nullptr) {
        m_rect = m_originalRect;
        createLineTexts();
        calculateTextSize();
        setAlignmentMode(m_AlignmentMode);
    }
}

bool Label::getEnableExpand() const{
    return m_enableExpand;
}

LabelBuilder::LabelBuilder(Control *parent, SRect rect, float xScale, float yScale):
    m_label(nullptr)
{
    m_label = make_shared<Label>(parent, rect, xScale, yScale);
}
LabelBuilder& LabelBuilder::setTextStateColor(StateColor stateColor){
    m_label->setTextStateColor(stateColor);
    return *this;
}
LabelBuilder& LabelBuilder::setTextShadowStateColor(StateColor stateColor){
    m_label->setTextShadowStateColor(stateColor);
    return *this;
}

LabelBuilder& LabelBuilder::setCaption(string caption){
    m_label->setCaption(caption);
    return *this;
}
LabelBuilder& LabelBuilder::setFont(FontName fontName){
    m_label->setFont(fontName);
    m_label->loadFromResource(m_label->m_fontFile.string());
    return *this;
}
LabelBuilder& LabelBuilder::setAlignmentMode(AlignmentMode Alignment){
    m_label->setAlignmentMode(Alignment);
    return *this;
}
LabelBuilder& LabelBuilder::setFontSize(int fontSize){
    m_label->setFontSize(fontSize);
    return *this;
}
LabelBuilder& LabelBuilder::setMargin(Margin margin){
    m_label->setMargin(margin);
    return *this;
}
LabelBuilder& LabelBuilder::setShadow(bool enabled){
    m_label->setShadow(enabled);
    return *this;
}

LabelBuilder& LabelBuilder::setShadowOffset(SPoint offset){
    m_label->setShadowOffset(offset);
    return *this;
}
LabelBuilder& LabelBuilder::setOnClick(Label::OnClickHandler handler){
    m_label->setOnClick(handler);
    return *this;
}
LabelBuilder& LabelBuilder::setId(int id){
    m_label->setId(id);
    return *this;
}

LabelBuilder& LabelBuilder::SetFontStyle(TTF_FontStyleFlags fontStyle){
    m_label->SetFontStyle(fontStyle);
    return *this;
}

LabelBuilder& LabelBuilder::setLineHeight(int height){
    m_label->setLineHeight(height);
    return *this;
}

LabelBuilder& LabelBuilder::setEnableExpand(bool enable){
    m_label->setEnableExpand(enable);
    return *this;
}

LabelBuilder& LabelBuilder::setBorderStateColor(StateColor stateColor){
    m_label->setBorderStateColor(stateColor);
    m_label->setBorderVisible(true);
    return *this;
}

shared_ptr<Label> LabelBuilder::build(void){
    return m_label;
}