#include "Actor.h"
#include "Surface.h"
#include "PlatformUtils.h"
#include "PropertyNames.h"
#include <cstring>

Actor::Actor(Control *parent, float xScale, float yScale):
    Material(parent, xScale, yScale),
    m_matchParentRect(false),
    m_scaleType(ScaleType::STRETCH)
{
    setParent(parent);
}

Actor::Actor(Control *parent, bool matchParentRect, float xScale, float yScale):
    Material(parent, xScale, yScale),
    m_matchParentRect(matchParentRect),
    m_scaleType(ScaleType::STRETCH)
{
    // setParent(parent);
}

Actor::Actor(Control *parent, fs::path filePath, bool matchParentRect, float xScale, float yScale):
    Material(parent, filePath, xScale, yScale),
    m_matchParentRect(matchParentRect),
    m_scaleType(ScaleType::STRETCH)
{
    setParent(parent);
    loadFromFile(filePath);
}

Actor::Actor(Control *parent, string resourceId, bool matchParentRect, float xScale, float yScale):
    Material(parent, resourceId, xScale, yScale),
    m_matchParentRect(matchParentRect),
    m_scaleType(ScaleType::STRETCH)
{
    setParent(parent);
    loadFromResource(resourceId);
}

Actor::Actor(const Actor& other):
    Material(other),
    m_matchParentRect(other.m_matchParentRect),
    m_scaleType(other.m_scaleType),
    m_explicitSize(other.m_explicitSize)
{
}

Actor& Actor::operator=(const Actor& other) {
    Material::operator=(other);
    m_matchParentRect = other.m_matchParentRect;
    m_scaleType = other.m_scaleType;
    m_explicitSize = other.m_explicitSize;
    return *this;
}

void Actor::create() {
    if (m_isCreated) return;
    if (GET_CONTEXT == nullptr) return;  // 未挂入实例上下文：延迟加载
    if (!m_resourceId.empty()) {
        loadFromResource(m_resourceId);
    } else if (!m_filePath.empty()) {
        loadFromFile(m_filePath);
    }
    ControlImpl::create();
}

void Actor::loadFromFile(fs::path filePath) {
    m_filePath = filePath;
    if (GET_CONTEXT == nullptr) return;  // 两阶段：挂树后由 create() 加载
    if (filePath.is_relative()) {
        filePath = fs::path(Platform::GetBasePath()) / filePath;
    }
    m_surface = Surface::loadFromFile(filePath.string());
    if (m_surface) {
        loadTextureFromSurface(m_surface.get());
        return;
    }

    // Fallback: try RenderDevice::createTextureFromFile directly
    // (used in fromsource / bridge paths where Surface factories may not be registered)
    auto* device = getRenderDevice();
    if (device) {
        m_texture = device->createTextureFromFile(filePath.string());
        if (m_texture) {
            // 显式 rect 优先：仅当未显式指定尺寸时覆盖为纹理自然尺寸（换图后跟随新图）
            if (m_matchParentRect && getParent() != nullptr) {
                m_rect.width = getParent()->getRect().width;
                m_rect.height = getParent()->getRect().height;
            } else if (!m_explicitSize) {
                m_rect.width = (float)m_texture->width();
                m_rect.height = (float)m_texture->height();
            }
            return;
        }
    }

    Platform::Log("Actor::loadFromFile failed for '%s'\n", filePath.string().c_str());
}

void Actor::loadFromResource(string resourceId) {
    m_resourceId = resourceId;
    if (GET_CONTEXT == nullptr) return;  // 两阶段：挂树后由 create() 加载
    ResourceProvider* provider = getResourceProvider();
    if (provider == nullptr) {
        Platform::Log("Actor::loadFromResource: No resource provider\n");
        return;
    }

    shared_ptr<vector<char>> imageData = provider->readFile(resourceId);
    if (imageData == nullptr || imageData->empty()) {
        Platform::Log("Actor::loadFromResource: '%s' not found\n", resourceId.c_str());
        return;
    }

    m_surface = Surface::loadFromMemory(imageData->data(), imageData->size());
    if (!m_surface) {
        Platform::Log("Actor::loadFromResource Error\n");
        return;
    }

    loadTextureFromSurface(m_surface.get());
}

void Actor::loadTextureFromSurface(Surface* surface) {
    // 显式 rect 优先：仅当未显式指定尺寸时覆盖为纹理自然尺寸（换图后跟随新图）
    if (m_matchParentRect && getParent() != nullptr) {
        // create() 时可能尚未挂树（setParent 尚未调用），尺寸由 setParent 修正
        m_rect.width = getParent()->getRect().width;
        m_rect.height = getParent()->getRect().height;
    } else if (!m_explicitSize) {
        m_rect.width = (float)surface->width();
        m_rect.height = (float)surface->height();
    }

    m_texture = surface->createTexture(getRenderDevice());
    if (!m_texture) {
        Platform::Log("Surface::createTexture failed\n");
        return;
    }
}

