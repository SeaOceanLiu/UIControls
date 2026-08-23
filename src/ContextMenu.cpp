// ============================================================================
// ContextMenu.cpp -- 右键上下文菜单（Popup 浮层 + MenuPanel 内容）
// ============================================================================
#include "ContextMenu.h"
#include "Bench.h"
#include "MainWindow.h"

ContextMenu::ContextMenu(Control* parent, float xScale, float yScale)
    : Popup(parent, SRect(0, 0, 10, 10), xScale, yScale)
{
    m_ctlType = ControlType::Popup;
    m_menuPanel = make_shared<MenuPanel>(this, xScale, yScale);
    setContent(m_menuPanel);
}

void ContextMenu::show(float x, float y) {
    // 确保上下文就绪（未挂树的浮层借最近实例）
    if (GET_CONTEXT == nullptr) {
        UIContext* ctx = UIContext::getLastInstance();
        if (ctx) setContext(ctx);
    }
    if (!m_menuPanel) return;

    if (!m_menuPanel->isCreated()) m_menuPanel->create();
    m_menuPanel->recalculateSize();     // 第一遍（可能走兜底估算宽度）

    Popup::open();                      // 挂树 + 创建 + 布局 + 显示 + 注册 watcher

    // 第二遍：context 就绪后按真实字体重算尺寸，并据视口钳制
    m_menuPanel->recalculateSize();
    float w = m_menuPanel->getRect().width;
    float h = m_menuPanel->getRect().height;

    SRect vp = GET_CONTEXT ? GET_CONTEXT->viewport : SRect(0, 0, 1024, 768);
    float px = x, py = y;
    if (px + w > vp.left + vp.width)  px = vp.left + vp.width  - w;
    if (py + h > vp.top + vp.height)  py = vp.top + vp.height - h;
    if (px < vp.left) px = vp.left;
    if (py < vp.top)  py = vp.top;

    setRect(SRect(px, py, w, h));
    m_menuPanel->setRect(SRect(0, 0, w, h));
    m_menuPanel->layoutItems();
}

void ContextMenu::close(DialogResult result) {
    if (m_menuPanel) m_menuPanel->hide();
    Popup::close(result);
}

void ContextMenu::addItem(shared_ptr<MenuItem> item) {
    if (!item || !m_menuPanel) return;
    auto orig = item->getOnClick();
    item->setOnClick([this, orig](shared_ptr<MenuItem> it) {
        if (orig) orig(it);
        this->close();
    });
    m_menuPanel->addItem(item);
}

void ContextMenu::addItem(const string& caption, ItemClickHandler onClick) {
    auto item = make_shared<MenuItem>(m_menuPanel.get(), MenuItemType::Normal, 1.f, 1.f);
    item->setCaption(caption);
    if (onClick) {
        item->setOnClick([this, onClick](shared_ptr<MenuItem> it) {
            onClick(it);
            this->close();
        });
    } else {
        item->setOnClick([this](shared_ptr<MenuItem>) { this->close(); });
    }
    m_menuPanel->addItem(item);
}

void ContextMenu::addSeparator() {
    if (m_menuPanel) m_menuPanel->addSeparator();
}

bool ContextMenu::isContainsPoint(float x, float y) {
    if (ControlImpl::isContainsPoint(x, y)) return true;
    if (m_menuPanel && m_menuPanel->isContainsPoint(x, y)) return true;
    return false;
}

void ContextMenu::layoutContent() {
    if (!m_menuPanel) return;
    m_menuPanel->setRect(SRect(0, 0, m_rect.width, m_rect.height));
    m_menuPanel->layoutItems();
    m_menuPanel->show();
}
