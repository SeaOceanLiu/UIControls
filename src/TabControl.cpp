// ============================================================================
// TabControl.cpp -- 选项卡控件（四方向页签条 + 内容区，一体自绘）
// ============================================================================
#include "TabControl.h"
#include "Bench.h"
#include "MainWindow.h"
#include "RenderDevice.h"
#include "TextRenderer.h"
#include "ResourceProvider.h"
#include "ConstDef.h"
#include "PropertyNames.h"
#include "EventTypes.h"

TabControl::TabControl(Control* parent, const SRect& rect, float xScale, float yScale)
    : ControlImpl(parent, xScale, yScale)
{
    m_ctlType = ControlType::TabControl;
    m_rect = rect;
    setFocusable(true);
    relayout();
}

void TabControl::ensureFont() {
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

int TabControl::addTab(const string& title, shared_ptr<Control> page) {
    TabPage tp;
    tp.title = title;
    tp.page = page;
    m_tabs.push_back(std::move(tp));
    int idx = static_cast<int>(m_tabs.size()) - 1;
    if (page) {
        page->setParent(this);
        addControl(page);
    }
    relayout();
    if (m_currentIndex < 0) setCurrentIndex(0);
    return idx;
}

void TabControl::insertTab(int index, const string& title, shared_ptr<Control> page) {
    TabPage tp;
    tp.title = title;
    tp.page = page;
    if (index < 0) index = 0;
    if (index > static_cast<int>(m_tabs.size())) index = static_cast<int>(m_tabs.size());
    m_tabs.insert(m_tabs.begin() + index, std::move(tp));
    if (page) {
        page->setParent(this);
        addControl(page);
    }
    if (m_currentIndex >= index) ++m_currentIndex;
    relayout();
    if (m_currentIndex < 0) setCurrentIndex(0);
}

void TabControl::removeTab(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;
    if (m_tabs[index].page) removeControl(m_tabs[index].page);
    m_tabs.erase(m_tabs.begin() + index);
    if (m_currentIndex > index) --m_currentIndex;
    else if (m_currentIndex == index) {
        m_currentIndex = m_tabs.empty() ? -1 : std::min(m_currentIndex, static_cast<int>(m_tabs.size()) - 1);
    }
    relayout();
    applyCurrentPage();
}

void TabControl::setCurrentIndex(int index) {
    if (m_tabs.empty()) { m_currentIndex = -1; return; }
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;
    if (index == m_currentIndex) return;
    m_currentIndex = index;
    applyCurrentPage();
    if (m_onTabChange) m_onTabChange(std::static_pointer_cast<TabControl>(shared_from_this()), m_currentIndex);
}

void TabControl::applyCurrentPage() {
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        auto& tp = m_tabs[i];
        if (!tp.page) continue;
        tp.page->setRect(SRect(m_contentRect.left, m_contentRect.top,
                               m_contentRect.width, m_contentRect.height));
        tp.page->setVisible(i == m_currentIndex);
    }
}

void TabControl::setTabText(int index, const string& title) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;
    m_tabs[index].title = title;
    relayout();
}

void TabControl::setTabPage(int index, shared_ptr<Control> page) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;
    if (m_tabs[index].page) removeControl(m_tabs[index].page);
    m_tabs[index].page = page;
    if (page) {
        page->setParent(this);
        addControl(page);
    }
    applyCurrentPage();
}

void TabControl::setTabLeadingControl(int index, shared_ptr<Control> ctl) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;
    m_tabs[index].leadingControl = ctl;
    relayout();
}

void TabControl::setPosition(TabPosition pos) {
    m_position = pos;
    relayout();
}

void TabControl::setFontSize(float px) {
    m_fontSize = px;
    m_font.reset();
    ensureFont();
    relayout();
}

