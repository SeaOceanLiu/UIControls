#include "CheckBox.h"
#include "GraphTool.h"

CheckBox::CheckBox(Control *parent, SRect rect, float xScale, float yScale):
    ControlImpl(parent, xScale, yScale),
    m_checkState(CheckState::Unchecked),
    m_style(CheckBoxStyle::Classic),
    m_layout(CheckBoxLayout::TextRight),
    m_verticalAlign(CheckBoxVerticalAlign::Center),
    m_caption(nullptr),
    m_onCheckChanged(nullptr),
    m_sizeRatio(ConstDef::CHECKBOX_SIZE_RATIO),
    m_captionSize(ConstDef::CHECKBOX_DEFAULT_CAPTION_SIZE),
    m_captionText(""),
    m_triStateEnabled(true),
    m_checkStateColor(StateColor::Type::Text),
    m_crossStateColor(StateColor::Type::Text),
    m_indeterminateStateColor(StateColor::Type::Text)
{
    m_checkStateColor.setNormal(ConstDef::CHECKBOX_CHECK_COLOR);
    m_crossStateColor.setNormal(ConstDef::CHECKBOX_CROSS_COLOR);
    m_indeterminateStateColor.setNormal(ConstDef::CHECKBOX_INDETERMINATE_COLOR);
    m_boxBorderStateColor.setNormal(ConstDef::DEFAULT_BORDER_NORMAL_COLOR);
    
    m_rect = rect;
    setTransparent(true);
    setBorderVisible(false);
}

void CheckBox::update(void) {
    if (!getEnable()) return;
    ControlImpl::update();
}

