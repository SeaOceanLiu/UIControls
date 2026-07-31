// 由AI(GLM 5.1)生成，可能不完整或有错误，请自行检查和修改
// Menu.cpp - VSCode风格菜单控件实现
// 重构说明：去除内嵌 Label 控件，文本直接经 TextRenderer 绘制（同 TreeView 模式），
// 字体/尺寸状态为实例成员，hover 统一由 MenuPanel 管理。

#include "Menu.h"
#include "PropertyNames.h"

// ==================== VSCode Dark主题颜色 ====================
namespace MenuColors {
    // 菜单栏
    constexpr SColor BAR_BG(60, 60, 60, 255);
    constexpr SColor BAR_TEXT(204, 204, 204, 255);
    constexpr SColor BAR_HOVER_BG(80, 80, 80, 255);
    constexpr SColor BAR_ACTIVE_BG(9, 71, 113, 255);

    // 下拉面板
    constexpr SColor PANEL_BG(37, 37, 38, 255);
    constexpr SColor PANEL_BORDER(69, 69, 69, 255);
    constexpr SColor PANEL_SHADOW(0, 0, 0, 102);

    // 菜单项
    constexpr SColor ITEM_TEXT(204, 204, 204, 255);
    constexpr SColor ITEM_HOVER_BG(9, 71, 113, 255);
    constexpr SColor ITEM_DISABLED(90, 90, 90, 255);
    constexpr SColor SHORTCUT_TEXT(133, 133, 133, 255);
    constexpr SColor ARROW_COLOR(204, 204, 204, 255);

    // 分隔线
    constexpr SColor SEPARATOR(69, 69, 69, 255);

    // 尺寸（基于字体大小 × 比例系数计算，见 MenuPanel::m_fontSize / m_heightRatio）
    constexpr float DEFAULT_TEXT_SIZE = 20.0f;
    constexpr float DEFAULT_HEIGHT_RATIO = 1.6f;
    constexpr float MIN_HEIGHT_RATIO = 1.0f;
    constexpr float MAX_HEIGHT_RATIO = 3.0f;
    constexpr float ITEM_LEFT_PADDING   = 28.0f;
    constexpr float ITEM_RIGHT_PADDING  = 20.0f;
    constexpr float ICON_AREA_WIDTH     = 20.0f;
    constexpr float SHORTCUT_MIN_WIDTH  = 60.0f;
    constexpr float ARROW_AREA_WIDTH    = 20.0f;
    constexpr float SEPARATOR_HEIGHT    = 1.0f;
    constexpr float SEPARATOR_MARGIN    = 10.0f;
    constexpr float PANEL_RADIUS        = 5.0f;
    constexpr float PANEL_SHADOW_OFFSET = 3.0f;
    constexpr float PANEL_SHADOW_BLUR   = 8.0f;
    constexpr FontName MENU_FONT        = FontName::MapleMono_NF_CN_Regular;
}

namespace {

// 加载菜单字体（同 TreeView::ensureFont 模式）：从资源读取字体文件并经 TextRenderer 创建
SharedFont loadMenuFont(Control* ctl, FontName fontName, float fontSize) {
    TextRenderer* renderer = ctl->getTextRenderer();
    if (!renderer) return nullptr;
    ResourceProvider* provider = ctl->getResourceProvider();
    if (!provider) return nullptr;

    auto it = ConstDef::fontFiles.find(fontName);
    if (it == ConstDef::fontFiles.end()) return nullptr;
    string fontPath = ConstDef::pathPrefix.string() + "/" + it->second;
    auto data = provider->readFile(fontPath);
    if (!data || data->empty()) return nullptr;

    int scaledSize = static_cast<int>(fontSize * ctl->getScaleXX());
    return renderer->loadFontFromMemoryWithText(data->data(), data->size(), scaledSize, "W");
}

} // namespace

// ==================== MenuItem 实现 ====================

MenuItem::MenuItem(Control *parent, MenuItemType type, float xScale, float yScale)
    : ControlImpl(parent, xScale, yScale)
    , m_type(type)
    , m_caption("")
    , m_shortcut("")
    , m_checked(false)
    , m_onClick(nullptr)
    , m_subMenu(nullptr)
    , m_font(nullptr)
    , m_fontSize(MenuColors::DEFAULT_TEXT_SIZE)
{
    setRect(SRect(0, 0, 0, 0));
}

MenuItem::~MenuItem() = default;

void MenuItem::create() {
    ControlImpl::create();
}

void MenuItem::setMenuFont(SharedFont font, float fontSize) {
    m_font = std::move(font);
    m_fontSize = fontSize;
}

void MenuItem::setCaption(const string& caption) {
    m_caption = caption;
}

void MenuItem::setShortcut(const string& shortcut) {
    m_shortcut = shortcut;
}

void MenuItem::setChecked(bool checked) {
    m_checked = checked;
}

void MenuItem::setSubMenu(shared_ptr<MenuPanel> panel) {
    m_subMenu = panel;
    m_type = MenuItemType::SubMenu;
}