void TabControl::relayout() {
    const float barT = m_fontSize * 1.4f + 2.f * m_padding;  // Top/Bottom 厚度
    const float fullW = m_rect.width, fullH = m_rect.height;
    SRect bar, content;

    if (m_position == TabPosition::Top) {
        bar = SRect(0, 0, fullW, barT);
        content = SRect(0, barT, fullW, fullH - barT);
    } else if (m_position == TabPosition::Bottom) {
        bar = SRect(0, fullH - barT, fullW, barT);
        content = SRect(0, 0, fullW, fullH - barT);
    } else if (m_position == TabPosition::Left) {
        bar = SRect(0, 0, 0, fullH);     // 宽在下方按最大标题计算
        content = SRect(0, 0, fullW, fullH);
    } else { // Right
        bar = SRect(fullW, 0, 0, fullH);
        content = SRect(0, 0, fullW, fullH);
    }

    // 左/右侧：按最大标题宽定 bar 宽
    if (m_position == TabPosition::Left || m_position == TabPosition::Right) {
        float maxW = 40.f;
        ensureFont();
        TextRenderer* r = getTextRenderer();
        for (auto& tp : m_tabs) {
            float w = r && m_font ? r->measureText(m_font.get(), tp.title).width : tp.title.length() * m_fontSize * 0.6f;
            w += 2.f * m_padding;
            if (tp.leadingControl) w += m_fontSize * 1.4f + 4.f;
            if (w > maxW) maxW = w;
        }
        const float tabH = m_fontSize * 1.4f + 2.f * m_padding;
        if (m_position == TabPosition::Left) {
            bar = SRect(0, 0, maxW, fullH);
            content = SRect(maxW, 0, fullW - maxW, fullH);
        } else {
            bar = SRect(fullW - maxW, 0, maxW, fullH);
            content = SRect(0, 0, fullW - maxW, fullH);
        }
        // 页签竖排
        float y = m_padding;
        for (auto& tp : m_tabs) {
            tp.tabRect = SRect(bar.left, y, bar.width, tabH);
            y += tabH;
        }
    } else {
        // 上/下：页签横排
        float x = m_padding;
        ensureFont();
        TextRenderer* r = getTextRenderer();
        for (auto& tp : m_tabs) {
            float w = r && m_font ? r->measureText(m_font.get(), tp.title).width : tp.title.length() * m_fontSize * 0.6f;
            w += 2.f * m_padding;
            if (tp.leadingControl) w += m_fontSize * 1.4f + 4.f;
            tp.tabRect = SRect(x, bar.top, w, bar.height);
            x += w;
        }
    }

    m_contentRect = content;
    applyCurrentPage();
}

int TabControl::hitTestTab(float x, float y) const {
    const float ox = m_rect.left, oy = m_rect.top;
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        const auto& r = m_tabs[i].tabRect;
        if (x >= ox + r.left && x < ox + r.left + r.width &&
            y >= oy + r.top && y < oy + r.top + r.height)
            return i;
    }
    return -1;
}

void TabControl::drawTabBar() {
    RenderDevice* dev = getRenderDevice();
    if (!dev) return;
    const float ox = m_rect.left, oy = m_rect.top;
    ensureFont();
    TextRenderer* r = getTextRenderer();

    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        const auto& tp = m_tabs[i];
        const SRect tr = tp.tabRect;
        const bool sel = (i == m_currentIndex);
        const bool hov = (i == m_hoveredTab);

        // 页签底色
        if (sel) dev->setDrawColor(SColor(45, 45, 52));
        else if (hov) dev->setDrawColor(SColor(60, 60, 70));
        else dev->setDrawColor(SColor(37, 37, 42));
        dev->fillRect(SRect(ox + tr.left, oy + tr.top, tr.width, tr.height));

        // 选中指示条（贴页签条内侧）
        if (sel) {
            dev->setDrawColor(SColor(0, 122, 204));
            if (m_position == TabPosition::Top)
                dev->fillRect(SRect(ox + tr.left, oy + tr.top + tr.height - 3.f, tr.width, 3.f));
            else if (m_position == TabPosition::Bottom)
                dev->fillRect(SRect(ox + tr.left, oy + tr.top, tr.width, 3.f));
            else if (m_position == TabPosition::Left)
                dev->fillRect(SRect(ox + tr.left + tr.width - 3.f, oy + tr.top, 3.f, tr.height));
            else // Right
                dev->fillRect(SRect(ox + tr.left, oy + tr.top, 3.f, tr.height));
        }

        // 图标
        float tx = ox + tr.left + m_padding;
        if (tp.leadingControl) {
            float isz = m_fontSize * 1.4f;
            tp.leadingControl->setRect(SRect(tx, oy + tr.top + (tr.height - isz) / 2.f, isz, isz));
            tp.leadingControl->draw();
            tx += isz + 4.f;
        }
        // 文字
        if (r && m_font) {
            r->drawText(m_font.get(), tp.title, tx,
                        oy + tr.top + (tr.height - m_fontSize) / 2.f,
                        sel ? SColor(235, 235, 235) : SColor(180, 180, 185));
        }
    }
}