void Actor::setParent(Control *parent){
    Material::setParent(parent);

    if (m_matchParentRect && getParent() != nullptr) {
        m_rect.width = getParent()->getRect().width;
        m_rect.height = getParent()->getRect().height;

        if (m_surface && !m_texture) {
            m_texture = m_surface->createTexture(getRenderDevice());
            if (!m_texture) {
                Platform::Log("Surface::createTexture failed\n");
                return;
            }
        }
    }
}

void Actor::setRect(SRect rect) {
    if (rect.width > 0 || rect.height > 0) m_explicitSize = true;
    ControlImpl::setRect(rect);
}

void Actor::draw(float posx, float posy, Uint8 alpha) {
    inheritRenderer();
    if (!m_texture) return;

    SRect targetRect = getRect();
    targetRect.left = posx - m_anchorPoint.x;
    targetRect.top = posy - m_anchorPoint.y;

    SRect drawRect = mapToDrawRect(targetRect);

    m_texture->setBlendMode(BlendMode::Blend);
    m_texture->setAlphaMod(alpha);

    float texW = (float)m_texture->width();
    float texH = (float)m_texture->height();
    if (texW <= 0 || texH <= 0) return;

    if (m_scaleType == ScaleType::STRETCH) {
        getRenderDevice()->drawTexture(m_texture.get(), nullptr, &drawRect);
        return;
    }

    switch (m_scaleType) {
        case ScaleType::FIT_CENTER: {
            float scale = min(drawRect.width / texW, drawRect.height / texH);
            float w = texW * scale;
            float h = texH * scale;
            SRect fitRect(
                drawRect.left + (drawRect.width - w) * 0.5f,
                drawRect.top + (drawRect.height - h) * 0.5f,
                w, h
            );
            getRenderDevice()->drawTexture(m_texture.get(), nullptr, &fitRect);
            break;
        }
        case ScaleType::CENTER_CROP: {
            float scale = max(drawRect.width / texW, drawRect.height / texH);
            float cropW = drawRect.width / scale;
            float cropH = drawRect.height / scale;
            SRect srcRect(
                (texW - cropW) * 0.5f,
                (texH - cropH) * 0.5f,
                cropW, cropH
            );
            getRenderDevice()->drawTexture(m_texture.get(), &srcRect, &drawRect);
            break;
        }
        case ScaleType::NONE: {
            SRect naturalRect(
                drawRect.left + (drawRect.width - texW) * 0.5f,
                drawRect.top + (drawRect.height - texH) * 0.5f,
                texW, texH
            );
            getRenderDevice()->drawTexture(m_texture.get(), nullptr, &naturalRect);
            break;
        }
        default:
            break;
    }
}

ActorBuilder::ActorBuilder(Control *parent, float xScale, float yScale):
    m_actor(nullptr)
{
    m_actor = make_shared<Actor>(parent, xScale, yScale);
}
ActorBuilder& ActorBuilder::loadFromFile(fs::path filePath){
    m_actor->loadFromFile(filePath);
    return *this;
}
ActorBuilder& ActorBuilder::setMatchParentRect(bool matchParentRect){
    m_actor->setMatchParentRect(matchParentRect);
    return *this;
}
ActorBuilder& ActorBuilder::setScaleType(ScaleType type){
    m_actor->m_scaleType = type;
    return *this;
}
shared_ptr<Actor> ActorBuilder::build(void){
    return m_actor;
}

void Actor::draw(void) {
    draw(m_rect.left + m_anchorPoint.x, m_rect.top + m_anchorPoint.y, m_alpha);
}

// ── 属性系统（C ABI 分发，惯例同 ProgressBar.cpp:316-341） ──

int Actor::setStringProperty(const char* prop, const char* value) {
    if (strcmp(prop, PropertyNames::kImage) == 0) { loadFromFile(fs::path(value)); return 1; }
    if (strcmp(prop, PropertyNames::kImageResource) == 0) { loadFromResource(value); return 1; }
    return ControlImpl::setStringProperty(prop, value);
}

int Actor::setBoolProperty(const char* prop, int value) {
    if (strcmp(prop, PropertyNames::kMatchParentRect) == 0) { setMatchParentRect(value != 0); return 1; }
    return ControlImpl::setBoolProperty(prop, value);
}

int Actor::setIntProperty(const char* prop, int value) {
    if (strcmp(prop, PropertyNames::kAlpha) == 0) { setAlpha((uint8_t)value); return 1; }
    return ControlImpl::setIntProperty(prop, value);
}