void MenuItem::draw() {
    if (!getVisible()) return;
    if (m_type == MenuItemType::Separator) return; // 分隔线由MenuPanel绘制

    SRect drawRect = getDrawRect();

    // 绘制勾选标记
    if (m_checked) {
        GET_RENDERDEVICE->setDrawColor(MenuColors::ITEM_TEXT);
        float cx = drawRect.left + MenuColors::ICON_AREA_WIDTH / 2.0f;
        float cy = drawRect.top + drawRect.height / 2.0f;
        GET_RENDERDEVICE->drawLine(cx - 4, cy, cx - 1, cy + 3);
        GET_RENDERDEVICE->drawLine(cx - 1, cy + 3, cx + 4, cy - 3);
    }

    TextRenderer* renderer = getTextRenderer();
    if (renderer && m_font) {
        int fontHeight = renderer->getFontHeight(m_font.get());
        float textY = drawRect.top + (drawRect.height - fontHeight) / 2;

        // 标题（左侧 padding 后开始）
        if (!m_caption.empty()) {
            renderer->drawText(m_font.get(), m_caption,
                drawRect.left + MenuColors::ITEM_LEFT_PADDING, textY, MenuColors::ITEM_TEXT);
        }

        // 快捷键（右侧右对齐，箭头区域之前）
        if (!m_shortcut.empty()) {
            SSize sz = renderer->measureText(m_font.get(), m_shortcut);
            float shortcutWidth = sz.width / getScaleXX();
            float right = drawRect.right();
            if (m_type == MenuItemType::SubMenu) right -= MenuColors::ARROW_AREA_WIDTH;
            renderer->drawText(m_font.get(), m_shortcut,
                right - MenuColors::ITEM_RIGHT_PADDING - shortcutWidth, textY, MenuColors::SHORTCUT_TEXT);
        }
    }

    // 子菜单箭头（最右侧，图形绘制不依赖字体字形）
    if (m_type == MenuItemType::SubMenu) {
        float cx = drawRect.right() - MenuColors::ARROW_AREA_WIDTH / 2.0f;
        float cy = drawRect.top + drawRect.height / 2.0f;
        float size = 4.0f;
        GET_RENDERDEVICE->drawTriangle(
            cx - size * 0.577f, cy - size,
            cx - size * 0.577f, cy + size,
            cx + size * 0.577f * 2, cy,
            MenuColors::ARROW_COLOR);
    }
}

bool MenuItem::handleEvent(shared_ptr<Event> event) {
    if (!getEnable() || !getVisible()) return false;
    if (m_type == MenuItemType::Separator) return false;

    float mx, my;
    bool gotPos = false;
    if (event->m_type == EventType::MouseMove) { mx = event->mousePos.x; my = event->mousePos.y; gotPos = true; }
    else if (event->m_type == EventType::MouseDown || event->m_type == EventType::MouseUp) {
        mx = event->mouseButton.x; my = event->mouseButton.y; gotPos = true;
    }
    if (gotPos) {
        SRect drawRect = getDrawRect();
        if (drawRect.contains(mx, my)) {
            if (event->m_type == EventType::MouseDown && event->mouseButton.button == MouseButton::Left) {
                if (m_type == MenuItemType::Normal && m_onClick) {
                    closeMenuChain();
                    m_onClick(dynamic_pointer_cast<MenuItem>(getThis()));
                    fireCCallback(PropertyNames::kEventClick, CCallbackData::None, nullptr);
                }
                return true;
            }
            return true;
        }
    }
    return false;
}

void MenuItem::closeMenuChain() {
    // 向上查找MenuPanel或MenuBar，关闭整个菜单链
    Control* p = getParent();
    while (p) {
        auto panel = dynamic_cast<MenuPanel*>(p);
        if (panel) {
            panel->closeWithChildren();
            // 继续向上查找MenuBar
            p = panel->getParent();
            continue;
        }
        auto bar = dynamic_cast<MenuBar*>(p);
        if (bar) {
            bar->closeAllMenus();
            bar->exitMenuMode();
            return;
        }
        p = p->getParent();
    }
}

// ==================== MenuPanel 实现 ====================

MenuPanel::MenuPanel(Control *parent, float xScale, float yScale)
    : ControlImpl(parent, xScale, yScale)
    , m_fontSize(MenuColors::DEFAULT_TEXT_SIZE)
    , m_heightRatio(MenuColors::DEFAULT_HEIGHT_RATIO)
    , m_itemHeight(m_fontSize * m_heightRatio)
    , m_iconAreaWidth(MenuColors::ICON_AREA_WIDTH)
    , m_shortcutAreaWidth(0)
    , m_arrowAreaWidth(MenuColors::ARROW_AREA_WIDTH)
    , m_hoveredIndex(-1)
    , m_visible(false)
    , m_openSubMenu(nullptr)
    , m_font(nullptr)
    , m_fontName(MenuColors::MENU_FONT)
    , m_bgColor(MenuColors::PANEL_BG)
    , m_borderColor(MenuColors::PANEL_BORDER)
    , m_hoverColor(MenuColors::ITEM_HOVER_BG)
    , m_separatorColor(MenuColors::SEPARATOR)
    , m_shadowRadius(MenuColors::PANEL_SHADOW_BLUR)
{
    setRect(SRect(0, 0, 0, 0));
    setBorderVisible(true);
}

