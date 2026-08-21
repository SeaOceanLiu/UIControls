// LeadingControlSlot.cpp — 行内前置控件支持组件实现（见 LeadingControlSlot.h）
#include "LeadingControlSlot.h"
#include "Actor.h"
#include "ControlBase.h"
#include "EventQueue.h"
#include "PropertyNames.h"

float LeadingControlSlot::naturalRatio(Control* ctl) {
    if (!ctl) return 1.0f;
    if (auto* actor = dynamic_cast<Actor*>(ctl)) {
        Texture* tex = actor->getTexture();
        if (tex && tex->height() > 0)
            return (float)tex->width() / tex->height();
    }
    float ow = ctl->getRect().width;
    float oh = ctl->getRect().height;
    if (oh > 0 && ow > 0) return ow / oh;
    return 1.0f;
}

float LeadingControlSlot::getSlotWidth(float fontH) const {
    return getSlotHeight(fontH) * naturalRatio(m_control.get());
}

float LeadingControlSlot::verticalFactor() const {
    return verticalFactor(m_align);
}

float LeadingControlSlot::verticalFactor(AlignmentMode align) {
    switch (align) {
        case AlignmentMode::AM_TOP_LEFT:
        case AlignmentMode::AM_TOP_CENTER:
        case AlignmentMode::AM_TOP_RIGHT:      return 0.0f;
        case AlignmentMode::AM_BOTTOM_LEFT:
        case AlignmentMode::AM_BOTTOM_CENTER:
        case AlignmentMode::AM_BOTTOM_RIGHT:   return 1.0f;
        default:                               return 0.5f;
    }
}

SRect LeadingControlSlot::layout(float rowTopPx, float rowHPx, float slotStartXPx,
                                 float crLeftPx, float crTopPx, float scaleX, float scaleY,
                                 float fontH) const {
    float slotH = getSlotHeight(fontH);
    SRect r;
    r.left = (slotStartXPx - crLeftPx) / scaleX;
    float slotTopPx = rowTopPx + (rowHPx - slotH * scaleY) * verticalFactor();
    r.top = (slotTopPx - crTopPx) / scaleY;
    r.width = getSlotWidth(fontH);
    r.height = slotH;
    return r;
}

float LeadingControlSlot::textStartX(float slotStartXPx, float scaleX, float fontH) const {
    return slotStartXPx + getSlotWidth(fontH) * scaleX + m_gap * scaleX;
}

void LeadingControlSlot::attachTo(Control* host) {
    if (m_attached || !m_control || !host) return;
    if (m_control->getParent() != host) host->addControl(m_control);
    m_attached = true;
}

void LeadingControlSlot::detachFrom(Control* host) {
    if (!m_attached || !m_control || !host) return;
    if (m_control->getParent() == host) host->removeControl(m_control);
    m_attached = false;
}

bool LeadingControlSlot::containsPoint(float mx, float my) const {
    return m_control && m_control->isContainsPoint(mx, my);
}

bool LeadingControlSlot::handleEvent(const std::shared_ptr<Event>& ev) const {
    return m_control && m_control->handleEvent(ev);
}

const char* LeadingControlSlot::alignmentString() const {
    switch (m_align) {
        case AlignmentMode::AM_TOP_LEFT:      return PropertyNames::kAlignLowerTopLeft;
        case AlignmentMode::AM_TOP_CENTER:    return PropertyNames::kAlignLowerTopCenter;
        case AlignmentMode::AM_TOP_RIGHT:     return PropertyNames::kAlignLowerTopRight;
        case AlignmentMode::AM_MID_LEFT:      return PropertyNames::kAlignLowerMidLeft;
        case AlignmentMode::AM_CENTER:        return PropertyNames::kAlignLowerCenter;
        case AlignmentMode::AM_MID_RIGHT:     return PropertyNames::kAlignLowerMidRight;
        case AlignmentMode::AM_BOTTOM_LEFT:   return PropertyNames::kAlignLowerBottomLeft;
        case AlignmentMode::AM_BOTTOM_CENTER: return PropertyNames::kAlignLowerBottomCenter;
        case AlignmentMode::AM_BOTTOM_RIGHT:  return PropertyNames::kAlignLowerBottomRight;
    }
    return PropertyNames::kAlignLowerMidLeft;
}

bool LeadingControlSlot::parseAlignmentString(const char* value, AlignmentMode& out) {
    if (!value) return false;
    struct Pair { const char* name; AlignmentMode mode; };
    static const Pair kMap[] = {
        {PropertyNames::kAlignLowerTopLeft,      AlignmentMode::AM_TOP_LEFT},
        {PropertyNames::kAlignLowerTopCenter,    AlignmentMode::AM_TOP_CENTER},
        {PropertyNames::kAlignLowerTopRight,     AlignmentMode::AM_TOP_RIGHT},
        {PropertyNames::kAlignLowerMidLeft,      AlignmentMode::AM_MID_LEFT},
        {PropertyNames::kAlignLowerCenter,       AlignmentMode::AM_CENTER},
        {PropertyNames::kAlignLowerMidRight,     AlignmentMode::AM_MID_RIGHT},
        {PropertyNames::kAlignLowerBottomLeft,   AlignmentMode::AM_BOTTOM_LEFT},
        {PropertyNames::kAlignLowerBottomCenter, AlignmentMode::AM_BOTTOM_CENTER},
        {PropertyNames::kAlignLowerBottomRight,  AlignmentMode::AM_BOTTOM_RIGHT},
    };
    for (const auto& p : kMap) {
        if (strcmp(p.name, value) == 0) { out = p.mode; return true; }
    }
    return false;
}