void CheckBox::draw(void) {
    if (!getVisible()) return;

    SRect drawRect = getDrawRect();
    float boxSize = calculateCheckBoxSize();
    SRect boxRect = calculateCheckBoxRect();
    
    drawCheckBoxFrame(boxRect);
    
    switch (m_checkState) {
        case CheckState::Checked:
            if (m_style == CheckBoxStyle::Cross) {
                drawCrossMark(boxRect);
            } else {
                drawCheckMark(boxRect);
            }
            break;
        case CheckState::Indeterminate:
            drawIndeterminateMark(boxRect);
            break;
        case CheckState::Unchecked:
        default:
            break;
    }
    
    if (m_caption != nullptr) {
        SRect captionRect = calculateCaptionRect();
        m_caption->setRect(captionRect);
        m_caption->draw();
    }
    
    ControlImpl::draw();
}

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
                        if (m_triStateEnabled) {
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
                        } else {
                            switch (m_checkState) {
                                case CheckState::Unchecked:
                                    setCheckState(CheckState::Checked);
                                    break;
                                case CheckState::Checked:
                                    setCheckState(CheckState::Unchecked);
                                    break;
                                case CheckState::Indeterminate:
                                    setCheckState(CheckState::Unchecked);
                                    break;
                            }
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

void CheckBox::setRect(SRect rect) {
    ControlImpl::setRect(rect);
    updateCaptionPosition();
}

void CheckBox::onMouseEnter(float x, float y) {
    setState(ControlState::Hover);
}

void CheckBox::onMouseLeave(float x, float y) {
    setState(ControlState::Normal);
}

void CheckBox::setCheckState(CheckState state) {
    if (state == CheckState::Indeterminate && !m_triStateEnabled) {
        m_checkState = CheckState::Unchecked;
    } else {
        m_checkState = state;
    }
}

CheckState CheckBox::getCheckState() const {
    return m_checkState;
}

void CheckBox::setTriStateEnabled(bool enabled) {
    m_triStateEnabled = enabled;
    if (!enabled && m_checkState == CheckState::Indeterminate) {
        m_checkState = CheckState::Unchecked;
    }
}

bool CheckBox::isTriStateEnabled() const {
    return m_triStateEnabled;
}

void CheckBox::setStyle(CheckBoxStyle style) {
    m_style = style;
}

CheckBoxStyle CheckBox::getStyle() const {
    return m_style;
}

void CheckBox::setLayout(CheckBoxLayout layout) {
    m_layout = layout;
    updateCaptionPosition();
}

CheckBoxLayout CheckBox::getLayout() const {
    return m_layout;
}

void CheckBox::setVerticalAlign(CheckBoxVerticalAlign align) {
    m_verticalAlign = align;
}

CheckBoxVerticalAlign CheckBox::getVerticalAlign() const {
    return m_verticalAlign;
}

void CheckBox::setSizeRatio(float ratio) {
    m_sizeRatio = ratio;
}

float CheckBox::getSizeRatio() const {
    return m_sizeRatio;
}

void CheckBox::setCaption(string caption) {
    m_captionText = caption;
    
    if (m_caption != nullptr) {
        m_caption.reset();
    }
    
    if (m_captionText.length() > 0) {
        SRect captionRect = calculateCaptionRect();
        
        AlignmentMode alignment = (m_layout == CheckBoxLayout::TextRight) 
            ? AlignmentMode::AM_MID_LEFT 
            : AlignmentMode::AM_MID_RIGHT;
        
        m_caption = LabelBuilder(nullptr, captionRect)
            .setFont(FontName::HarmonyOS_Sans_SC_Regular)
            .setAlignmentMode(alignment)
            .setFontSize((int)m_captionSize)
            .setCaption(m_captionText)
            .setTextStateColor(m_textColor)
            .setMargin({0, 0, 0, 0})
            .build();
    }
}

string CheckBox::getCaption() const {
    return m_captionText;
}

void CheckBox::setCaptionSize(float size) {
    m_captionSize = size;
    if (m_caption != nullptr) {
        m_caption->setFontSize((int)m_captionSize);
    }
}

void CheckBox::setOnCheckChanged(OnCheckChangedHandler handler) {
    m_onCheckChanged = handler;
}

void CheckBox::setCheckColor(SDL_Color color) {
    m_checkStateColor.setNormal(color);
}

SDL_Color CheckBox::getCheckColor() {
    return m_checkStateColor.getNormal();
}

void CheckBox::setCrossColor(SDL_Color color) {
    m_crossStateColor.setNormal(color);
}

SDL_Color CheckBox::getCrossColor() {
    return m_crossStateColor.getNormal();
}

void CheckBox::setIndeterminateColor(SDL_Color color) {
    m_indeterminateStateColor.setNormal(color);
}

SDL_Color CheckBox::getIndeterminateColor() {
    return m_indeterminateStateColor.getNormal();
}

void CheckBox::setFont(FontName fontName) {
    if (m_caption != nullptr) {
        m_caption->setFont(fontName);
    }
}

void CheckBox::setFontSize(int fontSize) {
    if (m_caption != nullptr) {
        m_caption->setFontSize(fontSize);
    }
    m_captionSize = (float)fontSize;
}

void CheckBox::setAlignmentMode(AlignmentMode mode) {
    if (m_caption != nullptr) {
        m_caption->setAlignmentMode(mode);
    }
}

void CheckBox::setShadow(bool enabled) {
    if (m_caption != nullptr) {
        m_caption->setShadow(enabled);
    }
}

void CheckBox::setShadowOffset(SPoint offset) {
    if (m_caption != nullptr) {
        m_caption->setShadowOffset(offset);
    }
}

void CheckBox::setBoxBorderColor(SDL_Color color) {
    m_boxBorderStateColor.setNormal(color);
}

SDL_Color CheckBox::getBoxBorderColor() {
    return m_boxBorderStateColor.getNormal();
}

float CheckBox::calculateCheckBoxSize() {
    float fontSize = m_captionSize > 0 ? m_captionSize : ConstDef::CHECKBOX_DEFAULT_CAPTION_SIZE;
    return fontSize * getScaleXX() * m_sizeRatio;
}

SRect CheckBox::calculateCheckBoxRect() {
    SRect drawRect = getDrawRect();
    float boxSize = calculateCheckBoxSize();
    
    float boxX = (m_layout == CheckBoxLayout::TextRight) 
        ? drawRect.left 
        : drawRect.left + drawRect.width - boxSize;
    
    float boxY;
    switch (m_verticalAlign) {
        case CheckBoxVerticalAlign::Top:
            boxY = drawRect.top;
            break;
        case CheckBoxVerticalAlign::Bottom:
            boxY = drawRect.top + drawRect.height - boxSize;
            break;
        case CheckBoxVerticalAlign::Center:
        default:
            boxY = drawRect.top + (drawRect.height - boxSize) / 2;
            break;
    }
    
    return {boxX, boxY, boxSize, boxSize};
}

SRect CheckBox::calculateCaptionRect() {
    SRect drawRect = getDrawRect();
    float boxSize = calculateCheckBoxSize();
    float scaledMargin = ConstDef::CHECKBOX_BOX_MARGIN * getScaleXX();
    
    if (m_layout == CheckBoxLayout::TextRight) {
        float captionWidth = drawRect.width - boxSize - scaledMargin;
        return {drawRect.left + boxSize + scaledMargin, drawRect.top, 
                captionWidth, drawRect.height};
    } else {
        float captionWidth = drawRect.width - boxSize - scaledMargin;
        float boxX = drawRect.left + drawRect.width - boxSize;
        return {boxX - scaledMargin - captionWidth, drawRect.top, captionWidth, drawRect.height};
    }
}

void CheckBox::updateCaptionPosition() {
    if (m_caption != nullptr) {
        SRect captionRect = calculateCaptionRect();
        m_caption->setRect(captionRect);
    }
}

void CheckBox::drawCheckBoxFrame(SRect boxRect) {
    SDL_Renderer *renderer = getRenderer();
    if (!renderer) return;
    
    SDL_Color borderColor = getEnable() ? m_boxBorderStateColor.getNormal() : ConstDef::DEFAULT_BORDER_DISABLED_COLOR;
    
    if (!SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a)) {
        return;
    }
    
    float penWidth = 2.0f;
    
    switch (m_style) {
        case CheckBoxStyle::Classic:
        case CheckBoxStyle::Cross: {
            SPoint topLeft(boxRect.left + penWidth / 2, boxRect.top + penWidth / 2);
            SPoint topRight(boxRect.left + boxRect.width - penWidth / 2, boxRect.top + penWidth / 2);
            SPoint bottomRight(boxRect.left + boxRect.width - penWidth / 2, boxRect.top + boxRect.height - penWidth / 2);
            SPoint bottomLeft(boxRect.left + penWidth / 2, boxRect.top + boxRect.height - penWidth / 2);
            
            GraphTool::DrawingContext dc(renderer);
            dc.setPenColor(GraphTool::SColor(borderColor.r / 255.0f, borderColor.g / 255.0f, borderColor.b / 255.0f, borderColor.a / 255.0f));
            dc.setPenWidth(penWidth);
            
            dc.drawLine(topLeft, topRight);
            dc.drawLine(topRight, bottomRight);
            dc.drawLine(bottomRight, bottomLeft);
            dc.drawLine(bottomLeft, topLeft);
            break;
        }
        case CheckBoxStyle::Circle: {
            float centerX = boxRect.left + boxRect.width / 2;
            float centerY = boxRect.top + boxRect.height / 2;
            float radius = (boxRect.width < boxRect.height ? boxRect.width : boxRect.height) / 2 - penWidth / 2;
            
            const int numPoints = 36;
            GraphTool::DrawingContext dc(renderer);
            dc.setPenColor(GraphTool::SColor(borderColor.r / 255.0f, borderColor.g / 255.0f, borderColor.b / 255.0f, borderColor.a / 255.0f));
            dc.setPenWidth(penWidth);
            
            for (int i = 0; i < numPoints; i++) {
                float angle1 = 2.0f * M_PI * i / numPoints;
                float angle2 = 2.0f * M_PI * (i + 1) / numPoints;
                
                float x1 = centerX + radius * cos(angle1);
                float y1 = centerY + radius * sin(angle1);
                float x2 = centerX + radius * cos(angle2);
                float y2 = centerY + radius * sin(angle2);
                
                dc.drawLine(x1, y1, x2, y2);
            }
            break;
        }
    }
}