MenuPanel::~MenuPanel() = default;

void MenuPanel::ensureFont() {
    if (m_font) return;
    m_font = loadMenuFont(this, m_fontName, m_fontSize);
}

void MenuPanel::updateItemsFont() {
    for (auto& item : m_items) {
        item->setMenuFont(m_font, m_fontSize);
    }
}

void MenuPanel::setFontSize(float size) {
    if (m_fontSize == size) return;
    m_fontSize = size;
    m_itemHeight = m_fontSize * m_heightRatio;
    m_font.reset();
    if (m_isCreated) {
        ensureFont();
        updateItemsFont();
    }
    recalculateSize();
}

void MenuPanel::setItemHeightRatio(float ratio) {
    if (ratio < MenuColors::MIN_HEIGHT_RATIO) ratio = MenuColors::MIN_HEIGHT_RATIO;
    if (ratio > MenuColors::MAX_HEIGHT_RATIO) ratio = MenuColors::MAX_HEIGHT_RATIO;
    if (m_heightRatio == ratio) return;
    m_heightRatio = ratio;
    m_itemHeight = m_fontSize * m_heightRatio;
    recalculateSize();
}

void MenuPanel::setFontName(FontName fontName) {
    if (m_fontName == fontName && m_font) return;
    m_fontName = fontName;
    m_font.reset();
    if (m_isCreated) {
        ensureFont();
        updateItemsFont();
    }
}

void MenuPanel::addItem(shared_ptr<MenuItem> item) {
    if (!item) return;
    ensureFont();
    item->setParent(this);
    item->setMenuFont(m_font, m_fontSize);
    m_items.push_back(item);
    recalculateSize();
}

void MenuPanel::addSeparator() {
    auto sep = make_shared<MenuItem>(this, MenuItemType::Separator);
    m_items.push_back(sep);
    recalculateSize();
}

void MenuPanel::removeItem(shared_ptr<MenuItem> item) {
    auto it = std::find(m_items.begin(), m_items.end(), item);
    if (it != m_items.end()) {
        m_items.erase(it);
        recalculateSize();
    }
}

void MenuPanel::show() {
    m_visible = true;
    setVisible(true);
}

void MenuPanel::hide() {
    m_visible = false;
    setVisible(false);
    if (m_openSubMenu) {
        m_openSubMenu->hide();
        m_openSubMenu = nullptr;
    }
    m_hoveredIndex = -1;
}

void MenuPanel::setPosition(float x, float y) {
    setRect(SRect(x, y, getRect().width, getRect().height));
    layoutItems();
}

void MenuPanel::recalculateSize() {
    if (m_items.empty()) {
        setRect(SRect(getRect().left, getRect().top, 0, 0));
        return;
    }

    // 计算最大宽度（需要容纳最长的标题+快捷键+箭头）
    float maxCaptionWidth = 0;
    float maxShortcutWidth = 0;
    bool hasSubMenu = false;

    TextRenderer* renderer = getTextRenderer();
    for (auto& item : m_items) {
        if (item->getType() == MenuItemType::Separator) continue;
        if (item->hasSubMenu()) hasSubMenu = true;

        // 获取标题宽度
        if (renderer && item->m_font && !item->m_caption.empty()) {
            float w = renderer->measureText(item->m_font.get(), item->m_caption).width / getScaleXX();
            if (w > maxCaptionWidth) maxCaptionWidth = w;
        }
        // 获取快捷键宽度
        if (renderer && item->m_font && !item->m_shortcut.empty()) {
            float w = renderer->measureText(item->m_font.get(), item->m_shortcut).width / getScaleXX();
            if (w > maxShortcutWidth) maxShortcutWidth = w;
        }
    }

    m_shortcutAreaWidth = (std::max)(maxShortcutWidth, MenuColors::SHORTCUT_MIN_WIDTH);

    // 计算面板宽度
    float panelWidth = MenuColors::ITEM_LEFT_PADDING
                     + maxCaptionWidth
                     + (m_shortcutAreaWidth > 0 ? m_shortcutAreaWidth + 10.0f : 0)
                     + (hasSubMenu ? m_arrowAreaWidth : 0)
                     + MenuColors::ITEM_RIGHT_PADDING;

    // 计算面板高度
    float panelHeight = 0;
    for (auto& item : m_items) {
        if (item->getType() == MenuItemType::Separator) {
            panelHeight += MenuColors::SEPARATOR_HEIGHT + 2 * MenuColors::SEPARATOR_MARGIN;
        } else {
            panelHeight += m_itemHeight;
        }
    }

    setRect(SRect(getRect().left, getRect().top, panelWidth, panelHeight));
    layoutItems();
}

