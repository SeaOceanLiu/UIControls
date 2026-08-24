// ============================================================================
// StatusBar.cpp -- 状态栏控件实现（design/StatusBar_Design.md）
// left/right 两组布局；点击 item 向上弹出共享 MenuPanel；外部点击关闭。
// ============================================================================
#include "StatusBar.h"
#include "Menu.h"
#include "PropertyNames.h"
#include "RenderDevice.h"
#include "TextRenderer.h"

#include <algorithm>
#include <cmath>

using std::string;
using std::vector;
using std::shared_ptr;
using std::function;

StatusBar::StatusBar(Control* parent, const SRect& rect, float xScale, float yScale)
    : ControlImpl(parent, xScale, yScale)
{
    m_ctlType = ControlType::StatusBar;
    m_rect = rect;
    m_itemHeight = rect.height;
    setNormalStateBGColor(SColor(0, 122, 204));   // VSCode 状态栏蓝
}

// ── 数据操作 ──
void StatusBar::addStatusItem(const string& id, const string& text, bool rightAlign) {
    StatusItem item;
    item.id = id;
    item.text = text;
    item.rightAlign = rightAlign;
    m_items.push_back(std::move(item));
    relayout();
}
void StatusBar::updateStatusItemText(const string& id, const string& text) {
    for (auto& item : m_items) {
        if (item.id == id) { item.text = text; return; }
    }
}
void StatusBar::removeStatusItem(const string& id) {
    m_items.erase(std::remove_if(m_items.begin(), m_items.end(),
        [&](const StatusItem& it) { return it.id == id; }), m_items.end());
    relayout();
}
void StatusBar::setStatusItemMenu(const string& id, shared_ptr<MenuPanel> panel) {
    for (auto& item : m_items) {
        if (item.id == id) { item.menuPanel = std::move(panel); return; }
    }
}
bool StatusBar::isPopupOpen() const {
    return m_popupPanel && m_popupPanel->getVisible();
}
void StatusBar::setStatusItemLeadingControl(const string& id, shared_ptr<Control> ctl) {
    for (auto& item : m_items) {
        if (item.id == id) { item.leadingControl = std::move(ctl); relayout(); return; }
    }
}
void StatusBar::setStatusItemOnClick(const string& id, function<void(shared_ptr<StatusItem>)> cb) {
    for (auto& item : m_items) {
        if (item.id == id) { item.onClick = std::move(cb); return; }
    }
}
StatusItem* StatusBar::getStatusItem(const string& id) {
    for (auto& item : m_items) if (item.id == id) return &item;
    return nullptr;
}

void StatusBar::setFontSize(float size) {
    if (size > 0 && m_fontSize != size) { m_fontSize = size; relayout(); }
}
void StatusBar::setItemHeight(float px) {
    if (px > 0 && m_itemHeight != px) { m_itemHeight = px; m_rect.height = px; relayout(); }
}

void StatusBar::ensureFont() {
    if (m_font) return;
    TextRenderer* renderer = getTextRenderer();
    ResourceProvider* provider = getResourceProvider();
    if (!renderer || !provider) return;
    auto it = ConstDef::fontFiles.find(FontName::HarmonyOS_Sans_SC_Regular);
    if (it == ConstDef::fontFiles.end()) return;
    string fontPath = ConstDef::pathPrefix.string() + "/" + it->second;
    auto data = provider->readFile(fontPath);
    if (data && !data->empty()) {
        int scaledSize = static_cast<int>(m_fontSize * getScaleXX());
        m_font = renderer->loadFontFromMemoryWithText(data->data(), data->size(), scaledSize, "W");
    }
}

// ── 布局 ──
void StatusBar::relayout() {
    if (m_items.empty()) return;
    ensureFont();
    float leftX = m_padding;
    const float cy = (m_rect.height - m_itemHeight) / 2.f;
    TextRenderer* renderer = getTextRenderer();

    // 先算左组
    for (auto& item : m_items) {
        if (item.rightAlign) continue;
        float w = m_itemHeight;
        if (item.leadingControl) w += m_fontSize * 1.4f;
        if (renderer && m_font) w += renderer->measureText(m_font.get(), item.text).width;
        else w += item.text.length() * m_fontSize * 0.6f;
        item.hitRect = SRect(leftX, cy, w + m_spacing, m_itemHeight);
        leftX += w + m_spacing;
    }

    // 右组从右向左
    float rightX = m_rect.width - m_padding;
    for (int i = static_cast<int>(m_items.size()) - 1; i >= 0; --i) {
        auto& item = m_items[i];
        if (!item.rightAlign) continue;
        float w = m_itemHeight;
        if (item.leadingControl) w += m_fontSize * 1.4f;
        if (renderer && m_font) w += renderer->measureText(m_font.get(), item.text).width;
        else w += item.text.length() * m_fontSize * 0.6f;
        item.hitRect = SRect(rightX - w, cy, w + m_spacing, m_itemHeight);
        rightX -= w + m_spacing;
    }
}

int StatusBar::hitTestIndex(float x) const {
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        const auto& r = m_items[i].hitRect;
        if (x >= r.left && x < r.left + r.width) return i;
    }
    return -1;
}