void CheckBox::drawCheckMark(SRect boxRect) {
    SDL_Renderer *renderer = getRenderer();
    if (!renderer) return;
    
    SDL_Color checkColor = getEnable() ? m_checkStateColor.getNormal() : ConstDef::DEFAULT_TEXT_DISABLED_COLOR;
    
    GraphTool::DrawingContext dc(renderer);
    dc.setPenColor(GraphTool::SColor(checkColor.r / 255.0f, checkColor.g / 255.0f, checkColor.b / 255.0f, checkColor.a / 255.0f));
    dc.setPenWidth(2.5f);
    
    float padding = boxRect.width * 0.2f;
    float startX = boxRect.left + padding;
    float endX = boxRect.left + boxRect.width - padding;
    float startY = boxRect.top + boxRect.height / 2;
    float midX = boxRect.left + boxRect.width * 0.4f;
    float midY = boxRect.top + boxRect.height * 0.7f;
    
    dc.drawLine(startX, startY, midX, midY);
    dc.drawLine(midX, midY, endX, boxRect.top + padding);
}

void CheckBox::drawCrossMark(SRect boxRect) {
    SDL_Renderer *renderer = getRenderer();
    if (!renderer) return;
    
    SDL_Color crossColor = getEnable() ? m_crossStateColor.getNormal() : ConstDef::DEFAULT_TEXT_DISABLED_COLOR;
    
    GraphTool::DrawingContext dc(renderer);
    dc.setPenColor(GraphTool::SColor(crossColor.r / 255.0f, crossColor.g / 255.0f, crossColor.b / 255.0f, crossColor.a / 255.0f));
    dc.setPenWidth(2.5f);
    
    float padding = boxRect.width * 0.2f;
    float startX = boxRect.left + padding;
    float endX = boxRect.left + boxRect.width - padding;
    float startY = boxRect.top + padding;
    float endY = boxRect.top + boxRect.height - padding;
    
    dc.drawLine(startX, startY, endX, endY);
    dc.drawLine(endX, startY, startX, endY);
}