void MenuPanel::layoutItems() {
    float y = 0;
    for (auto& item : m_items) {
        if (item->getType() == MenuItemType::Separator) {
            item->setRect(SRect(0, y, getRect().width,
                MenuColors::SEPARATOR_HEIGHT + 2 * MenuColors::SEPARATOR_MARGIN));
            y += item->getRect().height;
        } else {
            item->setRect(SRect(0, y, getRect().width, m_itemHeight));
            y += m_itemHeight;
        }
    }
}

int MenuPanel::hitTest(float x, float y) {
    SRect drawRect = getDrawRect();
    // 转换为局部坐标
    float localX = x - drawRect.left;
    float localY = y - drawRect.top;

    float itemY = 0;
    for (size_t i = 0; i < m_items.size(); ++i) {
        auto& item = m_items[i];
        float itemHeight = (item->getType() == MenuItemType::Separator)
            ? MenuColors::SEPARATOR_HEIGHT + 2 * MenuColors::SEPARATOR_MARGIN
            : m_itemHeight;

        if (localY >= itemY && localY < itemY + itemHeight) {
            return (item->getType() == MenuItemType::Separator) ? -1 : (int)i;
        }
        itemY += itemHeight;
    }
    return -1;
}

shared_ptr<MenuItem> MenuPanel::getItemAt(float x, float y) {
    int index = hitTest(x, y);
    if (index >= 0 && index < (int)m_items.size()) {
        return m_items[index];
    }
    return nullptr;
}

void MenuPanel::setOpenSubMenu(shared_ptr<MenuPanel> panel) {
    if (m_openSubMenu == panel) return;

    // 关闭之前的子菜单
    if (m_openSubMenu) {
        m_openSubMenu->hide();
    }
    m_openSubMenu = panel;
    if (m_openSubMenu) {
        m_openSubMenu->show();
    }
}

void MenuPanel::setHoveredIndex(int index) {
    if (m_hoveredIndex == index) return;

    m_hoveredIndex = index;

    // 如果hover到子菜单项，展开子菜单
    if (m_hoveredIndex >= 0 && m_hoveredIndex < (int)m_items.size()) {
        auto& item = m_items[m_hoveredIndex];
        if (item->hasSubMenu()) {
            SRect itemRect = item->getDrawRect();
            auto subMenu = item->getSubMenu();
            subMenu->setPosition(itemRect.right(), itemRect.top);
            setOpenSubMenu(subMenu);
        } else {
            setOpenSubMenu(nullptr);
        }
    } else {
        setOpenSubMenu(nullptr);
    }
}

int MenuPanel::setIntProperty(const char* prop, int value) {
    if (strcmp(prop, PropertyNames::kHoveredIndex) == 0) { setHoveredIndex(value); return 1; }
    return ControlImpl::setIntProperty(prop, value);
}

int MenuPanel::getIntProperty(const char* prop, int& out) {
    if (strcmp(prop, PropertyNames::kHoveredIndex) == 0) { out = m_hoveredIndex; return 1; }
    return ControlImpl::getIntProperty(prop, out);
}

void MenuPanel::closeWithChildren() {
    if (m_openSubMenu) {
        m_openSubMenu->closeWithChildren();
    }
    hide();
}

void MenuPanel::drawShadow() {
    SRect drawRect = getDrawRect();

    // 使用GraphTool绘制阴影
    GraphTool::DrawingContext dc(getRenderDevice());
    dc.setFillColor(GraphTool::SColor(
        MenuColors::PANEL_SHADOW.red(),
        MenuColors::PANEL_SHADOW.green(),
        MenuColors::PANEL_SHADOW.blue(),
        MenuColors::PANEL_SHADOW.alpha()));

    // 绘制阴影矩形（偏移+模糊区域）
    float offset = MenuColors::PANEL_SHADOW_OFFSET;
    float blur = MenuColors::PANEL_SHADOW_BLUR;
    dc.drawRoundedRect(
        ::SRect(drawRect.left + offset - blur/2,
                        drawRect.top + offset - blur/2,
                        drawRect.width + blur,
                        drawRect.height + blur),
        MenuColors::PANEL_RADIUS + blur/2, true);
}