void TabControl::draw(void) {
    ControlImpl::beforeDraw();   // 整体背景 + 边框
    RenderDevice* dev = getRenderDevice();
    if (dev) {
        // 内容区底色（浅色）
        dev->setDrawColor(SColor(50, 50, 56));
        dev->fillRect(SRect(m_rect.left + m_contentRect.left, m_rect.top + m_contentRect.top,
                            m_contentRect.width, m_contentRect.height));
    }
    drawTabBar();
    ControlImpl::draw();         // 子控件（当前页）
    ControlImpl::afterDraw();
}

bool TabControl::handleEvent(shared_ptr<Event> event) {
    if (!m_enable || !m_visible) return false;

    if (event->m_type == EventType::MouseMove) {
        m_hoveredTab = hitTestTab(event->mousePos.x, event->mousePos.y);
        return ControlImpl::handleEvent(event);
    }

    if (event->m_type == EventType::MouseDown &&
        event->mouseButton.button == MouseButton::Left) {
        int idx = hitTestTab(event->mouseButton.x, event->mouseButton.y);
        if (idx >= 0) {
            setCurrentIndex(idx);
            return true;
        }
    }

    if (event->m_type == EventType::KeyDown) {
        int n = static_cast<int>(m_tabs.size());
        bool vertical = (m_position == TabPosition::Left || m_position == TabPosition::Right);
        auto nav = [&](int delta) {
            if (n == 0) return;
            int cur = m_currentIndex < 0 ? 0 : m_currentIndex;
            int next = (cur + delta + n) % n;
            setCurrentIndex(next);
        };
        switch (event->keyEvent.keycode) {
            case KeyCode::Left:  if (!vertical) nav(-1); break;
            case KeyCode::Right: if (!vertical) nav(1);  break;
            case KeyCode::Up:    if (vertical)  nav(-1); break;
            case KeyCode::Down:  if (vertical)  nav(1);  break;
            case KeyCode::Home:  setCurrentIndex(0); break;
            case KeyCode::End:   if (n) setCurrentIndex(n - 1); break;
            default: break;
        }
    }

    return ControlImpl::handleEvent(event);
}

void TabControl::setRect(SRect rect) {
    ControlImpl::setRect(rect);
    relayout();
}

int TabControl::setEnumProperty(const char* prop, const char* value) {
    if (strcmp(prop, PropertyNames::kJsonPosition) == 0) {
        if (strcmp(value, "top") == 0) setPosition(TabPosition::Top);
        else if (strcmp(value, "bottom") == 0) setPosition(TabPosition::Bottom);
        else if (strcmp(value, "left") == 0) setPosition(TabPosition::Left);
        else if (strcmp(value, "right") == 0) setPosition(TabPosition::Right);
        else return 0;
        return 1;
    }
    return ControlImpl::setEnumProperty(prop, value);
}
int TabControl::getEnumProperty(const char* prop, const char*& out) {
    if (strcmp(prop, PropertyNames::kJsonPosition) == 0) {
        switch (m_position) {
            case TabPosition::Top:    out = "top"; break;
            case TabPosition::Bottom: out = "bottom"; break;
            case TabPosition::Left:   out = "left"; break;
            case TabPosition::Right:  out = "right"; break;
        }
        return 1;
    }
    return ControlImpl::getEnumProperty(prop, out);
}
int TabControl::setIntProperty(const char* prop, int value) {
    if (strcmp(prop, PropertyNames::kJsonCurrentIndex) == 0) { setCurrentIndex(value); return 1; }
    return ControlImpl::setIntProperty(prop, value);
}
int TabControl::getIntProperty(const char* prop, int& out) {
    if (strcmp(prop, PropertyNames::kJsonCurrentIndex) == 0) { out = m_currentIndex; return 1; }
    return ControlImpl::getIntProperty(prop, out);
}
int TabControl::setFloatProperty(const char* prop, float value) {
    if (strcmp(prop, PropertyNames::kFontSize) == 0) { setFontSize(value); return 1; }
    return ControlImpl::setFloatProperty(prop, value);
}
int TabControl::getFloatProperty(const char* prop, float& out) {
    if (strcmp(prop, PropertyNames::kFontSize) == 0) { out = m_fontSize; return 1; }
    return ControlImpl::getFloatProperty(prop, out);
}
