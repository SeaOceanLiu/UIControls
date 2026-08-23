// ============================================================================
// StatusBar.h -- 状态栏控件（VSCode 风格底部状态栏）
// 设计：design/StatusBar_Design.md（决策点 1-6 已拍板）
// 弹窗：内嵌共享 MenuPanel,点击 item 向上弹出（MenuBar 反向）,外部点击关闭
// ============================================================================
#pragma once

#include "ControlBase.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

using std::string;
using std::vector;
using std::shared_ptr;
using std::function;

class MenuPanel;

struct StatusItem {
    std::string id;
    std::string text;
    bool rightAlign = false;
    std::shared_ptr<Control> leadingControl;
    std::function<void(std::shared_ptr<StatusItem>)> onClick;
    std::shared_ptr<MenuPanel> menuPanel;
    SRect hitRect;
};

class StatusBar : public ControlImpl {
public:
    StatusBar(Control* parent, const SRect& rect, float xScale = 1.0f, float yScale = 1.0f);

    // ── 数据操作 ──
    void addStatusItem(const string& id, const string& text, bool rightAlign = false);
    void updateStatusItemText(const string& id, const string& text);
    void removeStatusItem(const string& id);
    void setStatusItemMenu(const string& id, shared_ptr<class MenuPanel> panel);
    void setStatusItemLeadingControl(const string& id, shared_ptr<Control> ctl);
    void setStatusItemOnClick(const string& id, function<void(shared_ptr<StatusItem>)> cb);

    StatusItem* getStatusItem(const string& id);
    shared_ptr<class MenuPanel> getPopupPanel() const { return m_popupPanel; }
    bool isPopupOpen() const;

    // ── 属性 setter（§5.1 矩阵,全部触发 relayout）──
    void setFontSize(float size);
    void setItemHeight(float px);

    // ── 查询 ──
    float getFontSize() const { return m_fontSize; }
    float getItemHeight() const { return m_itemHeight; }

    // ── 引擎接口 ──
    void draw(void) override;
    bool handleEvent(shared_ptr<Event> event) override;
    void setRect(SRect rect) override;

    // ── 属性系统 override ──
    int setFloatProperty(const char* prop, float value) override;   // font-size/item-height
    int getFloatProperty(const char* prop, float& out) override;

private:
    friend class StatusBarBuilder;

    void relayout();
    void updateItem(int index);
    void ensureFont();
    int hitTestIndex(float x) const;              // 命中 item 索引；-1 未命中
    void openPopup(int itemIndex);
    void closePopup();

    vector<StatusItem> m_items;
    shared_ptr<class MenuPanel> m_popupPanel;     // 共享弹窗（惰性创建）
    int m_hoveredItem = -1;
    float m_fontSize = 13.0f;
    float m_itemHeight = 24.0f;
    float m_spacing = 8.0f;
    float m_padding = 12.0f;
    SharedFont m_font;
};