void MenuPanel::draw() {
    if (!m_visible) return;

    SRect drawRect = getDrawRect();

    // 1. 绘制阴影
    drawShadow();

    // 2. 绘制背景（圆角矩形）
    {
        GraphTool::DrawingContext dc(getRenderDevice());
        dc.setFillColor(GraphTool::SColor(
            m_bgColor.red(), m_bgColor.green(),
            m_bgColor.blue(), m_bgColor.alpha()));
        dc.drawRoundedRect(::SRect(drawRect.left, drawRect.top,
            drawRect.width, drawRect.height), MenuColors::PANEL_RADIUS, true);
    }

    // 3. 绘制边框
    {
        GraphTool::DrawingContext dc(getRenderDevice());
        dc.setPen(GraphTool::SPen(
            GraphTool::SColor(m_borderColor.red(), m_borderColor.green(),
                             m_borderColor.blue(), m_borderColor.alpha()), 1.0f));
        dc.drawRoundedRect(::SRect(drawRect.left, drawRect.top,
            drawRect.width, drawRect.height), MenuColors::PANEL_RADIUS, false);
    }

    // 4. 绘制菜单项和分隔线（hover 背景统一由面板管理）
    for (size_t i = 0; i < m_items.size(); ++i) {
        auto& item = m_items[i];
        if (item->getType() == MenuItemType::Separator) {
            SRect itemRect = item->getDrawRect();
            SRect lineRect(
                drawRect.left + MenuColors::SEPARATOR_MARGIN,
                itemRect.top + MenuColors::SEPARATOR_MARGIN,
                drawRect.width - 2 * MenuColors::SEPARATOR_MARGIN,
                MenuColors::SEPARATOR_HEIGHT
            );
            GET_RENDERDEVICE->setDrawColor(m_separatorColor);
            GET_RENDERDEVICE->fillRect(lineRect);
        } else {
            if ((int)i == m_hoveredIndex) {
                SRect itemRect = item->getDrawRect();
                SRect bgRect(drawRect.left, itemRect.top, drawRect.width, itemRect.height);
                GET_RENDERDEVICE->setDrawColor(m_hoverColor);
                GET_RENDERDEVICE->fillRect(bgRect);
            }
            item->draw();
        }
    }

    // 5. 绘制打开的子菜单
    if (m_openSubMenu) {
        m_openSubMenu->draw();
    }
}

bool MenuPanel::handleEvent(shared_ptr<Event> event) {
    if (!m_visible) return false;

    // 先让子菜单处理事件
    if (m_openSubMenu && m_openSubMenu->handleEvent(event)) return true;

    float mx, my;
    bool gotPos = false;
    if (event->m_type == EventType::MouseMove) { mx = event->mousePos.x; my = event->mousePos.y; gotPos = true; }
    else if (event->m_type == EventType::MouseDown || event->m_type == EventType::MouseUp) {
        mx = event->mouseButton.x; my = event->mouseButton.y; gotPos = true;
    }
    if (gotPos) {
        SRect drawRect = getDrawRect();
        if (drawRect.contains(mx, my)) {
            int index = hitTest(mx, my);
            if (event->m_type == EventType::MouseMove) {
                setHoveredIndex(index);
                return true;
            }
            if (event->m_type == EventType::MouseDown && event->mouseButton.button == MouseButton::Left) {
                if (index >= 0) {
                    auto& item = m_items[index];
                    if (item->getType() == MenuItemType::Normal && item->m_onClick) {
                        item->closeMenuChain();
                        item->m_onClick(dynamic_pointer_cast<MenuItem>(item->getThis()));
                        fireCCallback(PropertyNames::kEventClick, CCallbackData::None, nullptr);
                        return true;
                    }
                }
                return true;
            }
            return true;
        }
    }
    return false;
}

bool MenuPanel::isContainsPoint(float x, float y) {
    if (!m_visible) return false;
    if (ControlImpl::isContainsPoint(x, y)) return true;
    if (m_openSubMenu && m_openSubMenu->isContainsPoint(x, y)) return true;
    return false;
}

// ==================== MenuBar 实现 ====================

MenuBar::MenuBar(Control *parent, float xScale, float yScale)
    : ControlImpl(parent, xScale, yScale)
    , m_barHeight(MenuColors::DEFAULT_TEXT_SIZE * MenuColors::DEFAULT_HEIGHT_RATIO)
    , m_hoveredIndex(-1)
    , m_activeIndex(-1)
    , m_menuMode(false)
    , m_bgColor(MenuColors::BAR_BG)
    , m_textColor(MenuColors::BAR_TEXT)
    , m_hoverBgColor(MenuColors::BAR_HOVER_BG)
    , m_hoverTextColor(MenuColors::BAR_TEXT)
    , m_activeBgColor(MenuColors::BAR_ACTIVE_BG)
    , m_itemHeightRatio(MenuColors::DEFAULT_HEIGHT_RATIO)
    , m_menuTextSize(MenuColors::DEFAULT_TEXT_SIZE)
    , m_font(nullptr)
    , m_fontName(MenuColors::MENU_FONT)
{
    setRect(SRect(0, 0, 0, m_barHeight));
    setBorderVisible(false);
}

MenuBar::~MenuBar() = default;

void MenuBar::ensureFont() {
    if (m_font) return;
    m_font = loadMenuFont(this, m_fontName, m_menuTextSize);
}

void MenuBar::setParent(Control *parent) {
    ControlImpl::setParent(parent);
    if (parent) {
        setRect(SRect(0, 0, parent->getRect().width, m_barHeight));
        layoutEntries();
    }
}

void MenuBar::setRect(SRect rect) {
    ControlImpl::setRect(rect);
    layoutEntries();
}

void MenuBar::addMenu(const string& caption, shared_ptr<MenuPanel> panel) {
    ensureFont();
    MenuEntry entry;
    entry.caption = caption;
    entry.panel = panel;
    entry.panel->setParent(this);
    entry.panel->setFontSize(m_menuTextSize);
    entry.panel->setItemHeightRatio(m_itemHeightRatio);
    entry.panel->hide();

    m_entries.push_back(std::move(entry));
    layoutEntries();
}