void CheckBox::drawIndeterminateMark(SRect boxRect) {
    SDL_Renderer *renderer = getRenderer();
    if (!renderer) return;
    
    SDL_Color indColor = getEnable() ? m_indeterminateStateColor.getNormal() : ConstDef::DEFAULT_TEXT_DISABLED_COLOR;
    
    GraphTool::DrawingContext dc(renderer);
    dc.setPenColor(GraphTool::SColor(indColor.r / 255.0f, indColor.g / 255.0f, indColor.b / 255.0f, indColor.a / 255.0f));
    dc.setPenWidth(2.5f);
    
    float padding = boxRect.width * 0.25f;
    float lineStartX = boxRect.left + padding;
    float lineEndX = boxRect.left + boxRect.width - padding;
    float lineY = boxRect.top + boxRect.height / 2;
    
    dc.drawLine(lineStartX, lineY, lineEndX, lineY);
}

/*********************************************************for Builder mode**********************************************************/

CheckBoxBuilder::CheckBoxBuilder(Control *parent, SRect rect, float xScale, float yScale):
    m_checkBox(nullptr)
{
    m_checkBox = make_shared<CheckBox>(parent, rect, xScale, yScale);
}

CheckBoxBuilder& CheckBoxBuilder::setStyle(CheckBoxStyle style) {
    m_checkBox->setStyle(style);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setLayout(CheckBoxLayout layout) {
    m_checkBox->setLayout(layout);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setVerticalAlign(CheckBoxVerticalAlign align) {
    m_checkBox->setVerticalAlign(align);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setCheckState(CheckState state) {
    m_checkBox->setCheckState(state);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setSizeRatio(float ratio) {
    m_checkBox->setSizeRatio(ratio);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setCaption(string caption) {
    m_checkBox->setCaption(caption);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setCaptionSize(float size) {
    m_checkBox->setCaptionSize(size);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setFontSize(int fontSize) {
    m_checkBox->setFontSize(fontSize);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setTriStateEnabled(bool enabled) {
    m_checkBox->setTriStateEnabled(enabled);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setOnCheckChanged(CheckBox::OnCheckChangedHandler handler) {
    m_checkBox->setOnCheckChanged(handler);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setCheckColor(SDL_Color color) {
    m_checkBox->setCheckColor(color);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setCrossColor(SDL_Color color) {
    m_checkBox->setCrossColor(color);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setIndeterminateColor(SDL_Color color) {
    m_checkBox->setIndeterminateColor(color);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setBoxBorderColor(SDL_Color color) {
    m_checkBox->setBoxBorderColor(color);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setBackgroundStateColor(StateColor stateColor) {
    m_checkBox->setBackgroundStateColor(stateColor);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setBorderStateColor(StateColor stateColor) {
    m_checkBox->setBorderStateColor(stateColor);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setTextStateColor(StateColor stateColor) {
    m_checkBox->setTextStateColor(stateColor);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setId(int id) {
    m_checkBox->setId(id);
    return *this;
}

CheckBoxBuilder& CheckBoxBuilder::setEnable(bool enable) {
    m_checkBox->setEnable(enable);
    return *this;
}

shared_ptr<CheckBox> CheckBoxBuilder::build(void) {
    return m_checkBox;
}