int Actor::setEnumProperty(const char* prop, const char* value) {
    if (strcmp(prop, PropertyNames::kScaleType) == 0) {
        if (_stricmp(value, PropertyNames::kScaleTypeStretch) == 0)     { setScaleType(ScaleType::STRETCH);     return 1; }
        if (_stricmp(value, PropertyNames::kScaleTypeFitCenter) == 0)  { setScaleType(ScaleType::FIT_CENTER);  return 1; }
        if (_stricmp(value, PropertyNames::kScaleTypeCenterCrop) == 0) { setScaleType(ScaleType::CENTER_CROP); return 1; }
        if (_stricmp(value, PropertyNames::kScaleTypeNone) == 0)        { setScaleType(ScaleType::NONE);        return 1; }
        return 0;
    }
    if (strcmp(prop, PropertyNames::kAnchor) == 0) {
        if (_stricmp(value, PropertyNames::kAlignLowerTopLeft) == 0)      { setAnchorPoint(AnchorType::AT_TOP_LEFT);      return 1; }
        if (_stricmp(value, PropertyNames::kAlignLowerMidLeft) == 0)      { setAnchorPoint(AnchorType::AT_MID_LEFT);      return 1; }
        if (_stricmp(value, PropertyNames::kAlignLowerBottomLeft) == 0)   { setAnchorPoint(AnchorType::AT_BOTTOM_LEFT);   return 1; }
        if (_stricmp(value, PropertyNames::kAlignLowerTopRight) == 0)     { setAnchorPoint(AnchorType::AT_TOP_RIGHT);     return 1; }
        if (_stricmp(value, PropertyNames::kAlignLowerMidRight) == 0)     { setAnchorPoint(AnchorType::AT_MID_RIGHT);     return 1; }
        if (_stricmp(value, PropertyNames::kAlignLowerBottomRight) == 0)  { setAnchorPoint(AnchorType::AT_BOTTOM_RIGHT);  return 1; }
        if (_stricmp(value, PropertyNames::kAlignLowerTopCenter) == 0)    { setAnchorPoint(AnchorType::AT_TOP_CENTER);    return 1; }
        if (_stricmp(value, PropertyNames::kAlignLowerCenter) == 0)        { setAnchorPoint(AnchorType::AT_CENTER);        return 1; }
        if (_stricmp(value, PropertyNames::kAlignLowerBottomCenter) == 0) { setAnchorPoint(AnchorType::AT_BOTTOM_CENTER); return 1; }
        return 0;
    }
    return ControlImpl::setEnumProperty(prop, value);
}

int Actor::getEnumProperty(const char* prop, const char*& out) {
    if (strcmp(prop, PropertyNames::kScaleType) == 0) {
        switch (m_scaleType) {
            case ScaleType::STRETCH:     out = PropertyNames::kScaleTypeStretch;     break;
            case ScaleType::FIT_CENTER:  out = PropertyNames::kScaleTypeFitCenter;  break;
            case ScaleType::CENTER_CROP: out = PropertyNames::kScaleTypeCenterCrop; break;
            case ScaleType::NONE:        out = PropertyNames::kScaleTypeNone;        break;
            default:                     out = PropertyNames::kScaleTypeStretch;     break;
        }
        return 1;
    }
    if (strcmp(prop, PropertyNames::kAnchor) == 0) {
        switch (m_anchorType) {
            case AnchorType::AT_TOP_LEFT:      out = PropertyNames::kAlignLowerTopLeft;      break;
            case AnchorType::AT_MID_LEFT:      out = PropertyNames::kAlignLowerMidLeft;      break;
            case AnchorType::AT_BOTTOM_LEFT:   out = PropertyNames::kAlignLowerBottomLeft;   break;
            case AnchorType::AT_TOP_RIGHT:     out = PropertyNames::kAlignLowerTopRight;     break;
            case AnchorType::AT_MID_RIGHT:     out = PropertyNames::kAlignLowerMidRight;     break;
            case AnchorType::AT_BOTTOM_RIGHT:  out = PropertyNames::kAlignLowerBottomRight;  break;
            case AnchorType::AT_TOP_CENTER:    out = PropertyNames::kAlignLowerTopCenter;    break;
            case AnchorType::AT_CENTER:        out = PropertyNames::kAlignLowerCenter;        break;
            case AnchorType::AT_BOTTOM_CENTER: out = PropertyNames::kAlignLowerBottomCenter; break;
            default:                           out = PropertyNames::kAlignLowerTopLeft;      break;
        }
        return 1;
    }
    return ControlImpl::getEnumProperty(prop, out);
}

int Actor::getBoolProperty(const char* prop, int& out) {
    if (strcmp(prop, PropertyNames::kMatchParentRect) == 0) { out = m_matchParentRect ? 1 : 0; return 1; }
    return ControlImpl::getBoolProperty(prop, out);
}

int Actor::getIntProperty(const char* prop, int& out) {
    if (strcmp(prop, PropertyNames::kAlpha) == 0) { out = (int)m_alpha; return 1; }
    return ControlImpl::getIntProperty(prop, out);
}