void MenuBar::removeMenu(const string& caption) {
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [&caption](const MenuEntry& e) { return e.caption == caption; });
    if (it != m_entries.end()) {
        m_entries.erase(it);
        layoutEntries();
    }
}

void MenuBar::layoutEntries() {
    float x = 0;
    TextRenderer* renderer = getTextRenderer();
    for (auto& entry : m_entries) {
        float entryWidth = 0;
        if (renderer && m_font) {
            float textWidth = renderer->measureText(m_font.get(), entry.caption).width / getScaleXX();
            entryWidth = textWidth + 2 * MenuColors::ITEM_LEFT_PADDING;
        }
        entry.hitRect = SRect(x, 0, entryWidth, m_barHeight);
        x += entryWidth;
    }
    // 更新菜单栏宽度
    if (getParent()) {
        ControlImpl::setRect(SRect(0, 0, getParent()->getRect().width, m_barHeight));
    }
}

int MenuBar::hitTest(float x, float y) {
    SRect drawRect = getDrawRect();
    float localX = x - drawRect.left;
    float localY = y - drawRect.top;

    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].hitRect.contains(localX, localY)) {
            return (int)i;
        }
    }
    return -1;
}

void MenuBar::openMenu(int index) {
    // 关闭当前打开的菜单
    if (m_activeIndex >= 0 && m_activeIndex < (int)m_entries.size()) {
        m_entries[m_activeIndex].panel->hide();
    }

    m_activeIndex = index;
    if (index >= 0 && index < (int)m_entries.size()) {
        auto& entry = m_entries[index];
        SRect hitRect = entry.hitRect;
        SRect drawRect = getDrawRect();
        // MenuPanel 是 MenuBar 的子控件，需使用相对 MenuBar 的坐标（扣除父链偏移）
        float offsetX = drawRect.left - getRect().left * getScaleXX();
        float offsetY = drawRect.top - getRect().top * getScaleYY();
        entry.panel->setPosition(offsetX + hitRect.left, offsetY + hitRect.top + hitRect.height);
        entry.panel->show();
    }
}

void MenuBar::switchMenu(int index) {
    if (m_activeIndex == index) return;
    openMenu(index);
}

void MenuBar::enterMenuMode(int index) {
    m_menuMode = true;
    openMenu(index);
}

void MenuBar::exitMenuMode() {
    m_menuMode = false;
    if (m_activeIndex >= 0 && m_activeIndex < (int)m_entries.size()) {
        m_entries[m_activeIndex].panel->hide();
    }
    m_activeIndex = -1;
    m_hoveredIndex = -1;
}

void MenuBar::closeAllMenus() {
    if (m_activeIndex >= 0 && m_activeIndex < (int)m_entries.size()) {
        m_entries[m_activeIndex].panel->closeWithChildren();
        m_entries[m_activeIndex].panel->hide();
    }
    m_activeIndex = -1;
}

void MenuBar::setBarHeight(float height) {
    m_barHeight = height;
    setRect(SRect(getRect().left, getRect().top, getRect().width, m_barHeight));
    layoutEntries();
}

void MenuBar::setItemHeightRatio(float ratio) {
    if (ratio < MenuColors::MIN_HEIGHT_RATIO) ratio = MenuColors::MIN_HEIGHT_RATIO;
    if (ratio > MenuColors::MAX_HEIGHT_RATIO) ratio = MenuColors::MAX_HEIGHT_RATIO;
    if (m_itemHeightRatio == ratio) return;
    m_itemHeightRatio = ratio;
    for (auto& entry : m_entries) {
        entry.panel->setItemHeightRatio(ratio);
    }
}

void MenuBar::setFontSize(float size) {
    if (m_menuTextSize == size) return;
    m_menuTextSize = size;
    m_font.reset();
    if (m_isCreated) ensureFont();
    for (auto& entry : m_entries) {
        entry.panel->setFontSize(size);
    }
    layoutEntries();
}

int MenuBar::setFloatProperty(const char* prop, float value) {
    if (strcmp(prop, PropertyNames::kItemHeightRatio) == 0) { setItemHeightRatio(value); return 1; }
    if (strcmp(prop, PropertyNames::kValue) == 0)           { setFontSize(value);        return 1; }
    if (strcmp(prop, PropertyNames::kBarHeight) == 0)       { setBarHeight(value);       return 1; }
    return ControlImpl::setFloatProperty(prop, value);
}

int MenuBar::setEnumProperty(const char* prop, const char* value) {
    if (strcmp(prop, PropertyNames::kFont) == 0) {
        FontName fn = FontNameFromString(value);
        if (m_fontName == fn) return 1;
        m_fontName = fn;
        m_font.reset();
        if (m_isCreated) ensureFont();
        for (auto& entry : m_entries) {
            entry.panel->setFontName(fn);
        }
        layoutEntries();
        return 1;
    }
    return ControlImpl::setEnumProperty(prop, value);
}