// ── 绘制 ──
void StatusBar::draw(void) {
    const bool hadFont = m_font != nullptr;
    ensureFont();
    if (!hadFont && m_font && !m_items.empty()) relayout();   // 字体首次就绪→按实测宽度重排
    ControlImpl::beforeDraw();                 // 背景（蓝）/ 边框
    RenderDevice* dev = getRenderDevice();
    TextRenderer* renderer = getTextRenderer();
    const float ox = m_rect.left, oy = m_rect.top;

    if (dev) {
        // hover 高亮（浅蓝）
        if (m_hoveredItem >= 0 && m_hoveredItem < static_cast<int>(m_items.size())) {
            const auto& r = m_items[m_hoveredItem].hitRect;
            dev->setDrawColor(SColor(36, 142, 222));
            dev->fillRect(SRect(ox + r.left, oy + r.top, r.width, r.height));
        }

        for (const auto& item : m_items) {
            float x = ox + item.hitRect.left + 4.f;
            float iy = oy + item.hitRect.top + (m_itemHeight - m_fontSize * 1.4f) / 2.f;
            if (item.leadingControl) {
                const float isz = m_fontSize * 1.4f;
                item.leadingControl->setRect(SRect(ox + item.hitRect.left + 4.f, iy, isz, isz));
                item.leadingControl->draw();
                x += isz + 4.f;
            }
            if (renderer && m_font) {
                renderer->drawText(m_font.get(), item.text, x,
                                   oy + item.hitRect.top + (m_itemHeight - m_fontSize) / 2.f,
                                   SColor(235, 235, 235));
            }
        }
    }

    ControlImpl::draw();                       // 子控件（弹窗 MenuPanel）
    ControlImpl::afterDraw();
}

// ── 弹窗 ──
void StatusBar::openPopup(int itemIndex) {
    if (itemIndex < 0 || itemIndex >= static_cast<int>(m_items.size())) return;
    auto& item = m_items[itemIndex];
    if (!item.menuPanel) return;

    // 直接挂载并显示该 item 自带的菜单面板（共享复用，免复制菜单项）
    if (m_popupPanel != item.menuPanel) {
        if (m_popupPanel) removeControl(m_popupPanel);
        m_popupPanel = item.menuPanel;
        addControl(m_popupPanel);
        m_popupPanel->setContext(getContext());
        m_popupPanel->create();
    }

    // 菜单语义：点击任意项关闭弹窗。MenuItem::handleEvent 仅在 onClick 非空时
    // 走 closeMenuChain()，故为无回调的项补 no-op onClick（用户回调不受影响）。
    for (auto& mi : item.menuPanel->getItems()) {
        if (mi && !mi->getOnClick())
            mi->setOnClick([](shared_ptr<MenuItem>) {});
    }

    item.menuPanel->recalculateSize();
    const float panelH = item.menuPanel->getRect().height;

    // 向上定位：面板底缘贴 item 顶缘。
    // 坐标系注意：m_popupPanel 是 bar 的子控件，getDrawRect() 会叠加父偏移
    // （ControlBase.cpp getDrawRect），故此处必须用【bar 相对坐标】。
    float localY = item.hitRect.top - panelH;
    // 顶部越界钳制（不出窗口顶部：相对坐标 < -bar.top 等价绝对越界）
    if (m_rect.top + localY < 0) localY = -m_rect.top;

    m_popupPanel->setPosition(item.hitRect.left, localY);
    m_popupPanel->recalculateSize();
    m_popupPanel->show();
}

void StatusBar::closePopup() {
    if (m_popupPanel) m_popupPanel->hide();
}

// ── 事件 ──
bool StatusBar::handleEvent(shared_ptr<Event> event) {
    if (!m_enable || !m_visible) return false;

    if (event->m_type == EventType::MouseMove) {
        const float x = event->mousePos.x - m_rect.left;
        m_hoveredItem = hitTestIndex(x);
    }

    if (event->m_type == EventType::MouseDown &&
        event->mouseButton.button == MouseButton::Left) {
        const float x = event->mouseButton.x - m_rect.left;
        const int idx = hitTestIndex(x);
        if (idx >= 0) {
            auto& item = m_items[idx];
            if (item.menuPanel) {
                openPopup(idx);
            } else if (item.onClick) {
                auto self = shared_from_this();
                item.onClick(shared_ptr<StatusItem>(&item, [](StatusItem*){}));
            }
            return true;
        } else {
            // 点击 item 外部 → 关闭弹窗
            closePopup();
        }
    }

    return ControlImpl::handleEvent(event);
}

void StatusBar::setRect(SRect rect) {
    ControlImpl::setRect(rect);
    relayout();
}

// ── 属性系统 override ──
int StatusBar::setFloatProperty(const char* prop, float value) {
    if (strcmp(prop, PropertyNames::kFontSize) == 0)      { setFontSize(value); return 1; }
    if (strcmp(prop, PropertyNames::kItemHeight) == 0)     { setItemHeight(value); return 1; }
    return ControlImpl::setFloatProperty(prop, value);
}
int StatusBar::getFloatProperty(const char* prop, float& out) {
    if (strcmp(prop, PropertyNames::kFontSize) == 0)  { out = m_fontSize;  return 1; }
    if (strcmp(prop, PropertyNames::kItemHeight) == 0) { out = m_itemHeight; return 1; }
    return ControlImpl::getFloatProperty(prop, out);
}