int MenuBar::getFloatProperty(const char* prop, float& out) {
    if (strcmp(prop, PropertyNames::kItemHeightRatio) == 0) { out = m_itemHeightRatio; return 1; }
    if (strcmp(prop, PropertyNames::kValue) == 0)           { out = m_menuTextSize;    return 1; }
    if (strcmp(prop, PropertyNames::kBarHeight) == 0)       { out = m_barHeight;       return 1; }
    return ControlImpl::getFloatProperty(prop, out);
}

int MenuBar::getEnumProperty(const char* prop, const char*& out) {
    if (strcmp(prop, PropertyNames::kFont) == 0) { out = FontNameToString(m_fontName); return 1; }
    return ControlImpl::getEnumProperty(prop, out);
}

void MenuBar::draw() {
    if (!getVisible()) return;

    SRect drawRect = getDrawRect();

    // 1. 绘制菜单栏背景
    GET_RENDERDEVICE->setDrawColor(m_bgColor);
    GET_RENDERDEVICE->fillRect(SRect(drawRect.left, drawRect.top, drawRect.width, drawRect.height));

    // 2. 绘制菜单项（背景 + 标题文本）
    TextRenderer* renderer = getTextRenderer();
    for (size_t i = 0; i < m_entries.size(); ++i) {
        auto& entry = m_entries[i];
        SRect hitRect = entry.hitRect;

        if ((int)i == m_activeIndex) {
            SRect itemBg(drawRect.left + hitRect.left, drawRect.top, hitRect.width, hitRect.height);
            GET_RENDERDEVICE->setDrawColor(m_activeBgColor);
            GET_RENDERDEVICE->fillRect(itemBg);
        } else if ((int)i == m_hoveredIndex) {
            SRect itemBg(drawRect.left + hitRect.left, drawRect.top, hitRect.width, hitRect.height);
            GET_RENDERDEVICE->setDrawColor(m_hoverBgColor);
            GET_RENDERDEVICE->fillRect(itemBg);
        }

        if (renderer && m_font) {
            SSize sz = renderer->measureText(m_font.get(), entry.caption);
            float textWidth = sz.width / getScaleXX();
            int fontHeight = renderer->getFontHeight(m_font.get());
            float textY = drawRect.top + (drawRect.height - fontHeight) / 2;
            float textX = drawRect.left + hitRect.left + (hitRect.width - textWidth) / 2;
            SColor color = ((int)i == m_hoveredIndex) ? m_hoverTextColor : m_textColor;
            renderer->drawText(m_font.get(), entry.caption, textX, textY, color);
        }
    }

    // 3. 绘制底部分隔线
    GET_RENDERDEVICE->setDrawColor(MenuColors::PANEL_BORDER);
    GET_RENDERDEVICE->drawLine(drawRect.left, drawRect.top + drawRect.height - 1,
                               drawRect.left + drawRect.width, drawRect.top + drawRect.height - 1);

    // 4. 绘制打开的下拉菜单
    if (m_activeIndex >= 0 && m_activeIndex < (int)m_entries.size()) {
        m_entries[m_activeIndex].panel->draw();
    }
}

bool MenuBar::handleEvent(shared_ptr<Event> event) {
    if (!getEnable() || !getVisible()) return false;

    // 先让打开的下拉菜单处理事件
    if (m_activeIndex >= 0 && m_activeIndex < (int)m_entries.size()) {
        if (m_entries[m_activeIndex].panel->handleEvent(event)) return true;
    }

    float mx, my;
    bool gotPos = false;
    if (event->m_type == EventType::MouseMove) { mx = event->mousePos.x; my = event->mousePos.y; gotPos = true; }
    else if (event->m_type == EventType::MouseDown || event->m_type == EventType::MouseUp) {
        mx = event->mouseButton.x; my = event->mouseButton.y; gotPos = true;
    }
    if (gotPos) {
        SRect drawRect = getDrawRect();
        int index = hitTest(mx, my);

        if (event->m_type == EventType::MouseDown && event->mouseButton.button == MouseButton::Left) {
            if (index >= 0) {
                if (m_menuMode && m_activeIndex == index) {
                    exitMenuMode();
                } else {
                    enterMenuMode(index);
                }
                return true;
            } else if (!drawRect.contains(mx, my)) {
                bool inPanel = false;
                if (m_activeIndex >= 0) {
                    inPanel = m_entries[m_activeIndex].panel->isContainsPoint(mx, my);
                }
                if (!inPanel) {
                    exitMenuMode();
                    return false;
                }
            }
        }

        if (event->m_type == EventType::MouseMove) {
            if (m_menuMode) {
                if (index >= 0 && index != m_activeIndex) {
                    switchMenu(index);
                }
                return true;
            } else {
                m_hoveredIndex = index;
            }
        }

        if (drawRect.contains(mx, my)) return true;
        if (m_menuMode && m_activeIndex >= 0) {
            if (m_entries[m_activeIndex].panel->isContainsPoint(mx, my)) {
                return true;
            }
        }
    }
    return false;
}

bool MenuBar::isContainsPoint(float x, float y) {
    if (ControlImpl::isContainsPoint(x, y)) return true;
    if (m_activeIndex >= 0 && m_activeIndex < (int)m_entries.size()) {
        if (m_entries[m_activeIndex].panel->isContainsPoint(x, y)) return true;
    }
    return false;
}

// ── MenuItem property system ──
int MenuItem::setBoolProperty(const char* prop, int value) {
    if (strcmp(prop, PropertyNames::kChecked) == 0) { setChecked(value != 0); return 1; }
    return ControlImpl::setBoolProperty(prop, value);
}
int MenuItem::setStringProperty(const char* prop, const char* value) {
    if (strcmp(prop, PropertyNames::kCaption) == 0)  { setCaption(value);  return 1; }
    if (strcmp(prop, PropertyNames::kShortcut) == 0) { setShortcut(value); return 1; }
    return ControlImpl::setStringProperty(prop, value);
}
int MenuItem::getBoolProperty(const char* prop, int& out) {
    if (strcmp(prop, PropertyNames::kChecked) == 0) { out = m_checked ? 1 : 0; return 1; }
    return ControlImpl::getBoolProperty(prop, out);
}
int MenuItem::getStringProperty(const char* prop, const char*& out) {
    if (strcmp(prop, PropertyNames::kCaption) == 0)  { out = m_caption.c_str();  return 1; }
    if (strcmp(prop, PropertyNames::kShortcut) == 0) { out = m_shortcut.c_str(); return 1; }
    return ControlImpl::getStringProperty(prop, out);
}
int MenuItem::setCallbackProperty(const char* event, void (*cb)(void*, const void*, void*), void* userData) {
    return ControlImpl::setCallbackProperty(event, cb, userData);
}

// ==================== Builder实现 ====================

MenuItemBuilder::MenuItemBuilder(const string& caption, float xScale, float yScale)
    : m_item(make_shared<MenuItem>(nullptr, MenuItemType::Normal, xScale, yScale))
{
    m_item->setCaption(caption);
}

MenuItemBuilder& MenuItemBuilder::setShortcut(const string& shortcut) {
    m_item->setShortcut(shortcut);
    return *this;
}

MenuItemBuilder& MenuItemBuilder::setOnClick(MenuItem::OnClickHandler handler) {
    m_item->setOnClick(handler);
    return *this;
}

MenuItemBuilder& MenuItemBuilder::setChecked(bool checked) {
    m_item->setChecked(checked);
    return *this;
}

MenuItemBuilder& MenuItemBuilder::setSubMenu(shared_ptr<MenuPanel> panel) {
    m_item->setSubMenu(panel);
    return *this;
}

MenuItemBuilder& MenuItemBuilder::setEnabled(bool enabled) {
    m_item->setEnable(enabled);
    return *this;
}

MenuItemBuilder& MenuItemBuilder::setBackgroundStateColor(StateColor stateColor) {
    m_item->setBackgroundStateColor(stateColor);
    return *this;
}

MenuItemBuilder& MenuItemBuilder::setTextStateColor(StateColor stateColor) {
    m_item->setTextStateColor(stateColor);
    return *this;
}

shared_ptr<MenuItem> MenuItemBuilder::build() {
    m_item->create();
    return m_item;
}

MenuPanelBuilder::MenuPanelBuilder(float xScale, float yScale)
    : m_panel(make_shared<MenuPanel>(nullptr, xScale, yScale))
{
}

MenuPanelBuilder& MenuPanelBuilder::addItem(shared_ptr<MenuItem> item) {
    m_items.push_back(item);
    return *this;
}

MenuPanelBuilder& MenuPanelBuilder::addSeparator() {
    m_items.push_back(nullptr); // 用nullptr标记分隔线
    return *this;
}

shared_ptr<MenuPanel> MenuPanelBuilder::build() {
    m_panel->create();
    for (auto& item : m_items) {
        if (item) {
            m_panel->addItem(item);
        } else {
            m_panel->addSeparator();
        }
    }
    return m_panel;
}

MenuBarBuilder::MenuBarBuilder(Control *parent, float xScale, float yScale)
    : m_menuBar(make_shared<MenuBar>(parent, xScale, yScale))
{
}

MenuBarBuilder& MenuBarBuilder::addMenu(const string& caption, shared_ptr<MenuPanel> panel) {
    m_menuBar->addMenu(caption, panel);
    return *this;
}

MenuBarBuilder& MenuBarBuilder::setBarHeight(float height) {
    m_menuBar->setBarHeight(height);
    return *this;
}

MenuBarBuilder& MenuBarBuilder::setBackgroundStateColor(StateColor stateColor) {
    m_menuBar->setBackgroundStateColor(stateColor);
    return *this;
}

MenuBarBuilder& MenuBarBuilder::setTextStateColor(StateColor stateColor) {
    m_menuBar->setTextStateColor(stateColor);
    return *this;
}

shared_ptr<MenuBar> MenuBarBuilder::build() {
    m_menuBar->create();
    return m_menuBar;
}
